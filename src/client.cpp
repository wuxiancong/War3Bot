#include "client.h"
#include "logger.h"
#include "bnethash.h"
#include "bnetsrp3.h"
#include "calculate.h"
#include "bncsutil/checkrevision.h"

#include <QDir>
#include <QtEndian>
#include <QFileInfo>
#include <QDateTime>
#include <QTextCodec>
#include <QDataStream>
#include <QRandomGenerator>
#include <QCoreApplication>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QCryptographicHash>
#include <QRegularExpression>

#include <zlib.h>

// =========================================================
// 1. 生命周期 (构造与析构)
// =========================================================

Client::Client(QObject *parent)
    : QObject(parent)
    , m_mapSize(0)
    , m_srp(nullptr)
    , m_udpSocket(nullptr)
    , m_tcpSocket(nullptr)
    , m_loginProtocol(Protocol_Old_0x29)
{
    // 1. 打印根节点
    LOG_INFO("🧩 [Client] 实例初始化启动");

    m_pingTimer = new QTimer(this);
    m_udpSocket = new QUdpSocket(this);
    m_tcpServer = new QTcpServer(this);
    m_tcpSocket = new QTcpSocket(this);

    m_startTimer = new QTimer(this);
    m_startTimer->setSingleShot(true);

    m_startLagTimer = new QTimer(this);
    m_startLagTimer->setSingleShot(true);

    m_gameTickTimer = new QTimer(this);
    m_gameTickTimer->setInterval(m_gameTickInterval);

    // 2. 信号槽连接
    connect(m_pingTimer, &QTimer::timeout, this, &Client::sendPingLoop);
    connect(m_startTimer, &QTimer::timeout, this, &Client::onGameStarted);
    connect(m_gameTickTimer, &QTimer::timeout, this, &Client::onGameTick);
    connect(m_startLagTimer, &QTimer::timeout, this, &Client::onStartLagFinished);

    connect(m_tcpSocket, &QTcpSocket::connected, this, &Client::onConnected);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &Client::onTcpReadyRead);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &Client::onDisconnected);
    connect(m_tcpServer, &QTcpServer::newConnection, this, &Client::onNewConnection);
    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError){
        LOG_ERROR(QString("战网连接错误: %1").arg(m_tcpSocket->errorString()));
    });
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &Client::onUdpReadyRead);

    LOG_INFO("   ├─ ⚙️ 环境构建: 定时器/Socket对象已创建，信号已连接");

    // 初始化 UDP
    if (!bindToRandomPort()) {
        LOG_ERROR("   ├─ ❌ 网络绑定: 随机端口绑定失败");
    } else {
        LOG_INFO(QString("   ├─ 📡 网络绑定: TCP/UDP 监听端口 %1").arg(m_udpSocket->localPort()));
    }

    // 3. 资源路径搜索
    QStringList searchPaths;
    searchPaths << QCoreApplication::applicationDirPath() + "/war3files";
#ifdef Q_OS_LINUX
    searchPaths << "/opt/War3Bot/war3files";
#endif
    searchPaths << QDir::currentPath() + "/war3files";
    searchPaths << QCoreApplication::applicationDirPath();

    LOG_INFO("   └─ 🔍 资源扫描: War3 核心文件检查");

    for (const QString &pathStr : qAsConst(searchPaths)) {
        QDir dir(pathStr);
        // 尝试寻找 War3.exe
        if (dir.exists("War3.exe")) {
            // --- 🎯 找到 War3 核心 ---
            m_war3ExePath = dir.absoluteFilePath("War3.exe");
            m_gameDllPath = dir.absoluteFilePath("Game.dll");
            m_stormDllPath = dir.absoluteFilePath("Storm.dll");
            m_dota683dPath = dir.absoluteFilePath("maps/DotA v6.83d.w3x");

            // 设置默认地图
            m_currentMapPath = m_dota683dPath;

            LOG_INFO(QString("      ├─ ✅ 命中路径: %1").arg(dir.absolutePath()));

            // --- 🗺️ 检查默认地图 ---
            if (QFile::exists(m_dota683dPath)) {
                LOG_INFO(QString("      └─ 🗺️ 发现地图: %1").arg(m_dota683dPath));

                // 尝试加载地图
                if (m_war3Map.load(m_dota683dPath)) {
                    m_mapData = m_war3Map.getMapRawData();
                    m_mapSize = (quint32)m_mapData.size();

                    LOG_INFO(QString("         └─ ✅ 加载成功: %1 bytes").arg(m_mapSize));
                } else {
                    // 地图坏了
                    m_mapSize = 0;
                    LOG_ERROR(QString("         └─ ❌ 加载失败: 格式错误或文件损坏"));
                }
            } else {
                // War3 找到了，但没地图
                m_mapSize = 0;
                LOG_INFO(QString("      └─ ⚠️ 地图缺失: %1 (下载功能将不可用)").arg(m_dota683dPath));
            }

            // 既然找到了 War3，就不需要继续循环了，直接跳出
            break;
        }
    }

    // 4. 最终检查
    if (m_war3ExePath.isEmpty()) {
        LOG_CRITICAL("      └─ ❌ 致命错误: 未能找到 War3.exe！");
        LOG_INFO("         ├─ 请确保 'war3files' 目录存在于程序运行目录");
        LOG_INFO("         └─ 已扫描路径:");
        for(const QString &p : qAsConst(searchPaths)) {
            LOG_INFO(QString("            • %1").arg(p));
        }
    }
}

Client::~Client()
{
    disconnectFromHost();
    if (m_srp) {
        delete m_srp;
        m_srp = nullptr;
    }
    if (m_tcpSocket) {
        m_tcpSocket->close();
        delete m_tcpSocket;
    }
    if (m_udpSocket) {
        m_udpSocket->close();
        delete m_udpSocket;
    }
}

// =========================================================
// 2. 连接控制与配置
// =========================================================

void Client::setCredentials(const QString &user, const QString &pass, LoginProtocol protocol)
{
    m_user = user.trimmed();
    m_pass = pass.trimmed();
    m_loginProtocol = protocol;

    QString protoName;
    if (protocol == Protocol_Old_0x29) protoName = "Old (0x29)";
    else if (protocol == Protocol_Logon2_0x3A) protoName = "Logon2 (0x3A)";
    else protoName = "SRP (0x53)";

    // 树状日志
    LOG_INFO("🔧 [配置设定] 更新凭据");
    LOG_INFO(QString("   ├─ 👤 用户: %1").arg(m_user));
    LOG_INFO(QString("   ├─ 🔑 密码: %1").arg(m_pass));
    LOG_INFO(QString("   └─ 📡 协议: %1").arg(protoName));
}

void Client::connectToHost(const QString &address, quint16 port)
{
    m_serverAddr = address;
    m_serverPort = port;

    // 树状日志
    LOG_INFO("🔌 [网络请求] 发起战网连接");
    LOG_INFO(QString("   └─ 🎯 目标: %1:%2").arg(address).arg(port));

    m_tcpSocket->connectToHost(address, port);
}

void Client::disconnectFromHost()
{
    m_tcpSocket->disconnectFromHost();
}

bool Client::isConnected() const
{
    return m_tcpSocket->state() == QAbstractSocket::ConnectedState;
}

bool Client::isConnecting() const
{
    return m_tcpSocket->state() == QAbstractSocket::HostLookupState ||
           m_tcpSocket->state() == QAbstractSocket::ConnectingState;
}

void Client::onDisconnected()
{
    LOG_INFO("🔌 [网络状态] 战网连接断开");
    LOG_INFO("   └─ ⚠️ 状态: Disconnected");

    emit disconnected();
}

void Client::onConnected()
{
    // 树状日志
    LOG_INFO("✅ [网络状态] TCP 链路已建立");
    LOG_INFO("   ├─ 🤝 握手: 发送协议字节 (0x01)");
    LOG_INFO("   └─ 🚀 动作: 发送 AuthInfo -> 触发 connected 信号");

    char protocolByte = 1;
    m_tcpSocket->write(&protocolByte, 1);
    sendAuthInfo();
    emit connected();
}

void Client::onNewConnection()
{
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        if (!socket) continue;

        if (m_gameStarted || m_playerSockets.size() >= 15) {
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }

        socket->setParent(this);

        QString peerAddr = socket->peerAddress().toString();
        quint16 peerPort = socket->peerPort();

        LOG_INFO(QString("🎮 [玩家连接] 来源: %1:%2").arg(peerAddr).arg(peerPort));

        m_playerSockets.append(socket);
        m_playerBuffers.insert(socket, QByteArray());

        connect(socket, &QTcpSocket::readyRead, this, &Client::onPlayerReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &Client::onPlayerDisconnected);
        connect(socket, &QAbstractSocket::errorOccurred, this, [peerAddr, peerPort](QAbstractSocket::SocketError err){
            LOG_ERROR(QString("❗ [Socket错误] %1:%2 | 错误码: %3").arg(peerAddr).arg(peerPort).arg(err));
        });
    }
}

// =========================================================
// 3. TCP 核心处理 (收发包)
// =========================================================

void Client::sendPacket(BNETPacketID id, const QByteArray &payload)
{
    if (!m_tcpSocket) {
        LOG_ERROR("❌ 发送失败: Socket 未初始化");
        return;
    }

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // BNET 协议头 (通常是 0xFF)
    out << (quint8)BNET_HEADER;
    out << (quint8)id;
    out << (quint16)(payload.size() + 4);

    if (!payload.isEmpty()) {
        packet.append(payload);
    }

    m_tcpSocket->write(packet);

    // 1. 获取可读名称
    QString packetName = getBnetPacketName(id);
    QString idHex = QString("0x%1").arg((quint8)id, 2, 16, QChar('0')).toUpper();

    // 2. 格式化 Hex
    QString hexData = packet.toHex().toUpper();
    QString formattedHex;
    int maxPreviewBytes = 256;

    int previewLen = qMin(packet.size(), maxPreviewBytes);
    for (int i = 0; i < previewLen; ++i) {
        formattedHex += hexData.mid(i * 2, 2) + " ";
    }

    if (packet.size() > maxPreviewBytes) {
        formattedHex += "... (Total " + QString::number(packet.size()) + " bytes)";
    } else {
        formattedHex = formattedHex.trimmed();
    }

    // 3. 树状输出
    LOG_INFO("📤 [BNET 协议发送]");
    LOG_INFO(QString("   ├─ 🆔 指令: %1 [%2]").arg(packetName, idHex));
    LOG_INFO(QString("   ├─ 📏 长度: %1 字节 (Payload: %2)").arg(packet.size()).arg(payload.size()));
    LOG_INFO(QString("   └─ 📦 数据: %1").arg(formattedHex));
}

void Client::initiateMapDownload(quint8 pid)
{
    // 1. 安全检查
    if (!m_players.contains(pid)) return;
    PlayerData &playerData = m_players[pid];
    QTcpSocket* socket = playerData.socket;

    LOG_INFO(QString("🚀 [下载流程] 触发初始化/重置下载 [pID: %1]").arg(pid));

    // --- 步骤 A: 发送开始信号 (0x3F) ---
    // 告诉客户端：你是下载者，去那个位置准备接收
    socket->write(createW3GSStartDownloadPacket(m_botPid));

    // --- 步骤 B: 更新大厅槽位状态 (0x09) ---
    // 立即发一个 SlotInfo 更新状态
    socket->write(createW3GSSlotInfoPacket());

    // 强制刷入网络，确保客户端先收到这两个控制包
    socket->flush();

    // --- 步骤 C: 准备状态 ---
    playerData.currentDownloadOffset    = 0;
    playerData.lastDownloadOffset       = 0;
    playerData.bytesSentInWindow        = 0;
    playerData.bytesSentThisSecond      = 0;
    playerData.isDownloadStart          = true;
    playerData.downloadStartTime        = QDateTime::currentMSecsSinceEpoch();
    playerData.lastSpeedUpdateTime      = playerData.downloadStartTime;
    playerData.secondStartTime          = playerData.downloadStartTime;

    // --- 步骤 D: 立即发送第一波数据 ---
    sendNextMapPart(pid);

    LOG_INFO(QString("   └─ 📤 初始序列完成，首块数据已发送"));
}

void Client::sendNextMapPart(quint8 toPid, quint8 fromPid)
{
    if (!m_players.contains(toPid)) return;
    PlayerData &playerData = m_players[toPid];

    if (!playerData.isDownloadStart || m_mapSize == 0) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    quint32 maxBytesPerSecond = (m_maxDownloadSpeed == 0) ? 0xFFFFFFFF : (m_maxDownloadSpeed * 1024);

    if (now - playerData.secondStartTime >= 1000) {
        playerData.secondStartTime = now;
        playerData.bytesSentThisSecond = 0;
    }

    if (playerData.bytesSentThisSecond >= maxBytesPerSecond) {
        QTimer::singleShot(300, this, [this, toPid, fromPid](){ sendNextMapPart(toPid, fromPid); });
        return;
    }

    while (playerData.socket->bytesToWrite() < 64 * 1024)
    {
        if (playerData.bytesSentThisSecond >= maxBytesPerSecond) break;

        int chunkSize = MAX_CHUNK_SIZE;
        if (playerData.currentDownloadOffset + chunkSize > m_mapSize) {
            chunkSize = m_mapSize - playerData.currentDownloadOffset;
        }

        if (chunkSize <= 0) break;

        QByteArray chunk = m_mapData.mid(playerData.currentDownloadOffset, chunkSize);
        QByteArray packet = createW3GSMapPartPacket(toPid, fromPid, playerData.currentDownloadOffset, chunk);

        if (playerData.socket->write(packet) > 0) {
            playerData.currentDownloadOffset += chunkSize;
            playerData.bytesSentThisSecond += packet.size();
            playerData.bytesSentInWindow += packet.size();
        } else {
            playerData.isDownloadStart = false;
            return;
        }
    }
}

void Client::onTcpReadyRead()
{
    while (m_tcpSocket->bytesAvailable() > 0) {
        if (m_tcpSocket->bytesAvailable() < 4) return;

        QByteArray headerData = m_tcpSocket->peek(4);
        if ((quint8)headerData[0] != BNET_HEADER) {
            LOG_ERROR(QString("⚠️ [网络] 发现非法协议头: 0x%1，跳过该字节").arg((quint8)headerData[0], 2, 16, QChar('0')));
            m_tcpSocket->read(1);
            continue;
        }

        quint16 length;
        QDataStream lenStream(headerData.mid(2, 2));
        lenStream.setByteOrder(QDataStream::LittleEndian);
        lenStream >> length;
        LOG_DEBUG(QString("📥 [TCP流] 捕获包头: ID=0x%1, 声明长度=%2, 当前缓冲区=%3")
                      .arg((quint8)headerData[1], 2, 16, QChar('0')).arg(length).arg(m_tcpSocket->bytesAvailable()));

        if (m_tcpSocket->bytesAvailable() < length) return;

        QByteArray packetData = m_tcpSocket->read(length);
        quint8 packetIdVal = (quint8)packetData[1];
        handleBNETTcpPacket((BNETPacketID)packetIdVal, packetData.mid(4));
    }
}

void Client::handleBNETTcpPacket(BNETPacketID id, const QByteArray &data)
{
    // 忽略心跳包的日志，避免刷屏
    if (id != SID_PING) {
        // 1. 打印根节点 (包名 + ID)
        QString packetName = getBnetPacketName(id);
        LOG_INFO(QString("📥 [BNET] 收到数据包: %1 (0x%2)")
                     .arg(packetName, QString::number(id, 16).toUpper()));
    }

    switch (id) {
    case SID_PING:
    {
        if (data.size() < 4) return;
        quint32 pingValue;
        QDataStream ds(data);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds >> pingValue;

        // Debug 级别，平时不可见
        LOG_DEBUG(QString("💓 [Ping] Value: %1 -> 回应 Pong").arg(pingValue));
        sendPacket(SID_PING, data);
    }
    break;

    case SID_ENTERCHAT:
    {
        LOG_INFO(QString("   └─ [协议追踪] Bot-%1 收到进入聊天确认 (0x0A)").arg(m_user));
        emit enteredChat();
        queryChannelList();
    }
    break;

    case SID_GETCHANNELLIST: // 0x0B
    {
        LOG_INFO(QString("   └─ [协议追踪] Bot-%1 收到获取频道列表(0x0B)").arg(m_user));
        m_channelList.clear();
        int offset = 0;
        while (offset < data.size()) {
            int strEnd = data.indexOf('\0', offset);
            if (strEnd == -1) {
                if (offset < data.size()) {
                    QString lastStr = QString::fromUtf8(data.mid(offset));
                    if (!lastStr.isEmpty()) m_channelList.append(lastStr);
                }
                break;
            }
            QByteArray rawStr = data.mid(offset, strEnd - offset);
            QString channelName = QString::fromUtf8(rawStr);
            if (!channelName.isEmpty()) m_channelList.append(channelName);
            offset = strEnd + 1;
        }

        LOG_INFO(QString("   ├─ 📋 频道列表: 共 %1 个").arg(m_channelList.size()));

        // 打印前几个频道作为示例，防止列表太长刷屏
        int printLimit = qMin(m_channelList.size(), 3);
        for(int i=0; i<printLimit; ++i) {
            LOG_INFO(QString("   │  ├─ %1").arg(m_channelList[i]));
        }
        if (m_channelList.size() > printLimit) {
            LOG_INFO(QString("   │  └─ ... (还有 %1 个)").arg(m_channelList.size() - printLimit));
        }

        if (m_channelList.isEmpty()) {
            LOG_INFO("   └─ ⚠️ [异常] 列表为空 -> 加入默认频道 'The Void'");
            joinChannel("The Void");
        }
        else {
            QString target;
            if (m_isBot) {
                int index = QRandomGenerator::global()->bounded(m_channelList.size());
                target = m_channelList.at(index);
                LOG_INFO(QString("   └─ 🎲 [Bot随机] 选中频道: %1").arg(target));
            }
            else {
                target = m_channelList.first();
                LOG_INFO(QString("   └─ ➡️ [默认] 加入首个频道: %1").arg(target));
            }
            joinChannel(target);
        }
    }
    break;

    case SID_CHATCOMMAND: // 0x0E
    {
        LOG_INFO(QString("   └─ [协议追踪] Bot-%1 收到聊天命令消息(0x0E)").arg(m_user));
        QString cmdText = QString::fromUtf8(data).trimmed();
        QString hexData = data.toHex(' ').toUpper();
        LOG_INFO("💬 [BNET] 捕获到聊天/指令包 (SID_CHATCOMMAND - 0x0E)");
        LOG_INFO(QString("   ├─ 📦 原始 Hex: %1").arg(hexData));
        LOG_INFO(QString("   ├─ 📝 文本内容: %1").arg(cmdText.isEmpty() ? "<空>" : cmdText));
        LOG_INFO("   └─ ℹ️ 状态: 仅记录日志，跳过逻辑处理");
    }
    break;

    case SID_CHATEVENT:
    {
        LOG_INFO(QString("   └─ [协议追踪] Bot-%1 收到聊天事件消息(0x0F)").arg(m_user));
        if (data.size() < 24) return;

        QDataStream in(data);
        in.setByteOrder(QDataStream::LittleEndian);
        quint32 eventId, flags, ping, ipAddress, accountNum, regAuthority;
        in >> eventId >> flags >> ping >> ipAddress >> accountNum >> regAuthority;

        int currentOffset = 24;
        auto readString = [&](int &offset) -> QString {
            if (offset >= data.size()) return QString();
            int end = data.indexOf('\0', offset);
            if (end == -1) return QString();
            QString s = QString::fromUtf8(data.mid(offset, end - offset));
            offset = end + 1;
            return s;
        };

        QString username = readString(currentOffset);
        QString text = readString(currentOffset);

        // 显示事件类型
        LOG_INFO(QString("   ├─ 🎫 事件ID: 0x%1").arg(QString::number(eventId, 16).toUpper()));
        LOG_INFO(QString("   ├─ 👤 用户名: %1").arg(username));

        // 指令捕获逻辑
        if (text.startsWith("/")) {
            LOG_INFO(QString("   ├─ ⚡ [指令捕获] %1").arg(text));
        }

        // 分类日志输出
        QString contentLog;
        switch (eventId) {
        case 0x01: contentLog = QString("用户展示 (Ping: %1)").arg(ping); break;
        case 0x02: contentLog = "加入频道"; break;
        case 0x03: contentLog = "离开频道"; break;
        case 0x04: contentLog = QString("来自私聊: %1").arg(text); break;
        case 0x05: contentLog = QString("频道发言: %1").arg(text); break;
        case 0x06: contentLog = QString("系统广播: %1").arg(text); break;
        case 0x07: contentLog = QString("进入频道: %1").arg(text); break;
        case 0x09: contentLog = QString("状态更新 (Flags: %1)").arg(QString::number(flags, 16)); break;
        case 0x0A: contentLog = QString("发送私聊 -> %1").arg(text); break;
        case 0x12: contentLog = QString("Info: %1").arg(text); break;
        case 0x13: contentLog = QString("Error: %1").arg(text); break; // 错误特殊处理
        case 0x17: contentLog = QString("表情: %1").arg(text); break;
        default:   contentLog = "未知事件"; break;
        }

        if (eventId == 0x13) {
            LOG_INFO(QString("📧 [系统消息] %1").arg(text));
            LOG_INFO(QString("   └─ 📝 内容: %1").arg(contentLog));
        } else {
            LOG_INFO(QString("   └─ 📝 内容: %1").arg(contentLog));
        }
    }
    break;

    case SID_STARTADVEX3:
    {
        LOG_INFO(QString("   └─ [协议追踪] Bot-%1 收到房间创建结果 (0x1C)").arg(m_user));
        if (data.size() < 4) return;
        GameCreationStatus status;
        QDataStream ds(data);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds >> status;

        if (status == GameCreate_Ok) {
            LOG_INFO("   ├─ ✅ 结果: 房间创建成功");
            LOG_INFO("   └─ 📢 状态: 广播已启动");
            emit gameCreateSuccess(From_Client, m_isRefreshingAdv);
        } else {
            QString errStr;
            switch (status) {
            case GameCreate_NameExists:      errStr = "房间名已存在"; break;
            case GameCreate_TypeUnavailable: errStr = "游戏类型不可用"; break;
            case GameCreate_Error:           errStr = "游戏创建错误"; break;
            default:                         errStr = QString("Code 0x%1").arg(QString::number(status, 16)); break;
            }
            LOG_ERROR(QString("   ├─ ❌ 结果: 创建失败"));
            LOG_INFO(QString("   └─ 📝 原因: %1").arg(errStr));

            // 触发失败信号，BotManager 会处理并通知客户端
            emit gameCreateFail(status);
        }
    }
    break;

    case SID_LOGONRESPONSE: // 0x29
    case SID_LOGONRESPONSE2: // 0x3A
    {
        LOG_INFO(QString("   └─ [协议追踪] Bot-%1 收到登录结果响应 (0x29/0x3A)").arg(m_user));
        if (data.size() < 4) return;
        quint32 result;
        QDataStream ds(data);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds >> result;

        // 兼容两种协议的成功码 (0x29是1, 0x3A是0)
        bool isSuccess = (id == SID_LOGONRESPONSE && result == 1) || (id == SID_LOGONRESPONSE2 && result == 0);

        if (isSuccess) {
            LOG_INFO("   ├─ 🎉 结果: 成功");
            LOG_INFO("   └─ 🚀 动作: 发出 authenticated 信号 -> 进入聊天");
            emit authenticated();
        } else {
            LOG_ERROR(QString("   └─ ❌ 结果: 失败 (Code: 0x%1)").arg(QString::number(result, 16)));
        }
    }
    break;

    case SID_AUTH_INFO:
    case SID_AUTH_CHECK:
    {
        LOG_INFO(QString("   └─ [协议追踪] Bot-%1 收到版本校验挑战 (0x50/0x51)").arg(m_user));
        handleAuthCheck(data);
    }
    break;

    case SID_AUTH_ACCOUNTCREATE:
    {
        LOG_INFO(QString("   └─ [协议追踪] Bot-%1 收到账号创建结果 (0x52)").arg(m_user));
        if (data.size() < 4) return;
        quint32 status;
        QDataStream ds(data);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds >> status;

        if (status == 0) {
            LOG_INFO("   ├─ 🎉 结果: 注册成功");
            LOG_INFO("   └─ 🚀 动作: 自动尝试登录...");
            emit accountCreated();
            sendLoginRequest(Protocol_SRP_0x53);
        } else if (status == 0x04) {
            LOG_INFO("   ├─ ⚠️ 结果: 账号已存在");
            LOG_INFO("   └─ 🚀 动作: 尝试直接登录...");
            sendLoginRequest(Protocol_SRP_0x53);
        } else {
            LOG_ERROR(QString("   └─ ❌ 结果: 注册失败 (Code: 0x%1)").arg(QString::number(status, 16)));
        }
    }
    break;

    case SID_AUTH_ACCOUNTLOGON:
    {
        LOG_INFO(QString("   └─ [协议追踪] Bot-%1 收到登录挑战响应 (0x53)").arg(m_user));
        if (m_loginProtocol == Protocol_SRP_0x53) {
            handleSRPLoginResponse(data);
        }
    }
    break;

    case SID_AUTH_ACCOUNTLOGONPROOF:
    {
        LOG_INFO(QString("   └─ [协议追踪] Bot-%1 收到登录最终证明 (0x54)").arg(m_user));
        if (data.size() < 4) return;
        quint32 status;
        QDataStream ds(data);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds >> status;

        if (status == 0 || status == 0x0E) {
            LOG_INFO("   ├─ 🎉 结果: SRP 验证通过");
            LOG_INFO("   └─ 🚀 动作: 进入聊天");
            emit authenticated();
            enterChat();
        } else {
            QString reason = "未知错误";
            if (status == 0x02) reason = "密码错误";
            else if (status == 0x0D) reason = "账号不存在";

            LOG_ERROR(QString("   ├─ ❌ 结果: 验证失败 (0x%1)").arg(QString::number(status, 16)));
            LOG_INFO(QString("   └─ 📝 原因: %1").arg(reason));
        }
    }
    break;

    default: break;
    }
}

void Client::onPlayerReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    if (!m_playerBuffers.contains(socket)) {
        m_playerBuffers.insert(socket, QByteArray());
    }

    QByteArray &buffer = m_playerBuffers[socket];
    buffer.append(socket->readAll());

    // --- 缓冲区大小限制，防止内存攻击 ---
    if (buffer.size() > 1024 * 1024) {
        LOG_ERROR("❌ 缓冲区溢出，强制断开");
        socket->disconnectFromHost();
        return;
    }

    while (true) {
        if (!m_playerBuffers.contains(socket)) return;

        if (buffer.size() < 4) break;

        // --- 发现非法协议头必须立即断开并清理 ---
        if ((quint8)buffer[0] != 0xF7) {
            LOG_ERROR("❌ 非法协议头，强制清理并断开");
            m_playerBuffers.remove(socket); // 清理内存
            socket->disconnectFromHost();   // 断开连接
            return;
        }

        quint16 length = (quint8)buffer[2] | ((quint8)buffer[3] << 8);

        // --- 长度校验，W3GS 协议包头+ID+长度 至少 4 字节 ---
        if (length < 4) {
            LOG_ERROR("❌ 收到非法包长度，断开连接");
            socket->disconnectFromHost();
            return;
        }

        if (buffer.size() < length) break;

        QByteArray packet = buffer.mid(0, length);
        buffer.remove(0, length);

        quint8 msgId = (quint8)packet[1];
        QByteArray payload = packet.mid(4);

        handleW3GSPacket(socket, msgId, payload);

        // 处理完一个包后，如果 socket 已断开，直接退出
        if (socket->state() == QAbstractSocket::UnconnectedState) {
            return;
        }
    }
}

void Client::handleW3GSPacket(QTcpSocket *socket, quint8 id, const QByteArray &payload)
{
    // 忽略高频包的入口日志，避免刷屏
    if (id != 0x42 && id != 0x44 && id != 0x46) {
        LOG_INFO(QString("📥 [W3GS] 收到数据包: 0x%1").arg(QString::number(id, 16).toUpper()));
    }

    quint8 currentPid = getPidBySocket(socket);
    if (id != W3GS_REQJOIN && currentPid == 0) return;
    QString playerName = getPlayerNameByPid(currentPid);

    switch (id) {
    case W3GS_REQJOIN: //  [0x1E] 客户端请求加入游戏
    {
        // 1. 解析客户端请求
        QDataStream in(payload);
        in.setByteOrder(QDataStream::LittleEndian);

        quint32 clientHostCounter = 0;
        quint32 clientEntryKey = 0;
        quint8  clientUnknown8 = 0;
        quint16 clientListenPort = 0;
        quint32 clientPeerKey = 0;
        QString clientPlayerName = "";
        quint32 clientUnknown32 = 0;
        quint16 clientInternalPort = 0;
        quint32 clientInternalIP = 0;

        if (payload.size() >= 15) {
            in >> clientHostCounter >> clientEntryKey >> clientUnknown8 >> clientListenPort >> clientPeerKey;
            if (in.status() != QDataStream::Ok) {
                LOG_ERROR("❌ 协议解析失败：数据流损坏");
                return;
            }
            QByteArray nameBytes;
            int safetyCounter = 0;
            char c;
            while (!in.atEnd() && safetyCounter++ < 32) {
                in.readRawData(&c, 1);
                if (c == 0) break;
                nameBytes.append(c);
            }
            clientPlayerName = QString::fromUtf8(nameBytes);
            if (!in.atEnd()) {
                in >> clientUnknown32 >> clientInternalPort >> clientInternalIP;
            }
        } else {
            LOG_ERROR(QString("   └─ ❌ [错误] 包长度不足: %1").arg(payload.size()));
            return;
        }

        QHostAddress iAddr(qToBigEndian(clientInternalIP));

        LOG_INFO(QString("   ├─ 👤 玩家名: %1").arg(clientPlayerName));
        LOG_INFO(QString("   ├─ 🌍 内网IP: %1:%2").arg(iAddr.toString()).arg(clientInternalPort));
        LOG_INFO(QString("   ├─ 🔧 监听端口: %1").arg(clientListenPort));

        QString currentClientId = (m_netManager) ? m_netManager->getClientIdByPreJoinName(clientPlayerName) : "";
        QString identifier = currentClientId.isEmpty() ? clientPlayerName : currentClientId;

        if (!identifier.isEmpty() && m_rejoinCooldowns.contains(identifier)) {
            PlayerRejoin playerRejoin = m_rejoinCooldowns.value(identifier);
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            qint64 elapsed = now - playerRejoin.leaveTime;

            // 房主限制 1500ms，普通玩家 500ms
            int cooldownLimit = playerRejoin.isVisualHost ? 1500 : 500;

            if (elapsed < cooldownLimit) {
                quint32 remaining = static_cast<quint32>(cooldownLimit - elapsed);
                LOG_INFO(QString("🛑 [重入拦截] 玩家 %1 离场不足 %2ms (当前: %3ms)，拒绝加入房间")
                             .arg(clientPlayerName)
                             .arg(cooldownLimit)
                             .arg(elapsed));
                emit rejoinRejected(currentClientId, remaining);
                socket->write(createW3GSRejectJoinPacket(BAD_GAME));
                socket->flush();
                socket->disconnectFromHost();
                return;
            } else {
                m_rejoinCooldowns.remove(identifier);
            }
        }

        // 1.1 房主校验
        bool nameMatch = (!m_host.isEmpty() && m_host.compare(clientPlayerName, Qt::CaseInsensitive) == 0);
        LOG_INFO(QString("   ├─ 🔍 房主校验: 预设[%1] vs 玩家[%2] -> %3")
                     .arg(m_host, clientPlayerName, nameMatch ? "✅ 匹配" : "❌ 不匹配"));

        // 1.2 逻辑判断：房主是否在场
        if (!isHostJoined()) {
            // A. 如果来的不是房主 -> 拒绝
            if (!nameMatch) {
                LOG_INFO(QString("   └─ 🛑 [拒绝加入] 原因: 等待房主 [%1] 进场中...").arg(m_host));
                socket->write(createW3GSRejectJoinPacket(BAD_GAME));
                socket->flush();
                socket->disconnectFromHost();
                return;
            }
            // B. 如果来的是房主 -> 允许
            else {
                LOG_INFO(QString("   ├─ 👑 [房主到达] 房间锁定解除，允许其他人加入"));
                emit hostJoinedGame(clientPlayerName);
            }
        }
        else {
            // C. 房主已在场，防止重名攻击
            if (nameMatch) {
                LOG_INFO(QString("   └─ ⚠️ [拒绝加入] 原因: 检测到重复的房主名 [%1]").arg(clientPlayerName));
                socket->write(createW3GSRejectJoinPacket(BAD_GAME));
                socket->disconnectFromHost();
                return;
            }
        }

        // 2. 槽位分配
        int slotIndex = -1;
        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].slotStatus == Open && m_slots[i].pid == 0) {
                slotIndex = i;
                break;
            }
        }

        if (slotIndex == -1) {
            LOG_INFO("   └─ ⚠️ [拒绝加入] 原因: 房间已满");
            socket->write(createW3GSRejectJoinPacket(FULL));
            socket->flush();
            socket->disconnectFromHost();
            return;
        }

        if (m_gameStarted) {
            LOG_INFO("   └─ 🛑 [拒绝加入] 原因: 游戏已经开始");
            socket->write(createW3GSRejectJoinPacket(STARTED));
            socket->flush();
            socket->disconnectFromHost();
            return;
        }

        // 分配 PID
        QString existingPids;
        quint8 botPidFound = 0;
        for(auto it = m_players.begin(); it != m_players.end(); ++it) {
            existingPids += QString::number(it.key()) + " ";
            if (it.key() == m_botPid) botPidFound = m_botPid;
        }

        // 执行分配
        quint8 newPid = findFreePid();

        LOG_INFO(QString("🔍 [PID 分配诊断] 玩家: %1").arg(clientPlayerName));
        LOG_INFO(QString("   ├─ 📊 当前已存 PID: [ %1]").arg(existingPids));
        LOG_INFO(QString("   ├─ 🤖 机器人 PID: %1").arg(botPidFound != 0 ? QString::number(botPidFound) : "未找到(危险!)"));

        if (newPid == 0) {
            LOG_ERROR("   └─ ❌ 分配失败: 无PID可分配(FULL)");
            socket->write(createW3GSRejectJoinPacket(FULL));
            socket->disconnectFromHost();
            return;
        }

        if (newPid == botPidFound) {
            LOG_CRITICAL(QString("   └─ 💥 [严重冲突] 新玩家分配了 PID %1，但这与机器人重叠！").arg(newPid));
            while (m_players.contains(newPid) || newPid == m_botPid) {
                newPid++;
            }
            LOG_INFO(QString("      └─ 🔧 自动修正为: PID %1").arg(newPid));
        } else {
            LOG_INFO(QString("   └─ ✅ 分配成功: PID %1 (无冲突)").arg(newPid));
        }

        m_slots[slotIndex].pid = newPid;
        m_slots[slotIndex].slotStatus = Occupied;
        m_slots[slotIndex].downloadStatus = DownloadStart;
        m_slots[slotIndex].computer = Human;

        qint64 now = QDateTime::currentMSecsSinceEpoch();

        // 注册玩家
        PlayerData playerData;
        playerData.pid = newPid;
        playerData.name = clientPlayerName;
        playerData.socket = socket;
        playerData.extIp = socket->peerAddress();
        playerData.extPort = socket->peerPort();
        playerData.intIp = QHostAddress(qToBigEndian(clientInternalIP));
        playerData.intPort = clientInternalPort;
        playerData.language = "en";
        playerData.codec = QTextCodec::codecForName("Windows-1252");
        playerData.lastResponseTime = now;
        playerData.lastDownloadTime = now;
        playerData.isVisualHost = nameMatch;
        playerData.readyCountdown = 10;
        playerData.isReady = nameMatch;
        playerData.lastResponseTime = QDateTime::currentMSecsSinceEpoch();
        playerData.clientId = currentClientId;
        if (!playerData.clientId.isEmpty()) {
            LOG_INFO(QString("🔗 [关联成功] 玩家: %1 <-> ClientId: %2")
                         .arg(clientPlayerName, playerData.clientId));
        } else {
            LOG_WARNING(QString("⚠️ [关联失败] 玩家 %1 确实没有申报 ClientId").arg(clientPlayerName));
        }

        m_players.insert(newPid, playerData);

        LOG_INFO(QString("   ├─ 💾 玩家注册: PID %1 (Slot %2)").arg(newPid).arg(slotIndex));
        int humanCount = qMax(0, m_players.size() - 1);
        emit playerCountChanged(humanCount);
        LOG_INFO(QString("   ├─ 👥 状态更新: 房间真人数量变动为 %1，已发射同步信号").arg(humanCount));

        // 3. 构建握手响应
        QByteArray finalPacket;
        QHostAddress hostIp = socket->peerAddress();
        quint16 hostPort = m_udpSocket->localPort();

        finalPacket.append(createW3GSSlotInfoJoinPacket(newPid, hostIp, hostPort)); // 0x04
        finalPacket.append(createPlayerInfoPacket(m_botPid, m_botDisplayName, QHostAddress("0.0.0.0"), 0, QHostAddress("0.0.0.0"), 0)); // 0x06 (Bot)

        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            const PlayerData &p = it.value();
            if (p.pid == newPid || p.pid == m_botPid) continue;
            finalPacket.append(createPlayerInfoPacket(p.pid, p.name, p.extIp, p.extPort, p.intIp, p.intPort));
        }

        finalPacket.append(createW3GSMapCheckPacket()); // 0x3D
        finalPacket.append(createW3GSSlotInfoPacket()); // 0x09

        socket->write(finalPacket);
        socket->flush();

        LOG_INFO("   ├─ 📤 发送握手: 0x04 -> 0x06 -> 0x3D -> 0x09");

        // 4. 广播
        QByteArray newPlayerInfoPacket = createPlayerInfoPacket(
            playerData.pid, playerData.name, playerData.extIp, playerData.extPort, playerData.intIp, playerData.intPort);
        broadcastPacket(newPlayerInfoPacket, newPid);
        broadcastSlotInfo();
        syncPlayerReadyStates();

        LOG_INFO("   └─ 📢 广播状态: 同步新玩家信息 & 刷新槽位");
    }
    break;

    case W3GS_LEAVEREQ: // [0x21] 客户端发送离开房间
    {
        LOG_INFO(QString("   └─ 👋 [离开请求] 来源: %1").arg(socket->peerAddress().toString()));
        socket->disconnectFromHost();
    }
    break;

    case W3GS_GAMELOADED_SELF: // [0x23] 客户端发送加载完成信号
    {
        m_players[currentPid].isFinishedLoading = true;
        m_players[currentPid].lastResponseTime = QDateTime::currentMSecsSinceEpoch();

        LOG_INFO(QString("⏳ [加载完成] 玩家: %1 (PID: %2) - 暂存状态，等待全员就绪").arg(m_players[currentPid].name).arg(currentPid));

        // 触发检查
        checkAllPlayersLoaded();
    }
    break;

    case W3GS_OUTGOING_ACTION: // [0x26] 客户端发送的游戏内操作
    {
        if (payload.size() < 4) {
            LOG_ERROR(QString("❌ [W3GS] 动作包长度不足: %1").arg(payload.size()));
            return;
        }

        QByteArray crcData = payload.left(4);
        QDataStream crcStream(crcData);
        quint32 crcValue;

        crcStream.setByteOrder(QDataStream::LittleEndian);
        crcStream >> crcValue;

        QByteArray actionData = payload.mid(4);

        if (!actionData.isEmpty()) {
            m_actionQueue.append({currentPid, actionData});
            m_players[currentPid].lastResponseTime = QDateTime::currentMSecsSinceEpoch();

            static int logCount = 0;
            bool shouldLog = (logCount == 0 || logCount % m_actionLogFrequency < m_actionLogShowLines);

            if (shouldLog) {
                QString hexData = actionData.toHex().toUpper();
                if (hexData.length() > 50) hexData = hexData.left(47) + "...";
                LOG_INFO(QString("🎮 [游戏动作] 收到玩家指令 (0x26)"));
                LOG_INFO(QString("   ├─ 👤 来源: %1 (PID: %2)").arg(playerName).arg(currentPid));
                LOG_INFO(QString("   ├─ 🛡️ CRC32: 0x%1").arg(QString::number(crcValue, 16).toUpper().rightJustified(8, '0')));
                LOG_INFO(QString("   ├─ 📦 数据: %1 (%2 bytes)").arg(hexData).arg(actionData.size()));
                LOG_INFO(QString("   └─ 📥 状态: 已加入广播队列 (当前队列深度: %1)").arg(m_actionQueue.size()));
            }

            logCount++;

        } else {
            m_players[currentPid].lastResponseTime = QDateTime::currentMSecsSinceEpoch();
            LOG_DEBUG(QString("💓 [游戏心跳] 玩家 %1 发送了空动作包 (KeepAlive)").arg(currentPid));
        }
    }
    break;

    case W3GS_OUTGOING_KEEPALIVE: // [0x27] 客户端发送的保持连接包
    {
        if (payload.size() < 5) {
            LOG_INFO(QString("   └─ ⚠️ [警告] KeepAlive 包长度异常: %1 (期望 >= 5)").arg(payload.size()));
            return;
        }

        QDataStream in(payload);
        in.setByteOrder(QDataStream::LittleEndian);

        quint8 unknownByte;
        quint32 checkSum;
        in >> unknownByte >> checkSum;

        qint64 now = QDateTime::currentMSecsSinceEpoch();
        m_players[currentPid].lastResponseTime = now;

        LOG_INFO(QString("💓 [保持连接] 收到心跳包 (0x27)"));
        LOG_INFO(QString("   ├─ 👤 来源: %1 (PID: %2)").arg(playerName).arg(currentPid));
        LOG_INFO(QString("   ├─ ❓ 标志: 0x%1").arg(QString::number(unknownByte, 16).toUpper().rightJustified(2, '0')));
        LOG_INFO(QString("   ├─ 🛡️ 校验: 0x%1 (CheckSum)").arg(QString::number(checkSum, 16).toUpper().rightJustified(8, '0')));
        LOG_INFO(QString("   └─ ⏱️ 动作: 刷新活跃时间戳 -> %1").arg(now));
    }
    break;

    case W3GS_CHAT_TO_HOST: // [0x28] 客户端发送聊天/大厅指令
    {
        if (payload.size() < 3) return;

        QDataStream in(payload);
        in.setByteOrder(QDataStream::LittleEndian);

        quint8 numReceivers;
        in >> numReceivers;

        if (payload.size() < 1 + numReceivers + 2) {
            LOG_ERROR(QString("   └─ ❌ [错误] 包长度不足 (Receivers: %1)").arg(numReceivers));
            return;
        }

        QList<quint8> receiverPids;
        for (int i = 0; i < numReceivers; ++i) {
            quint8 rpid;
            in >> rpid;
            receiverPids.append(rpid);
        }

        quint8 fromPid, flag;
        in >> fromPid >> flag;

        QTextCodec *codec  = getCodecByPid(currentPid);

        QString typeStr;
        switch(flag) {
        case 0x10: typeStr = "消息 (Message)"; break;
        case 0x11: typeStr = "变更队伍 (Team)"; break;
        case 0x12: typeStr = "变更颜色 (Color)"; break;
        case 0x13: typeStr = "变更种族 (Race)"; break;
        case 0x14: typeStr = "变更让分 (Handicap)"; break;
        case 0x20: typeStr = "扩展消息 (Extra)"; break;
        default:   typeStr = QString("未知 (0x%1)").arg(QString::number(flag, 16)); break;
        }

        LOG_INFO(QString("   ├─ 👤 发送者: %1 (PID: %2)").arg(playerName).arg(currentPid));
        LOG_INFO(QString("   ├─ 🚩 类型: %1").arg(typeStr));

        int currentOffset = 1 + numReceivers + 2; // 当前解析到的字节位置

        if (flag == 0x10) // [16] 聊天消息
        {
            if (currentOffset < payload.size()) {
                QByteArray rawMsg = payload.mid(currentOffset);
                // 1. 移除末尾的 \0
                if (rawMsg.endsWith('\0')) rawMsg.chop(1);

                // 2. 转码并立即去除前后空格
                QString msg = codec->toUnicode(rawMsg).trimmed();

                // 3. 检查处理后是否为空
                if (msg.isEmpty()) return;

                LOG_INFO(QString("   └─ 💬 内容: %1").arg(msg));

                // A. 指令处理
                if (msg.startsWith("/")) {
                    LOG_INFO(QString("      └─ ⚡ [指令] 检测到命令，转交机器人处理..."));
                    handleChatCommand(currentPid, msg);
                }
                // B. 普通聊天转发
                else {
                    QString receiverStrCN, receiverStrEN;
                    bool isToAll = (numReceivers == 0 || receiverPids.contains(m_botPid));

                    // 获取发送者的状态数据
                    if (!m_players.contains(currentPid)) return;
                    PlayerData &senderData = m_players[currentPid];

                    // 1. 获取发送者彩色名字
                    QString coloredSender = getColoredTextByState(senderData, senderData.name, true);

                    // 2. 获取彩色消息内容
                    QString coloredMsg = getColoredTextByState(senderData, msg, false);

                    if (isToAll) {
                        receiverStrCN = "所有人";
                        receiverStrEN = "Everyone";
                    }
                    else {
                        QStringList coloredNames;
                        for (quint8 rpid : receiverPids) {
                            if (m_players.contains(rpid)) {
                                coloredNames.append(getColoredTextByState(m_players[rpid], m_players[rpid].name, true));
                            }
                        }
                        receiverStrCN = receiverStrEN = coloredNames.join(", ");
                    }

                    LOG_INFO(QString("      └─ 💬 [%1] 对 [%2] 说: %3")
                                 .arg(playerName, isToAll ? "所有人" : "指定玩家", msg));

                    // 3. 构建并广播消息
                    if (!coloredSender.isEmpty() && !receiverStrCN.isEmpty() && !msg.isEmpty())
                    {
                        MultiLangMsg chatMsg;
                        chatMsg.add("zh_CN", QString("%1 对 %2 说: %3").arg(coloredSender, receiverStrCN, coloredMsg));
                        chatMsg.add("en", QString("%1 to %2 says: %3").arg(coloredSender, receiverStrEN, coloredMsg));

                        broadcastChatMessage(chatMsg, currentPid);
                    }
                }
            }
        }
        else if (flag >= 0x11 && flag <= 0x14) // [17-20] 状态变更请求
        {
            if (currentOffset < payload.size()) {
                quint8 byteVal;
                in >> byteVal;

                QString actionLog;

                switch(flag) {
                case 0x11: // Team
                    actionLog = QString("请求换至队伍: %1").arg(byteVal);
                    break;
                case 0x12: // Color
                    actionLog = QString("请求更换颜色: %1").arg(byteVal);
                    break;
                case 0x13: // Race
                {
                    QString raceStr;
                    if(byteVal == 1) raceStr = "Human";
                    else if(byteVal == 2) raceStr = "Orc";
                    else if(byteVal == 3) raceStr = "Undead";
                    else if(byteVal == 4) raceStr = "NightElf";
                    else raceStr = "Random";
                    actionLog = QString("请求更换种族: %1 (%2)").arg(raceStr).arg(byteVal);
                }
                break;
                case 0x14: // Handicap
                    actionLog = QString("请求变更生命值: %1%").arg(byteVal);
                    break;
                }

                LOG_INFO(QString("   └─ ⚙️ 动作: %1").arg(actionLog));
            }
        }
        else if (flag == 0x20) // [32] 带额外标志的消息 (通常是类似 Ping 或特殊指令)
        {
            if (payload.size() >= currentOffset + 4) {
                quint32 extraFlags;
                in >> extraFlags;

                // 读取剩余的字符串
                QByteArray rawMsg = payload.mid(currentOffset + 4);
                if (rawMsg.endsWith('\0')) rawMsg.chop(1);
                QString msg = codec->toUnicode(rawMsg);

                LOG_INFO(QString("   ├─ 🔧 额外标志: %1").arg(extraFlags));
                LOG_INFO(QString("   └─ 💬 内容: %1").arg(msg));
            }
        }
        else {
            LOG_INFO("   └─ ⚠️ [未知] 无法解析的 Payload 数据");
        }
    }
    break;

    case W3GS_CLIENTINFO: // [0x37] 客户端信息
        LOG_INFO(QString("ℹ️ [W3GS] 客户端 PID %1 已确认收到槽位信息 (0x37)").arg((quint8)payload[8]));
        break;

    case W3GS_STARTDOWNLOAD: // [0x3F] 客户端主动请求开始下载
    {
        PlayerData &playerData = m_players[currentPid];

        LOG_INFO(QString("📥 [W3GS] 收到请求: 0x3F (StartDownload)"));
        LOG_INFO(QString("   └─ 👤 玩家: %1 (PID: %2)").arg(playerData.name).arg(currentPid));

        if (playerData.isDownloadStart) {
            LOG_INFO("   └─ ⚠️ 忽略: 已经在下载进程中");
            return;
        }

        bool validSlot = false;
        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].pid == currentPid) {
                if (m_slots[i].downloadStatus != Completed) {
                    m_slots[i].downloadStatus = DownloadStart   ;
                    validSlot = true;
                }
                break;
            }
        }

        if (validSlot) {
            initiateMapDownload(currentPid);
        } else {
            LOG_INFO("   └─ ℹ️ 忽略: 玩家已有地图或槽位无效");
        }
    }
    break;

    case W3GS_MAPSIZE: // [0x42] 客户端报告地图状态
    {
        if (payload.size() < 9) return;
        QDataStream in(payload);
        in.setByteOrder(QDataStream::LittleEndian);
        quint32 unknown; quint8 sizeFlag; quint32 clientMapSize;
        in >> unknown >> sizeFlag >> clientMapSize;

        quint32 hostMapSize = m_war3Map.getMapSize();
        PlayerData &playerData = m_players[currentPid];
        playerData.lastResponseTime = QDateTime::currentMSecsSinceEpoch();

        if (sizeFlag != 1 && sizeFlag != 3) {
            LOG_INFO(QString("⚠️ [W3GS] 收到罕见 Flag: %1 (Size: %2)").arg(sizeFlag).arg(clientMapSize));
        }

        bool isMapMatched = (clientMapSize == hostMapSize && sizeFlag == 1);
        bool isDownloadFinished = (clientMapSize == hostMapSize);

        bool slotUpdated = false;
        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].pid == currentPid) {

                // [A] 下载完成
                if (isMapMatched || isDownloadFinished) {
                    if (m_slots[i].downloadStatus != Completed) {
                        m_slots[i].downloadStatus = Completed;
                        playerData.isDownloadStart    = false;
                        slotUpdated = true;
                        LOG_INFO("   └─ ✅ 状态: 地图完整/校验通过");
                    }
                }
                // [B] 需要下载
                else {
                    if (m_slots[i].downloadStatus != DownloadStart   ) {
                        m_slots[i].downloadStatus = DownloadStart   ;
                    }

                    // 情况 1: 初始请求 / 开始下载 (Flag=1)
                    if (sizeFlag == 1) {
                        if(clientMapSize == 0) {
                            initiateMapDownload(currentPid);
                        }
                    }
                    // 情况 2: 进度同步 / 重传请求 (Flag=3)
                    else {
                        // 每传输 ~1MB 触发一次
                        if (clientMapSize % (1024 * 1024) < 2000) {
                            LOG_INFO(QString("🔄 重发分块"));
                            LOG_INFO(QString("   ├─ 💻 客户端报告: %1").arg(clientMapSize));
                            LOG_INFO(QString("   ├─ 💻 服务端最后: %1").arg(playerData.lastDownloadOffset));
                            LOG_INFO(QString("   ├─ 💻 服务端当前: %1").arg(playerData.currentDownloadOffset));
                        }
                        if(playerData.lastDownloadOffset != clientMapSize) {
                            sendNextMapPart(currentPid);
                            LOG_ERROR(QString("   └─ ❌ 客户端(%1) != 服务端(%2) 需要重传").arg(clientMapSize, playerData.lastDownloadOffset));
                        } else {
                            if (clientMapSize % (1024 * 1024) < 2000) {
                                LOG_INFO(QString("   └─ ✅ 客户端(%1) == 服务端(%2) 无需重传").arg(clientMapSize, playerData.lastDownloadOffset));
                            }
                        }
                    }
                }
                break;
            }
        }
        if (slotUpdated) broadcastSlotInfo();
    }
    break;

    case W3GS_MAPPARTOK: //  [0x44] 客户端报告成功
    {
        if (payload.size() < 10) return;
        QDataStream in(payload);
        in.setByteOrder(QDataStream::LittleEndian);
        quint8 fromPid, toPid; quint32 unknownFlag, clientOffset;
        in >> fromPid >> toPid >> unknownFlag >> clientOffset;

        if (m_mapSize == 0) return;

        if (m_players.contains(currentPid)) {
            PlayerData &playerData = m_players[currentPid];            
            qint64 now = QDateTime::currentMSecsSinceEpoch();

            playerData.lastResponseTime = now;
            playerData.lastDownloadOffset = clientOffset;

            // 每传输 ~1MB 触发一次
            if (playerData.lastDownloadOffset % (1024 * 1024) < 2000) {
                LOG_INFO(QString("🔄 接收成功"));
                LOG_INFO(QString("   └─ ✅ 客户端接收: %1").arg(clientOffset));
                int percent = (int)((double)playerData.lastDownloadOffset / m_mapSize * 100);
                if (percent > 99) percent = 99;
                LOG_INFO(QString("📤 [分块传输] 缓冲中... %1% (Offset: %2)")
                             .arg(percent)
                             .arg(playerData.lastDownloadOffset));
                bool needBroadcast = false;
                for (int i = 0; i < m_slots.size(); ++i) {
                    if (m_slots[i].pid == toPid) {
                        quint8 oldStatus = m_slots[i].downloadStatus;
                        if (oldStatus != Completed && percent > oldStatus && (percent - oldStatus >= 5)) {
                            m_slots[i].downloadStatus = static_cast<quint8>(percent);
                            needBroadcast = true;
                        }
                        break;
                    }
                }
                if (needBroadcast) broadcastSlotInfo();
            }

            // 传输完成判断
            if (playerData.lastDownloadOffset >= m_mapSize) {
                LOG_INFO(QString("✅ [分块传输] 传输完成: %1").arg(playerData.name));
                LOG_INFO(QString("   ├─ 📊 数据统计: %1 / %2 bytes").arg(playerData.currentDownloadOffset).arg(m_mapSize));

                playerData.isDownloadStart = false;

                // 更新槽位为 100%
                for (int i = 0; i < m_slots.size(); ++i) {
                    if (m_slots[i].pid == toPid) {
                        m_slots[i].downloadStatus = 100;
                        break;
                    }
                }
                broadcastSlotInfo();

                // 发送完成确认包
                playerData.socket->write(createW3GSSlotInfoPacket());
                playerData.socket->flush();
                return;
            }

            // 每 1 秒计算一次平均速度
            qint64 elapsed = now - playerData.lastSpeedUpdateTime;
            if (elapsed >= 1000) {
                // 速度 = (这段时间发的字节 / 1024) / (秒)
                playerData.currentSpeedKBps = (playerData.bytesSentInWindow / 1024.0) / (elapsed / 1000.0);

                // 重置窗口
                playerData.bytesSentInWindow = 0;
                playerData.lastSpeedUpdateTime = now;

                LOG_INFO(QString("📊 [下载监控] 玩家: %1 | 进度: %2% | 速度: %3 KB/s")
                             .arg(playerData.name)
                             .arg((int)((double)playerData.lastDownloadOffset/m_mapSize*100))
                             .arg(playerData.currentSpeedKBps, 0, 'f', 1));
            }

            // 发送下一块
            sendNextMapPart(currentPid);
        }
    }
    break;

    case W3GS_MAPPARTNOTOK: // [0x45] 客户端报告失败
    {
        quint32 unknownValue = 0;
        QString rawHex = payload.toHex().toUpper();

        if (payload.size() >= 4) {
            QDataStream in(payload);
            in.setByteOrder(QDataStream::LittleEndian);
            in >> unknownValue;
        }

        LOG_INFO(QString("🚀 下载错误 (C>S 0x45 W3GS_MAPPARTNOTOK) [pID: %1]").arg(currentPid));
        LOG_INFO(QString("   ├─ ❓ [Unknown] Value: %1 (0x%2)")
                     .arg(unknownValue)
                     .arg(unknownValue, 8, 16, QChar('0')).toUpper());
        LOG_INFO(QString("   ├─ 📦 [Payload] Raw: %1").arg(rawHex));

        LOG_INFO("   └─ 可能原因: (以下错误会跳转到 Game.dll + 67FBF9) [v1.26.0.6401]");
        LOG_INFO("      ├─ ❶ [Game.dll + 67FA78] 状态异常: 客户端期望偏移量 >= 地图总大小");
        LOG_INFO("      ├─ ❷ [Game.dll + 67FA82] 偏移量不匹配: Packet Offset != Client Expected");
        LOG_INFO("      ├─ ❸ [Game.dll + 67FA8C] 数据越界: (Offset + ChunkSize) > MapTotalSize");
        LOG_INFO("      └─ ❹ [Game.dll + 67FAA3] CRC 校验失败: 算出值(EAX) != 包内值(Stack)");
    }
    break;

    case W3GS_PONG_TO_HOST: //  [0x46] 客户端回复 PING
    {
        if (payload.size() < 4) return;
        QDataStream in(payload);
        in.setByteOrder(QDataStream::LittleEndian);
        quint32 sentTick; in >> sentTick;

        qint64 now = QDateTime::currentMSecsSinceEpoch();
        quint32 nowTick = static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF);
        PlayerData &p = m_players[currentPid];
        p.currentLatency = nowTick - sentTick;
        p.lastResponseTime = now;

        LOG_DEBUG(QString("💓 Pong [PID:%1]: %2 ms").arg(currentPid).arg(p.currentLatency));

        QMap<quint8, quint32> pingMap;
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.key() == m_botPid) continue;
            pingMap.insert(it.key(), it.value().currentLatency);
        }

        emit roomPingsUpdated(pingMap);
    }
    break;

    default:
        LOG_INFO(QString("   └─ ❓ [未知包] 忽略处理"));
        break;
    }
}

void Client::handleChatCommand(quint8 senderPid, const QString &fullMsg)
{
    if (!m_players.contains(senderPid)) return;
    PlayerData &sender = m_players[senderPid];

    // 1. 解析指令和参数
    QString trimmed = fullMsg.trimmed();
    QStringList parts = trimmed.split(" ", Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;

    QString cmd = parts[0].toLower();
    QString args = trimmed.mid(parts[0].length()).trimmed();

    LOG_INFO(QString("⚡ [指令处理器] 来源: %1 | 指令: %2 | 参数: %3")
                 .arg(sender.name, cmd, args));

    // --- 分支 1: /ready ---
    if (cmd == "/ready") {
        if (!sender.isReady) {
            sender.isReady = true;
            sender.readyCountdown = 10;
            LOG_INFO(QString("   └─ ✅ 玩家 [%1] 已准备").arg(sender.name));

            MultiLangMsg msg;
            msg.add("zh_CN", QString("[%1] 已准备。").arg(sender.name))
                .add("en", QString("[%1] is ready.").arg(sender.name));
            broadcastChatMessage(msg);
            syncPlayerReadyStates();
        }
        return;
    }

    // --- 分支 2: /unready ---
    else if (cmd == "/unready") {
        if (sender.isReady && !sender.isVisualHost) {
            sender.isReady = false;
            sender.readyCountdown = 10;
            LOG_INFO(QString("   └─ 🔄 玩家 [%1] 取消准备").arg(sender.name));

            MultiLangMsg msg;
            msg.add("zh_CN", QString("[%1] 取消了准备，请在 10 秒内重新准备。").arg(sender.name))
                .add("en", QString("[%1] is no longer ready. 10s remaining.").arg(sender.name));
            broadcastChatMessage(msg);
            syncPlayerReadyStates();
        } else if (sender.isVisualHost) {
            LOG_INFO("   └─ ⚠️ 房主无需取消准备");
        }
        return;
    }

    // --- 分支 3: /w 或 /whisper (房间内私聊转发) ---
    else if (cmd == "/w" || cmd == "/whisper") {
        if (parts.size() < 3) {
            LOG_INFO("   └─ ❌ 私聊格式错误。用法: /w <玩家名> <内容>");
            return;
        }

        QString targetName = parts[1];
        QString content = trimmed.mid(trimmed.indexOf(targetName) + targetName.length()).trimmed();

        // 在房间内查找目标 PID
        quint8 targetPid = 0;
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().name.compare(targetName, Qt::CaseInsensitive) == 0) {
                targetPid = it.key();
                break;
            }
        }

        if (targetPid == 0 || !m_players.contains(targetPid)) return;

        if (m_players[targetPid].socket) {
            LOG_INFO(QString("   └─ 🔒 私聊转发: %1 -> %2").arg(sender.name, targetName));

            QByteArray msgBytes = content.toUtf8();
            QByteArray pkt = createW3GSChatFromHostPacket(msgBytes, senderPid, targetPid, ChatFlag::Message);
            m_players[targetPid].socket->write(pkt);
            m_players[targetPid].socket->flush();
        } else {
            LOG_INFO(QString("   └─ ❌ 找不到目标玩家: %1").arg(targetName));
        }
        return;
    }
}

void Client::onPlayerDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    // --- 1. 错误诊断与日志记录 ---
    qintptr socketId = socket->socketDescriptor();
    QString socketStr = (socketId == -1) ? "已失效(-1)" : QString::number(socketId);
    QString reason = translateSocketError(socket->error(), socket->errorString());

    LOG_INFO(QString("🔌 [断开诊断] Socket ID: %1 | 来源: %2:%3 | 原因: %4")
                 .arg(socketStr, socket->peerAddress().toString())
                 .arg(socket->peerPort())
                 .arg(reason));

    // --- 2. 预查找玩家信息 ---
    quint8 pidToRemove = 0;
    QString nameToRemove = "";
    QString clientIdToRemove = "";
    bool wasVisualHost = false;

    auto it = m_players.begin();
    while (it != m_players.end()) {
        if (it.value().socket == socket) {
            pidToRemove = it.key();
            nameToRemove = it.value().name;
            clientIdToRemove = it.value().clientId;
            wasVisualHost = it.value().isVisualHost;
            break;
        }
        ++it;
    }

    if (pidToRemove == 0) return;

    // --- 3. 记录重入冷却逻辑 ---
    QString identifier = clientIdToRemove.isEmpty() ? nameToRemove : clientIdToRemove;
    if (!identifier.isEmpty()) {
        PlayerRejoin pr;
        pr.leaveTime = QDateTime::currentMSecsSinceEpoch();
        pr.isVisualHost = wasVisualHost;
        m_rejoinCooldowns.insert(identifier, pr);

        LOG_INFO(QString("⏳ [冷却启动] 玩家 %1 离开，已记录重入限制").arg(nameToRemove));
    }

    // --- 4. 房主交接逻辑预处理 ---
    quint8 heirPid = 0;
    int oldHostSlotIndex = -1;

    if (wasVisualHost && !m_gameStarted) {
        LOG_INFO(QString("👑 [房主交接] 房主 %1 离线，寻找继承人...").arg(nameToRemove));

        // 记录原房主所在的槽位索引
        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].pid == pidToRemove) {
                oldHostSlotIndex = i;
                break;
            }
        }

        // 寻找新的继承人
        for (auto pIt = m_players.begin(); pIt != m_players.end(); ++pIt) {
            if (pIt.key() != m_botPid && pIt.key() != pidToRemove) {
                heirPid = pIt.key();
                break;
            }
        }
    }

    // --- 5. 执行物理移除 ---
    m_players.remove(pidToRemove);
    m_playerSockets.removeAll(socket);
    m_playerBuffers.remove(socket);
    socket->deleteLater();

    // 更新真人数量并向外发射信号
    int humanCount = 0;
    for(const auto& p : qAsConst(m_players)) if(p.pid != m_botPid) humanCount++;
    emit playerCountChanged(humanCount);

    LOG_INFO(QString("🔌 [断开连接] 玩家离线: %1 (PID: %2) | 剩余真人: %3")
                 .arg(nameToRemove).arg(pidToRemove).arg(humanCount));

    // --- 6. 房间存续判定 ---
    if (humanCount == 0) {
        LOG_INFO("🛑 [房间解散] 所有真实玩家已离开，执行资源回收");
        if (m_gameTickTimer->isActive()) m_gameTickTimer->stop();
        m_gameStarted = false;
        cancelGame();
        return;
    }

    // --- 7. 执行继承与看板热刷新逻辑 ---
    if (heirPid != 0 && oldHostSlotIndex != -1) {
        int heirCurrentSlotIdx = -1;
        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].pid == heirPid) {
                heirCurrentSlotIdx = i;
                break;
            }
        }

        if (heirCurrentSlotIdx != -1) {
            LOG_INFO(QString("   │  ├─ 🔍 选中继承人: %1 (PID: %2)").arg(m_players[heirPid].name).arg(heirPid));

            // A. 物理交换槽位信息
            GameSlot &hostSlot = m_slots[oldHostSlotIndex];
            GameSlot &heirSlot = m_slots[heirCurrentSlotIdx];

            std::swap(hostSlot.pid,            heirSlot.pid);
            std::swap(hostSlot.downloadStatus, heirSlot.downloadStatus);
            std::swap(hostSlot.slotStatus,     heirSlot.slotStatus);
            std::swap(hostSlot.computer,       heirSlot.computer);
            std::swap(hostSlot.computerType,   heirSlot.computerType);
            std::swap(hostSlot.handicap,       heirSlot.handicap);

            // B. 更新内部状态
            m_players[heirPid].isVisualHost = true;
            m_host = m_players[heirPid].name;

            // C. 驱动战网热刷新过程
            updateAdv();

            emit roomHostChanged(heirPid);

            LOG_INFO(QString("   │  └─ ✅ 继承人移至 Slot %1，看板刷新序列已启动").arg(oldHostSlotIndex + 1));
        }
    }

    // --- 8. 清理离开者的槽位 ---
    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].pid == pidToRemove || (m_slots[i].pid == 0 && m_slots[i].slotStatus == Occupied)) {
            m_slots[i].pid = 0;
            m_slots[i].slotStatus = Open;
            m_slots[i].downloadStatus = NotStarted;
        }
    }

    // --- 9. 广播状态同步至房间内其他玩家 ---
    // A. 发送离开协议包 (0x07)
    LeaveReason lr = m_gameStarted ? LEAVE_DISCONNECT : LEAVE_LOBBY;
    broadcastPacket(createW3GSPlayerLeftPacket(pidToRemove, lr), 0);

    // B. 发送房主离开事件信号
    if (wasVisualHost) {
        emit visualHostLeft();
    }

    // C. 发送聊天通知
    MultiLangMsg leaveMsg;
    leaveMsg.add("zh_CN", QString("玩家 [%1] 离开了房间。").arg(nameToRemove))
        .add("en", QString("Player [%1] has left the game.").arg(nameToRemove));

    if (heirPid != 0) {
        leaveMsg.add("zh_CN", QString("房主已更换为 [%1]。").arg(m_host))
            .add("en", QString("Host left. [%1] is the new host.").arg(m_host));
    }
    broadcastChatMessage(leaveMsg, 0);

    // D. 刷新所有人的 Slot 列表和准备状态
    broadcastSlotInfo();
    syncPlayerReadyStates();

    LOG_INFO(QString("📢 [状态同步] 离线处理完成，当前房主: %1").arg(m_host));
}

void Client::onGameStarted()
{
    if (m_gameStarted) return;

    // 1. 打印根节点
    LOG_INFO("🚀 [游戏启动] 倒计时结束，切换至 Loading 阶段");

    // 2. 标记游戏开始
    m_gameStarted = true;
    LOG_INFO("   ├─ ⚙️ 状态更新: m_gameStarted = true");

    // 3. 发送倒计时结束包
    broadcastPacket(createW3GSCountdownEndPacket(), 0);
    LOG_INFO("   ├─ 📡 广播指令: W3GS_COUNTDOWN_END (0x0B)");

    // 4. 处理机器人隐身
    QTimer::singleShot(300, this, [this](){
        broadcastPacket(createW3GSPlayerLeftPacket(2, LEAVE_LOBBY), 0, false);
        LOG_INFO("   ├─ 👻 [幽灵模式] 已向全员广播机器人(PID:2)离开");
    });

    // 5. 重置剩余玩家的加载状态
    LOG_INFO("   └─ 🔄 初始化玩家加载状态:");

    int waitCount = 0;
    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        quint8 pid = it.key();
        QString pName = it.value().name;

        if (pid == m_botPid) {
            it.value().isFinishedLoading = true;
            LOG_INFO(QString("      ├─ 🤖 [Bot] %1 (PID:%2) -> ✅ Auto Ready (无需等待)")
                         .arg(pName).arg(pid));
        } else {
            it.value().isFinishedLoading = false;
            waitCount++;
            LOG_INFO(QString("      ├─ 👤 [Player] %1 (PID:%2) -> ⏳ 等待加载...")
                         .arg(pName).arg(pid));
        }
    }

    LOG_INFO(QString("      └─ 📊 统计: 共需等待 %1 名真实玩家").arg(waitCount));

    emit gameStarted();
}

void Client::onGameTick()
{
    if (!m_gameStarted) {
        LOG_INFO("🛑 [GameTick] 游戏标志位为 False，停止定时器");
        m_gameTickTimer->stop();
        return;
    }

    QByteArray packet = createW3GSIncomingActionPacket(m_gameTickInterval);

    static int logCount = 0;

    bool hasAction = (packet.size() > 8);
    bool shouldLog = (logCount == 0 || hasAction || (logCount % m_actionLogFrequency < m_actionLogShowLines));

    if (shouldLog) {
        LOG_INFO(QString("⏰ [GameTick] 周期 #%1 执行中... (粘合模式)").arg(logCount));

        // [A] 包内容分析
        QString hexData = packet.toHex().toUpper();
        LOG_INFO(QString("   ├─ 📦 总发送数据: %1 bytes").arg(packet.size()));
        LOG_INFO(QString("   ├─ 🔢 HEX: %1").arg(hexData));

        if (hasAction)
            LOG_INFO("   ├─ ⚡ 类型: [动作包]");
        else
            LOG_INFO("   ├─ 💓 类型: [空心跳]");

        // [B] 发送通道检查
        LOG_INFO(QString("   └─ 📡 广播目标检查 (当前玩家数: %1):").arg(m_players.size() - 1));

        int validTargets = 0;
        bool canSend = false;

        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            quint8 pid = it.key();
            const PlayerData &p = it.value();

            if (pid == m_botPid) continue;

            QString statusStr;
            if (!p.socket) {
                statusStr = "❌ [错误] Socket 指针为空";
            }
            else if (p.socket->state() != QAbstractSocket::ConnectedState) {
                statusStr = QString("⚠️ [异常] Socket 状态不对 (%1)").arg(p.socket->state());
            }
            else if (!p.socket->isValid()) {
                statusStr = "❌ [错误] Socket 句柄无效";
            }
            else {
                statusStr = QString("✅ [正常] 缓冲: %1 bytes").arg(p.socket->bytesToWrite());
                canSend = true;
                validTargets++;
            }

            LOG_INFO(QString("      ├─ 🎯 玩家 [%1] %2 -> %3")
                         .arg(pid)
                         .arg(p.name, statusStr));
        }

        if (validTargets == 0 || !canSend) {
            LOG_ERROR("      └─ ❌ [严重故障] 没有有效的发送目标！");
        }
    }

    logCount++;

    // 6. 执行发送
    broadcastPacket(packet, 0);
}

void Client::onStartLagFinished()
{
    // 树状日志接续
    LOG_INFO("🎬 [缓冲结束] StartLag 计时器触发");

    // 二次安全检查
    if (!m_gameStarted) {
        LOG_INFO("   └─ 🛑 状态: 游戏已取消，停止启动流程");
        return;
    }

    if (m_players.size() <= 1) {
        LOG_INFO("   └─ 🛑 状态: 房间已空 (无真实玩家)，停止启动");
        cancelGame();
        return;
    }

    // 正式启动
    LOG_INFO(QString("   ├─ ✅ 状态: 客户端应已进入画面"));
    LOG_INFO(QString("   └─ 🚀 动作: 正式开启 GameTick 循环 (Interval: %1 ms)").arg(m_gameTickInterval));

    m_gameTickTimer->start();
}

// =========================================================
// 4. UDP 核心处理
// =========================================================

void Client::onUdpReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        handleW3GSUdpPacket(datagram.data(), datagram.senderAddress(), datagram.senderPort());
    }
}

void Client::handleW3GSUdpPacket(const QByteArray &data, const QHostAddress &sender, quint16 senderPort)
{
    // 1. 基础长度校验
    if (data.size() < 4) return;

    quint16 magic = qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(data.data()));
    if (magic == PROTOCOL_MAGIC) {
        LOG_INFO("🔌 [Bot-UDP] 捕获到平台自定义协议包 (F801)");
        LOG_INFO(QString("   ├── 🌍 来源地址: %1:%2").arg(sender.toString()).arg(senderPort));
        LOG_INFO(QString("   └── 🚀 执行动作: 进入 handlePlatformUdpPacket 处理器"));

        handlePlatformUdpPacket(data, sender, senderPort);
        return;
    }

    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);
    quint8 header, msgId;
    quint16 length;
    in >> header >> msgId >> length;

    // 2. 协议头校验 (W3GS UDP 必须以 0xF7 开头)
    if (header != 0xF7) return;

    // 3. 打印根节点信息
    LOG_INFO(QString("📨 [UDP] 收到数据包: 0x%1").arg(QString::number(msgId, 16).toUpper()));
    LOG_INFO(QString("   ├─ 🌍 来源: %1:%2 (Len: %3)")
                 .arg(sender.toString()).arg(senderPort).arg(data.size()));

    // 4. 格式化 Hex 字符串 (每字节加空格)
    QString hexStr = data.toHex().toUpper();
    for(int i = 2; i < hexStr.length(); i += 3) hexStr.insert(i, " ");

    // 如果包太大，截断显示，防止日志刷屏
    if (hexStr.length() > 60) {
        hexStr = hexStr.left(57) + "...";
    }
    LOG_INFO(QString("   ├─ 📦 内容: %1").arg(hexStr));

    // 5. 分发处理
    switch (msgId) {
    case W3GS_TEST: // 0x88
    {
        // 读取剩余的数据作为字符串
        QByteArray payload = data.mid(4);
        QString msg = QString::fromUtf8(payload);

        LOG_INFO("   ├─ 🧪 类型: 连通性测试 (W3GS_TEST)");
        LOG_INFO(QString("   ├─ 📝 消息: %1").arg(msg));

        // 回显数据
        m_udpSocket->writeDatagram(data, sender, senderPort);

        LOG_INFO("   └─ 🚀 动作: 已执行 Echo 回显");
    }
    break;

        // 可以在这里添加更多 case，比如 W3GS_SEARCHGAME (0x2F) 等

    default:
        LOG_INFO("   └─ ❓ 状态: 未知/未处理的包 ID");
        break;
    }
}

void Client::handlePlatformUdpPacket(const QByteArray &data, const QHostAddress &sender, quint16 senderPort)
{
    // 1. 头部长度校验
    if (data.size() < (int)sizeof(PacketHeader)) {
        LOG_ERROR("   └── ❌ 协议错误: 数据包长度不足以解析 Header");
        return;
    }

    PacketHeader *header = (PacketHeader*)data.data();
    PacketType cmd = static_cast<PacketType>(header->command);
    const char* payload = data.data() + sizeof(PacketHeader);

    // 2. 打印指令基础信息
    LOG_INFO(QString("   ├── 🆔 指令编号: 0x%1").arg(QString::number(header->command, 16).toUpper()));

    // 3. 针对不同指令的分发处理
    if (cmd == PacketType::C_S_ROOM_PING) {
        LOG_INFO("   ├── 🎯 业务类型: C_S_ROOM_PING (直连探测)");

        if (data.size() < (int)(sizeof(PacketHeader) + sizeof(CSRoomPingPacket))) {
            LOG_ERROR("   └── ❌ 校验失败: 探测包 Payload 长度不匹配");
            return;
        }

        const CSRoomPingPacket *ping = reinterpret_cast<const CSRoomPingPacket*>(payload);

        // 获取房间实时状态
        quint8 current = static_cast<quint8>(getOccupiedSlots());
        quint8 max     = static_cast<quint8>(getTotalSlots());

        // 构造响应数据
        SCRoomPongPacket resp;
        resp.clientSendTime = ping->clientSendTime;
        resp.currentPlayers = current;
        resp.maxPlayers     = max;

        // 封包
        QByteArray response = createPlatformPacket(PacketType::S_C_ROOM_PONG, &resp, sizeof(resp));

        // 原路返回数据包
        m_udpSocket->writeDatagram(response, sender, senderPort);

        // 4. 打印完成日志
        LOG_INFO(QString("   ├── 📊 房间状态: %1 / %2 (实时同步)").arg(current).arg(max));
        LOG_INFO(QString("   └── ✅ 响应成功: 已回发 S_C_ROOM_PONG (%1 字节)").arg(response.size()));
    }
    else {
        LOG_INFO(QString("   └── ❓ 处理中止: 指令 0x%1 暂无 Bot 侧逻辑实现")
                     .arg(QString::number(header->command, 16).toUpper()));
    }
}

QByteArray Client::createPlatformPacket(PacketType type, const void *payload, quint16 payloadSize)
{
    // 1. 计算总长度并初始化缓冲区
    int headerSize = sizeof(PacketHeader);
    int totalSize = headerSize + payloadSize;
    QByteArray buffer(totalSize, 0);

    // 2. 填充包头基础字段
    PacketHeader *header = reinterpret_cast<PacketHeader*>(buffer.data());
    header->magic      = PROTOCOL_MAGIC;
    header->version    = PROTOCOL_VERSION;
    header->command    = static_cast<quint8>(type);
    header->sessionId  = 0;
    header->seq        = ++m_platformSeq;
    header->payloadLen = payloadSize;

    // 预清空校验位
    header->checksum = 0;
    memset(header->signature, 0, 16);

    // 3. 拷贝负载数据
    if (payload && payloadSize > 0) {
        memcpy(buffer.data() + headerSize, payload, payloadSize);
    }

    // 4. 计算标准 CRC16 校验
    header->checksum = calculateStandardCRC16(buffer);

    // 5. 计算安全签名
    QByteArray secret = "CC_War3_@#_Platform_2026_SecureKey";
    QByteArray signSource = buffer + secret;
    QByteArray fullHash = QCryptographicHash::hash(signSource, QCryptographicHash::Sha256);

    // 取 Hash 的前 16 字节作为签名
    memcpy(header->signature, fullHash.constData(), 16);

    // 6. 打印构建日志
    LOG_DEBUG(QString("📦 [Bot-Packet] 构建完成: Type=0x%1, Size=%2, Seq=%3")
                  .arg(QString::number(type, 16).toUpper())
                  .arg(totalSize).arg(header->seq));

    return buffer;
}
// =========================================================
// 5. 认证与登录逻辑 (Hash, SRP, Register)
// =========================================================

void Client::sendAuthInfo()
{
    QString localIpStr = getPrimaryIPv4();
    quint32 localIp = localIpStr.isEmpty() ? 0 : ipToUint32(localIpStr);

    // 1. 打印根节点
    LOG_INFO("📤 [Auth Info] 发送认证信息 (0x50)");

    // 2. 打印关键参数分支
    LOG_INFO(QString("   ├─ 🌍 本地 IP: %1").arg(localIpStr.isEmpty() ? "Unknown (0)" : localIpStr));

    // 硬编码的常量参数解释
    // Platform: IX86, Product: W3XP (冰封王座), Version: 26 (1.26)
    LOG_INFO("   ├─ 🎮 客户端: W3XP (IX86) | Ver: 26");

    // Locale: 2052 (zh-CN), Timezone: -480 (UTC+8)
    LOG_INFO("   ├─ 🌏 区域: CHN (China) | LCID: 2052 | TZ: UTC+8");

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // Protocol ID
    out << (quint32)0;

    // Platform ("IX86" reversed -> "68XI")
    out.writeRawData("68XI", 4);

    // Product ("W3XP" reversed -> "PX3W")
    out.writeRawData("PX3W", 4);

    // Version Byte
    out << (quint32)26;

    // Language ("enUS" reversed -> "SUne")
    out.writeRawData("SUne", 4);

    // Local IP
    out << localIp;

    // Timezone Bias (UTC+8 = -480 min)
    out << (quint32)0xFFFFFE20;

    // Locale ID & Language ID (2052 = zh-CN)
    out << (quint32)2052 << (quint32)2052;

    // Country Abbr & Name
    out.writeRawData("CHN", 3); out.writeRawData("\0", 1);
    out.writeRawData("China", 5); out.writeRawData("\0", 1);

    // 3. 闭环日志
    LOG_INFO("   └─ 🚀 动作: 数据打包发送 -> 等待 Auth Check (0x51)");

    sendPacket(SID_AUTH_INFO, payload);
}

void Client::handleAuthCheck(const QByteArray &data)
{
    // 1. 打印根节点
    LOG_INFO(QString("🔍 [Auth Check] 处理认证挑战 (0x51) | 有效载荷长度: %1").arg(data.size()));

    dumpPacket(data);

    if (data.size() < 20) {
        LOG_CRITICAL("❌ [协议错误] 服务端返回的数据过短，无法解析 Token。");
        LOG_CRITICAL("👉 可能原因: 1. 服务器拒绝了你的 Version(26) 或 ProductID(W3XP); 2. 你的 IP 被服务器防火墙拉黑。");

        if (data.size() >= 4) {
            quint32 errType = qFromLittleEndian<quint32>(data.left(4).data());
            LOG_ERROR(QString("   └─ 服务端返回 LogonType: 0x%1 (0 表示旧版认证或失败)").arg(errType, 8, 16, QChar('0')));
        }
        return;
    }

    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);
    quint32 udpToken; quint64 mpqFileTime;
    in >> m_logonType >> m_serverToken >> udpToken >> mpqFileTime;
    m_clientToken = QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF;

    int offset = 20;
    int strEnd = data.indexOf('\0', offset);
    QByteArray mpqFileName = data.mid(offset, strEnd - offset);
    offset = strEnd + 1;
    strEnd = data.indexOf('\0', offset);
    if (strEnd == -1) {
        if (offset < data.size()) {
            strEnd = data.size();
        } else {
            LOG_ERROR("   └─ ❌ [错误] 无法找到 Formula String");
            return;
        }
    }
    QByteArray formulaString = data.mid(offset, strEnd - offset);
    int mpqNumber = extractMPQNumber(mpqFileName.constData());

    // 2. 打印解析出的服务端参数
    LOG_INFO("   ├─ 📥 [服务端参数]");
    LOG_INFO(QString("   │  ├─ Logon Type:   %1").arg(m_logonType));
    LOG_INFO(QString("   │  ├─ Server Token: 0x%1").arg(QString::number(m_serverToken, 16).toUpper()));
    LOG_INFO(QString("   │  ├─ MPQ File:     %1").arg(QString(mpqFileName)));
    LOG_INFO(QString("   │  └─ Formula:      %1").arg(QString(formulaString)));

    // 3. 执行哈希计算
    unsigned long checkSum = 0;
    if (QFile::exists(m_war3ExePath)) {
        checkRevisionFlat(formulaString.constData(), m_war3ExePath.toUtf8().constData(),
                          m_stormDllPath.toUtf8().constData(), m_gameDllPath.toUtf8().constData(),
                          mpqNumber, &checkSum);

        LOG_INFO("   ├─ 🧮 [版本校验]");
        LOG_INFO(QString("   │  ├─ Core Path: %1").arg(m_war3ExePath));
        LOG_INFO(QString("   │  └─ Checksum:  0x%1").arg(QString::number(checkSum, 16).toUpper()));
    } else {
        LOG_CRITICAL(QString("   └─ ❌ [严重错误] War3.exe 缺失: %1").arg(m_war3ExePath));
        return;
    }

    // 4. 构造响应包
    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    quint32 exeVersion = 0x011a0001;
    out << m_clientToken << exeVersion << (quint32)checkSum << (quint32)1 << (quint32)0;
    out << (quint32)20 << (quint32)18 << (quint32)0 << (quint32)0;
    out.writeRawData(QByteArray(20, 0).data(), 20);

    QString exeInfoString;
    QFileInfo fileInfo(m_war3ExePath);
    if (fileInfo.exists()) {
        exeInfoString = QString("%1 %2 %3").arg(fileInfo.fileName(), fileInfo.lastModified().toString("MM/dd/yy HH:mm:ss"), QString::number(fileInfo.size()));
        out.writeRawData(exeInfoString.toUtf8().constData(), exeInfoString.length());
        out << (quint8)0;
    } else {
        exeInfoString = "War3.exe 03/18/11 02:00:00 471040";
        out.writeRawData("War3.exe 03/18/11 02:00:00 471040\0", 38);
    }
    out.writeRawData(m_user.toUtf8().constData(), m_user.toUtf8().size());
    out << (quint8)0;

    LOG_INFO("   ├─ 📤 [构造响应]");
    LOG_INFO(QString("   │  ├─ Client Token: 0x%1").arg(QString::number(m_clientToken, 16).toUpper()));
    LOG_INFO(QString("   │  └─ Exe Info:     %1").arg(exeInfoString));

    // 5. 发送并推进流程
    sendPacket(SID_AUTH_CHECK, response);

    LOG_INFO(QString("   └─ 🚀 [流程推进] 发送校验响应 -> 发起登录请求 (%1)").arg(m_loginProtocol));
    sendLoginRequest(m_loginProtocol);
}

void Client::sendLoginRequest(LoginProtocol protocol)
{
    // 1. 打印根节点
    LOG_INFO(QString("🔑 [登录请求] 发起身份验证 (Protocol: 0x%1)").arg(QString::number(protocol, 16).toUpper()));

    if (protocol == Protocol_Old_0x29 || protocol == Protocol_Logon2_0x3A) {
        // === 旧版 DoubleHash 逻辑 ===
        LOG_INFO("   ├─ 📜 算法: DoubleHash (Broken SHA1)");

        QByteArray proof = calculateOldLogonProof(m_pass, m_clientToken, m_serverToken);

        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        out << m_clientToken << m_serverToken;
        out.writeRawData(proof.data(), 20);
        out.writeRawData(m_user.toUtf8().constData(), m_user.toUtf8().size());
        out << (quint8)0;

        BNETPacketID pktId = (protocol == Protocol_Old_0x29 ? SID_LOGONRESPONSE : SID_LOGONRESPONSE2);
        LOG_INFO(QString("   └─ 🚀 动作: 发送 Hash 证明 -> 0x%1").arg(QString::number(pktId, 16).toUpper()));

        sendPacket(pktId, payload);
    }
    else if (protocol == Protocol_SRP_0x53) {
        // === 新版 SRP 逻辑 ===
        LOG_INFO("   ├─ 📜 算法: SRP (Secure Remote Password)");
        LOG_INFO("   ├─ 🔢 步骤: 1/2 (Client Hello)");

        if (m_srp) delete m_srp;
        m_srp = new BnetSRP3(m_user, m_pass);

        BigInt A = m_srp->getClientSessionPublicKey();
        QByteArray A_bytes = A.toByteArray(32, 1, false);

        LOG_INFO("   ├─ 🧮 计算: 生成客户端公钥 (A)");

        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        out.writeRawData(A_bytes.constData(), 32);
        out.writeRawData(m_user.trimmed().toUtf8().constData(), m_user.length());
        out << (quint8)0;

        LOG_INFO("   └─ 🚀 动作: 发送公钥 A + 用户名 -> 等待 0x53");
        sendPacket(SID_AUTH_ACCOUNTLOGON, payload);
    }
}

void Client::handleSRPLoginResponse(const QByteArray &data)
{
    // 1. 打印根节点
    LOG_INFO("🔐 [SRP 响应] 处理服务端挑战 (0x53)");

    if (data.size() < 68) {
        LOG_ERROR(QString("   └─ ❌ [错误] 包长度不足: %1").arg(data.size()));
        return;
    }

    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);
    quint32 status;
    QByteArray saltBytes(32, 0);
    QByteArray serverKeyBytes(32, 0);
    in >> status;
    in.readRawData(saltBytes.data(), 32);
    in.readRawData(serverKeyBytes.data(), 32);

    // 2. 状态检查分支
    if (status != 0) {
        if (status == 0x01) {
            LOG_INFO("   ├─ ⚠️ 状态: 账号不存在 (Code 0x01)");
            LOG_INFO("   └─ 🔄 动作: 触发自动注册流程 -> createAccount()");
            createAccount();
        } else if (status == 0x05) {
            LOG_ERROR("   └─ ❌ 状态: 密码错误 (Code 0x05)");
        } else {
            LOG_ERROR(QString("   └─ ❌ 状态: 登录拒绝 (Code 0x%1)").arg(QString::number(status, 16)));
        }
        return;
    }

    // 3. 计算分支
    if (!m_srp) return;

    LOG_INFO("   ├─ ✅ 状态: 握手继续 (服务端已接受 A)");
    LOG_INFO("   ├─ 📥 参数: 接收 Salt & 服务端公钥 (B)");

    // SRP 数学计算
    m_srp->setSalt(BigInt((const unsigned char*)saltBytes.constData(), 32, 4, false));
    BigInt B_val((const unsigned char*)serverKeyBytes.constData(), 32, 1, false);
    BigInt K = m_srp->getHashedClientSecret(B_val);
    BigInt A = m_srp->getClientSessionPublicKey();
    BigInt M1 = m_srp->getClientPasswordProof(A, B_val, K);
    QByteArray proofBytes = M1.toByteArray(20, 1, false);

    LOG_INFO("   ├─ 🧮 计算: 派生 SessionKey (K) -> 生成证明 (M1)");

    // 构造响应
    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData(proofBytes.constData(), 20);
    out.writeRawData(QByteArray(20, 0).data(), 20); // M2 placeholder/Salt2

    // 4. 闭环日志
    LOG_INFO("   └─ 🚀 动作: 发送 M1 证明 (0x54) -> 等待最终结果");
    sendPacket(SID_AUTH_ACCOUNTLOGONPROOF, response);
}

void Client::createAccount()
{
    // 1. 打印根节点
    LOG_INFO("📝 [账号注册] 发起注册请求 (0x52)");

    if (m_user.isEmpty() || m_pass.isEmpty()) {
        LOG_ERROR("   └─ ❌ [错误] 用户名或密码为空");
        return;
    }

    LOG_INFO(QString("   ├─ 👤 用户: %1").arg(m_user));

    // 生成随机 Salt 和 Verifier (模拟)
    QByteArray s_bytes(32, 0);
    for (int i = 0; i < 32; ++i) s_bytes[i] = (char)(QRandomGenerator::global()->generate() & 0xFF);

    QByteArray v_bytes(32, 0); // 明文密码模式 (PVPGN常见配置)
    QByteArray passRaw = m_pass.toLatin1();
    memcpy(v_bytes.data(), passRaw.constData(), qMin(passRaw.size(), 32));

    LOG_INFO("   ├─ 🎲 生成: Random Salt (32 bytes) & Password Hash");

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData(s_bytes.constData(), 32);
    out.writeRawData(v_bytes.constData(), 32);
    out.writeRawData(m_user.toLower().trimmed().toLatin1().constData(), m_user.length());
    out << (quint8)0;

    // 2. 闭环日志
    LOG_INFO("   └─ 🚀 动作: 数据打包发送 -> 等待结果");
    sendPacket(SID_AUTH_ACCOUNTCREATE, payload);
}

// =========================================================
// 6. 数据加密与算法
// =========================================================

QByteArray Client::calculateBrokenSHA1(const QByteArray &data)
{
    t_hash hashOut;
    bnet_hash(&hashOut, data.size(), data.constData());
    QByteArray result;
    QDataStream ds(&result, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    for(int i = 0; i < 5; i++) ds << hashOut[i];
    return result;
}

QByteArray Client::calculateOldLogonProof(const QString &password, quint32 clientToken, quint32 serverToken)
{
    QByteArray passHashBE = calculateBrokenSHA1(password.toLatin1());
    QByteArray buffer;
    QDataStream ds(&buffer, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << clientToken << serverToken;
    QDataStream dsReader(passHashBE);
    dsReader.setByteOrder(QDataStream::BigEndian);
    for(int i=0; i<5; i++) { quint32 val; dsReader >> val; ds << val; }

    QByteArray finalHashBE = calculateBrokenSHA1(buffer);
    QByteArray proofToSend;
    QDataStream dsFinal(&proofToSend, QIODevice::WriteOnly);
    dsFinal.setByteOrder(QDataStream::LittleEndian);
    QDataStream dsFinalReader(finalHashBE);
    dsFinalReader.setByteOrder(QDataStream::BigEndian);
    for(int i=0; i<5; i++) { quint32 val; dsFinalReader >> val; dsFinal << val; }
    return proofToSend;
}

// =========================================================
// 7. 聊天与频道管理
// =========================================================

void Client::enterChat() {
    // 树状日志
    LOG_INFO("🚪 [进入聊天] 发送 SID_ENTERCHAT (0x0A)");
    LOG_INFO("   └─ 🚀 动作: 请求进入聊天室环境");

    sendPacket(SID_ENTERCHAT, QByteArray(2, '\0'));
}

void Client::queryChannelList() {
    // 树状日志
    LOG_INFO("📜 [频道列表] 发起查询请求 (0x0B)");
    LOG_INFO("   └─ 🚀 动作: 等待服务器返回列表...");

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out << (quint32)0;
    sendPacket(SID_GETCHANNELLIST, payload);
}

void Client::joinChannel(const QString &channelName) {
    if (channelName.isEmpty()) return;

    // 树状日志
    LOG_INFO(QString("💬 [加入频道] 请求加入: %1").arg(channelName));
    LOG_INFO("   ├─ 🚩 标志: First Join (0x01)");
    LOG_INFO("   └─ 🚀 动作: 发送 SID_JOINCHANNEL (0x0C)");

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out << (quint32)0x01; // First Join
    out.writeRawData(channelName.toUtf8().constData(), channelName.toUtf8().size());
    out << (quint8)0;
    sendPacket(SID_JOINCHANNEL, payload);
}

void Client::joinRandomChannel()
{
    // 1. 打印根节点
    LOG_INFO(QString("🎲 [随机频道] Bot-%1 正在选择...").arg(m_user));

    QStringList channels = {"The Void", "Frozen Throne", "Chat", "USA-1", "Human Castle", "Op War3Bot"};
    QString source = "默认列表 (Default)";

    // 2. 尝试从配置文件读取
    QString configPath = QCoreApplication::applicationDirPath() + "/config/war3bot.ini";
    if (QFile::exists(configPath)) {
        QSettings settings(configPath, QSettings::IniFormat);
        QString configChans = settings.value("bots/channels", "").toString();

        if (!configChans.isEmpty()) {
            QStringList customList = configChans.split(",", Qt::SkipEmptyParts);
            if (!customList.isEmpty()) {
                channels = customList;
                source = "配置文件 (Config)";
            }
        }
    }

    // 3. 打印候选池信息
    LOG_INFO(QString("   ├─ 📚 候选池来源: %1").arg(source));
    LOG_INFO(QString("   ├─ 📊 候选数量: %1 个").arg(channels.size()));

    // 4. 随机选择并执行
    if (!channels.isEmpty()) {
        int index = QRandomGenerator::global()->bounded(channels.size());
        QString targetChannel = channels.at(index).trimmed();

        LOG_INFO(QString("   └─ 🎯 命中目标: %1 -> 执行 joinChannel").arg(targetChannel));
        joinChannel(targetChannel);
    } else {
        LOG_WARNING("   └─ ⚠️ [警告] 候选列表为空，无法加入");
    }
}

// =========================================================
// 8. 房间主机逻辑
// =========================================================

void Client::stopAdv(const QString &context)
{
    LOG_INFO(QString("🛑 [停止广播] 发送 SID_STOPADV (0x02) | 来源: %1").arg(context));
    sendPacket(SID_STOPADV, QByteArray());
}

void Client::updateAdv()
{
    // 1. 基础状态检查
    if (!isConnected() || m_gameStarted) return;

    m_isRefreshingAdv = true;

    LOG_INFO(QString("♻️ [Adv热更新] 开始复刻创建逻辑以同步看板 | 房主: %1").arg(m_host));

    // 2. 地图加载校验
    if (!m_war3Map.isValid() || m_lastLoadedMapPath != m_currentMapPath) {
        LOG_INFO(QString("   ├─ 🔄 检测到地图状态变更，尝试重新加载: %1").arg(m_currentMapPath));
        if (!m_war3Map.load(m_currentMapPath)) {
            LOG_ERROR("   └─ ❌ [严重错误] 重新加载地图失败，热更新中止");
            return;
        }
        setMapData(m_war3Map.getMapRawData());
        m_lastLoadedMapPath = m_currentMapPath;
    }

    // 3. 同步地图选项
    if (m_enableObservers) {
        m_war3Map.enableObservers();
    }

    // 4. 获取最新的 StatString
    QString statDisplayName = m_host.isEmpty() ? m_botDisplayName : m_host;
    QByteArray encodedData = m_war3Map.getEncodedStatString(statDisplayName);

    // 5. 准备看板数据
    m_hostCounter++;

    // 计算空位
    int freeSlots = m_slots.size() - getOccupiedSlots() + 1;
    if (freeSlots < 0) freeSlots = 0;
    if (freeSlots > 9) freeSlots = 9;

    QByteArray finalStatString;
    finalStatString.append('0' + freeSlots);

    QString hexCounter = QString("%1").arg(m_hostCounter, 8, 16, QChar('0'));
    for(int i = hexCounter.length() - 1; i >= 0; i--) {
        finalStatString.append(hexCounter[i].toLatin1());
    }
    finalStatString.append(encodedData);

    // 6. 构建并发送 0x1C 数据包
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    quint32 state = m_gameConfig.password.isEmpty() ? 0x00000010 : 0x00000011;

    out << state << (quint32)0
        << (quint16)m_gameConfig.comboGameType
        << (quint16)m_gameConfig.subGameType
        << (quint32)m_gameConfig.providerVersion
        << (quint32)m_gameConfig.ladderType;

    out.writeRawData(m_gameConfig.gameName.toUtf8().constData(), m_gameConfig.gameName.toUtf8().size()); out << (quint8)0;
    out.writeRawData(m_gameConfig.password.toUtf8().constData(), m_gameConfig.password.toUtf8().size()); out << (quint8)0;
    out.writeRawData(finalStatString.constData(), finalStatString.size()); out << (quint8)0;

    sendPacket(SID_STARTADVEX3, payload);

    LOG_INFO(QString("   └─ ✅ 列表热更新已发出 (HostCounter: %1, 空位: %2)").arg(m_hostCounter).arg(freeSlots));
}

void Client::resetGame()
{
    LOG_INFO(QString("🧹 [Client-%1] 执行内存重置").arg(m_botPid));

    // 1. 停止计时器
    if (m_startLagTimer) m_startLagTimer->stop();
    if (m_gameTickTimer) m_gameTickTimer->stop();
    if (m_startTimer)    m_startTimer->stop();
    if (m_pingTimer)     m_pingTimer->stop();

    // 2. 清理玩家连接
    if (!m_playerSockets.isEmpty()) {
        LOG_INFO(QString("   ├─ 🔌 正在释放 %1 个玩家 Socket").arg(m_playerSockets.size()));
        for (auto socket : qAsConst(m_playerSockets)) {
            if (socket) {
                socket->disconnectFromHost();
                socket->deleteLater();
            }
        }
        m_playerSockets.clear();
    }

    // 3. 清空容器
    m_playerBuffers.clear();
    m_actionQueue.clear();
    m_players.clear();

    // 4. 槽位状态重置
    initSlots();
    LOG_INFO(QString("   ├─ 🧩 槽位状态已重置 (当前槽位总数: %1)").arg(m_slots.size()));

    // 5. 状态标志位复位
    m_gameStarted = false;
    m_isRefreshingAdv = false;

    LOG_INFO(QString("   └─ ✅ 重置完成"));
}

void Client::cancelGame()
{
    if (m_isCanceling) return;

    LOG_INFO("📡 [协议动作] 正在向战网发送取消房间指令...");

    m_isCanceling = true;

    if (isConnected()) {
        enterChat();
        joinRandomChannel();
        LOG_INFO("   ├─ 🔄 状态同步: 已请求回到大厅并加入频道");
    }

    m_isCanceling = false;

    emit gameCancelled();
}

void Client::createGame(const QString &gameName, const QString &password, ProviderVersion providerVersion, ComboGameType comboGameType, SubGameType subGameType, LadderType ladderType, CommandSource commandSource)
{
    // 1. 初始化槽位
    if (m_enableObservers) {
        initSlotsFromMap(12);
    } else {
        initSlotsFromMap(10);
    }

    QString sourceStr = (commandSource == From_Server) ? "Server" : "Client";
    LOG_INFO(QString("🚀 [创建房间] 发起请求: [%1]").arg(gameName));
    LOG_INFO(QString("   ├─ 🎮 来源: %1 | 密码: %2 | 槽位: %3 裁判: %4")
                 .arg(sourceStr, password.isEmpty() ? "None" : "***",
                      m_enableObservers ? "12" : "10", m_enableObservers ? "有" : "无"));

    m_gameConfig = { gameName, password, providerVersion, comboGameType, subGameType, ladderType };

    // 2. UDP 端口汇报检查
    if (m_udpSocket->state() == QAbstractSocket::BoundState) {
        quint16 localPort = m_udpSocket->localPort();
        QByteArray portPayload;
        QDataStream portOut(&portPayload, QIODevice::WriteOnly);
        portOut.setByteOrder(QDataStream::LittleEndian);
        portOut << (quint16)localPort;
        sendPacket(SID_NETGAMEPORT, portPayload);

        LOG_INFO(QString("   ├─ 🔧 端口汇报: UDP %1 -> SID_NETGAMEPORT").arg(localPort));
    } else {
        LOG_CRITICAL("   └─ ❌ [严重错误] UDP 未绑定，无法创建游戏");
        return;
    }

    // 3. 地图加载
    if (!QFile::exists(m_currentMapPath)) {
        LOG_CRITICAL(QString("   └─ ❌ [严重错误] 地图文件不存在: %1").arg(m_currentMapPath));
        return;
    }

    if (!m_war3Map.isValid() || m_lastLoadedMapPath != m_currentMapPath) {

        LOG_INFO(QString("   ├─ 🔄 正在加载地图文件: %1 ...").arg(m_currentMapPath));
        QElapsedTimer timer;
        timer.start();

        if (!m_war3Map.load(m_currentMapPath)) {
            LOG_CRITICAL(QString("   └─ ❌ [严重错误] 地图加载失败: %1").arg(m_currentMapPath));
            return;
        }

        setMapData(m_war3Map.getMapRawData());
        m_lastLoadedMapPath = m_currentMapPath;

        LOG_INFO(QString("   ├─ ✅ 地图加载完毕 (耗时: %1 ms)").arg(timer.elapsed()));
    } else {
        LOG_INFO(QString("   ├─ ⚡️ 命中内存缓存，跳过加载: %1").arg(QFileInfo(m_currentMapPath).fileName()));
    }

    // 设置裁判
    if (m_enableObservers) {
        m_war3Map.enableObservers();
    }

    QString mapName = QFileInfo(m_lastLoadedMapPath).fileName();
    QString statDisplayName = m_host.isEmpty() ? m_botDisplayName : m_host;
    QByteArray encodedData = m_war3Map.getEncodedStatString(statDisplayName);
    if (encodedData.isEmpty()) {
        LOG_CRITICAL("   └─ ❌ [严重错误] StatString 生成失败");
        return;
    }

    LOG_INFO(QString("   ├─ 🗺️ 地图加载: %1").arg(mapName));
    LOG_INFO(QString("   ├─ 👤 视觉房主: %1 %2")
                 .arg(statDisplayName, m_host.isEmpty() ? "(机器人)" : ""));

    // 4. 参数构建
    m_hostCounter++;
    m_randomSeed = (quint32)QRandomGenerator::global()->generate();

    QByteArray finalStatString;
    finalStatString.append('9'); // 空闲槽位标识

    QString hexCounter = QString("%1").arg(m_hostCounter, 8, 16, QChar('0'));
    for(int i = hexCounter.length() - 1; i >= 0; i--) {
        finalStatString.append(hexCounter[i].toLatin1());
    }
    finalStatString.append(encodedData);

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    quint32 state = password.isEmpty() ? 0x00000010 : 0x00000011;

    out << state << (quint32)0 << (quint16)comboGameType << (quint16)subGameType
        << (quint32)providerVersion << (quint32)ladderType;

    out.writeRawData(gameName.toUtf8().constData(), gameName.toUtf8().size()); out << (quint8)0;
    out.writeRawData(password.toUtf8().constData(), password.toUtf8().size()); out << (quint8)0;
    out.writeRawData(finalStatString.constData(), finalStatString.size()); out << (quint8)0;

    // 5. 发送并启动计时器
    sendPacket(SID_STARTADVEX3, payload);

    if (!m_pingTimer->isActive()) {
        m_pingTimer->start(1000);
        LOG_INFO("   └─ 💓 动作: 发送请求(0x1C) + 启动 Ping 循环 (1s)");
    } else {
        LOG_INFO("   └─ 📤 动作: 发送请求(0x1C) (Ping 循环运行中)");
    }
}

void Client::startGame()
{
    if (m_gameStarted) return;
    if (m_startTimer->isActive()) return;

    // 1. 先关大门：停止广播，停止 Ping
    stopAdv("Start Game"); // 停止 UDP 广播
    if (m_pingTimer && m_pingTimer->isActive()) {
        m_pingTimer->stop();
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        it.value().lastResponseTime = now;
    }
    LOG_INFO("🛡️ [状态保护] 已刷新全员活跃时间，防止倒计时期间超时");

    // 2. 发送最后一条大厅消息
    MultiLangMsg msg;
    msg.add("zh_CN", "游戏将于 5 秒后开始...")
        .add("en", "Game starts in 5 seconds...");

    broadcastChatMessage(msg);

    // 3. 发送倒计时包
    broadcastPacket(createW3GSCountdownStartPacket(), 0);

    // 4. 最后启动定时器
    m_startTimer->start(5000);

    LOG_INFO("⏳ [游戏启动] 开始倒计时...");
}

void Client::abortGame()
{
    if (m_startTimer->isActive()) {
        m_startTimer->stop();
        MultiLangMsg msg;
        msg.add("zh_CN", "倒计时已取消。")
            .add("en", "Countdown aborted.");
        broadcastChatMessage(msg);

        // 3. 恢复 Ping 循环
        if (!m_pingTimer->isActive()) {
            m_pingTimer->start();
            LOG_INFO("✅ [状态恢复] Ping 循环已重启");
        }

        // 恢复广播
        // sendPacket(SID_STARTADVEX3, ...);
    }
}

// =========================================================
// 9. 地图数据处理
// =========================================================

void Client::setMapData(const QByteArray &data)
{
    m_mapData = data; // 浅拷贝
    m_mapSize = (quint32)m_mapData.size();

    // 可选：打印日志
    if (m_mapSize > 0) {
        LOG_INFO(QString("🗺️ [Client] 地图数据初始化完成，大小: %1").arg(m_mapSize));
    }
}

void Client::setCurrentMap(const QString &filePath)
{
    if (filePath.isEmpty()) {
        m_currentMapPath = m_dota683dPath;
        LOG_INFO(QString("🗺️ [设置地图] 恢复默认地图: %1").arg(QFileInfo(m_currentMapPath).fileName()));
    } else {
        m_currentMapPath = filePath;
        LOG_INFO(QString("🗺️ [设置地图] 切换为: %1").arg(QFileInfo(m_currentMapPath).fileName()));
    }
}

void Client::setGameTickInterval(quint16 interval)
{
    if (interval < 50) interval = 50;
    if (interval > 200) interval = 200;

    if (m_gameTickInterval != interval) {
        m_gameTickInterval = interval;

        if (m_gameTickTimer) {
            m_gameTickTimer->setInterval(m_gameTickInterval);
        }

        LOG_INFO(QString("⚙️ [设置时间] 游戏心跳间隔调整为: %1 ms").arg(m_gameTickInterval));
    }
}

// =========================================================
// 10. 游戏数据处理
// =========================================================

QByteArray Client::createW3GSPingFromHostPacket()
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // 1. Header (F7 01 08 00)
    // ID: 0x01 (W3GS_PING_FROM_HOST)
    // Length: 8 bytes
    out << (quint8)0xF7 << (quint8)0x01 << (quint16)8;

    // 2. Payload (4 bytes)
    // 通常使用当前系统运行时间 (毫秒)
    // 客户端收到后会在 0x46 (Pong) 包里原样发回来，用于计算延迟
    quint32 currentTick = static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF);
    out << currentTick;

    return packet;
}

QByteArray Client::createW3GSSlotInfoJoinPacket(quint8 playerID, const QHostAddress& externalIp, quint16 localPort)
{
    // 这个包很重要，保留详细日志
    LOG_INFO("📦 [构建包] W3GS_SLOTINFOJOIN (0x04)");

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    QByteArray slotData = serializeSlotData();
    quint16 slotBlockSize = (quint16)slotData.size() + 6;

    out << (quint8)0xF7 << (quint8)0x04 << (quint16)0; // Header
    out << slotBlockSize;
    out.writeRawData(slotData.data(), slotData.size());
    out << (quint32)m_randomSeed << (quint8)m_layoutStyle << (quint8)m_slots.size();

    out << (quint8)playerID;
    out << (quint16)2 << (quint16)qToBigEndian(localPort);
    writeIpToStreamWithLog(out, externalIp);
    out << (quint32)0 << (quint32)0;

    quint16 totalSize = (quint16)packet.size();
    QDataStream lenStream(&packet, QIODevice::ReadWrite);
    lenStream.setByteOrder(QDataStream::LittleEndian);
    lenStream.skipRawData(2);
    lenStream << totalSize;

    LOG_INFO(QString("   ├─ 📏 尺寸: 总长 %1 / 块长 %2").arg(totalSize).arg(slotBlockSize));
    LOG_INFO(QString("   └─ 👤 专属: PID %1 (IP: %2)").arg(playerID).arg(externalIp.toString()));

    // 校验逻辑保持不变，但换成 tree log
    if (packet.size() > 6 + slotBlockSize) {
        int pidOffset = 6 + slotBlockSize;
        quint8 pidInPacket = (quint8)packet.at(pidOffset);
        if (pidInPacket != playerID) {
            LOG_CRITICAL(QString("   └─ ❌ [严重警告] PID 偏移校验失败! (读到: %1)").arg(pidInPacket));
        }
    }

    return packet;
}

QByteArray Client::createW3GSRejectJoinPacket(RejectReason reason)
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // Header: F7 05 [Length]
    out << (quint8)0xF7 << (quint8)0x05 << (quint16)0;

    out << (quint32)reason;

    // 回填长度
    QDataStream lenStream(&packet, QIODevice::ReadWrite);
    lenStream.setByteOrder(QDataStream::LittleEndian);
    lenStream.skipRawData(2);
    lenStream << (quint16)packet.size();

    return packet;
}

QByteArray Client::createPlayerInfoPacket(quint8 pid, const QString& name,
                                          const QHostAddress& externalIp, quint16 externalPort,
                                          const QHostAddress& internalIp, quint16 internalPort)
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // 1. 写入 Header (长度稍后回填)
    out << (quint8)0xF7 << (quint8)0x06 << (quint16)0;

    // 2. 写入 Id
    out << (quint32)2;  // Internal ID / P2P Key
    out << (quint8)pid;

    // 3. 写入玩家名字
    QByteArray nameBytes = name.toUtf8();
    out.writeRawData(nameBytes.data(), nameBytes.length());
    out << (quint8)0;   // Null terminator

    out << (quint16)1;  // Unknown

    // 4. 写入网络配置
    out << (quint16)2;
    out << (quint16)qToBigEndian(externalPort);
    writeIpToStreamWithLog(out, externalIp);
    out << (quint32)0 << (quint32)0;

    out << (quint16)2;
    out << (quint16)qToBigEndian(internalPort);
    writeIpToStreamWithLog(out, internalIp);
    out << (quint32)0 << (quint32)0;

    // 5. 回填包总长度
    QDataStream lenStream(&packet, QIODevice::ReadWrite);
    lenStream.setByteOrder(QDataStream::LittleEndian);
    lenStream.skipRawData(2);
    lenStream << (quint16)packet.size();

    return packet;
}

QByteArray Client::createW3GSPlayerLeftPacket(quint8 pid, LeaveReason reason)
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // Header: F7 07 [Length]
    out << (quint8)0xF7 << (quint8)0x07 << (quint16)0;

    // Player ID (1 byte)
    out << (quint8)pid;

    // Reason (4 bytes)
    // 0x01 = Remote Connection Closed (掉线)
    // 0x08 = Left the game (主动离开)
    // 0x0C = Kicked (被踢)
    out << (quint32)reason;

    // 回填长度
    QDataStream lenStream(&packet, QIODevice::ReadWrite);
    lenStream.setByteOrder(QDataStream::LittleEndian);
    lenStream.skipRawData(2);
    lenStream << (quint16)packet.size();

    return packet;
}

QByteArray Client::createW3GSSlotInfoPacket()
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // 1. 写入 Header (长度稍后回填)
    out << (quint8)0xF7 << (quint8)0x09 << (quint16)0;

    // 2. 获取槽位数据 (包含 numSlots 和所有 slot 的 9字节数据)
    // 注意：serializeSlotData 应该返回 [NumSlots(1)][Slot1(9)]...[SlotN(9)]
    QByteArray slotData = serializeSlotData();

    // 长度 = slotData.size() + 4(Seed) + 1(Layout) + 1(NumPlayers)
    quint16 internalDataLen = (quint16)slotData.size() + 6;

    // 3. 写入内部数据长度
    out << internalDataLen;

    // 4. 写入槽位数据
    out.writeRawData(slotData.data(), slotData.size());

    // 5. 写入随机种子、布局样式、槽位总数
    out << (quint32)m_randomSeed;                           // 随机种子
    out << (quint8)m_layoutStyle;                           // 布局样式
    out << (quint8)m_slots.size();                          // 槽位总数

    // 6. 回填包总长度
    QDataStream lenStream(&packet, QIODevice::ReadWrite);
    lenStream.setByteOrder(QDataStream::LittleEndian);
    lenStream.skipRawData(2);
    lenStream << (quint16)packet.size();

    return packet;
}

QByteArray Client::createW3GSPlayerLoadedPacket(quint8 pid)
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    // Header: F7 08 05 00
    out << (quint8)0xF7 << (quint8)0x08 << (quint16)5;
    // Payload: PID (1 byte)
    out << (quint8)pid;
    return packet;
}

QByteArray Client::createW3GSCountdownStartPacket()
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    // Header: F7 0A 04 00 (长度固定为4)
    out << (quint8)0xF7 << (quint8)0x0A << (quint16)4;
    return packet;
}

QByteArray Client::createW3GSCountdownEndPacket()
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    // Header: F7 0B 04 00
    out << (quint8)0xF7 << (quint8)0x0B << (quint16)4;
    return packet;
}

QByteArray Client::createW3GSIncomingActionPacket(quint16 sendInterval)
{
    // 1. 处理空包 (严格 6 字节)
    if (m_actionQueue.isEmpty()) {
        QByteArray packet;
        QDataStream out(&packet, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        out << (quint8)0xF7 << (quint8)0x0C << (quint16)6 << (quint16)sendInterval;
        return packet;
    }

    // 2. 准备 Payload (动作块集合)
    // 所有的 PID, Length, Data 必须连续写入，且不能发生指针覆盖
    QByteArray payload;
    QDataStream ds(&payload, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    // 备份并清空队列
    auto currentActions = m_actionQueue;
    m_actionQueue.clear();

    for (const auto &act : currentActions) {
        ds << (quint8)act.pid;
        ds << (quint16)act.data.size();
        // 关键：必须使用流写入数据，确保 ds 的指针向后移动
        ds.writeRawData(act.data.constData(), act.data.size());
    }

    // 3. 计算 CRC (范围：仅针对上面生成的全部 Payload)
    quint16 crcVal = calculateCRC32Lower16(payload);

    // 4. 组装最终发出的 TCP 包
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out << (quint8)0xF7 << (quint8)0x0C;                    // 2 bytes
    out << (quint16)(8 + payload.size());                   // 2 bytes (Total Length)
    out << (quint16)sendInterval;                           // 2 bytes
    out << (quint16)crcVal;                                 // 2 bytes
    out.writeRawData(payload.constData(), payload.size());  // N bytes (Actions)

    return packet;
}

QByteArray Client::createW3GSChatFromHostPacket(const QByteArray &rawBytes, quint8 senderPid, quint8 toPid, ChatFlag flag, quint32 extraData)
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // 1. Header
    out << (quint8)0xF7 << (quint8)0x0F << (quint16)0;

    // 2. Body
    out << (quint8)1; // Num Receivers
    out << (quint8)toPid;
    out << (quint8)senderPid;
    out << (quint8)flag;

    switch (flag) {
    case TeamChange:
    case ColorChange:
    case RaceChange:
    case HandicapChange:
        out << (quint8)(extraData & 0xFF);
        break;
    case Scope:
        out << (quint32)extraData;
        break;
    default: break; // Message has no extra
    }

    out.writeRawData(rawBytes.data(), rawBytes.length());
    out << (quint8)0;

    // 3. Length
    quint16 totalSize = (quint16)packet.size();
    QDataStream lenStream(&packet, QIODevice::ReadWrite);
    lenStream.setByteOrder(QDataStream::LittleEndian);
    lenStream.skipRawData(2);
    lenStream << totalSize;

    // 格式化 Hex 用于日志
    QString hexPreview = QString(rawBytes.toHex().toUpper());

    LOG_INFO(QString("📦 [构建包] 聊天/控制 (0x0F)"));
    LOG_INFO(QString("   ├─ 🎯 目标: %1 -> %2").arg(senderPid).arg(toPid));
    LOG_INFO(QString("   ├─ 🚩 类型: 0x%1 (Extra: %2)").arg(QString::number((int)flag, 16)).arg(extraData));
    LOG_INFO(QString("   └─ 📝 数据: %1").arg(hexPreview));

    return packet;
}

QByteArray Client::createW3GSMapCheckPacket()
{
    LOG_INFO("📦 [构建包] W3GS_MAPCHECK (0x3D)");

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out << (quint8)0xF7 << (quint8)0x3D << (quint16)0;
    out << (quint32)1; // Unknown

    QString mapPath = "Maps\\Download\\" + m_war3Map.getMapName();
    out.writeRawData(mapPath.toLocal8Bit().data(), mapPath.toLocal8Bit().length());
    out << (quint8)0;

    quint32 fileSize = m_war3Map.getMapSize();
    quint32 fileInfo = m_war3Map.getMapInfo();
    quint32 fileCRC  = m_war3Map.getMapCRC();
    QByteArray sha1 = m_war3Map.getMapSHA1Bytes();

    out << fileSize << fileInfo << fileCRC;

    if (sha1.size() != 20) {
        LOG_INFO(QString("   ├─ ⚠️ SHA1 长度异常 (%1) -> 补零").arg(sha1.size()));
        sha1.resize(20);
    }
    out.writeRawData(sha1.data(), 20);

    quint16 totalSize = (quint16)packet.size();
    QDataStream lenStream(&packet, QIODevice::ReadWrite);
    lenStream.setByteOrder(QDataStream::LittleEndian);
    lenStream.skipRawData(2);
    lenStream << totalSize;

    QString sha1Hex = sha1.toHex().toUpper();
    LOG_INFO(QString("   ├─ 📊 参数: Size=%1 | CRC=0x%2").arg(fileSize).arg(QString::number(fileCRC, 16).toUpper()));
    LOG_INFO(QString("   └─ 🔐 SHA1: %1...").arg(sha1Hex));

    return packet;
}

QByteArray Client::createW3GSStartDownloadPacket(quint8 fromPid)
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // Header: F7 3F [Len]
    out << (quint8)0xF7 << (quint8)0x3F << (quint16)0;
    // (UINT32) Unknown
    out << (quint32)1;

    // (UINT8) Player number
    out << (quint8)fromPid;

    QDataStream lenStream(&packet, QIODevice::ReadWrite);
    lenStream.setByteOrder(QDataStream::LittleEndian);
    lenStream.skipRawData(2);
    lenStream << (quint16)packet.size();

    return packet;
}

QByteArray Client::createW3GSMapPartPacket(quint8 toPid, quint8 fromPid, quint32 offset, const QByteArray &chunkData)
{
    // 1. 使用工业标准 zlib 计算 CRC32
    uLong zCrc = crc32(0L, Z_NULL, 0);
    zCrc = crc32(zCrc, reinterpret_cast<const Bytef*>(chunkData.constData()), chunkData.size());
    quint32 finalCrc = static_cast<quint32>(zCrc);

    // 2. 构建数据包
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // 包头 (Header)
    out << (quint8)0xF7 << (quint8)0x43 << (quint16)0;

    // 协议字段 (Fields)
    out << (quint8)toPid;      // 目标 PID
    out << (quint8)fromPid;    // 来源 PID (主机)
    out << (quint32)1;         // Unknown (固定为 1)
    out << (quint32)offset;    // 当前分片的偏移量
    out << (quint32)finalCrc;  // 数据内容的 CRC32

    if (chunkData.size() > 0) {
        out.writeRawData(chunkData.constData(), chunkData.size());
    }

    // 3. 回填包长度
    out.device()->seek(2);
    out << (quint16)packet.size();

    // 调试输出
    if (offset == 1442) {
        LOG_INFO(QString("Sending Chunk 1442. Size: %1 CRC: %2")
                     .arg(QString::number(chunkData.size()), QString::number(finalCrc, 16)));
    }

    return packet;
}

void Client::broadcastChatMessage(const MultiLangMsg& msg, quint8 excludePid)
{
    if (m_gameStarted || m_startTimer->isActive()) {
        qDebug() << "🛑 [拦截] 试图在游戏/倒计时期间发送大厅消息，已阻止：" << msg.get("en");
        return;
    }

    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        quint8 pid = it.key();

        // 排除 PID 2 (Host) 和 指定排除的 PID
        if (pid == excludePid || pid == m_botPid) continue;

        PlayerData &playerData = it.value();
        QTcpSocket* socket = playerData.socket;

        if (!socket || socket->state() != QAbstractSocket::ConnectedState) continue;

        // 1. 根据玩家的语言标记 (cn/en/ru...) 获取对应文本
        QString textToSend = msg.get(playerData.language);

        // 2. 转码 (根据玩家特定的 Codec)
        // 注意：这里 textToSend 已经是对应语言的 Unicode 字符串了
        // 例如俄语玩家获取到了俄文，然后用 CP1251 转码
        QByteArray finalBytes = playerData.codec->fromUnicode(textToSend);

        // 3. 造包并发送
        QByteArray chatPacket = createW3GSChatFromHostPacket(
            finalBytes,
            m_botPid,           // From Host
            pid,                // To Target Player
            ChatFlag::Message
            );

        socket->write(chatPacket);
        socket->flush();
    }
}

void Client::broadcastPacket(const QByteArray &packet, quint8 pid, bool includeOnly)
{
    // 遍历所有玩家
    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        const PlayerData &playerData = it.value();

        bool shouldSend = false;

        if (includeOnly) {
            // 模式 A: 私人模式
            if (playerData.pid == pid) {
                shouldSend = true;
            }
        } else {
            // 模式 B: 广播模式
            if (pid == 0 || playerData.pid != pid) {
                shouldSend = true;
            }
        }

        if (!shouldSend) continue;

        // 🛡️ Socket 安全检查
        if (!playerData.socket || playerData.socket->state() != QAbstractSocket::ConnectedState) {
            continue;
        }

        // 执行发送
        playerData.socket->write(packet);
        playerData.socket->flush();
    }
}

void Client::broadcastSlotInfo(quint8 excludePid)
{
    QByteArray slotPacket = createW3GSSlotInfoPacket();

    // 调用 broadcastPacket
    broadcastPacket(slotPacket, excludePid);

    QString excludeStr = (excludePid != 0) ? QString(" (排除 PID: %1)").arg(excludePid) : "";
    LOG_INFO(QString("📢 [广播状态] 槽位更新 (0x09)%1").arg(excludeStr));
}

// =========================================================
// 11. 槽位辅助函数
// =========================================================

void Client::initSlots(quint8 maxPlayers)
{
    LOG_INFO(QString("🧹 [槽位重置] 地图槽位数: %1").arg(maxPlayers));

    m_slots.clear();
    m_slots.resize(maxPlayers);
    m_players.clear();

    for (auto socket : qAsConst(m_playerSockets)) {
        if (socket->state() == QAbstractSocket::ConnectedState) socket->disconnectFromHost();
        socket->deleteLater();
    }
    m_playerSockets.clear();
    m_playerBuffers.clear();

    static const quint8 DOTA_COLORS[] = {
        1, 2, 3, 4, 5,
        7, 8, 9, 10, 11,
        0, 6
    };

    // 初始化地图槽
    for (quint8 i = 0; i < maxPlayers; ++i) {
        GameSlot &slot = m_slots[i];
        slot = GameSlot();

        slot.pid            = 0;
        slot.slotStatus     = Open;
        slot.computer       = Human;
        slot.downloadStatus = NotStarted;

        if (i < sizeof(DOTA_COLORS)) {
            slot.color = DOTA_COLORS[i];
        } else {
            slot.color = 0;
        }

        slot.handicap       = 100;


        if (i < 5) {
            slot.team = (quint8)SlotTeam::Sentinel;
            slot.race = (quint8)SlotRace::NightElf;
        } else if (i < 10) {
            slot.team = (quint8)SlotTeam::Scourge;
            slot.race = (quint8)SlotRace::Undead;
        } else {
            slot.team = (quint8)SlotTeam::Observer;
            slot.race = (quint8)SlotRace::Observer;
        }
    }

    initBotPlayerData();

    LOG_INFO("✨ 地图槽位初始化完成 (虚拟主机模式)");
}

void Client::initSlotsFromMap(quint8 maxPlayers)
{
    // 1. 基础校验
    if (!m_war3Map.isValid()) {
        LOG_ERROR("🗺️ [地图槽位] 初始化失败: 地图对象无效");
        return;
    }

    auto players = m_war3Map.getPlayers();
    auto forces = m_war3Map.getForces();
    int mapSlotCount = players.size();

    // 2. 决定最终槽位数量
    int finalSlotCount = (maxPlayers > mapSlotCount) ? maxPlayers : mapSlotCount;

    // 3. 打印根节点信息
    LOG_INFO("🗺️ [地图槽位] 开始从 w3i 数据加载配置");
    LOG_INFO(QString("   ├─ 📂 地图定义: %1 人 | 🎯 目标配置: %2 人")
                 .arg(mapSlotCount).arg(finalSlotCount));

    // 4. 重置容器
    initSlots(finalSlotCount);

    // 5. 第一阶段：遍历解析地图定义的槽位
    for (int i = 0; i < mapSlotCount; ++i) {
        const W3iPlayer &wp = players[i];

        // --- A. 计算队伍归属 ---
        int teamId = 0;
        for (int f = 0; f < forces.size(); ++f) {
            if (forces[f].playerMasks & (1 << wp.id)) {
                teamId = f;
                break;
            }
        }

        GameSlot &slot = m_slots[i];
        QString typeLog;
        QString raceLog;

        // --- B. 设置类型 (Type) ---
        if (wp.type == 1) {
            slot.slotStatus     = Open;
            slot.computer       = Human;
            typeLog             = "Human";
        } else if (wp.type == 2) {
            slot.slotStatus     = Open;
            slot.computer       = Computer;
            slot.computerType   = Normal;
            typeLog             = "Computer";
        } else {
            slot.slotStatus     = Close;
            slot.computer       = Human;
            typeLog             = "Closed";
        }

        // --- C. 设置种族 (Race) ---
        if (wp.race == 1) { slot.race = 1; raceLog = "Human"; }
        else if (wp.race == 2) { slot.race = 2; raceLog = "Orc"; }
        else if (wp.race == 3) { slot.race = 8; raceLog = "Undead"; }
        else if (wp.race == 4) { slot.race = 4; raceLog = "NightElf"; }
        else { slot.race = 32; raceLog = "Random"; }

        slot.team = teamId;

        // 打印日志
        LOG_INFO(QString("   ├─ 🎰 Slot %1: [%2] Team %3 | Race: %4")
                     .arg(i + 1, 2).arg(typeLog, -8).arg(teamId).arg(raceLog));
    }

    // 6. 第二阶段：处理额外的裁判槽位
    if (finalSlotCount > mapSlotCount) {
        LOG_INFO(QString("   ├─ 👓 扩展裁判位: Slot %1 - %2").arg(mapSlotCount + 1).arg(finalSlotCount));

        for (int i = mapSlotCount; i < finalSlotCount; ++i) {
            GameSlot &slot = m_slots[i];

            // 裁判的标准设置
            slot.pid            = 0;
            slot.downloadStatus = DownloadStart;
            slot.slotStatus     = Open;
            slot.computer       = Human;
            slot.team           = (quint8)SlotTeam::Observer;
            slot.race           = (quint8)SlotRace::Observer;
            slot.color          = 12;
            slot.handicap       = 100;

            LOG_INFO(QString("   │  ├─ 🎰 Slot %1: [Observer] Team 12 (Ref)").arg(i + 1, 2));
        }
    }

    // 7. 结尾统计
    int humanCount = 0;
    int compCount = 0;
    int obsCount = 0;
    for(const auto &s : qAsConst(m_slots)) {
        if (s.slotStatus == Open) {
            if (s.team == 12) obsCount++;
            else if (s.computer == Human) humanCount++;
            else if (s.computer == Computer) compCount++;
        }
    }

    LOG_INFO(QString("   └─ ✅ 配置完成: 玩家 %1 | 电脑 %2 | 裁判 %3").arg(humanCount).arg(compCount).arg(obsCount));
}

QByteArray Client::serializeSlotData() {
    QByteArray data;
    QDataStream ds(&data, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds << (quint8)m_slots.size();

    for (const auto &slot : qAsConst(m_slots)) {
        ds << slot.pid;
        ds << slot.downloadStatus;
        ds << slot.slotStatus;
        ds << slot.computer;
        ds << slot.team;
        ds << slot.color;
        ds << slot.race;
        ds << slot.computerType;
        ds << slot.handicap;
    }
    return data;
}

quint8  Client::getTotalSlots() const
{
    if (m_slots.isEmpty()) return 10;
    return m_slots.size();
}

quint8  Client::getOccupiedSlots() const
{
    if (m_slots.isEmpty()) return 1;

    quint8  count = 0;
    for (const auto &slot : m_slots) {
        // 统计状态为 Occupied 的槽位
        if (slot.slotStatus == Occupied) {
            count++;
        }
    }
    return count;
}

void Client::swapSlots(int slot1, int slot2)
{
    // 1. 基础校验
    if (m_gameStarted || !isConnected()) return;

    int maxSlots = m_slots.size();

    // 2. 转换索引 (用户输入 1-12 -> 数组索引 0-11)
    int idx1 = slot1 - 1;
    int idx2 = slot2 - 1;

    // 3. 越界检查
    if (idx1 < 0 || idx1 >= maxSlots || idx2 < 0 || idx2 >= maxSlots) {
        LOG_INFO(QString("⚠️ [Swap] 索引越界: %1 <-> %2 (Max: %3)").arg(slot1).arg(slot2).arg(maxSlots));
        return;
    }

    // 4. 保护检查 (防止交换 HostBot，PID 2)
    if (m_slots[idx1].pid == m_botPid || m_slots[idx2].pid == m_botPid) {
        return;
    }

    // 5. 获取引用
    GameSlot &s1 = m_slots[idx1];
    GameSlot &s2 = m_slots[idx2];

    // [A] 交换玩家身份与状态 (PID, 下载状态, 槽位开关, 电脑设置)
    std::swap(s1.pid,            s2.pid);            // 交换 PID
    std::swap(s1.downloadStatus, s2.downloadStatus); // 交换下载进度
    std::swap(s1.slotStatus,     s2.slotStatus);     // 交换开/关/占用状态
    std::swap(s1.computer,       s2.computer);       // 交换电脑标志
    std::swap(s1.computerType,   s2.computerType);   // 交换电脑难度
    std::swap(s1.handicap,       s2.handicap);       // 交换生命值设定

    // [B] 以下属性不要交换，保留在原槽位上：
    // s1.team  vs s2.team   (队伍必须固定在槽位上)
    // s1.color vs s2.color  (颜色通常固定在槽位上)
    // s1.race  vs s2.race   (DotA中 1-5是暗夜, 6-10是不死，必须固定)

    // 6. 打印日志
    // s1.team 是 quint8，直接 arg() 会变成不可见字符或导致崩溃
    LOG_INFO(QString("🔄 [Slot] 交换完成: %1 (Team %2) <-> %3 (Team %4)")
                 .arg(slot1)
                 .arg((int)s1.team)
                 .arg(slot2)
                 .arg((int)s2.team));

    // 7. 广播更新
    broadcastSlotInfo();
}

int Client::getSlotIndexByPid(quint8 pid) const
{
    if (pid == 0) return -1;

    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

quint8 Client::findFreePid() const
{
    bool pid1_taken = m_players.contains(1);
    if (!pid1_taken) {
        for(const auto& s : m_slots) if(s.pid == 1) pid1_taken = true;
    }
    if (!pid1_taken) return 1;

    for (quint8 pid = 3; pid < 255; ++pid) {
        if (m_players.contains(pid)) continue;

        bool usedInSlot = false;
        for (const auto &slot : m_slots) {
            if (slot.pid == pid) {
                usedInSlot = true;
                break;
            }
        }
        if (usedInSlot) continue;

        return pid;
    }
    return 0;
}

quint8 Client::getPidBySocket(QTcpSocket *socket) const
{
    if (!socket) return 0;

    for (auto it = m_players.constBegin(); it != m_players.constEnd(); ++it) {
        if (it.value().socket == socket) {
            return it.key();
        }
    }

    return 0;
}

quint8 Client::getPidByPlayerName(const QString &PlayerName) const
{
    for (auto it = m_players.constBegin(); it != m_players.constEnd(); ++it) {
        if (it.value().name.compare(PlayerName, Qt::CaseInsensitive) == 0) {
            return it.key();
        }
    }
    return 0;
}

quint8 Client::getPidByClientId(const QString &clientId) const
{
    if (clientId.isEmpty()) return 0;

    for (auto it = m_players.constBegin(); it != m_players.constEnd(); ++it) {
        if (it.value().clientId == clientId) {
            return it.key();
        }
    }
    return 0;
}

QString Client::getPlayerNameBySocket(QTcpSocket *socket) const
{
    if (!socket) return QString();

    for (auto it = m_players.constBegin(); it != m_players.constEnd(); ++it) {
        if (it.value().socket == socket) {
            return it.value().name;
        }
    }
    return QString();
}

QString Client::getPlayerNameByPid(quint8 pid) const
{
    if (pid == 0) return QString("");
    auto it = m_players.constFind(pid);
    if (it != m_players.constEnd()) {
        return it.value().name;
    }
    return QString("");
}

QTextCodec* Client::getCodecBySocket(QTcpSocket *socket) const
{
    QTextCodec *defaultCodec = QTextCodec::codecForName("Windows-1252");

    if (!socket) return defaultCodec;

    for (auto it = m_players.constBegin(); it != m_players.constEnd(); ++it) {
        if (it.value().socket == socket) {
            return it.value().codec ? it.value().codec : defaultCodec;
        }
    }

    return defaultCodec;
}

QTextCodec* Client::getCodecByPid(quint8 pid) const
{
    QTextCodec *defaultCodec = QTextCodec::codecForName("Windows-1252");

    if (m_players.contains(pid)) {
        QTextCodec *pCodec = m_players.value(pid).codec;
        return pCodec ? pCodec : defaultCodec;
    }

    return defaultCodec;
}

bool Client::isSlotOccupied(int slotIndex) const {
    int idx = slotIndex - 1;
    if (idx < 0 || idx >= m_slots.size()) return false;
    return m_slots[idx].slotStatus == Occupied;
}

QString Client::getSlotInfoString() const
{
    // 格式化为 (占用/总数)
    return QString("(%1/%2)").arg(getOccupiedSlots()).arg(getTotalSlots());
}

// =========================================================
// 12. 玩家辅助函数
// =========================================================

bool Client::isHostJoined()
{
    if (m_host.isEmpty()) {
        return false;
    }

    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        const QString &existingName = it.value().name;
        if (existingName.compare(m_host, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

void Client::initBotPlayerData()
{
    PlayerData bot;
    bot.pid                 = 2;
    bot.name                = m_botDisplayName;
    bot.socket              = nullptr;
    bot.isFinishedLoading   = true;
    bot.isDownloadStart     = false;
    bot.isReady             = true;
    bot.language            = "en";
    bot.extIp               = QHostAddress("0.0.0.0");
    bot.intIp               = QHostAddress("0.0.0.0");

    m_players.insert(bot.pid, bot);

    LOG_INFO(QString("🤖 Host Bot 注册完成 (PID: 2) | 状态: 自动就绪 | 显示名: %1").arg(m_botDisplayName));
}

void Client::checkAllPlayersLoaded()
{
    if (m_gameTickTimer->isActive() || m_startLagTimer->isActive()) return;

    bool allReady = true;
    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        if (it.key() == m_botPid) continue;
        if (!it.value().isFinishedLoading) {
            allReady = false;
            break;
        }
    }

    if (allReady && m_players.size() > 1) {
        LOG_INFO("🏁 [全员就绪] 触发一次性同步广播...");

        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            quint8 pid = it.key();
            if (pid == m_botPid) continue;

            QByteArray playerLoadedPacket = createW3GSPlayerLoadedPacket(pid);
            broadcastPacket(playerLoadedPacket, 0);
        }

        m_gameStarted = true;
        LOG_INFO(QString("      └─ ⏳ 动作: 启动 StartLag 缓冲计时器 (%1 ms)...").arg(m_gameStartLag));
        m_startLagTimer->start(m_gameStartLag);
    }
}

// =========================================================
// 13. 辅助工具函数
// =========================================================

void Client::dumpPacket(const QByteArray &bytes)
{
    QString hex;
    QString ascii;
    for (int i = 0; i < bytes.size(); ++i) {
        uchar c = static_cast<uchar>(bytes.at(i));
        hex += QString("%1 ").arg(c, 2, 16, QChar('0')).toUpper();
        ascii += (c >= 32 && c <= 126) ? QChar(c) : QChar('.');
        if ((i + 1) % 16 == 0 || i == bytes.size() - 1) {
            int padding = (15 - (i % 16)) * 3;
            LOG_INFO(QString("      %1 %2 %3").arg(hex.leftJustified(hex.size() + padding), "|", ascii));
            hex.clear(); ascii.clear();
        }
    }
};

bool Client::bindToRandomPort()
{
    if (m_udpSocket->state() != QAbstractSocket::UnconnectedState) m_udpSocket->close();
    if (m_tcpServer->isListening()) m_tcpServer->close();

    // 尝试绑定函数
    auto tryBind = [&](quint16 port) -> bool {
        // 1. 绑定 UDP
        if (!m_udpSocket->bind(QHostAddress::AnyIPv4, port)) return false;

        // 2. 绑定 TCP
        if (!m_tcpServer->listen(QHostAddress::AnyIPv4, port)) {
            m_udpSocket->close(); // 回滚 UDP
            return false;
        }

        LOG_INFO(QString("✅ 双协议绑定成功: UDP & TCP 端口 %1").arg(port));
        return true;
    };

    // 随机范围
    for (int i = 0; i < 200; ++i) {
        quint16 p = QRandomGenerator::global()->bounded(6113, 7113);
        if (isBlackListedPort(p)) continue;
        if (tryBind(p)) return true;
    }
    return false;
}

quint16 Client::getListenPort() const
{
    LOG_INFO("------------------------------------------");
    LOG_INFO("[端口探针] 正在为 Client 指令检索核心通信端口...");

    // 1. 优先检索 UDP 套接字
    if (m_udpSocket && m_udpSocket->state() == QAbstractSocket::BoundState) {
        quint16 udpP = m_udpSocket->localPort();
        LOG_INFO("├─ 📡 [命中] UDP 广播套接字处于 BoundState");
        LOG_INFO(QString("│  ├─ 通信协议: War3/UDP (用于游戏握手和同步)"));
        LOG_INFO(QString("│  └─ 本地端口: %1 (核心游戏端口)").arg(udpP));
        LOG_INFO(QString("└─ ✅ [选用] 返回机器人动态 UDP 端口: %1").arg(udpP));
        return udpP;
    }

    // 2. 检索 TCP 会话套接字
    if (m_tcpSocket && m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        quint16 tcpP = m_tcpSocket->localPort();
        LOG_INFO("├─ 📞 [命中] TCP 套接字处于 ConnectedState");
        LOG_INFO(QString("│  ├─ 通信协议: War3/TCP (用于 Session 数据流)"));
        LOG_INFO(QString("│  └─ 本地映射端口: %1").arg(tcpP));
        LOG_INFO(QString("└─ ⚠️ [选用] UDP 无效，返回 TCP 本地分配端口: %1").arg(tcpP));
        return tcpP;
    }

    // 3. 检测 TCP Server 端口
    if (m_tcpServer && m_tcpServer->isListening()) {
        quint16 serverP = m_tcpServer->serverPort();
        LOG_INFO("├─ 🏢 [诊断] 监控到活跃的 TCP 管理服务端(Server)");
        LOG_INFO(QString("│  └─ 服务侦听端口: %1 (注意: 此为固定监听端口)").arg(serverP));
        LOG_INFO("│      ❗ 策略过滤: 已阻止返回该端口。魔兽客户端连接 6116 将会被防火墙拦截。");
    }

    // 4. 全部失败
    LOG_INFO("└─ ❌ [失败] 未能在 Client 中捕获任何有效的业务数据套接字");
    return 0;
}

bool Client::isBlackListedPort(quint16 port)
{
    static const QSet<quint16> blacklist = {
        22, 53, 3478, 53820, 57289, 57290, 80, 443, 8080, 8443, 3389, 5900, 3306, 5432, 6379, 27017
    };
    return blacklist.contains(port);
}

quint32 Client::getMapCRC() const
{
    if (!m_war3Map.isValid()) {
        return 0;
    }

    return m_war3Map.getMapCRC();
}

void Client::syncPingsToLauncher()
{
    QMap<quint8, quint32> pingMap;
    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        if (it.key() == m_botPid) continue;
        pingMap.insert(it.key(), it.value().currentLatency);
    }
    emit roomPingsUpdated(pingMap);
}

void Client::sendPingLoop()
{
    // 状态检查：如果游戏已开始或正在倒计时，必须停止！
    if (m_gameStarted || m_startTimer->isActive()) {
        if (m_pingTimer->isActive()) {
            m_pingTimer->stop();
            LOG_INFO("🛑 [自动修正] 检测到 Ping 循环在游戏期间运行，已强制停止");
        }
        return;
    }

    updateCountdowns();

    checkPlayerTimeout();

    if (!isConnected() || m_players.isEmpty()) {
        return;
    }

    QByteArray pingPacket = createW3GSPingFromHostPacket();

    bool shouldSendChat = false;

    MultiLangMsg waitMsg;

    m_chatIntervalCounter++;
    if (m_chatIntervalCounter >= 10) {
        int realPlayerCount = 0;
        for(auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.key() != m_botPid) realPlayerCount++;
        }

        // 填充多语言内容
        waitMsg.add("zh_CN", QString("请耐心等待，当前已有 %1 个玩家...").arg(realPlayerCount))
            .add("en", QString("Please wait, %1 players present...").arg(realPlayerCount));

        shouldSendChat = true;
        m_chatIntervalCounter = 0;
    }

    QList<quint8> pids = m_players.keys();
    for (quint8 pid : qAsConst(pids)) {
        if (!m_players.contains(pid)) continue;
        PlayerData &playerData = m_players[pid];
        QTcpSocket *socket = playerData.socket;

        if (!socket || socket->state() != QAbstractSocket::ConnectedState) continue;

        // A. 发送 Ping
        socket->write(pingPacket);

        // B. 发送聊天
        if (shouldSendChat) {
            // 获取对应语言文本
            QString text = waitMsg.get(playerData.language);
            QByteArray finalBytes = playerData.codec->fromUnicode(text);

            QByteArray chatPacket = createW3GSChatFromHostPacket(finalBytes, m_botPid, pid, ChatFlag::Message);
            socket->write(chatPacket);
        }

        socket->flush();
    }
}

void Client::updateCountdowns()
{
    bool needSync = false;
    QList<quint8> pidsToKick;

    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        PlayerData &p = it.value();

        // 机器人和房主不需要准备
        if (it.key() == m_botPid || p.isVisualHost) continue;

        if (!p.isReady) {
            if (p.readyCountdown > 0) {
                p.readyCountdown--;
                needSync = true;

                // 仅在 10秒 和 5秒 时提醒，避免刷屏
                if (p.readyCountdown == 9 || p.readyCountdown == 4) {
                    MultiLangMsg msg;
                    int seconds = p.readyCountdown + 1; // 补偿显示的秒数

                    msg.add("zh_CN", QString("玩家 [%1] 请在 %2 秒内输入 /ready 准备，否则将被踢出。")
                                         .arg(p.name).arg(seconds))
                        .add("en", QString("Player [%1], please type /ready within %2s or you will be kicked.")
                                       .arg(p.name).arg(seconds));

                    broadcastChatMessage(msg);

                    if (p.readyCountdown == 9) {
                        MultiLangMsg helpMsg;
                        helpMsg.add("zh_CN", "指令：输入 /ready 准备，/unready 取消。")
                            .add("en", "Commands: Type /ready to ready up, /unready to cancel.");
                        broadcastChatMessage(helpMsg);
                    }
                }
            } else {
                pidsToKick.append(it.key());
            }
        }
    }

    if (needSync) {
        syncPlayerReadyStates();
    }

    // 踢出逻辑
    for (quint8 pid : pidsToKick) {
        if (m_players.contains(pid)) {
            PlayerData &p = m_players[pid];

            MultiLangMsg kickMsg;
            kickMsg.add("zh_CN", QString("玩家 [%1] 因未准备被移除。").arg(p.name))
                .add("en", QString("Player [%1] was kicked for not being ready.").arg(p.name));
            broadcastChatMessage(kickMsg);

            if (p.socket) {
                p.socket->disconnectFromHost();
            }
        }
    }
}

void Client::syncPlayerReadyStates()
{
    QVariantMap syncMap;

    // 1. 预筛选需要同步的玩家
    QList<PlayerData> playersToSync;
    for (auto it = m_players.constBegin(); it != m_players.constEnd(); ++it) {
        if (it.key() != m_botPid) {
            playersToSync.append(it.value());
        }
    }

    LOG_INFO(QString("📡 [状态同步] 正在构建准备状态表 (玩家数: %1)...").arg(playersToSync.size()));

    int count = playersToSync.size();
    for (int i = 0; i < count; ++i) {
        const PlayerData &p = playersToSync[i];
        bool isLast = (i == count - 1);
        QString prefix = isLast ? "   └── " : "   ├── ";
        QString indent = isLast ? "       " : "   │   ";

        QVariantMap pInfo;
        pInfo["r"] = p.isReady;
        pInfo["c"] = p.readyCountdown;
        pInfo["pid"] = p.pid;
        pInfo["name"] = p.name;

        QString statusDesc = p.isReady ? "✅ 已就绪" : QString("⏳ 未准备 (%1s)").arg(p.readyCountdown);
        LOG_INFO(QString("%1[PID: %2] 状态: %3").arg(prefix).arg(p.pid, -2).arg(statusDesc));

        if (p.name.isEmpty()) {
            LOG_WARNING(QString("%1⚠️  [警告] 玩家 PID %2 的名字为空！魔兽客户端无法实时更新准备状态。")
                            .arg(indent).arg(p.pid));
        } else {
            LOG_INFO(QString("%1👤 玩家名: %2").arg(indent, p.name));
        }

        syncMap.insert(QString::number(p.pid), pInfo);

        if (!p.name.isEmpty()) {
            syncMap.insert(p.name, pInfo);
        }
    }

    emit readyStateChanged(syncMap);

    LOG_INFO(QString("   ✅ 同步完成。构建键值对: %1 (双索引模式)").arg(syncMap.size()));
}

void Client::setPlayerReadyStates(const QString &clientId, const QString &name, bool ready)
{
    bool found = false;
    QString matchType = "None";
    quint8 matchedPid = 0;

    // 1. 入口日志：记录收到的原始指令参数
    LOG_INFO(QString("⚙️ [准备状态变更] 收到指令请求:"));
    LOG_INFO(QString("   ├─ 👤 申报名字: %1").arg(name.isEmpty() ? "(空)" : name));
    LOG_INFO(QString("   ├─ 🆔 申报 ClientId: %1").arg(clientId.isEmpty() ? "(空)" : clientId));
    LOG_INFO(QString("   └─ 🚩 目标动作: %1").arg(ready ? "✅ READY" : "⏳ UNREADY"));

    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        PlayerData &p = it.value();

        // 执行判定
        bool clientIdMatch = (!clientId.isEmpty() && p.clientId == clientId);
        bool nameMatch = (!name.isEmpty() && p.name.compare(name, Qt::CaseInsensitive) == 0);

        if (clientIdMatch || nameMatch) {
            found = true;
            matchedPid = it.key();
            matchType = clientIdMatch ? "ClientId 匹配成功" : "名字匹配成功";

            // 2. 命中日志
            LOG_INFO(QString("   ├── ✅ 命中玩家: %1 (PID: %2)").arg(p.name).arg(matchedPid));
            LOG_INFO(QString("   │   ├─ 🔍 匹配结果: %1").arg(matchType));

            // 更新状态
            p.isReady = ready;
            if (!ready) {
                p.readyCountdown = 10;
            }

            break;
        }
    }

    // 4. 结果分支处理
    if (found) {
        LOG_INFO(QString("   └── 🚀 处理完成: 正在触发全房数据同步广播"));
        syncPlayerReadyStates();
    } else {
        // 5. 失败详细分析日志
        LOG_ERROR(QString("   └── ❌ [匹配失败] 房间内找不到该玩家！当前房间存量分析:"));

        if (m_players.isEmpty()) {
            LOG_INFO("       └─ ⚠️ 警告: 当前房间玩家列表竟然是空的 (m_players empty)");
        } else {
            int count = 0;
            for (auto it = m_players.constBegin(); it != m_players.constEnd(); ++it) {
                count++;
                bool isLast = (count == m_players.size());
                QString prefix = isLast ? "       └─ " : "       ├─ ";

                LOG_INFO(QString("%1PID:%2 | Name:\"%3\" | ClientId:\"%4\"")
                             .arg(prefix)
                             .arg(it.key())
                             .arg(it.value().name, it.value().clientId.isEmpty() ? "MISSING" : it.value().clientId));
            }
        }
    }
}

bool Client::hasPlayerByClientId(const QString &clientId) const {
    for (const auto &p : m_players) {
        if (p.clientId == clientId) return true;
    }
    return false;
}

bool Client::hasPlayerByUserName(const QString &userName) const
{
    if (userName.isEmpty()) return false;

    for (auto it = m_players.constBegin(); it != m_players.constEnd(); ++it) {
        if (it.value().name.compare(userName, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

void Client::checkPlayerTimeout()
{
    if (m_startTimer->isActive() || m_gameStarted) {
        LOG_DEBUG("🛡️ [超时监控] 游戏启动/进行中 -> 跳过检测 (安全)");
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    const double MIN_DOWNLOAD_SPEED     = 20.0;
    const qint64 GRACE_PERIOD           = 10000;
    const qint64 TIMEOUT_DOWNLOADING    = 60000; // 60秒
    const qint64 TIMEOUT_LOBBY_IDLE     = 10000; // 10秒

    QList<quint8> pidsToKick;

    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        quint8 pid = it.key();
        PlayerData &playerData = it.value();

        if (pid == m_botPid || playerData.isVisualHost) continue;

        bool kick = false;
        QString reasonCategory = "";

        qint64 timeSinceLastResponse = now - playerData.lastResponseTime;
        qint64 timeSinceLastDownload = now - playerData.lastDownloadTime;

        if (playerData.isDownloadStart) {
            if (timeSinceLastDownload > TIMEOUT_DOWNLOADING) {
                kick = true;
                reasonCategory = QString("下载卡死 (%1ms)").arg(timeSinceLastDownload);
            }

            qint64 downloadDuration = now - playerData.downloadStartTime;

            // 只有下载超过 10 秒后，才开始检查速度
            if (downloadDuration > GRACE_PERIOD) {
                if (playerData.currentSpeedKBps < MIN_DOWNLOAD_SPEED) {
                    LOG_INFO(QString("👢 [速度淘汰] 踢出玩家 %1 (PID: %2) - 速度太慢: %3 KB/s")
                                 .arg(playerData.name).arg(it.key())
                                 .arg(playerData.currentSpeedKBps, 0, 'f', 1));
                    pidsToKick.append(it.key());
                }
            }
        } else {
            if (timeSinceLastResponse > TIMEOUT_LOBBY_IDLE) {
                kick = true;
                reasonCategory = QString("房间无响应 (%1ms)").arg(timeSinceLastResponse);
            }
        }

        if (kick) {
            LOG_INFO(QString("👢 [超时裁判] 标记移除: %1 (PID: %2) - 原因: %3")
                         .arg(playerData.name).arg(pid).arg(reasonCategory));
            pidsToKick.append(pid);
        }
    }

    for (quint8 pid : pidsToKick) {
        if (m_players.contains(pid)) {
            PlayerData &p = m_players[pid];
            if (p.socket && p.pid != m_botPid) {
                LOG_INFO(QString("🔌 [执行踢出] 断开 PID %1 的连接").arg(pid));
                p.socket->disconnectFromHost();
            }
        }
    }
}

void Client::writeIpToStreamWithLog(QDataStream &out, const QHostAddress &ip)
{
    // 1. 获取主机序的整数
    quint32 ipVal = ip.toIPv4Address();

    // 2. 转换为网络大端序
    quint32 networkOrderIp = qToBigEndian(ipVal);

    // 3. 使用 writeRawData 直接写入内存数据
    out.writeRawData(reinterpret_cast<const char*>(&networkOrderIp), 4);

    const quint8 *bytes = reinterpret_cast<const quint8*>(&networkOrderIp);
    LOG_INFO(QString("🔧 IP (HEX): %1 %2 %3 %4")
                 .arg(bytes[0], 2, 16, QChar('0'))
                 .arg(bytes[1], 2, 16, QChar('0'))
                 .arg(bytes[2], 2, 16, QChar('0'))
                 .arg(bytes[3], 2, 16, QChar('0')).toUpper());
}

QString Client::getPrimaryIPv4() {
    foreach(const QNetworkInterface &interface, QNetworkInterface::allInterfaces()) {
        if (interface.flags() & QNetworkInterface::IsUp && interface.flags() & QNetworkInterface::IsRunning && !(interface.flags() & QNetworkInterface::IsLoopBack)) {
            foreach(const QNetworkAddressEntry &entry, interface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.ip().toString().startsWith("169.254.") && !entry.ip().toString().startsWith("127."))
                    return entry.ip().toString();
            }
        }
    }
    return QString();
}

QString Client::getBnetPacketName(BNETPacketID id)
{
    switch (id) {
    case SID_NULL:                   return "SID_NULL (空包)";
    case SID_STOPADV:                return "SID_STOPADV (停止广播)";
    case SID_ENTERCHAT:              return "SID_ENTERCHAT (进入聊天)";
    case SID_GETCHANNELLIST:         return "SID_GETCHANNELLIST (获取频道)";
    case SID_JOINCHANNEL:            return "SID_JOINCHANNEL (加入频道)";
    case SID_CHATCOMMAND:            return "SID_CHATCOMMAND (聊天命令)";
    case SID_CHATEVENT:              return "SID_CHATEVENT (聊天事件)";
    case SID_STARTADVEX3:            return "SID_STARTADVEX3 (创建房间)";
    case SID_PING:                   return "SID_PING (心跳)";
    case SID_LOGONRESPONSE:          return "SID_LOGONRESPONSE (登录响应-旧)";
    case SID_LOGONRESPONSE2:         return "SID_LOGONRESPONSE2 (登录响应-中)";
    case SID_NETGAMEPORT:            return "SID_NETGAMEPORT (游戏端口)";
    case SID_AUTH_INFO:              return "SID_AUTH_INFO (认证信息)";
    case SID_AUTH_CHECK:             return "SID_AUTH_CHECK (版本检查)";
    case SID_AUTH_ACCOUNTCREATE:     return "SID_AUTH_ACCOUNTCREATE (账号创建)";
    case SID_AUTH_ACCOUNTLOGON:      return "SID_AUTH_ACCOUNTLOGON (SRP登录请求)";
    case SID_AUTH_ACCOUNTLOGONPROOF: return "SID_AUTH_ACCOUNTLOGONPROOF (SRP登录验证)";
    default:                         return QString("UNKNOWN (0x%1)").arg(QString::number((int)id, 16).toUpper());
    }
}

QString Client::getCodecNameByLanguage(const QString &lang)
{
    // 简体中文
    if (lang == "zh_CN") return "GBK";

    // 繁体中文
    if (lang == "zh_TW") return "Big5";

    // 俄语、乌克兰语、保加利亚语 (西里尔字母)
    if (lang == "ru" || lang == "uk" || lang == "bg") return "Windows-1251";

    // 日语
    if (lang == "ja") return "Shift-JIS";

    // 韩语
    if (lang == "ko") return "CP949";

    // 中欧语言 (波兰、捷克、匈牙利、斯洛伐克)
    if (lang == "pl" || lang == "cs" || lang == "hu" || lang == "sk") return "Windows-1250";

    // 土耳其语
    if (lang == "tr") return "Windows-1254";

    // 阿拉伯语
    if (lang == "ar") return "Windows-1256";

    // 希伯来语
    if (lang == "he") return "Windows-1255";

    // 波罗的海语言 (拉脱维亚)
    if (lang == "lv") return "Windows-1257";

    // 西欧语言 (英语、西班牙、德语、法语、意大利、葡萄牙、丹麦、芬兰等)
    // 这是魔兽默认的 Latin-1 编码
    return "Windows-1252";
}

QString Client::getColoredTextByState(const PlayerData &p, const QString &text, bool isPlayerName)
{
    // 1. 如果是处理玩家名字，且长度超过15，则完全不加颜色代码
    if (isPlayerName && text.length() >= 15) {
        return text;
    }

    // 2. 根据玩家状态判定颜色代码
    // 房主或已准备：绿色 (|cff00ff00)，未准备：红色 (|cffff0000)
    QString colorCode = (p.isVisualHost || p.isReady) ? "|cff00ff00" : "|cffff0000";

    // 3. 返回着色文本
    return QString("%1%2|r").arg(colorCode, text);
}

QString Client::getColoredTextByState(quint8 pid, const QString &text, bool isPlayerName)
{
    if (m_players.contains(pid)) {
        return getColoredTextByState(m_players[pid], text, isPlayerName);
    }
    // 如果找不到玩家，默认不着色
    return text;
}

QString Client::stripColorCodes(const QString &text)
{
    if (text.isEmpty()) return text;

    // 正则表达式说明：
    // \\|c[0-9a-fA-F]{8} : 匹配 |c 及其后 8 位十六进制颜色码 (例如 |cff00ff00)
    // |                  : 或者
    // \\|r               : 匹配结束符 |r
    static QRegularExpression colorRegex("\\|c[0-9a-fA-F]{8}|\\|r", QRegularExpression::CaseInsensitiveOption);

    return QString(text).remove(colorRegex);
}

QString Client::translateSocketError(QAbstractSocket::SocketError err, const QString &errString) {
    switch (err) {
    case QAbstractSocket::RemoteHostClosedError:
        return "玩家客户端主动关闭了连接 (Normal Exit or Alt+F4)";
    case QAbstractSocket::NetworkError:
        return "底层网络错误";
    case QAbstractSocket::SocketTimeoutError:
        return "连接超时";
    case QAbstractSocket::ConnectionRefusedError:
        return "连接被拒绝";
    case QAbstractSocket::SocketResourceError:
        return "本地资源不足";
    case QAbstractSocket::SocketAccessError:
        return "访问权限不足";
    case QAbstractSocket::DatagramTooLargeError:
        return "数据包过大";
    case QAbstractSocket::AddressInUseError:
        return "地址已被占用";
    case QAbstractSocket::HostNotFoundError:
        return "未找到主机";
    case QAbstractSocket::UnsupportedSocketOperationError:
        return "不支持的操作";
    case QAbstractSocket::ProxyAuthenticationRequiredError:
        return "代理需要认证";
    case QAbstractSocket::SslHandshakeFailedError:
        return "SSL握手失败";
    case QAbstractSocket::UnfinishedSocketOperationError:
        return "操作未完成";
    case QAbstractSocket::ProxyConnectionRefusedError:
        return "代理拒绝连接";
    case QAbstractSocket::ProxyConnectionClosedError:
        return "代理连接关闭";
    case QAbstractSocket::ProxyConnectionTimeoutError:
        return "代理连接超时";
    case QAbstractSocket::ProxyNotFoundError:
        return "未找到代理";
    case QAbstractSocket::ProxyProtocolError:
        return "代理协议错误";
    case QAbstractSocket::OperationError:
        return "操作错误 (可能是 Socket 已被强制销毁)";
    case QAbstractSocket::SslInternalError:
        return "SSL内部错误";
    case QAbstractSocket::SslInvalidUserDataError:
        return "SSL无效用户数据";
    case QAbstractSocket::TemporaryError:
        return "临时性错误";
    case QAbstractSocket::UnknownSocketError:
    default:
        if (errString.contains("Unknown error", Qt::CaseInsensitive) || errString.isEmpty()) {
            return "正常断开 / 服务器主动断开 (No active error)";
        }
        return QString("其他错误 (Code: %1): %2").arg(err).arg(errString);
    }
}

quint32 Client::ipToUint32(const QHostAddress &address) { return address.toIPv4Address(); }
quint32 Client::ipToUint32(const QString &ipAddress) { return QHostAddress(ipAddress).toIPv4Address(); }

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

void Client::disconnectFromHost() {
    // 主动断开通常不需要太多日志，除非为了调试
    m_tcpSocket->disconnectFromHost();
}

bool Client::isConnected() const {
    return m_tcpSocket->state() == QAbstractSocket::ConnectedState;
}

void Client::onDisconnected() {
    // 树状日志
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

        // 树状日志
        LOG_INFO("🎮 [玩家连接] 检测到新 TCP 请求");
        LOG_INFO(QString("   └─ 🌍 来源: %1:%2")
                     .arg(socket->peerAddress().toString())
                     .arg(socket->peerPort()));

        m_playerSockets.append(socket);
        m_playerBuffers.insert(socket, QByteArray()); // 初始化缓冲区

        // 使用成员函数而非 Lambda
        connect(socket, &QTcpSocket::readyRead, this, &Client::onPlayerReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &Client::onPlayerDisconnected);
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
    playerData.isDownloadStart          = true;

    playerData.currentDownloadOffset    = 0;
    playerData.lastDownloadOffset       = 0;

    // --- 步骤 D: 立即发送第一波数据 ---
    sendNextMapPart(pid);

    LOG_INFO(QString("   └─ 📤 初始序列完成，首块数据已发送"));
}

void Client::sendNextMapPart(quint8 toPid, quint8 fromPid)
{
    // 1. 基础校验
    if (!m_players.contains(toPid)) return;
    PlayerData &playerData = m_players[toPid];

    // 更新活跃时间
    playerData.lastDownloadTime = QDateTime::currentMSecsSinceEpoch();

    if (!playerData.isDownloadStart) return;

    if (m_mapSize == 0) return;

    while (playerData.socket->bytesToWrite() < 64 * 1024)
    {
        // 计算分片大小
        int chunkSize = MAX_CHUNK_SIZE; // 1442
        if (playerData.currentDownloadOffset + chunkSize > m_mapSize) {
            chunkSize = m_mapSize - playerData.currentDownloadOffset;
        }

        // 发送数据
        QByteArray chunk = m_mapData.mid(playerData.currentDownloadOffset, chunkSize);
        QByteArray packet = createW3GSMapPartPacket(toPid, fromPid, playerData.currentDownloadOffset, chunk);

        qint64 written = playerData.socket->write(packet);

        if (written > 0) {
            playerData.currentDownloadOffset += chunkSize;
        } else {
            LOG_ERROR(QString("❌ [分块传输] Socket 写入失败: %1").arg(playerData.socket->errorString()));
            playerData.isDownloadStart = false;
            return;
        }
    }

    playerData.socket->flush();
}

void Client::onTcpReadyRead()
{
    while (m_tcpSocket->bytesAvailable() > 0) {
        if (m_tcpSocket->bytesAvailable() < 4) return;

        QByteArray headerData = m_tcpSocket->peek(4);
        if ((quint8)headerData[0] != BNET_HEADER) {
            m_tcpSocket->read(1);
            continue;
        }

        quint16 length;
        QDataStream lenStream(headerData.mid(2, 2));
        lenStream.setByteOrder(QDataStream::LittleEndian);
        lenStream >> length;

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
        LOG_INFO("   └─ ✅ 状态: 已进入聊天环境 (Unique Name Assigned)");
        queryChannelList();
        break;

    case SID_GETCHANNELLIST: // 0x0B
    {
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

    case SID_CHATEVENT:
    {
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

    case SID_LOGONRESPONSE: // 0x29
    case SID_LOGONRESPONSE2: // 0x3A
    {
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
            enterChat();
        } else {
            LOG_ERROR(QString("   └─ ❌ 结果: 失败 (Code: 0x%1)").arg(QString::number(result, 16)));
        }
    }
    break;

    case SID_AUTH_INFO:
    case SID_AUTH_CHECK:
        // 这个函数内部应该也有类似的日志优化
        handleAuthCheck(data);
        break;

    case SID_AUTH_ACCOUNTCREATE:
    {
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
        if (m_loginProtocol == Protocol_SRP_0x53) handleSRPLoginResponse(data);
        break;

    case SID_AUTH_ACCOUNTLOGONPROOF:
    {
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

    case SID_STARTADVEX3:
    {
        if (data.size() < 4) return;
        quint32 status;
        QDataStream ds(data);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds >> status;

        if (status == GameCreate_Ok) {
            LOG_INFO("   ├─ ✅ 结果: 房间创建成功");
            LOG_INFO("   └─ 📢 状态: 广播已启动");
            emit gameCreateSuccess(From_Client);
        } else {
            QString errStr;
            switch (status) {
            case GameCreate_NameExists:      errStr = "房间名已存在"; break;
            case GameCreate_TypeUnavailable: errStr = "游戏类型不可用"; break;
            case GameCreate_Error:           errStr = "通用创建错误"; break;
            default:                         errStr = QString("Code 0x%1").arg(QString::number(status, 16)); break;
            }
            LOG_ERROR(QString("   ├─ ❌ 结果: 创建失败"));
            LOG_INFO(QString("   └─ 📝 原因: %1").arg(errStr));

            // 触发失败信号，BotManager 会处理并通知客户端
            emit gameCreateFail();
        }
    }
    break;

    default: break;
    }
}

void Client::onPlayerReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    // 1. 确保缓冲区存在
    if (!m_playerBuffers.contains(socket)) {
        m_playerBuffers.insert(socket, QByteArray());
    }

    // 2. 追加新数据
    m_playerBuffers[socket].append(socket->readAll());

    // 3. 循环处理包
    while (true) {
        // 每次循环开始前，确认该 socket 的缓冲区还存在
        if (!m_playerBuffers.contains(socket)) {
            return;
        }

        QByteArray &buffer = m_playerBuffers[socket];

        if (buffer.size() < 4) {
            break; // 数据不足，等待下一次 readyRead
        }

        quint8 header = (quint8)buffer[0];
        if (header != 0xF7) {
            LOG_ERROR("❌ 非法协议头，断开连接");
            return; // 立即返回，防止后续访问
        }

        // 解析长度 (Little Endian)
        quint16 length = (quint8)buffer[2] | ((quint8)buffer[3] << 8);

        // 数据不足一个完整包，停止处理
        if (buffer.size() < length) {
            break;
        }

        // 提取完整包
        QByteArray packet = buffer.mid(0, length);

        // 先从缓冲区移除数据
        buffer.remove(0, length);

        // 准备函数参数
        quint8 msgId = (quint8)packet[1];
        QByteArray payload = packet.mid(4);

        // 调用处理函数
        handleW3GSPacket(socket, msgId, payload);

        if (!m_playerBuffers.contains(socket)) {
            return; // 玩家已断开，立即停止处理
        }
    }
}

void Client::handleW3GSPacket(QTcpSocket *socket, quint8 id, const QByteArray &payload)
{
    // 忽略高频包的入口日志，避免刷屏
    if (id != 0x42 && id != 0x44 && id != 0x46) {
        LOG_INFO(QString("📥 [W3GS] 收到数据包: 0x%1").arg(QString::number(id, 16).toUpper()));
    }

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
            QByteArray nameBytes;
            char c;
            while (!in.atEnd()) {
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

        // 打印解析详情
        LOG_INFO(QString("   ├─ 👤 玩家名: %1").arg(clientPlayerName));
        LOG_INFO(QString("   ├─ 🌍 内网IP: %1:%2").arg(iAddr.toString()).arg(clientInternalPort));
        LOG_INFO(QString("   ├─ 🔧 监听端口: %1").arg(clientListenPort));

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
        playerData.language = "EN";
        playerData.codec = QTextCodec::codecForName("Windows-1252");
        playerData.lastResponseTime = now;
        playerData.lastDownloadTime = now;
        playerData.isVisualHost = nameMatch;

        m_players.insert(newPid, playerData);

        LOG_INFO(QString("   ├─ 💾 玩家注册: PID %1 (Slot %2)").arg(newPid).arg(slotIndex));

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
        // 1. 查找发送者 PID
        quint8 currentPid = 0;
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().socket == socket) {
                currentPid = it.key();
                break;
            }
        }

        if (currentPid == 0) return;

        // 2. 标记该玩家加载完成 (状态: 5 -> 6)
        m_players[currentPid].isFinishedLoading = true;
        m_players[currentPid].lastResponseTime = QDateTime::currentMSecsSinceEpoch();
        LOG_INFO(QString("⏳ [加载进度] 玩家 %1 (PID: %2) 加载完成").arg(m_players[currentPid].name).arg(currentPid));

        // 3. 构造该玩家的加载完成包 (0x08)
        QByteArray selfLoadedPacket = createW3GSPlayerLoadedPacket(currentPid);

        // 4. 同步状态
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            quint8 targetPid = it.key();
            PlayerData &targetPlayer = it.value();

            // --- 机器人跳过 ---
            if (targetPid == m_botPid) continue;

            // A. 向所有人广播当前玩家已经加载好的消息
            if (targetPlayer.socket && targetPlayer.socket->state() == QAbstractSocket::ConnectedState) {
                targetPlayer.socket->write(selfLoadedPacket);
            }

            // B. 向当前玩家同步其他玩家信息
            if (targetPid != currentPid && targetPlayer.isFinishedLoading) {
                socket->write(createW3GSPlayerLoadedPacket(targetPid));
            }
        }

        // 5. 检查是否全员就绪，准备触发 0x0B (COUNTDOWN_END)
        checkAllPlayersLoaded();
    }
    break;

    case W3GS_OUTGOING_ACTION: // [0x26] 客户端发送的游戏内操作
    {
        if (payload.size() < 4) {
            LOG_ERROR(QString("❌ [W3GS] 动作包长度不足: %1").arg(payload.size()));
            return;
        }

        // 1. 查找发送者
        quint8 currentPid = 0;
        QString playerName = "";

        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().socket == socket) {
                currentPid = it.key();
                playerName = it.value().name;
                break;
            }
        }

        if (currentPid == 0) return;

        // 2. 提取数据
        QByteArray crcData = payload.left(4);
        QDataStream crcStream(crcData);
        quint32 crcValue;

        crcStream.setByteOrder(QDataStream::LittleEndian);
        crcStream >> crcValue;

        QByteArray actionData = payload.mid(4);

        // 3. 逻辑处理
        if (!actionData.isEmpty()) {
            m_actionQueue.append({currentPid, actionData});
            m_players[currentPid].lastResponseTime = QDateTime::currentMSecsSinceEpoch();

            // 4. 日志记录
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
        // 1. 长度校验
        if (payload.size() < 5) {
            LOG_INFO(QString("   └─ ⚠️ [警告] KeepAlive 包长度异常: %1 (期望 >= 5)").arg(payload.size()));
            return;
        }

        // 2. 解析数据
        QDataStream in(payload);
        in.setByteOrder(QDataStream::LittleEndian);

        quint8 unknownByte;
        quint32 checkSum;
        in >> unknownByte >> checkSum;

        // 3. 查找发送者
        quint8 currentPid = 0;
        QString senderName = "";

        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().socket == socket) {
                currentPid = it.key();
                senderName = it.value().name;
                break;
            }
        }

        if (currentPid != 0) {
            // 4. 逻辑处理
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            m_players[currentPid].lastResponseTime = now;

            // 5. 日志记录
            LOG_INFO(QString("💓 [保持连接] 收到心跳包 (0x27)"));
            LOG_INFO(QString("   ├─ 👤 来源: %1 (PID: %2)").arg(senderName).arg(currentPid));
            LOG_INFO(QString("   ├─ ❓ 标志: 0x%1").arg(QString::number(unknownByte, 16).toUpper().rightJustified(2, '0')));
            LOG_INFO(QString("   ├─ 🛡️ 校验: 0x%1 (CheckSum)").arg(QString::number(checkSum, 16).toUpper().rightJustified(8, '0')));
            LOG_INFO(QString("   └─ ⏱️ 动作: 刷新活跃时间戳 -> %1").arg(now));
        }
        else {
            LOG_INFO("   └─ ⚠️ [异常] 收到来自未知 Socket 的 KeepAlive");
        }
    }
    break;

    case W3GS_CHAT_TO_HOST: // [0x28] 客户端发送聊天/大厅指令
    {
        // 1. 基础长度校验 (Count(1) + From(1) + Flag(1) = 3)
        if (payload.size() < 3) return;

        QDataStream in(payload);
        in.setByteOrder(QDataStream::LittleEndian);

        // 2. 解析接收者列表
        quint8 numReceivers;
        in >> numReceivers;

        // 再次校验长度：确保 payload 包含所有接收者PID + FromPID + Flag
        if (payload.size() < 1 + numReceivers + 2) {
            LOG_ERROR(QString("   └─ ❌ [错误] 包长度不足 (Receivers: %1)").arg(numReceivers));
            return;
        }

        // 跳过接收者 PIDs
        in.skipRawData(numReceivers);

        // 3. 解析来源与标志
        quint8 fromPid, flag;
        in >> fromPid >> flag;

        // 4. 查找发送者 (Socket -> PID/Name/Codec)
        quint8 senderPid = 0;
        QString senderName = "";
        QTextCodec* codec = QTextCodec::codecForName("Windows-1252"); // 默认

        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().socket == socket) {
                senderPid = it.key();
                senderName = it.value().name;
                codec = it.value().codec;
                break;
            }
        }

        if (senderPid == 0) {
            LOG_INFO("   └─ ⚠️ [警告] 无法识别发送者 Socket，忽略请求");
            return;
        }

        // 打印通用头部日志
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

        LOG_INFO(QString("   ├─ 👤 发送者: %1 (PID: %2)").arg(senderName).arg(senderPid));
        LOG_INFO(QString("   ├─ 🚩 类型: %1").arg(typeStr));

        // 5. 根据 Flag 分流处理
        int currentOffset = 1 + numReceivers + 2; // 当前解析到的字节位置

        if (flag == 0x10) // [16] 聊天消息
        {
            if (currentOffset < payload.size()) {
                QByteArray rawMsg = payload.mid(currentOffset);
                // 移除末尾的 \0
                if (rawMsg.endsWith('\0')) rawMsg.chop(1);

                QString msg = codec->toUnicode(rawMsg);
                LOG_INFO(QString("   └─ 💬 内容: %1").arg(msg));

                // A. 指令处理
                if (msg.startsWith("/")) {
                    LOG_INFO(QString("      └─ ⚡ [指令] 检测到命令，转交处理器..."));
                    // handleChatCommand(senderPid, msg);
                }
                // B. 普通聊天转发
                else {
                    MultiLangMsg chatMsg;
                    chatMsg.add("CN", QString("%1: %2").arg(senderName, msg));
                    chatMsg.add("EN", QString("%1: %2").arg(senderName, msg));
                    broadcastChatMessage(chatMsg, senderPid);
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
        // 1. 查找玩家
        quint8 currentPid = 0;
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().socket == socket) { currentPid = it.key(); break; }
        }
        if (currentPid == 0) return;

        PlayerData &playerData = m_players[currentPid];

        LOG_INFO(QString("📥 [W3GS] 收到请求: 0x3F (StartDownload)"));
        LOG_INFO(QString("   └─ 👤 玩家: %1 (PID: %2)").arg(playerData.name).arg(currentPid));

        // 2. 防重复检查
        if (playerData.isDownloadStart) {
            LOG_INFO("   └─ ⚠️ 忽略: 已经在下载进程中");
            return;
        }

        // 3. 查找槽位并触发下载
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

        // 1. 查找玩家
        quint8 currentPid = 0;
        QString playerName = "";
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().socket == socket) {
                currentPid = it.key();
                playerName = it.value().name;
                break;
            }
        }
        if (currentPid == 0) return;

        quint32 hostMapSize = m_war3Map.getMapSize();
        PlayerData &playerData = m_players[currentPid];
        playerData.lastResponseTime = QDateTime::currentMSecsSinceEpoch();

        if (sizeFlag != 1 && sizeFlag != 3) {
            LOG_INFO(QString("⚠️ [W3GS] 收到罕见 Flag: %1 (Size: %2)").arg(sizeFlag).arg(clientMapSize));
        }

        // 状态判断
        bool isMapMatched = (clientMapSize == hostMapSize && sizeFlag == 1);
        bool isDownloadFinished = (clientMapSize == hostMapSize);

        // 2. 核心逻辑
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

        quint8 currentPid = 0;
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().socket == socket) { currentPid = it.key(); break; }
        }
        if (currentPid == 0) return;
        if (m_mapSize == 0) return;

        if (m_players.contains(currentPid)) {
            PlayerData &playerData = m_players[currentPid];
            playerData.lastResponseTime = QDateTime::currentMSecsSinceEpoch();
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

            // 发送下一块
            sendNextMapPart(currentPid);
        }
    }
    break;

    case W3GS_MAPPARTNOTOK: // [0x45] 客户端报告失败
    {
        // 1. 尝试解析 Unknown 字段 (通常是 4 字节)
        quint32 unknownValue = 0;
        QString rawHex = payload.toHex().toUpper();

        if (payload.size() >= 4) {
            QDataStream in(payload);
            in.setByteOrder(QDataStream::LittleEndian);
            in >> unknownValue;
        }

        // 2. 查找玩家 PID
        quint8 currentPid = 0;
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().socket == socket) { currentPid = it.key(); break; }
        }

        // 3. 打印详细日志
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

        quint8 currentPid = 0;
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().socket == socket) { currentPid = it.key(); break; }
        }

        if (currentPid != 0) {
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            PlayerData &p = m_players[currentPid];
            p.currentLatency = (quint32)(now - sentTick);
            p.lastResponseTime = now;

            LOG_DEBUG(QString("💓 Pong [PID:%1]: %2 ms").arg(currentPid).arg(p.currentLatency));
        }
    }
    break;

    default:
        LOG_INFO(QString("   └─ ❓ [未知包] 忽略处理"));
        break;
    }
}

void Client::onPlayerDisconnected() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    quint8 pidToRemove = 0;
    QString nameToRemove = "";
    bool wasVisualHost = false;

    // 1. 查找玩家并移除 Map 记录
    auto it = m_players.begin();
    while (it != m_players.end()) {
        if (it.value().socket == socket) {
            pidToRemove = it.key();
            nameToRemove = it.value().name;
            wasVisualHost = it.value().isVisualHost;

            it = m_players.erase(it);
            break;
        } else {
            ++it;
        }
    }

    m_playerSockets.removeAll(socket);
    m_playerBuffers.remove(socket);
    socket->deleteLater();

    if (pidToRemove != 0) {
        LOG_INFO(QString("🔌 [断开连接] 玩家离线: %1 (PID: %2)").arg(nameToRemove).arg(pidToRemove));

        // 记录被清理的槽位索引
        int oldHostSlotIndex = -1;

        // 2. 释放槽位逻辑
        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].pid == pidToRemove) {
                if (wasVisualHost) {
                    oldHostSlotIndex = i;
                }

                m_slots[i].pid = 0;
                m_slots[i].slotStatus = Open;
                m_slots[i].downloadStatus = NotStarted;
                break;
            }
        }

        bool humanRemains = false;
        for (const auto &p : qAsConst(m_players)) {
            if (p.pid != m_botPid) {
                humanRemains = true;
                break;
            }
        }

        if (!humanRemains) {
            LOG_INFO("🛑 [游戏终止] 所有真实玩家已离开，停止游戏循环");
            // 1. 停止时钟
            if (m_gameTickTimer->isActive()) {
                m_gameTickTimer->stop();
            }
            m_gameStarted = false;

            // 2. 重置游戏
            cancelGame();

            // 3. 直接返回
            return;
        }

        LOG_INFO("   ├─ 🧹 资源清理: Socket 移除 & 槽位重置");

        // 3. 房主离开处理逻辑
        if (wasVisualHost) {
            if(!m_gameStarted) {
                LOG_INFO("   ├─ 👑 [房主交接] 检测到房主离开...");

                // A. 寻找继承人 (排除 PID 2 的机器人)
                quint8 heirPid = 0;
                QString heirName = "";

                for (auto pIt = m_players.begin(); pIt != m_players.end(); ++pIt) {
                    if (pIt.key() != m_botPid) {
                        heirPid = pIt.key();
                        heirName = pIt.value().name;
                        break;
                    }
                }

                // B. 判断结果
                if (heirPid == 0) {
                    // 情况 1: 房间里没人了 (或者只剩 Bot)
                    LOG_INFO("   │  └─ 🛑 结果: 房间已空 (无继承人) -> 执行 cancelGame()");
                    cancelGame();
                    return; // 结束
                } else {
                    // 情况 2: 还有其他人，移交房主

                    // 1. 更新玩家标志
                    m_players[heirPid].isVisualHost = true;

                    // 2. 更新全局房主名字
                    m_host = heirName;

                    LOG_INFO(QString("   │  ├─ 🔍 继承人: %1 (PID: %2)").arg(heirName).arg(heirPid));

                    // 执行槽位移动 (Move Heir to Host Slot)
                    if (oldHostSlotIndex != -1) {
                        int heirSlotIndex = -1;

                        // 寻找继承人当前的槽位索引
                        for (int i = 0; i < m_slots.size(); ++i) {
                            if (m_slots[i].pid == heirPid) {
                                heirSlotIndex = i;
                                break;
                            }
                        }

                        // 如果找到了，并且位置不一样，则交换内容
                        if (heirSlotIndex != -1 && heirSlotIndex != oldHostSlotIndex) {

                            GameSlot &hostSlot = m_slots[oldHostSlotIndex]; // 此时它是空的 (PID=0, Open)
                            GameSlot &heirSlot = m_slots[heirSlotIndex];    // 此时它有人 (PID=Heir, Occupied)

                            std::swap(hostSlot.pid,            heirSlot.pid);
                            std::swap(hostSlot.downloadStatus, heirSlot.downloadStatus);
                            std::swap(hostSlot.slotStatus,     heirSlot.slotStatus);
                            std::swap(hostSlot.computer,       heirSlot.computer);
                            std::swap(hostSlot.computerType,   heirSlot.computerType);
                            std::swap(hostSlot.handicap,       heirSlot.handicap);

                            // 不需要交换 Team/Color/Race，继承人直接继承房主槽位的队伍和颜色

                            LOG_INFO(QString("   │  ├─ 🔄 位置调整: 继承人从 Slot %1 移至 Slot %2 (Host位)")
                                         .arg(heirSlotIndex).arg(oldHostSlotIndex));
                        }
                    }

                    LOG_INFO("   │  └─ ✅ 结果: 权限移交完成");

                    // 3. 广播移交通知
                    MultiLangMsg transferMsg;
                    transferMsg.add("CN", QString("系统: 房主已离开，[%1] 成为新房主。").arg(heirName))
                        .add("EN", QString("System: Host left. [%1] is the new host.").arg(heirName));
                    broadcastChatMessage(transferMsg, 0);
                }
            }
        }

        // 4. 广播离开
        if (!m_gameStarted) {
            if (!m_playerSockets.isEmpty()) {
                QByteArray leftPacket = createW3GSPlayerLeftPacket(pidToRemove, LEAVE_LOBBY);
                broadcastPacket(leftPacket, pidToRemove);

                MultiLangMsg leaveMsg;
                leaveMsg.add("CN", QString("玩家 [%1] 离开了游戏。").arg(nameToRemove))
                    .add("EN", QString("Player [%1] has left the game.").arg(nameToRemove));
                broadcastChatMessage(leaveMsg, pidToRemove);

                broadcastSlotInfo(pidToRemove);

                LOG_INFO("   └─ 📢 广播同步: 离开包(0x07) + 聊天通知 + 槽位刷新(0x09)");
            }
        } else {
            LOG_INFO("   └─ 🎮 [游戏内] 玩家断线，仅在服务端清理，不发送大厅协议包");
        }
    }
}

void Client::onGameStarted()
{
    // 1. 打印根节点
    LOG_INFO("🚀 [游戏启动] 倒计时结束，切换至 Loading 阶段");

    // 2. 标记游戏开始
    m_gameStarted = true;
    LOG_INFO("   ├─ ⚙️ 状态更新: m_gameStarted = true");

    // 3. 处理机器人隐身
    broadcastPacket(createW3GSPlayerLeftPacket(2, LEAVE_LOBBY), 0, false);
    LOG_INFO("   ├─ 👻 [幽灵模式] 已向全员广播机器人(PID:2)离开");

    // 4. 发送倒计时结束包
    broadcastPacket(createW3GSCountdownEndPacket(), 0);
    LOG_INFO("   ├─ 📡 广播指令: W3GS_COUNTDOWN_END (0x0B)");

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
    LOG_INFO("🔍 [Auth Check] 处理认证挑战 (0x51)");

    if (data.size() < 24) {
        LOG_ERROR(QString("   └─ ❌ [错误] 包长度不足: %1").arg(data.size()));
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

void Client::stopAdv() {
    LOG_INFO("🛑 [停止广播] 发送 SID_STOPADV (0x02)");
    sendPacket(SID_STOPADV, QByteArray());
}

void Client::cancelGame() {
    // 1. 打印根节点
    LOG_INFO("🔄 [重置游戏] 开始执行资源清理流程...");

    // 2. 网络层操作 (合并日志以减少刷屏)
    stopAdv();
    enterChat();
    joinRandomChannel();
    LOG_INFO("   ├─ 📡 网络动作: 停止广播 -> 请求进入大厅 -> 请求加入随机频道");

    // 3. 断开所有玩家连接
    int playerCount = m_playerSockets.size();
    if (playerCount > 0) {
        LOG_INFO(QString("   ├─ 🔌 连接清理: 正在断开 %1 名玩家 Socket").arg(playerCount));
        for (auto socket : qAsConst(m_playerSockets)) {
            if (socket->state() == QAbstractSocket::ConnectedState) {
                socket->disconnectFromHost();
            }
            socket->deleteLater();
        }
    } else {
        LOG_INFO("   ├─ 🔌 连接清理: 当前无活跃 TCP 连接");
    }

    // 4. 清理容器
    m_playerSockets.clear();
    m_playerBuffers.clear();
    m_players.clear();

    // 5. 重置槽位
    initSlots();
    LOG_INFO("   ├─ 🧹 内存清理: 玩家映射表清空 & 地图槽位重置");

    // 6. 停止各类计时器
    bool anyTimerActive = false;

    // A. 启动缓冲 (Start Lag)
    if (m_startLagTimer->isActive()) {
        m_startLagTimer->stop();
        LOG_INFO("   ├─ 🛑 [计时器] 强制中止: 启动缓冲 (StartLag)");
        anyTimerActive = true;
    }

    // B. 游戏心跳 (Game Tick)
    if (m_gameTickTimer->isActive()) {
        m_gameTickTimer->stop();
        LOG_INFO("   ├─ 🛑 [计时器] 强制停止: 游戏心跳 (GameTick)");
        anyTimerActive = true;
    }

    // C. 倒计时 (Countdown)
    if (m_startTimer->isActive()) {
        m_startTimer->stop();
        LOG_INFO("   ├─ 🛑 [计时器] 强制中止: 游戏开始倒计时 (Countdown)");
        anyTimerActive = true;
    }

    if (!anyTimerActive) {
        LOG_INFO("   ├─ ℹ️ [计时器] 无活跃的游戏逻辑计时器");
    }

    // 7. 重置标志位
    m_gameStarted = false;
    LOG_INFO(QString("   ├─ ⚙️ 标志重置: GameStarted=False | HostCounter (%1)").arg(m_hostCounter));

    // 8. 停止 Ping 循环 (最后一步)
    if (m_pingTimer->isActive()) {
        m_pingTimer->stop();
        LOG_INFO("   └─ 🛑 [计时器] 停止大厅 Ping 循环 -> 状态: IDLE");
    } else {
        LOG_INFO("   └─ ✅ [状态] 机器人已就绪 (Ping 循环未运行)");
    }

    emit gameCanceled();
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
    QByteArray encodedData = m_war3Map.getEncodedStatString(m_botDisplayName);
    if (encodedData.isEmpty()) {
        LOG_CRITICAL("   └─ ❌ [严重错误] StatString 生成失败");
        return;
    }
    LOG_INFO(QString("   ├─ 🗺️ 地图加载: %1 (StatString Ready)").arg(mapName));

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
        m_pingTimer->start(2000);
        LOG_INFO("   └─ 💓 动作: 发送请求(0x1C) + 启动 Ping 循环 (5s)");
    } else {
        LOG_INFO("   └─ 📤 动作: 发送请求(0x1C) (Ping 循环运行中)");
    }
}

void Client::startGame()
{
    if (m_gameStarted) return;
    if (m_startTimer->isActive()) return;

    // 1. 先关大门：停止广播，停止 Ping
    stopAdv(); // 停止 UDP 广播
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
    msg.add("CN", "游戏将于 5 秒后开始...")
        .add("EN", "Game starts in 5 seconds...");

    broadcastChatMessage(msg);

    // 3. 发送倒计时包
    broadcastPacket(createW3GSCountdownStartPacket(), 0);

    // 4. 最后启动定时器
    m_startTimer->start(5200);

    LOG_INFO("⏳ [游戏启动] 开始倒计时...");
}

void Client::abortGame()
{
    if (m_startTimer->isActive()) {
        m_startTimer->stop();
        MultiLangMsg msg;
        msg.add("CN", "倒计时已取消。")
            .add("EN", "Countdown aborted.");
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
    out << (quint32)QDateTime::currentMSecsSinceEpoch();

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

    // 5. 写入网络配置
    out << (quint16)2;
    out << (quint16)qToBigEndian(externalPort);
    writeIpToStreamWithLog(out, externalIp);
    out << (quint32)0 << (quint32)0;

    out << (quint16)2;
    out << (quint16)qToBigEndian(internalPort);
    writeIpToStreamWithLog(out, internalIp);
    out << (quint32)0 << (quint32)0;

    // 6. 回填包总长度
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
    if (hexPreview.length() > 30) hexPreview = hexPreview.left(27) + "...";

    // 只有在 flag 不是普通消息时，才打印构建日志，防止刷屏
    if (flag != ChatFlag::Message) {
        LOG_INFO(QString("📦 [构建包] 聊天/控制 (0x0F)"));
        LOG_INFO(QString("   ├─ 🎯 目标: %1 -> %2").arg(senderPid).arg(toPid));
        LOG_INFO(QString("   ├─ 🚩 类型: 0x%1 (Extra: %2)").arg(QString::number((int)flag, 16)).arg(extraData));
        LOG_INFO(QString("   └─ 📝 数据: %1").arg(hexPreview));
    }

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
    LOG_INFO(QString("   └─ 🔐 SHA1: %1...").arg(sha1Hex.left(20)));

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
        qDebug() << "🛑 [拦截] 试图在游戏/倒计时期间发送大厅消息，已阻止：" << msg.get("EN");
        return;
    }

    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        quint8 pid = it.key();

        // 排除 PID 2 (Host) 和 指定排除的 PID
        if (pid == excludePid || pid == m_botPid) continue;

        PlayerData &playerData = it.value();
        QTcpSocket* socket = playerData.socket;

        if (!socket || socket->state() != QAbstractSocket::ConnectedState) continue;

        // 1. 根据玩家的语言标记 (CN/EN/RU...) 获取对应文本
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
    bot.language            = "EN";
    bot.extIp               = QHostAddress("0.0.0.0");
    bot.intIp               = QHostAddress("0.0.0.0");

    m_players.insert(bot.pid, bot);

    LOG_INFO("🤖 Host Bot 注册完成 (PID: 2)");
}

void Client::checkAllPlayersLoaded()
{
    // 0. 前置检查：防止重复启动
    if (m_gameTickTimer->isActive()) return;
    if (m_startLagTimer->isActive()) return;

    // 1. 打印根节点
    LOG_INFO("🔍 [加载检查] 遍历玩家加载状态...");

    bool allLoaded = true;
    int loadedCount = 0;
    int totalCount = 0;

    // 2. 遍历玩家列表
    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        quint8 pid = it.key();
        const PlayerData &p = it.value();

        // 机器人不参与同步逻辑
        if (pid == m_botPid) continue;

        totalCount++;

        QString statusStr;
        if (p.isFinishedLoading) {
            loadedCount++;
            statusStr = "✅ 已就绪 (状态: 6)";
        } else {
            allLoaded = false;
            statusStr = "⏳ 加载中... (状态: 5)";
        }

        LOG_INFO(QString("   ├─ 👤 [PID: %1] %2 -> %3")
                     .arg(pid, -3)
                     .arg(p.name, -15)
                     .arg(statusStr));
    }

    // 3. 统计输出
    LOG_INFO(QString("   ├─ 📊 统计: 完成 %1 / 总计 %2").arg(loadedCount).arg(totalCount));

    // 4. 最终判定逻辑
    if (totalCount > 0 && allLoaded) {
        LOG_INFO("   └─ 🎉 结果: 全员已到达状态 6 -> 准备切换状态 7");
        m_gameStarted = true; // 确保游戏逻辑标志位开启
        m_startLagTimer->start(m_gameStartLag);

    } else {
        int remaining = totalCount - loadedCount;
        LOG_INFO(QString("   └─ 💤 结果: 还在等待 %1 名玩家...").arg(remaining));
    }
}

// =========================================================
// 13. 辅助工具函数
// =========================================================

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

bool Client::isBlackListedPort(quint16 port)
{
    static const QSet<quint16> blacklist = {
        22, 53, 3478, 53820, 57289, 57290, 80, 443, 8080, 8443, 3389, 5900, 3306, 5432, 6379, 27017
    };
    return blacklist.contains(port);
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

    checkPlayerTimeout();

    if (m_players.isEmpty()) return;

    QByteArray pingPacket = createW3GSPingFromHostPacket();

    bool shouldSendChat = false;

    MultiLangMsg waitMsg;

    m_chatIntervalCounter++;
    if (m_chatIntervalCounter >= 3) {
        int realPlayerCount = 0;
        for(auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.key() != m_botPid) realPlayerCount++;
        }

        // 填充多语言内容
        waitMsg.add("CN", QString("请耐心等待，当前已有 %1 个玩家...").arg(realPlayerCount))
            .add("EN", QString("Please wait, %1 players present...").arg(realPlayerCount));

        shouldSendChat = true;
        m_chatIntervalCounter = 0;
    }

    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        quint8 pid = it.key();
        PlayerData &playerData = it.value();
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

void Client::checkPlayerTimeout()
{
    if (m_startTimer->isActive() || m_gameStarted) {
        LOG_DEBUG("🛡️ [超时监控] 游戏启动/进行中 -> 跳过检测 (安全)");
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 场景 A: 下载中 (60秒)
    const qint64 TIMEOUT_DOWNLOADING = 60000;

    // 场景 B: 房间闲置 (10秒)
    const qint64 TIMEOUT_LOBBY_IDLE = 10000;

    QList<quint8> pidsToKick;

    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        quint8 pid = it.key();
        PlayerData &playerData = it.value();

        if (pid == m_botPid) continue; // 跳过机器人

        bool kick = false;
        QString reasonCategory = "";

        qint64 timeSinceLastResponse = now - playerData.lastResponseTime;
        qint64 timeSinceLastDownload = now - playerData.lastDownloadTime;

        if (playerData.isDownloadStart) {
            if (timeSinceLastDownload > TIMEOUT_DOWNLOADING) {
                kick = true;
                reasonCategory = QString("下载卡死 (%1ms)").arg(timeSinceLastDownload);
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

quint32 Client::ipToUint32(const QHostAddress &address) { return address.toIPv4Address(); }
quint32 Client::ipToUint32(const QString &ipAddress) { return QHostAddress(ipAddress).toIPv4Address(); }

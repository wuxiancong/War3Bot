#include "client.h"
#include "logger.h"
#include "command.h"
#include "bnethash.h"
#include "bnetsrp3.h"
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

// =========================================================
// 1. 生命周期 (构造与析构)
// =========================================================

Client::Client(QObject *parent)
    : QObject(parent)
    , m_srp(nullptr)
    , m_udpSocket(nullptr)
    , m_tcpSocket(nullptr)
    , m_loginProtocol(Protocol_Old_0x29)
{
    // 1. 打印根节点
    qDebug().noquote() << "🧩 [Client] 实例初始化启动";

    m_pingTimer = new QTimer(this);
    m_udpSocket = new QUdpSocket(this);
    m_tcpServer = new QTcpServer(this);
    m_tcpSocket = new QTcpSocket(this);

    // 信号槽连接
    connect(m_pingTimer, &QTimer::timeout, this, &Client::sendPingLoop);
    connect(m_tcpSocket, &QTcpSocket::connected, this, &Client::onConnected);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &Client::onTcpReadyRead);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &Client::onDisconnected);
    connect(m_tcpServer, &QTcpServer::newConnection, this, &Client::onNewConnection);
    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError){
        // 这里是运行时错误，不属于初始化日志树，用 ERROR 即可
        LOG_ERROR(QString("战网连接错误: %1").arg(m_tcpSocket->errorString()));
    });
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &Client::onUdpReadyRead);

    qDebug().noquote() << "   ├─ ⚙️ 环境构建: 定时器/Socket对象已创建，信号已连接";

    // 初始化 UDP
    if (!bindToRandomPort()) {
        qDebug().noquote() << "   ├─ ❌ 网络绑定: 随机端口绑定失败";
    } else {
        qDebug().noquote() << QString("   ├─ 📡 网络绑定: TCP/UDP 监听端口 %1").arg(m_udpSocket->localPort());
    }

    // 资源路径搜索逻辑
    QStringList searchPaths;
    searchPaths << QCoreApplication::applicationDirPath() + "/war3files";
#ifdef Q_OS_LINUX
    searchPaths << "/etc/War3Bot/war3files";
#endif
    searchPaths << QDir::currentPath() + "/war3files";
    searchPaths << QCoreApplication::applicationDirPath();

    bool foundResources = false;

    qDebug().noquote() << "   └─ 🔍 资源扫描: War3 核心文件检查";

    for (const QString &pathStr : qAsConst(searchPaths)) {
        QDir dir(pathStr);
        if (dir.exists("War3.exe")) {
            m_war3ExePath = dir.absoluteFilePath("War3.exe");
            m_gameDllPath = dir.absoluteFilePath("Game.dll");
            m_stormDllPath = dir.absoluteFilePath("Storm.dll");
            m_dota683dPath = dir.absoluteFilePath("maps/DotA v6.83d.w3x");

            // 成功找到
            qDebug().noquote() << QString("      ├─ ✅ 命中路径: %1").arg(dir.absolutePath());

            // 检查 Dota 地图是否存在
            if (QFile::exists(m_dota683dPath)) {
                qDebug().noquote() << QString("      └─ 🗺️ 地图确认: %1").arg(QFileInfo(m_dota683dPath).fileName());
            } else {
                qDebug().noquote() << QString("      └─ ⚠️ 地图缺失: %1 (请确保 maps 目录完整)").arg(m_dota683dPath);
            }

            foundResources = true;
            break;
        }
    }

    if (!foundResources) {
        qDebug().noquote() << "      └─ ❌ 致命错误: 未能找到 War3.exe！";
        qDebug().noquote() << "         ├─ 已尝试路径:";
        for(const QString &p : qAsConst(searchPaths)) {
            qDebug().noquote() << QString("         │  %1").arg(p);
        }
        LOG_ERROR("❌ 致命错误: 未能找到 War3.exe！");
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
    qDebug().noquote() << "🔧 [配置设定] 更新凭据";
    qDebug().noquote() << QString("   ├─ 👤 用户: %1").arg(m_user);
    qDebug().noquote() << QString("   ├─ 🔑 密码: %1").arg(m_pass);
    qDebug().noquote() << QString("   └─ 📡 协议: %1").arg(protoName);
}

void Client::connectToHost(const QString &address, quint16 port)
{
    m_serverAddr = address;
    m_serverPort = port;

    // 树状日志
    qDebug().noquote() << "🔌 [网络请求] 发起战网连接";
    qDebug().noquote() << QString("   └─ 🎯 目标: %1:%2").arg(address).arg(port);

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
    qDebug().noquote() << "🔌 [网络状态] 战网连接断开";
    qDebug().noquote() << "   └─ ⚠️ 状态: Disconnected";

    emit disconnected();
}

void Client::onConnected()
{
    // 树状日志
    qDebug().noquote() << "✅ [网络状态] TCP 链路已建立";
    qDebug().noquote() << "   ├─ 🤝 握手: 发送协议字节 (0x01)";
    qDebug().noquote() << "   └─ 🚀 动作: 发送 AuthInfo -> 触发 connected 信号";

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
        qDebug().noquote() << "🎮 [玩家连接] 检测到新 TCP 请求";
        qDebug().noquote() << QString("   └─ 🌍 来源: %1:%2")
                                  .arg(socket->peerAddress().toString())
                                  .arg(socket->peerPort());

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
        LOG_WARNING("❌ 发送失败: Socket 未初始化");
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

void Client::sendNextMapPart(quint8 toPid, quint8 fromPid)
{
    // 找不到玩家
    if (!m_players.contains(toPid)) {
        qDebug().noquote() << "❌ [地图上传] 失败";
        qDebug().noquote() << QString("   └─ 原因: 找不到目标 PID %1").arg(toPid);
        return;
    }

    PlayerData &playerData = m_players[toPid];

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 更新下载活跃时间
    if (now - playerData.lastDownloadTime < 5) {
        qDebug() << "限速拦截: " << (now - playerData.lastDownloadTime);
        return;
    }

    playerData.lastDownloadTime = now;

    // [检查点 1] 状态检查
    if (!playerData.isDownloading) {
        // 这种警告通常不需要树状结构，单行即可
        qDebug().noquote() << QString("⚠️ [地图上传] 忽略请求: 玩家 [%1] 未处于下载状态").arg(playerData.name);
        return;
    }

    // 获取原始地图数据
    const QByteArray &mapData = m_war3Map.getMapRawData();
    quint32 totalSize = (quint32)mapData.size();

    // [检查点 2] 数据有效性
    if (totalSize == 0) {
        qDebug().noquote() << "❌ [地图上传] 严重错误";
        qDebug().noquote() << "   └─ 原因: 内存中没有地图数据 (Size=0)";
        return;
    }

    // 分支 A: 传输完成
    if (playerData.downloadOffset >= totalSize) {
        qDebug().noquote() << QString("✅ [地图上传] 传输完成: %1").arg(playerData.name);
        qDebug().noquote() << QString("   ├─ 📊 数据统计: %1 / %2 bytes").arg(playerData.downloadOffset).arg(totalSize);
        qDebug().noquote() << "   └─ 🚀 动作: 标记完成 -> 广播 SlotInfo -> 发送确认包";

        playerData.isDownloading = false;

        // 更新槽位并广播
        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].pid == toPid) {
                m_slots[i].downloadStatus = 100;
                break;
            }
        }
        broadcastSlotInfo();

        // 发送最后一个 SlotInfo 给该玩家确认
        playerData.socket->write(createW3GSSlotInfoPacket());
        playerData.socket->flush();
        return;
    }

    // 分支 B: 计算与发送分片

    // 计算分片
    int chunkSize = MAX_CHUNK_SIZE;
    if (playerData.downloadOffset + chunkSize > totalSize) {
        chunkSize = totalSize - playerData.downloadOffset;
    }

    QByteArray chunk = mapData.mid(playerData.downloadOffset, chunkSize);

    // 构造包 (0x43)
    QByteArray packet = createW3GSMapPartPacket(toPid, fromPid, playerData.downloadOffset, chunk);

    qint64 written = playerData.socket->write(packet);
    playerData.socket->flush();

    // 分支 C: 发送结果处理
    if (written > 0) {
        if (playerData.downloadOffset == 0 || playerData.downloadOffset % (1024 * 1024) < 2000) {
            int percent = (int)((double)playerData.downloadOffset / totalSize * 100);

            // 使用简化的树状结构显示进度节点
            qDebug().noquote() << QString("📤 [地图上传] 传输中: %1").arg(playerData.name);
            qDebug().noquote() << QString("   └─ 📦 进度: %1% (Offset: %2 | Chunk: %3)")
                                      .arg(percent, 2) // 占位对齐
                                      .arg(playerData.downloadOffset)
                                      .arg(chunkSize);
        }

        // 更新偏移量
        playerData.downloadOffset += chunkSize;
    } else {
        qDebug().noquote() << QString("❌ [地图上传] Socket 写入失败: %1").arg(playerData.name);
        qDebug().noquote() << QString("   ├─ 📝 错误信息: %1").arg(playerData.socket->errorString());
        qDebug().noquote() << "   └─ 🛡️ 动作: 终止下载状态";

        playerData.isDownloading = false; // 终止下载
    }
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
        qDebug().noquote() << QString("📥 [BNET] 收到数据包: %1 (0x%2)")
                                  .arg(packetName, QString::number(id, 16).toUpper());
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
        qDebug().noquote() << "   └─ ✅ 状态: 已进入聊天环境 (Unique Name Assigned)";
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

        qDebug().noquote() << QString("   ├─ 📋 频道列表: 共 %1 个").arg(m_channelList.size());

        // 打印前几个频道作为示例，防止列表太长刷屏
        int printLimit = qMin(m_channelList.size(), 3);
        for(int i=0; i<printLimit; ++i) {
            qDebug().noquote() << QString("   │  ├─ %1").arg(m_channelList[i]);
        }
        if (m_channelList.size() > printLimit) {
            qDebug().noquote() << QString("   │  └─ ... (还有 %1 个)").arg(m_channelList.size() - printLimit);
        }

        if (m_channelList.isEmpty()) {
            qDebug().noquote() << "   └─ ⚠️ [异常] 列表为空 -> 加入默认频道 'The Void'";
            joinChannel("The Void");
        }
        else {
            QString target;
            if (m_isBot) {
                int index = QRandomGenerator::global()->bounded(m_channelList.size());
                target = m_channelList.at(index);
                qDebug().noquote() << QString("   └─ 🎲 [Bot随机] 选中频道: %1").arg(target);
            }
            else {
                target = m_channelList.first();
                qDebug().noquote() << QString("   └─ ➡️ [默认] 加入首个频道: %1").arg(target);
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
        qDebug().noquote() << QString("   ├─ 🎫 事件ID: 0x%1").arg(QString::number(eventId, 16).toUpper());
        qDebug().noquote() << QString("   ├─ 👤 用户名: %1").arg(username);

        // 指令捕获逻辑
        if (text.startsWith("/")) {
            qDebug().noquote() << QString("   ├─ ⚡ [指令捕获] %1").arg(text);
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
            qDebug().noquote() << QString("   └─ 📝 内容: %1").arg(contentLog);
        } else {
            qDebug().noquote() << QString("   └─ 📝 内容: %1").arg(contentLog);
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
            qDebug().noquote() << "   ├─ 🎉 结果: 成功";
            qDebug().noquote() << "   └─ 🚀 动作: 发出 authenticated 信号 -> 进入聊天";
            emit authenticated();
            enterChat();
        } else {
            qDebug().noquote() << QString("   └─ ❌ 结果: 失败 (Code: 0x%1)").arg(QString::number(result, 16));
            LOG_ERROR(QString("登录失败: 0x%1").arg(QString::number(result, 16)));
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
            qDebug().noquote() << "   ├─ 🎉 结果: 注册成功";
            qDebug().noquote() << "   └─ 🚀 动作: 自动尝试登录...";
            emit accountCreated();
            sendLoginRequest(Protocol_SRP_0x53);
        } else if (status == 0x04) {
            qDebug().noquote() << "   ├─ ⚠️ 结果: 账号已存在";
            qDebug().noquote() << "   └─ 🚀 动作: 尝试直接登录...";
            sendLoginRequest(Protocol_SRP_0x53);
        } else {
            qDebug().noquote() << QString("   └─ ❌ 结果: 注册失败 (Code: 0x%1)").arg(QString::number(status, 16));
            LOG_ERROR(QString("注册失败: 0x%1").arg(QString::number(status, 16)));
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
            qDebug().noquote() << "   ├─ 🎉 结果: SRP 验证通过";
            qDebug().noquote() << "   └─ 🚀 动作: 进入聊天";
            emit authenticated();
            enterChat();
        } else {
            QString reason = "未知错误";
            if (status == 0x02) reason = "密码错误";
            else if (status == 0x0D) reason = "账号不存在";

            qDebug().noquote() << QString("   ├─ ❌ 结果: 验证失败 (0x%1)").arg(QString::number(status, 16));
            qDebug().noquote() << QString("   └─ 📝 原因: %1").arg(reason);
            LOG_ERROR(QString("登录失败(SRP): %1").arg(reason));
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
            qDebug().noquote() << "   ├─ ✅ 结果: 房间创建成功";
            qDebug().noquote() << "   └─ 📢 状态: 广播已启动";
            emit gameCreateSuccess(From_Client);
        } else {
            QString errStr;
            switch (status) {
            case GameCreate_NameExists:      errStr = "房间名已存在"; break;
            case GameCreate_TypeUnavailable: errStr = "游戏类型不可用"; break;
            case GameCreate_Error:           errStr = "通用创建错误"; break;
            default:                         errStr = QString("Code 0x%1").arg(QString::number(status, 16)); break;
            }
            qDebug().noquote() << QString("   ├─ ❌ 结果: 创建失败");
            qDebug().noquote() << QString("   └─ 📝 原因: %1").arg(errStr);

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
            LOG_WARNING("❌ 非法协议头，断开连接");
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
    if (id != 0x44 && id != 0x46) {
        qDebug().noquote() << QString("📥 [W3GS] 收到数据包: 0x%1").arg(QString::number(id, 16).toUpper());
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
        QString clientPlayerName = "Unknown";
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
            qDebug().noquote() << QString("   └─ ❌ [错误] 包长度不足: %1").arg(payload.size());
            return;
        }

        QHostAddress iAddr(qToBigEndian(clientInternalIP));

        // 打印解析详情
        qDebug().noquote() << QString("   ├─ 👤 玩家名: %1").arg(clientPlayerName);
        qDebug().noquote() << QString("   ├─ 🌍 内网IP: %1:%2").arg(iAddr.toString()).arg(clientInternalPort);
        qDebug().noquote() << QString("   ├─ 🔧 监听端口: %1").arg(clientListenPort);

        // 1.1 房主校验
        bool nameMatch = (!m_host.isEmpty() && m_host.compare(clientPlayerName, Qt::CaseInsensitive) == 0);
        qDebug().noquote() << QString("   ├─ 🔍 房主校验: 预设[%1] vs 玩家[%2] -> %3")
                                  .arg(m_host, clientPlayerName, nameMatch ? "✅ 匹配" : "❌ 不匹配");

        // 1.2 逻辑判断：房主是否在场
        if (!isHostJoined()) {
            // A. 如果来的不是房主 -> 拒绝
            if (!nameMatch) {
                qDebug().noquote() << QString("   └─ 🛑 [拒绝加入] 原因: 等待房主 [%1] 进场中...").arg(m_host);
                socket->write(createW3GSRejectJoinPacket(BAD_GAME));
                socket->flush();
                socket->disconnectFromHost();
                return;
            }
            // B. 如果来的是房主 -> 允许
            else {
                qDebug().noquote() << QString("   ├─ 👑 [房主到达] 房间锁定解除，允许其他人加入");
                emit hostJoinedGame(clientPlayerName);
            }
        }
        else {
            // C. 房主已在场，防止重名攻击
            if (nameMatch) {
                qDebug().noquote() << QString("   └─ ⚠️ [拒绝加入] 原因: 检测到重复的房主名 [%1]").arg(clientPlayerName);
                socket->write(createW3GSRejectJoinPacket(BAD_GAME));
                socket->disconnectFromHost();
                return;
            }
        }

        // 2. 槽位分配
        int slotIndex = -1;
        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].slotStatus == Open) {
                slotIndex = i;
                break;
            }
        }

        if (slotIndex == -1) {
            qDebug().noquote() << "   └─ ⚠️ [拒绝加入] 原因: 房间已满";
            socket->write(createW3GSRejectJoinPacket(FULL));
            socket->flush();
            socket->disconnectFromHost();
            return;
        }

        if (m_gameStarted) {
            socket->write(createW3GSRejectJoinPacket(STARTED));
        }

        // 分配 PID
        quint8 hostId = slotIndex + 2;
        m_slots[slotIndex].pid = hostId;
        m_slots[slotIndex].slotStatus = Occupied;
        m_slots[slotIndex].downloadStatus = NotStarted;
        m_slots[slotIndex].computer = Human;

        qint64 now = QDateTime::currentMSecsSinceEpoch();

        // 注册玩家
        PlayerData playerData;
        playerData.pid = hostId;
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

        m_players.insert(hostId, playerData);

        qDebug().noquote() << QString("   ├─ 💾 玩家注册: PID %1 (Slot %2)").arg(hostId).arg(slotIndex);

        // 3. 构建握手响应
        QByteArray finalPacket;
        QHostAddress hostIp = socket->peerAddress();
        quint16 hostPort = m_udpSocket->localPort();

        finalPacket.append(createW3GSSlotInfoJoinPacket(hostId, hostIp, hostPort)); // 0x04
        finalPacket.append(createPlayerInfoPacket(1, m_botDisplayName, QHostAddress("0.0.0.0"), 0, QHostAddress("0.0.0.0"), 0)); // 0x06 (Bot)

        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            const PlayerData &p = it.value();
            if (p.pid == hostId || p.pid == 1) continue;
            finalPacket.append(createPlayerInfoPacket(p.pid, p.name, p.extIp, p.extPort, p.intIp, p.intPort));
        }

        finalPacket.append(createW3GSMapCheckPacket()); // 0x3D
        finalPacket.append(createW3GSSlotInfoPacket()); // 0x09

        socket->write(finalPacket);
        socket->flush();

        qDebug().noquote() << "   ├─ 📤 发送握手: 0x04 -> 0x06 -> 0x3D -> 0x09";

        // 4. 广播
        QByteArray newPlayerInfoPacket = createPlayerInfoPacket(
            playerData.pid, playerData.name, playerData.extIp, playerData.extPort, playerData.intIp, playerData.intPort);
        broadcastPacket(newPlayerInfoPacket, hostId);
        broadcastSlotInfo();

        qDebug().noquote() << "   └─ 📢 广播状态: 同步新玩家信息 & 刷新槽位";
    }
    break;

    case W3GS_LEAVEREQ: // 处理客户端发过来的 0x21 包
    {
        qDebug().noquote() << QString("   └─ 👋 [离开请求] 来源: %1").arg(socket->peerAddress().toString());
        socket->disconnectFromHost();
    }
    break;

    case W3GS_CHAT_TO_HOST: // [0x28] 客户端发送聊天消息
    {
        if (payload.size() < 7) return;
        QDataStream in(payload);
        in.setByteOrder(QDataStream::LittleEndian);
        quint8 numReceivers; in >> numReceivers;
        if (numReceivers > 0) in.skipRawData(numReceivers);
        quint8 fromPid, flag; quint32 extra; in >> fromPid >> flag >> extra;
        int headerSize = 1 + numReceivers + 1 + 1 + 4;

        // 查找发送者
        quint8 senderPid = 0;
        QString senderName = "";
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().socket == socket) {
                senderPid = it.key();
                senderName = it.value().name;
                break;
            }
        }

        if (senderPid == 0) {
            qDebug().noquote() << "   └─ ⚠️ [警告] 无法识别发送者 Socket";
            return;
        }

        if (payload.size() > headerSize) {
            QByteArray msgBytes = payload.mid(headerSize);
            if (msgBytes.endsWith('\0')) msgBytes.chop(1);
            QString msg = m_players[senderPid].codec->toUnicode(msgBytes);

            qDebug().noquote() << QString("   ├─ 👤 发送者: %1 (PID:%2)").arg(senderName).arg(senderPid);
            qDebug().noquote() << QString("   └─ 💬 内容: %1").arg(msg);

            // 指令处理
            if (msg.startsWith("/")) {
                qDebug().noquote() << QString("      ├─ 🔧 识别为指令: 房主=[%1]").arg(m_host);
                if (m_command) {
                    m_command->process(senderPid, msg);
                    qDebug().noquote() << "      └─ ✅ 指令已执行";
                }
            }

            // 转发聊天
            MultiLangMsg chatMsg;
            chatMsg.add("CN", QString("%1: %2").arg(senderName, msg));
            chatMsg.add("EN", QString("%1: %2").arg(senderName, msg));
            broadcastChatMessage(chatMsg, senderPid);
        }
    }
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

        qDebug().noquote() << QString("📥 [W3GS] 收到请求: 0x3F (StartDownload)");
        qDebug().noquote() << QString("   └─ 👤 玩家: %1 (PID: %2)").arg(playerData.name).arg(currentPid);

        // 2. 防重复检查
        if (playerData.isDownloading) {
            qDebug().noquote() << "   └─ ⚠️ 忽略: 已经在下载进程中";
            return;
        }

        // 3. 查找槽位并触发下载
        bool validSlot = false;
        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].pid == currentPid) {
                if (m_slots[i].downloadStatus != Completed) {
                    m_slots[i].downloadStatus = Downloading;
                    validSlot = true;
                }
                break;
            }
        }

        if (validSlot) {
            qDebug().noquote() << "   └─ 🚀 响应: 启动下载序列";

            // --- 步骤 A: 发送开始信号 (0x3F) ---
            socket->write(createW3GSStartDownloadPacket(1));
            socket->flush();

            // --- 步骤 B: 更新大厅槽位状态 (0x09) ---
            socket->write(createW3GSSlotInfoPacket());
            socket->flush();

            // --- 步骤 C: 准备状态 ---
            playerData.isDownloading = true;
            playerData.downloadOffset = 0;

            const QByteArray &mapData = m_war3Map.getMapRawData();
            int chunkSize = MAX_CHUNK_SIZE;
            if (mapData.size() < chunkSize) chunkSize = mapData.size();
            QByteArray firstChunk = mapData.mid(0, chunkSize);

            socket->write(createW3GSMapPartPacket(currentPid, 1, 0, firstChunk));
            socket->flush();

            playerData.downloadOffset += chunkSize;

            qDebug().noquote() << QString("   └─ 📤 已发送首块数据 (Size: %1)").arg(chunkSize);
        } else {
            qDebug().noquote() << "   └─ ℹ️ 忽略: 玩家已有地图或槽位无效";
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
        QString playerName = "Unknown";
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
                        playerData.isDownloading = false;
                        slotUpdated = true;
                        qDebug().noquote() << "   └─ ✅ 状态: 地图完整/校验通过";
                    }
                }
                // [B] 需要下载
                else {
                    if (m_slots[i].downloadStatus != Downloading) {
                        m_slots[i].downloadStatus = Downloading;
                    }
                    playerData.isDownloading = true;

                    // 情况 1: 初始请求 / 开始下载 (Flag=3)
                    if (sizeFlag == 1 && clientMapSize == 0) {
                        qDebug().noquote() << "   └─ 🚀 流程: 触发初始下载 (0x3F)";

                        socket->write(createW3GSStartDownloadPacket(1));
                        socket->write(createW3GSSlotInfoPacket());
                        socket->flush();

                        // 初始化
                        playerData.downloadOffset = 0;

                        // 延时发第一块
                        QTimer::singleShot(200, this, [this, currentPid]() {
                            if (m_players.contains(currentPid))
                                sendNextMapPart(currentPid);
                        });
                    }
                    // 情况 2: 进度同步 / 重传请求 (Flag=3)
                    else {
                        if (clientMapSize < playerData.downloadOffset) {
                            qDebug().noquote() << QString("   └─ 🔄 [回滚重传] Client: %1 < Server: %2 -> 重发块")
                                                      .arg(clientMapSize).arg(playerData.downloadOffset);

                            playerData.downloadOffset = clientMapSize;
                            sendNextMapPart(currentPid);
                        }
                        else if (clientMapSize == playerData.downloadOffset) {
                            qDebug().noquote() << "   └─ ℹ️ [进度同步] 状态一致，等待 ACK";
                            sendNextMapPart(currentPid);
                        }
                    }
                }
                break;
            }
        }
        if (slotUpdated) broadcastSlotInfo();
    }
    break;

    case W3GS_MAPPARTOK: //  [0x44] 客户端确认地图 OK
    {
        if (payload.size() < 9) return;
        QDataStream in(payload);
        in.setByteOrder(QDataStream::LittleEndian);
        quint8 fromPid, toPid; quint32 clientOffset;
        in >> fromPid >> toPid >> clientOffset;

        quint8 currentPid = 0;
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().socket == socket) { currentPid = it.key(); break; }
        }
        if (currentPid == 0) return;

        m_players[currentPid].lastResponseTime = QDateTime::currentMSecsSinceEpoch();
        if (m_players.contains(currentPid)) sendNextMapPart(currentPid);
    }
    break;

    case W3GS_MAPPARTNOTOK: // [0x45] 客户端报告 CRC 校验失败
    {
        quint8 currentPid = 0;
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().socket == socket) { currentPid = it.key(); break; }
        }

        qDebug().noquote() << QString("      └─ ⚠️ [下载错误] PID %1 报告 CRC 校验失败 (0x45) -> 等待 0x42 重同步").arg(currentPid);
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
        qDebug().noquote() << QString("   └─ ❓ [未知包] 忽略处理");
        break;
    }
}

void Client::onPlayerDisconnected() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    quint8 pidToRemove = 0;
    QString nameToRemove = "Unknown";
    bool wasVisualHost = false;

    // 1. 查找玩家
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
        // 1. 打印根节点
        qDebug().noquote() << QString("🔌 [断开连接] 玩家离线: %1 (PID: %2)").arg(nameToRemove).arg(pidToRemove);

        // 2. 释放槽位逻辑
        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].pid == pidToRemove) {
                m_slots[i].pid = 0;
                m_slots[i].slotStatus = Open;
                m_slots[i].downloadStatus = NotStarted;
                break;
            }
        }
        qDebug().noquote() << "   ├─ 🧹 资源清理: Socket 移除 & 槽位重置";

        // 3. 房主离开处理逻辑
        if (wasVisualHost) {
            qDebug().noquote() << "   ├─ 👑 [房主交接] 检测到房主离开...";

            // A. 寻找继承人 (排除 PID 1 的机器人)
            quint8 heirPid = 0;
            QString heirName = "";

            for (auto pIt = m_players.begin(); pIt != m_players.end(); ++pIt) {
                if (pIt.key() != 1) {
                    heirPid = pIt.key();
                    heirName = pIt.value().name;
                    break;
                }
            }

            // B. 判断结果
            if (heirPid == 0) {
                // 情况 1: 房间里没人了 (或者只剩 Bot)
                qDebug().noquote() << "   │  └─ 🛑 结果: 房间已空 (无继承人) -> 执行 cancelGame()";
                cancelGame();
                return; // 结束
            } else {
                // 情况 2: 还有其他人，移交房主

                // 1. 更新玩家标志
                m_players[heirPid].isVisualHost = true;

                // 2. 更新全局房主名字
                m_host = heirName;

                qDebug().noquote() << QString("   │  ├─ 🔍 继承人: %1 (PID: %2)").arg(heirName).arg(heirPid);
                qDebug().noquote() << "   │  └─ ✅ 结果: 权限移交完成";

                // 3. 广播移交通知
                MultiLangMsg transferMsg;
                transferMsg.add("CN", QString("系统: 房主已离开，[%1] 成为新房主。").arg(heirName))
                    .add("EN", QString("System: Host left. [%1] is the new host.").arg(heirName));
                broadcastChatMessage(transferMsg, 0); // 发给所有人

                // TODO: performSlotSwap(heirPid, 0);
            }
        }

        // 4. 广播协议层离开包 (0x07)
        QByteArray leftPacket = createW3GSPlayerLeftPacket(pidToRemove, 0x0D);
        broadcastPacket(leftPacket, pidToRemove);

        // 5. 广播聊天消息
        MultiLangMsg leaveMsg;
        leaveMsg.add("CN", QString("玩家 [%1] 离开了游戏。").arg(nameToRemove))
            .add("EN", QString("Player [%1] has left the game.").arg(nameToRemove));
        broadcastChatMessage(leaveMsg, pidToRemove);

        // 6. 广播槽位更新 (0x09)
        broadcastSlotInfo(pidToRemove);

        qDebug().noquote() << "   └─ 📢 广播同步: 离开包(0x07) + 聊天通知 + 槽位刷新(0x09)";
    }
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
    qDebug().noquote() << QString("📨 [UDP] 收到数据包: 0x%1").arg(QString::number(msgId, 16).toUpper());
    qDebug().noquote() << QString("   ├─ 🌍 来源: %1:%2 (Len: %3)")
                              .arg(sender.toString()).arg(senderPort).arg(data.size());

    // 4. 格式化 Hex 字符串 (每字节加空格)
    QString hexStr = data.toHex().toUpper();
    for(int i = 2; i < hexStr.length(); i += 3) hexStr.insert(i, " ");

    // 如果包太大，截断显示，防止日志刷屏
    if (hexStr.length() > 60) {
        hexStr = hexStr.left(57) + "...";
    }
    qDebug().noquote() << QString("   ├─ 📦 内容: %1").arg(hexStr);

    // 5. 分发处理
    switch (msgId) {
    case W3GS_TEST: // 0x88
    {
        // 读取剩余的数据作为字符串
        QByteArray payload = data.mid(4);
        QString msg = QString::fromUtf8(payload);

        qDebug().noquote() << "   ├─ 🧪 类型: 连通性测试 (W3GS_TEST)";
        qDebug().noquote() << QString("   ├─ 📝 消息: %1").arg(msg);

        // 回显数据
        m_udpSocket->writeDatagram(data, sender, senderPort);

        qDebug().noquote() << "   └─ 🚀 动作: 已执行 Echo 回显";
    }
    break;

        // 可以在这里添加更多 case，比如 W3GS_SEARCHGAME (0x2F) 等

    default:
        qDebug().noquote() << "   └─ ❓ 状态: 未知/未处理的包 ID";
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
    qDebug().noquote() << "📤 [Auth Info] 发送认证信息 (0x50)";

    // 2. 打印关键参数分支
    qDebug().noquote() << QString("   ├─ 🌍 本地 IP: %1").arg(localIpStr.isEmpty() ? "Unknown (0)" : localIpStr);

    // 硬编码的常量参数解释
    // Platform: IX86, Product: W3XP (冰封王座), Version: 26 (1.26)
    qDebug().noquote() << "   ├─ 🎮 客户端: W3XP (IX86) | Ver: 26";

    // Locale: 2052 (zh-CN), Timezone: -480 (UTC+8)
    qDebug().noquote() << "   ├─ 🌏 区域: CHN (China) | LCID: 2052 | TZ: UTC+8";

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
    qDebug().noquote() << "   └─ 🚀 动作: 数据打包发送 -> 等待 Auth Check (0x51)";

    sendPacket(SID_AUTH_INFO, payload);
}

void Client::handleAuthCheck(const QByteArray &data)
{
    // 1. 打印根节点
    qDebug().noquote() << "🔍 [Auth Check] 处理认证挑战 (0x51)";

    if (data.size() < 24) {
        qDebug().noquote() << QString("   └─ ❌ [错误] 包长度不足: %1").arg(data.size());
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
    qDebug().noquote() << "   ├─ 📥 [服务端参数]";
    qDebug().noquote() << QString("   │  ├─ Logon Type:   %1").arg(m_logonType);
    qDebug().noquote() << QString("   │  ├─ Server Token: 0x%1").arg(QString::number(m_serverToken, 16).toUpper());
    qDebug().noquote() << QString("   │  ├─ MPQ File:     %1").arg(QString(mpqFileName));
    qDebug().noquote() << QString("   │  └─ Formula:      %1").arg(QString(formulaString));

    // 3. 执行哈希计算
    unsigned long checkSum = 0;
    if (QFile::exists(m_war3ExePath)) {
        checkRevisionFlat(formulaString.constData(), m_war3ExePath.toUtf8().constData(),
                          m_stormDllPath.toUtf8().constData(), m_gameDllPath.toUtf8().constData(),
                          mpqNumber, &checkSum);

        qDebug().noquote() << "   ├─ 🧮 [版本校验]";
        qDebug().noquote() << QString("   │  ├─ Core Path: %1").arg(m_war3ExePath);
        qDebug().noquote() << QString("   │  └─ Checksum:  0x%1").arg(QString::number(checkSum, 16).toUpper());
    } else {
        qDebug().noquote() << QString("   └─ ❌ [严重错误] War3.exe 缺失: %1").arg(m_war3ExePath);
        LOG_ERROR("War3.exe 不存在，无法计算哈希");
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

    qDebug().noquote() << "   ├─ 📤 [构造响应]";
    qDebug().noquote() << QString("   │  ├─ Client Token: 0x%1").arg(QString::number(m_clientToken, 16).toUpper());
    qDebug().noquote() << QString("   │  └─ Exe Info:     %1").arg(exeInfoString);

    // 5. 发送并推进流程
    sendPacket(SID_AUTH_CHECK, response);

    qDebug().noquote() << QString("   └─ 🚀 [流程推进] 发送校验响应 -> 发起登录请求 (%1)").arg(m_loginProtocol);
    sendLoginRequest(m_loginProtocol);
}

void Client::sendLoginRequest(LoginProtocol protocol)
{
    // 1. 打印根节点
    qDebug().noquote() << QString("🔑 [登录请求] 发起身份验证 (Protocol: 0x%1)").arg(QString::number(protocol, 16).toUpper());

    if (protocol == Protocol_Old_0x29 || protocol == Protocol_Logon2_0x3A) {
        // === 旧版 DoubleHash 逻辑 ===
        qDebug().noquote() << "   ├─ 📜 算法: DoubleHash (Broken SHA1)";

        QByteArray proof = calculateOldLogonProof(m_pass, m_clientToken, m_serverToken);

        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        out << m_clientToken << m_serverToken;
        out.writeRawData(proof.data(), 20);
        out.writeRawData(m_user.toUtf8().constData(), m_user.toUtf8().size());
        out << (quint8)0;

        BNETPacketID pktId = (protocol == Protocol_Old_0x29 ? SID_LOGONRESPONSE : SID_LOGONRESPONSE2);
        qDebug().noquote() << QString("   └─ 🚀 动作: 发送 Hash 证明 -> 0x%1").arg(QString::number(pktId, 16).toUpper());

        sendPacket(pktId, payload);
    }
    else if (protocol == Protocol_SRP_0x53) {
        // === 新版 SRP 逻辑 ===
        qDebug().noquote() << "   ├─ 📜 算法: SRP (Secure Remote Password)";
        qDebug().noquote() << "   ├─ 🔢 步骤: 1/2 (Client Hello)";

        if (m_srp) delete m_srp;
        m_srp = new BnetSRP3(m_user, m_pass);

        BigInt A = m_srp->getClientSessionPublicKey();
        QByteArray A_bytes = A.toByteArray(32, 1, false);

        qDebug().noquote() << "   ├─ 🧮 计算: 生成客户端公钥 (A)";

        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        out.writeRawData(A_bytes.constData(), 32);
        out.writeRawData(m_user.trimmed().toUtf8().constData(), m_user.length());
        out << (quint8)0;

        qDebug().noquote() << "   └─ 🚀 动作: 发送公钥 A + 用户名 -> 等待 0x53";
        sendPacket(SID_AUTH_ACCOUNTLOGON, payload);
    }
}

void Client::handleSRPLoginResponse(const QByteArray &data)
{
    // 1. 打印根节点
    qDebug().noquote() << "🔐 [SRP 响应] 处理服务端挑战 (0x53)";

    if (data.size() < 68) {
        qDebug().noquote() << QString("   └─ ❌ [错误] 包长度不足: %1").arg(data.size());
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
            qDebug().noquote() << "   ├─ ⚠️ 状态: 账号不存在 (Code 0x01)";
            qDebug().noquote() << "   └─ 🔄 动作: 触发自动注册流程 -> createAccount()";
            createAccount();
        } else if (status == 0x05) {
            qDebug().noquote() << "   └─ ❌ 状态: 密码错误 (Code 0x05)";
            LOG_ERROR("密码错误");
        } else {
            qDebug().noquote() << QString("   └─ ❌ 状态: 登录拒绝 (Code 0x%1)").arg(QString::number(status, 16));
            LOG_ERROR("登录拒绝: 0x" + QString::number(status, 16));
        }
        return;
    }

    // 3. 计算分支
    if (!m_srp) return;

    qDebug().noquote() << "   ├─ ✅ 状态: 握手继续 (服务端已接受 A)";
    qDebug().noquote() << "   ├─ 📥 参数: 接收 Salt & 服务端公钥 (B)";

    // SRP 数学计算
    m_srp->setSalt(BigInt((const unsigned char*)saltBytes.constData(), 32, 4, false));
    BigInt B_val((const unsigned char*)serverKeyBytes.constData(), 32, 1, false);
    BigInt K = m_srp->getHashedClientSecret(B_val);
    BigInt A = m_srp->getClientSessionPublicKey();
    BigInt M1 = m_srp->getClientPasswordProof(A, B_val, K);
    QByteArray proofBytes = M1.toByteArray(20, 1, false);

    qDebug().noquote() << "   ├─ 🧮 计算: 派生 SessionKey (K) -> 生成证明 (M1)";

    // 构造响应
    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData(proofBytes.constData(), 20);
    out.writeRawData(QByteArray(20, 0).data(), 20); // M2 placeholder/Salt2

    // 4. 闭环日志
    qDebug().noquote() << "   └─ 🚀 动作: 发送 M1 证明 (0x54) -> 等待最终结果";
    sendPacket(SID_AUTH_ACCOUNTLOGONPROOF, response);
}

void Client::createAccount()
{
    // 1. 打印根节点
    qDebug().noquote() << "📝 [账号注册] 发起注册请求 (0x52)";

    if (m_user.isEmpty() || m_pass.isEmpty()) {
        qDebug().noquote() << "   └─ ❌ [错误] 用户名或密码为空";
        return;
    }

    qDebug().noquote() << QString("   ├─ 👤 用户: %1").arg(m_user);

    // 生成随机 Salt 和 Verifier (模拟)
    QByteArray s_bytes(32, 0);
    for (int i = 0; i < 32; ++i) s_bytes[i] = (char)(QRandomGenerator::global()->generate() & 0xFF);

    QByteArray v_bytes(32, 0); // 明文密码模式 (PVPGN常见配置)
    QByteArray passRaw = m_pass.toLatin1();
    memcpy(v_bytes.data(), passRaw.constData(), qMin(passRaw.size(), 32));

    qDebug().noquote() << "   ├─ 🎲 生成: Random Salt (32 bytes) & Password Hash";

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData(s_bytes.constData(), 32);
    out.writeRawData(v_bytes.constData(), 32);
    out.writeRawData(m_user.toLower().trimmed().toLatin1().constData(), m_user.length());
    out << (quint8)0;

    // 2. 闭环日志
    qDebug().noquote() << "   └─ 🚀 动作: 数据打包发送 -> 等待结果";
    sendPacket(SID_AUTH_ACCOUNTCREATE, payload);
}

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
// 6. 聊天与频道管理
// =========================================================

void Client::enterChat() {
    // 树状日志
    qDebug().noquote() << "🚪 [进入聊天] 发送 SID_ENTERCHAT (0x0A)";
    qDebug().noquote() << "   └─ 🚀 动作: 请求进入聊天室环境";

    sendPacket(SID_ENTERCHAT, QByteArray(2, '\0'));
}

void Client::queryChannelList() {
    // 树状日志
    qDebug().noquote() << "📜 [频道列表] 发起查询请求 (0x0B)";
    qDebug().noquote() << "   └─ 🚀 动作: 等待服务器返回列表...";

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out << (quint32)0;
    sendPacket(SID_GETCHANNELLIST, payload);
}

void Client::joinChannel(const QString &channelName) {
    if (channelName.isEmpty()) return;

    // 树状日志
    qDebug().noquote() << QString("💬 [加入频道] 请求加入: %1").arg(channelName);
    qDebug().noquote() << "   ├─ 🚩 标志: First Join (0x01)";
    qDebug().noquote() << "   └─ 🚀 动作: 发送 SID_JOINCHANNEL (0x0C)";

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
    qDebug().noquote() << QString("🎲 [随机频道] Bot-%1 正在选择...").arg(m_user);

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
    qDebug().noquote() << QString("   ├─ 📚 候选池来源: %1").arg(source);
    qDebug().noquote() << QString("   ├─ 📊 候选数量: %1 个").arg(channels.size());

    // 4. 随机选择并执行
    if (!channels.isEmpty()) {
        int index = QRandomGenerator::global()->bounded(channels.size());
        QString targetChannel = channels.at(index).trimmed();

        qDebug().noquote() << QString("   └─ 🎯 命中目标: %1 -> 执行 joinChannel").arg(targetChannel);
        joinChannel(targetChannel);
    } else {
        qDebug().noquote() << "   └─ ⚠️ [警告] 候选列表为空，无法加入";
    }
}

// =========================================================
// 7. 房间主机逻辑
// =========================================================

void Client::stopAdv() {
    qDebug().noquote() << "🛑 [停止广播] 发送 SID_STOPADV (0x02)";
    sendPacket(SID_STOPADV, QByteArray());
}

void Client::cancelGame() {
    // 1. 打印根节点
    qDebug().noquote() << "❌ [重置游戏] 执行网络层清理...";

    // 2. 停止广播
    stopAdv();

    // 3. 进入大厅
    enterChat();

    // 4. 进入频道
    joinRandomChannel();

    // 5. 断开所有玩家连接
    int playerCount = m_playerSockets.size();
    if (playerCount > 0) {
        qDebug().noquote() << QString("   ├─ 🔌 断开连接: 清理 %1 名玩家 Socket").arg(playerCount);
        for (auto socket : qAsConst(m_playerSockets)) {
            socket->disconnectFromHost();
            socket->deleteLater();
        }
    } else {
        qDebug().noquote() << "   ├─ ℹ️ 连接状态: 无活跃玩家";
    }

    // 清理容器
    m_playerSockets.clear();
    m_playerBuffers.clear();
    m_players.clear();

    // 6. 重置槽位
    initSlots();
    qDebug().noquote() << "   ├─ 🧹 内存清理: 槽位重置 & 容器清空";

    // 7. 重置标志位
    m_gameStarted = false;
    m_hostCounter++;

    // 8. 停止 Ping 循环
    if (m_pingTimer->isActive()) {
        m_pingTimer->stop();
        qDebug().noquote() << "   └─ 🛑 计时器: Ping 循环已停止";
    } else {
        qDebug().noquote() << "   └─ ✅ 状态: 就绪 (Idle)";
    }
    emit gameCanceled();
}

void Client::createGame(const QString &gameName, const QString &password, ProviderVersion providerVersion, ComboGameType comboGameType, SubGameType subGameType, LadderType ladderType, CommandSource commandSource)
{
    // 1. 打印根节点
    QString sourceStr = (commandSource == From_Server) ? "Server" : "Client";
    qDebug().noquote() << QString("🚀 [创建房间] 发起请求: [%1]").arg(gameName);
    qDebug().noquote() << QString("   ├─ 🎮 来源: %1 | 密码: %2").arg(sourceStr, password.isEmpty() ? "None" : "***");

    // 初始化槽位
    initSlots();

    // 2. UDP 端口汇报检查
    if (m_udpSocket->state() == QAbstractSocket::BoundState) {
        quint16 localPort = m_udpSocket->localPort();
        QByteArray portPayload;
        QDataStream portOut(&portPayload, QIODevice::WriteOnly);
        portOut.setByteOrder(QDataStream::LittleEndian);
        portOut << (quint16)localPort;
        sendPacket(SID_NETGAMEPORT, portPayload);

        qDebug().noquote() << QString("   ├─ 🔧 端口汇报: UDP %1 -> SID_NETGAMEPORT").arg(localPort);
    } else {
        qDebug().noquote() << "   └─ ❌ [严重错误] UDP 未绑定，无法创建游戏";
        return;
    }

    // 3. 地图加载
    QString mapName = QFileInfo(m_dota683dPath).fileName();
    if (!m_war3Map.load(m_dota683dPath)) {
        qDebug().noquote() << QString("   └─ ❌ [严重错误] 地图加载失败: %1").arg(m_dota683dPath);
        return;
    }

    QByteArray encodedData = m_war3Map.getEncodedStatString(m_botDisplayName);
    if (encodedData.isEmpty()) {
        qDebug().noquote() << "   └─ ❌ [严重错误] StatString 生成失败";
        return;
    }
    qDebug().noquote() << QString("   ├─ 🗺️ 地图加载: %1 (StatString Ready)").arg(mapName);

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
        m_pingTimer->start(5000);
        qDebug().noquote() << "   └─ 💓 动作: 发送请求(0x1C) + 启动 Ping 循环 (5s)";
    } else {
        qDebug().noquote() << "   └─ 📤 动作: 发送请求(0x1C) (Ping 循环运行中)";
    }
}

// =========================================================
// 8. 游戏数据处理
// =========================================================

void Client::initSlots(quint8 maxPlayers)
{
    qDebug().noquote() << QString("🧹 [槽位重置] 初始化房间槽位 (Max: %1)").arg(maxPlayers);

    // 1. 清空旧数据
    m_slots.clear();
    m_slots.resize(maxPlayers);

    // 2. 清空连接
    if (!m_playerSockets.isEmpty()) {
        qDebug().noquote() << QString("   ├─ 🔌 断开连接: %1 个残留 Socket").arg(m_playerSockets.size());
        for (auto socket : qAsConst(m_playerSockets)) {
            if (socket->state() == QAbstractSocket::ConnectedState) {
                socket->disconnectFromHost();
            }
        }
    }
    m_playerSockets.clear();
    m_playerBuffers.clear();

    // 3. 初始化槽位状态
    for (quint8 i = 0; i < maxPlayers; ++i) {
        m_slots[i] = GameSlot();
        m_slots[i].downloadStatus = NotStarted;
        m_slots[i].computer = Human;
        m_slots[i].color = i + 1;

        // Bot 占据最后一个槽位
        if (i == 11) {
            m_slots[i].pid = 1;
            m_slots[i].downloadStatus = Completed;
            m_slots[i].slotStatus = Occupied;
            m_slots[i].computer = Human;
            m_slots[i].team = (quint8)SlotTeam::Observer;
            m_slots[i].race = (quint8)SlotRace::Observer;
            continue;
        }

        // --- 正常玩家槽位 ---
        m_slots[i].pid = 0;
        m_slots[i].slotStatus = Open;

        if (i < 5) { // Sentinel
            m_slots[i].team = (quint8)SlotTeam::Sentinel;
            m_slots[i].race = (quint8)SlotRace::NightElf;
        } else if (i < 10) { // Scourge
            m_slots[i].team = (quint8)SlotTeam::Scourge;
            m_slots[i].race = (quint8)SlotRace::Undead;
        } else { // Slot 10 (Observer)
            m_slots[i].team = (quint8)SlotTeam::Observer;
            m_slots[i].race = (quint8)SlotRace::Observer;
        }
    }

    qDebug().noquote() << "   └─ ✨ 状态: 初始化完成 (Bot -> Slot 11)";
}

QByteArray Client::serializeSlotData() {
    QByteArray data;
    QDataStream ds(&data, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds << (quint8)m_slots.size(); // Num Slots

    for (const auto& slot : qAsConst(m_slots)) {
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
        qDebug().noquote() << QString("📦 [构建包] 聊天/控制 (0x0F)");
        qDebug().noquote() << QString("   ├─ 🎯 目标: %1 -> %2").arg(senderPid).arg(toPid);
        qDebug().noquote() << QString("   ├─ 🚩 类型: 0x%1 (Extra: %2)").arg(QString::number((int)flag, 16)).arg(extraData);
        qDebug().noquote() << QString("   └─ 📝 数据: %1").arg(hexPreview);
    }

    return packet;
}

QByteArray Client::createW3GSSlotInfoJoinPacket(quint8 playerID, const QHostAddress& externalIp, quint16 localPort)
{
    // 这个包很重要，保留详细日志
    qDebug().noquote() << "📦 [构建包] W3GS_SLOTINFOJOIN (0x04)";

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

    qDebug().noquote() << QString("   ├─ 📏 尺寸: 总长 %1 / 块长 %2").arg(totalSize).arg(slotBlockSize);
    qDebug().noquote() << QString("   └─ 👤 专属: PID %1 (IP: %2)").arg(playerID).arg(externalIp.toString());

    // 校验逻辑保持不变，但换成 tree log
    if (packet.size() > 6 + slotBlockSize) {
        int pidOffset = 6 + slotBlockSize;
        quint8 pidInPacket = (quint8)packet.at(pidOffset);
        if (pidInPacket != playerID) {
            qDebug().noquote() << QString("   └─ ❌ [严重警告] PID 偏移校验失败! (读到: %1)").arg(pidInPacket);
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

QByteArray Client::createW3GSPlayerLeftPacket(quint8 pid, quint32 reason)
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

QByteArray Client::createW3GSMapCheckPacket()
{
    qDebug().noquote() << "📦 [构建包] W3GS_MAPCHECK (0x3D)";

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
        qDebug().noquote() << QString("   ├─ ⚠️ SHA1 长度异常 (%1) -> 补零").arg(sha1.size());
        sha1.resize(20);
    }
    out.writeRawData(sha1.data(), 20);

    quint16 totalSize = (quint16)packet.size();
    QDataStream lenStream(&packet, QIODevice::ReadWrite);
    lenStream.setByteOrder(QDataStream::LittleEndian);
    lenStream.skipRawData(2);
    lenStream << totalSize;

    QString sha1Hex = sha1.toHex().toUpper();
    qDebug().noquote() << QString("   ├─ 📊 参数: Size=%1 | CRC=0x%2").arg(fileSize).arg(QString::number(fileCRC, 16).toUpper());
    qDebug().noquote() << QString("   └─ 🔐 SHA1: %1...").arg(sha1Hex.left(20));

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
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // Header: F7 43 [Len]
    out << (quint8)0xF7 << (quint8)0x43 << (quint16)0;

    out << (quint8)toPid;
    out << (quint8)fromPid;
    out << (quint32)1;      // Unknown
    out << (quint32)offset; // Current Offset
    out << (quint32)0;      // CRC Placeholder

    // Data
    out.writeRawData(chunkData.data(), chunkData.size());

    // Length
    quint16 totalSize = (quint16)packet.size();
    QDataStream ds(&packet, QIODevice::ReadWrite);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.skipRawData(2);
    ds << totalSize;

    // CRC Calculation (Standard Types)
    unsigned long crc = m_war3Map.calcCrc32(chunkData.constData(), chunkData.size());

    // Fill CRC (Offset 14)
    ds.device()->seek(14);
    ds << (quint32)crc;

    return packet;
}

void Client::broadcastChatMessage(const MultiLangMsg& msg, quint8 excludePid)
{
    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        quint8 pid = it.key();

        // 排除 PID 1 (Host) 和 指定排除的 PID
        if (pid == excludePid || pid == 1) continue;

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
            1,    // From Host
            pid,  // To Target Player
            ChatFlag::Message
            );

        socket->write(chatPacket);
        socket->flush();
    }
}

void Client::broadcastPacket(const QByteArray &packet, quint8 excludePid)
{
    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        const PlayerData &playerData = it.value();
        // 如果 PID 匹配排除项，或者 Socket 无效，则跳过
        if (excludePid != 0 && playerData.pid == excludePid) continue;
        if (!playerData.socket || playerData.socket->state() != QAbstractSocket::ConnectedState) continue;

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
    qDebug().noquote() << QString("📢 [广播状态] 槽位更新 (0x09)%1").arg(excludeStr);
}

// =========================================================
// 9. 槽位辅助函数
// =========================================================

int Client::getTotalSlots() const
{
    if (m_slots.isEmpty()) return 10;
    return m_slots.size();
}

int Client::getOccupiedSlots() const
{
    if (m_slots.isEmpty()) return 1;

    int count = 0;
    for (const auto &slot : m_slots) {
        // 统计状态为 Occupied 的槽位
        if (slot.slotStatus == Occupied) {
            count++;
        }
    }
    return count;
}

QString Client::getSlotInfoString() const
{
    // 格式化为 (占用/总数)
    return QString("(%1/%2)").arg(getOccupiedSlots()).arg(getTotalSlots());
}

// =========================================================
// 10. 玩家辅助函数
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

// =========================================================
// 11. 辅助工具函数
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
    if (m_players.isEmpty()) return;

    QByteArray pingPacket = createW3GSPingFromHostPacket();

    bool shouldSendChat = false;

    MultiLangMsg waitMsg;

    m_chatIntervalCounter++;
    if (m_chatIntervalCounter >= 3) {
        int realPlayerCount = 0;
        for(auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.key() != 1) realPlayerCount++;
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

            QByteArray chatPacket = createW3GSChatFromHostPacket(finalBytes, 1, pid, ChatFlag::Message);
            socket->write(chatPacket);
        }

        socket->flush();
    }
}

void Client::checkPlayerTimeout()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 定义超时阈值
    const qint64 TIMEOUT_CONNECTION = 60000;  // 60秒无心跳
    const qint64 TIMEOUT_DOWNLOAD   = 120000; // 120秒下载无进度

    auto it = m_players.begin();
    while (it != m_players.end()) {
        quint8 pid = it.key();
        PlayerData &playerData = it.value();

        // 跳过主机 (PID 1)
        if (pid == 1) {
            ++it;
            continue;
        }

        bool kick = false;
        QString reasonCategory = "";
        QString timeDetails = "";

        qint64 silenceTime = now - playerData.lastResponseTime;
        qint64 downloadSilenceTime = now - playerData.lastDownloadTime;

        // 1. 检查连接超时 (常规心跳)
        if (silenceTime > TIMEOUT_CONNECTION) {
            kick = true;
            reasonCategory = "连接超时 (Connection Timeout)";
            timeDetails = QString("%1 秒无响应 (阈值: %2)").arg(silenceTime / 1000).arg(TIMEOUT_CONNECTION / 1000);
        }
        // 2. 检查下载超时 (仅针对正在下载的玩家)
        else if (playerData.isDownloading && downloadSilenceTime > TIMEOUT_DOWNLOAD) {
            kick = true;
            reasonCategory = "下载卡死 (Download Stalled)";
            timeDetails = QString("%1 秒无进度 (阈值: %2)").arg(downloadSilenceTime / 1000).arg(TIMEOUT_DOWNLOAD / 1000);
        }

        if (kick) {
            // 打印树状日志
            qDebug().noquote() << QString("👢 [超时踢人] 移除玩家: %1 (PID: %2)").arg(playerData.name).arg(pid);
            qDebug().noquote() << QString("   ├─ 📝 类型: %1").arg(reasonCategory);
            qDebug().noquote() << QString("   ├─ ⏱️ 统计: %1").arg(timeDetails);
            qDebug().noquote() << "   └─ 🔌 动作: 强制断开 TCP 连接";

            if (playerData.socket) {
                // 这会触发 onDisconnected 信号，由槽函数处理 Map 移除和广播
                playerData.socket->disconnectFromHost();
            }

            // 继续检查下一个，不要在这里 erase，交给 onDisconnected 处理
            ++it;
        } else {
            ++it;
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

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

#include <zlib.h>

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
    initSlots();

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
        LOG_ERROR(QString("战网连接错误: %1").arg(m_tcpSocket->errorString()));
    });
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &Client::onUdpReadyRead);

    // 初始化 UDP
    if (!bindToRandomPort()) {
        LOG_ERROR("UDP 绑定随机端口失败");
    }

    // 初始化路径
    QString appDir = QCoreApplication::applicationDirPath();
    QDir dir(appDir);

    if (dir.cd("war3files")) {
        m_war3ExePath = dir.absoluteFilePath("War3.exe");
        m_gameDllPath = dir.absoluteFilePath("Game.dll");
        m_stormDllPath = dir.absoluteFilePath("Storm.dll");
        m_dota683dPath = dir.absoluteFilePath("maps/DotA v6.83d.w3x");
    } else {
        LOG_WARNING("找不到 war3files 目录，尝试直接读取当前目录下的 War3.exe");
        dir.setPath(appDir);
        m_war3ExePath = dir.absoluteFilePath("War3.exe");
        m_gameDllPath = dir.absoluteFilePath("Game.dll");
        m_stormDllPath = dir.absoluteFilePath("Storm.dll");
        m_dota683dPath = dir.absoluteFilePath("maps/DotA v6.83d.w3x");
    }

    LOG_INFO(QString("War3 路径: %1").arg(m_war3ExePath));
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

    LOG_INFO(QString("设置凭据: 用户[%1] 密码[%2] 协议[%3]").arg(m_user, m_pass, protoName));
}

void Client::connectToHost(const QString &address, quint16 port)
{
    m_serverAddr = address;
    m_serverPort = port;
    LOG_INFO(QString("正在建立 TCP 连接至战网: %1:%2").arg(address).arg(port));
    m_tcpSocket->connectToHost(address, port);
}

void Client::disconnectFromHost() {
    m_tcpSocket->disconnectFromHost();
}

bool Client::isConnected() const {
    return m_tcpSocket->state() == QAbstractSocket::ConnectedState;
}

void Client::onDisconnected() {
    LOG_WARNING("🔌 战网连接断开");
    emit disconnected();
}

void Client::onConnected()
{
    LOG_INFO("✅ TCP 链路已建立，发送协议握手字节...");
    char protocolByte = 1;
    m_tcpSocket->write(&protocolByte, 1);
    sendAuthInfo();
}

void Client::onNewConnection()
{
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        LOG_INFO(QString("🎮 新玩家连接! IP: %1").arg(socket->peerAddress().toString()));

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
    if (!m_players.contains(toPid)) {
        LOG_ERROR(QString("❌ sendNextMapPart: 找不到 PID %1").arg(toPid));
        return;
    }

    // 更新下载活跃时间
    m_players[toPid].lastDownloadTime = QDateTime::currentMSecsSinceEpoch();

    PlayerData &playerData = m_players[toPid];

    // [检查点 1] 状态检查
    if (!playerData.isDownloading) {
        LOG_WARNING(QString("⚠️ 玩家 [%1] 未处于下载状态，忽略发送请求").arg(playerData.name));
        return;
    }

    // 获取原始地图数据
    const QByteArray &mapData = m_war3Map.getMapRawData(); // 确保 War3Map 类里有这个方法
    quint32 totalSize = (quint32)mapData.size();

    // [检查点 2] 数据有效性
    if (totalSize == 0) {
        LOG_ERROR("❌ 严重错误: 内存中没有地图数据！");
        return;
    }

    // 检查是否完成
    if (playerData.downloadOffset >= totalSize) {
        LOG_INFO(QString("✅ 玩家 [%1] 地图下载完成 (Offset: %2 / %3)").arg(playerData.name).arg(playerData.downloadOffset).arg(totalSize));
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

    // 计算分片
    quint32 chunkSize = 1442; // 标准 MTU 安全大小
    if (playerData.downloadOffset + chunkSize > totalSize) {
        chunkSize = totalSize - playerData.downloadOffset;
    }

    QByteArray chunk = mapData.mid(playerData.downloadOffset, chunkSize);

    // 构造包 (0x43)
    // FromPID = 1 (Host)
    QByteArray packet = createW3GSMapPartPacket(toPid, fromPid, playerData.downloadOffset, chunk);

    qint64 written = playerData.socket->write(packet);
    playerData.socket->flush();

    if (written > 0) {
        // [日志] 仅每传输 1MB 打印一次，防止日志爆炸
        if (playerData.downloadOffset == 0 || playerData.downloadOffset % (1024 * 1024) < 2000) {
            int percent = (int)((double)playerData.downloadOffset / totalSize * 100);
            LOG_INFO(QString("📤 发送分片: Offset %1 (Size %2) -> [%3] (%4%)")
                         .arg(playerData.downloadOffset).arg(chunkSize).arg(playerData.name).arg(percent));
        }

        // 更新偏移量
        playerData.downloadOffset += chunkSize;
    } else {
        LOG_ERROR(QString("❌ Socket 写入失败: %1").arg(playerData.socket->errorString()));
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
    // 将日志级别降为 DEBUG，避免正常的 PING 包刷屏 INFO 日志
    if (id != SID_PING) {
        LOG_INFO(QString("📥 收到包 ID: 0x%1").arg(QString::number(id, 16)));
    }

    switch (id) {
    case SID_PING:
    {
        if (data.size() < 4) return; // 基础校验

        quint32 pingValue;
        QDataStream ds(data);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds >> pingValue;

        // 仅在 DEBUG 模式下打印，防止刷屏
        LOG_DEBUG(QString("💓 [心跳] 收到服务器 Ping: %1，已回应").arg(pingValue));

        // 核心逻辑：原样发回数据
        sendPacket(SID_PING, data);
    }
    break;

    case SID_ENTERCHAT:
        LOG_INFO("✅ 已成功进入聊天环境 (Unique Name Received)");
        queryChannelList();
        break;

    case SID_GETCHANNELLIST:
    {
        LOG_INFO("📦 收到频道列表包，正在解析...");
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

        if (m_channelList.isEmpty()) {
            LOG_WARNING("⚠️ 频道列表为空！尝试加入 'Waiting Players'");
            joinChannel("Waiting Players");
        } else {
            LOG_INFO(QString("📋 获取到 %1 个频道: %2").arg(m_channelList.size()).arg(m_channelList.join(", ")));
            joinChannel(m_channelList.first());
        }
    }
    break;

    case SID_CHATEVENT:
    {
        // 1. 基础校验
        if (data.size() < 24) return;

        // 2. 解析固定头部
        QDataStream in(data);
        in.setByteOrder(QDataStream::LittleEndian);
        quint32 eventId, flags, ping, ipAddress, accountNum, regAuthority;
        in >> eventId >> flags >> ping >> ipAddress >> accountNum >> regAuthority;

        // 3. 解析动态字符串
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

        // 4. 指令捕获记录
        if (text.startsWith("/")) {
            LOG_INFO(QString("⚡ [指令捕获] EID:0x%1 | 用户:%2 | 内容:%3")
                         .arg(QString::number(eventId, 16), username, text));
        }

        // 5. 日志分流
        switch (eventId) {
        case 0x01: LOG_INFO(QString("👤 [用户展示] %1 (Ping: %2)").arg(username).arg(ping)); break;
        case 0x02: LOG_INFO(QString("➡️ [加入频道] %1").arg(username)); break;
        case 0x03: LOG_INFO(QString("⬅️ [离开频道] %1").arg(username)); break;
        case 0x04: LOG_INFO(QString("📩 [来自私聊] %1: %2").arg(username, text)); break;
        case 0x05: LOG_INFO(QString("💬 [频道发言] %1: %2").arg(username, text)); break;
        case 0x06: LOG_INFO(QString("📢 [系统广播] %1").arg(text)); break;
        case 0x07: LOG_INFO(QString("🏠 [进入频道] %1").arg(text)); break;
        case 0x09: LOG_INFO(QString("🔧 [状态更新] %1 (Flags: %2)").arg(username, QString::number(flags, 16))); break;
        case 0x0A: LOG_INFO(QString("📤 [发送私聊] 你 -> %1: %2").arg(username, text)); break;
        case 0x12: LOG_INFO(QString("ℹ️ [INFO] %1").arg(text)); break;
        case 0x13: LOG_ERROR(QString("❌ [ERROR] %1").arg(text)); break;
        case 0x17: LOG_INFO(QString("✨ [表情] %1 %2").arg(username, text)); break;
        default:   break;
        }
    }
    break;

    case SID_LOGONRESPONSE: // 0x29
    {
        if (data.size() < 4) return;
        quint32 result;
        QDataStream ds(data);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds >> result;
        if (result == 1) {
            LOG_INFO("🎉 登录成功 (0x29)！");
            emit authenticated();
        } else {
            LOG_ERROR(QString("❌ 登录失败 (0x29): 0x%1").arg(QString::number(result, 16)));
        }
    }
    break;

    case SID_LOGONRESPONSE2: // 0x3A
    {
        if (data.size() < 4) return;
        quint32 result;
        QDataStream ds(data);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds >> result;
        if (result == 0) {
            LOG_INFO("🎉 登录成功 (0x3A)！");
            emit authenticated();
        } else {
            LOG_ERROR(QString("❌ 登录失败 (0x3A): 0x%1").arg(QString::number(result, 16)));
        }
    }
    break;

    case SID_AUTH_INFO:
    case SID_AUTH_CHECK:
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
            LOG_INFO("🎉 账号注册成功！自动登录中...");
            emit accountCreated();
            sendLoginRequest(Protocol_SRP_0x53);
        } else if (status == 0x04) {
            LOG_WARNING("⚠️ 账号已存在，尝试直接登录...");
            sendLoginRequest(Protocol_SRP_0x53);
        } else {
            LOG_ERROR(QString("❌ 注册失败: 0x%1").arg(QString::number(status, 16)));
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
        // 0x00 = OK, 0x0E = Email注册相关的OK
        if (status == 0 || status == 0x0E) {
            LOG_INFO("🎉 登录成功 (SRP)！");
            emit authenticated();
        } else {
            QString reason = "未知错误";
            if (status == 0x02) reason = "密码错误";
            else if (status == 0x0D) reason = "账号不存在";

            LOG_ERROR(QString("❌ 登录失败 (SRP): 0x%1 (%2)").arg(QString::number(status, 16), reason));
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
            LOG_INFO("✅ 房间创建成功！(广播已启动)");
            emit gameCreated(From_Client);
        } else {
            QString errStr;
            switch (status) {
            case GameCreate_NameExists:      errStr = "房间名已存在"; break;
            case GameCreate_TypeUnavailable: errStr = "游戏类型不可用"; break;
            case GameCreate_Error:           errStr = "通用创建错误"; break;
            default:                         errStr = QString("错误码 0x%1").arg(QString::number(status, 16)); break;
            }
            LOG_ERROR(QString("❌ 房间创建失败: %1").arg(errStr));
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
    switch (id) {
    case W3GS_REQJOIN: // W3GS_REQJOIN
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
            in >> clientHostCounter;
            in >> clientEntryKey;
            in >> clientUnknown8;
            in >> clientListenPort;
            in >> clientPeerKey;
            QByteArray nameBytes;
            char c;
            while (!in.atEnd()) {
                in.readRawData(&c, 1);
                if (c == 0) break;
                nameBytes.append(c);
            }
            clientPlayerName = QString::fromUtf8(nameBytes);
            if (!in.atEnd()) {
                in >> clientUnknown32;
                in >> clientInternalPort;
                in >> clientInternalIP;
            }
        } else {
            LOG_ERROR(QString("❌ W3GS_REQJOIN 包长度不足: %1").arg(payload.size()));
            return;
        }

        // 恢复你原始的详细日志输出
        LOG_INFO("------------------------------------------------");
        LOG_INFO("📥 [0x1E] 客户端加入请求解析结果:");
        LOG_INFO(QString("(UINT32) Host Counter: %1").arg(clientHostCounter));
        LOG_INFO(QString("(UINT32) Entry Key   : 0x%1").arg(QString::number(clientEntryKey, 16).toUpper()));
        LOG_INFO(QString("(UINT8)  Unknown     : %1").arg(clientUnknown8));
        LOG_INFO(QString("(UINT16) Listen Port : %1").arg(clientListenPort));
        LOG_INFO(QString("(UINT32) Peer Key    : 0x%1").arg(QString::number(clientPeerKey, 16).toUpper()));
        LOG_INFO(QString("(STRING) Player name : %1").arg(clientPlayerName));
        LOG_INFO(QString("(UINT32) Unknown     : %1").arg(clientUnknown32));
        LOG_INFO(QString("(UINT16) Intrnl Port : %1").arg(clientInternalPort));
        QHostAddress iAddr(qToBigEndian(clientInternalIP));
        LOG_INFO(QString("(UINT32) Intrnl IP   : %1 (%2)").arg(clientInternalIP).arg(iAddr.toString()));
        LOG_INFO("------------------------------------------------");

        // 2. 槽位与PID分配逻辑
        int slotIndex = -1;
        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].slotStatus == Open) {
                slotIndex = i;
                break;
            }
        }

        if (slotIndex == -1) {
            LOG_WARNING("⚠️ 房间已满，拒绝加入");
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

        // 更新内存中的槽位状态
        m_slots[slotIndex].pid = hostId;
        m_slots[slotIndex].slotStatus = Occupied;
        m_slots[slotIndex].downloadStatus = NotStarted;
        m_slots[slotIndex].computer = Human;

        qint64 now = QDateTime::currentMSecsSinceEpoch();

        // 保存玩家数据到列表
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

        m_players.insert(hostId, playerData);

        LOG_INFO(QString("💾 已注册玩家: [%1] PID: %2").arg(clientPlayerName).arg(hostId));

        // 3. 构建握手响应包序列 (发送给新玩家)
        QByteArray finalPacket;
        QHostAddress hostIp = socket->peerAddress();
        quint16 hostPort = m_udpSocket->localPort();

        // Step A: 发送 0x04 (SlotInfoJoin)
        finalPacket.append(createW3GSSlotInfoJoinPacket(hostId, hostIp, hostPort));

        // Step B: 发送 Host 信息 (PID 1)
        finalPacket.append(createPlayerInfoPacket(
            1, m_user, QHostAddress("0.0.0.0"), 0, QHostAddress("0.0.0.0"), 0));

        // Step C: 发送已存在的其他老玩家信息给新玩家
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            const PlayerData &p = it.value();
            if (p.pid == hostId || p.pid == 1) continue; // 跳过新人自己和房主
            finalPacket.append(createPlayerInfoPacket(p.pid, p.name, p.extIp, p.extPort, p.intIp, p.intPort));
        }

        // Step D: 发送地图校验 (0x3D)
        finalPacket.append(createW3GSMapCheckPacket());

        // Step E: 发送槽位信息 (0x09)
        finalPacket.append(createW3GSSlotInfoPacket());

        // 执行物理发送
        socket->write(finalPacket);
        socket->flush();

        LOG_INFO(QString("✅ 加入成功: 发送握手序列 (0x04 -> 0x06 -> 0x3D -> 0x09) PID: %1").arg(hostId));

        // 4. 广播逻辑

        // A. 广播新玩家加入信息 (0x06) 给所有老玩家 (排除新人自己)
        QByteArray newPlayerInfoPacket = createPlayerInfoPacket(
            playerData.pid, playerData.name, playerData.extIp, playerData.extPort, playerData.intIp, playerData.intPort);
        broadcastPacket(newPlayerInfoPacket, hostId);

        // B. 广播最新槽位图 (0x09) 给房间所有人 (不排除任何人，确保所有人的 UI 刷新)
        broadcastSlotInfo();

        LOG_INFO("📢 已向老玩家同步新成员并广播 UI 刷新");
    }
    break;

    case W3GS_LEAVEREQ: // W3GS_LEAVEREQ
    {
        LOG_INFO(QString("👋 收到主动离开请求 (0x21) 来自: %1").arg(socket->peerAddress().toString()));
        socket->disconnectFromHost();
    }
    break;

        // 这里是进入房间的指令
    case 0x28: // W3GS_CHAT_TO_HOST
    {
        // 基础长度检查
        if (payload.size() < 7) {
            LOG_WARNING(QString("[0x28] 包长度不足: %1").arg(payload.size()));
            return;
        }

        QDataStream in(payload);
        in.setByteOrder(QDataStream::LittleEndian);

        quint8 numReceivers;
        in >> numReceivers;

        // 跳过接收者列表
        if (numReceivers > 0) {
            if (payload.size() < 7 + numReceivers) return;
            in.skipRawData(numReceivers);
        }

        quint8 fromPid, flag;
        quint32 extra;
        in >> fromPid >> flag >> extra;

        // 打印头部解析结果，检查是否错位
        LOG_INFO(QString("[0x28] Header: Recvs=%1, From=%2, Flag=0x%3, Extra=%4")
                     .arg(numReceivers).arg(fromPid).arg(QString::number(flag, 16)).arg(extra));

        int headerSize = 1 + numReceivers + 1 + 1 + 4; // Num + Recvs + From + Flag + Extra

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
            LOG_WARNING("[0x28] 收到消息但无法识别发送者 Socket");
            return;
        }

        // 提取消息
        if (payload.size() > headerSize) {
            QByteArray msgBytes = payload.mid(headerSize);
            if (msgBytes.endsWith('\0')) msgBytes.chop(1);

            // 解码
            QString msg = m_players[senderPid].codec->toUnicode(msgBytes);

            LOG_INFO(QString("💬 [%1]: %2").arg(senderName, msg));

            // === 核心逻辑：判断是否是指令 ===
            if (msg.startsWith("/")) {
                LOG_INFO(QString("🔧 检测到指令: [%1] 来自 [%2] (房主是: [%3])")
                             .arg(msg, senderName, m_host));

                if (m_command) {
                    LOG_INFO(QString("✅ 执行房主指令: %1").arg(msg));
                    m_command->process(senderPid, msg);
                } else {
                    LOG_ERROR("❌ Command 处理器未初始化！");
                }
            }

            // 转发聊天给其他人 (Bot 的基本功能)
            MultiLangMsg chatMsg;
            chatMsg.add("CN", QString("%1: %2").arg(senderName, msg));
            chatMsg.add("EN", QString("%1: %2").arg(senderName, msg));
            broadcastChatMessage(chatMsg, senderPid); // 排除发送者自己
        }
    }
    break;

    case 0x42: // W3GS_MAPSIZE
    {
        if (payload.size() < 9) return;

        QDataStream in(payload);
        in.setByteOrder(QDataStream::LittleEndian);
        quint32 unknown; quint8 sizeFlag; quint32 clientMapSize;
        in >> unknown >> sizeFlag >> clientMapSize;

        LOG_INFO(QString("🗺️ [0x42] 收到玩家地图报告: %1 字节 (Flag: %2)").arg(clientMapSize).arg(sizeFlag));

        quint8 currentPid = 0;
        QString playerName = "Unknown";

        // 查找玩家
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

        bool slotUpdated = false;

        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].pid == currentPid) {
                // 情况 A: 拥有地图
                if (clientMapSize == hostMapSize && sizeFlag == 1) {
                    if (m_slots[i].downloadStatus != Completed) {
                        m_slots[i].downloadStatus = Completed;
                        slotUpdated = true;
                        playerData.isDownloading = false; // 确保关闭下载状态
                        LOG_INFO(QString("✅ 玩家 [%1] 地图校验通过").arg(playerName));
                    }
                }
                // 情况 B: 需要下载
                else {
                    // 检查是否需要下载
                    if (m_slots[i].downloadStatus != Downloading) {
                        // 1. 修改槽位状态
                        m_slots[i].downloadStatus = Downloading; // 0% Started

                        // 2. 修改玩家状态
                        playerData.isDownloading = true;
                        playerData.downloadOffset = 0;

                        // ====================================================
                        // 构建组合包 (Batch Packet)
                        // 顺序：3F(Start) -> 09(Slot Update) -> 43(First Chunk)
                        // ====================================================
                        QByteArray packetBatch;

                        // [包 1] 0x3F Start Download
                        packetBatch.append(createW3GSStartDownloadPacket(currentPid));

                        // [包 2] 0x09 Slot Info (广播新的下载状态 0%)
                        // 注意：虽然发给所有人的 0x09 都一样，但这里是专门发给下载者的
                        packetBatch.append(createW3GSSlotInfoPacket());

                        // [包 3] 0x43 Map Part (第一块数据，Offset 0)
                        // 获取第一块数据
                        const QByteArray &mapData = m_war3Map.getMapRawData();
                        int chunkSize = 1442;
                        if (mapData.size() < chunkSize) chunkSize = mapData.size();
                        QByteArray firstChunk = mapData.mid(0, chunkSize);

                        // 这里的 FromPid = 1 (主机), ToPid = currentPid
                        packetBatch.append(createW3GSMapPartPacket(currentPid, 1, 0, firstChunk));

                        // 一次性发送所有数据！
                        socket->write(packetBatch);
                        socket->flush();

                        // 更新偏移量，为下一次 0x44 ACK 做准备
                        playerData.downloadOffset += chunkSize;

                        LOG_INFO(QString("🚀 [加速传输] 已向 PID %1 发送 3F+09+43 (Header) 组合包").arg(currentPid));
                    }
                }
                break;
            }
        }

        // 广播或回发
        if (slotUpdated) {
            broadcastSlotInfo();
        } else {
            socket->write(createW3GSSlotInfoPacket());
            socket->flush();
        }
    }
    break;

    case 0x44: // W3GS_MAPPARTOK (客户端确认收到分片)
    {
        if (payload.size() < 9) return;
        QDataStream in(payload);
        in.setByteOrder(QDataStream::LittleEndian);
        quint8 fromPid, toPid; quint32 clientOffset;
        in >> fromPid >> toPid >> clientOffset; // Client Ack Offset

        // 找到玩家
        quint8 currentPid = 0;
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().socket == socket) { currentPid = it.key(); break; }
        }
        if (currentPid == 0) return;

        LOG_INFO(QString("📩 收到 ACK: PID %1 请求 Offset %2").arg(fromPid).arg(clientOffset));

        m_players[currentPid].lastResponseTime = QDateTime::currentMSecsSinceEpoch(); // 确认包也算心跳
        m_players[currentPid].lastDownloadTime = QDateTime::currentMSecsSinceEpoch(); // 更新下载活跃时间

        // 继续发送下一块
        sendNextMapPart(currentPid);
    }
    break;

    case 0x45: // W3GS_MAPPARTNOTOK
        LOG_ERROR("❌ 玩家报告地图分片 CRC 校验失败！下载可能损坏。");
        break;

    case 0x46: // W3GS_PONG_TO_HOST
    {
        // 结构: Header(4) + TickCount(4)
        if (payload.size() < 4) return;

        QDataStream in(payload);
        in.setByteOrder(QDataStream::LittleEndian);
        quint32 sentTick;
        in >> sentTick;

        // 查找玩家
        quint8 currentPid = 0;
        for (auto it = m_players.begin(); it != m_players.end(); ++it) {
            if (it.value().socket == socket) {
                currentPid = it.key();
                break;
            }
        }

        if (currentPid != 0) {
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            PlayerData &p = m_players[currentPid];

            // 1. 计算延迟
            // 注意：这里可能会有溢出回绕的问题，但在短时间会话中通常忽略
            p.currentLatency = (quint32)(now - sentTick);

            // 2. 更新最后活跃时间
            p.lastResponseTime = now;

            LOG_INFO(QString("💓 收到 Pong [PID:%1]: 延迟 %2 ms").arg(currentPid).arg(p.currentLatency));
        }
    }
    break;

    default:
        LOG_INFO(QString("❓ 未处理的 TCP 包 ID: 0x%1").arg(QString::number(id, 16)));
        break;
    }
}

void Client::onPlayerDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    quint8 pidToRemove = 0;
    QString nameToRemove = "Unknown";

    // 1. 查找玩家
    auto it = m_players.begin();
    while (it != m_players.end()) {
        if (it.value().socket == socket) {
            pidToRemove = it.key();
            nameToRemove = it.value().name;
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
        LOG_INFO(QString("🔌 玩家 [%1] (PID: %2) 断开连接").arg(nameToRemove).arg(pidToRemove));

        // 2. 释放槽位逻辑 (保持你原有的不变)
        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].pid == pidToRemove) {
                m_slots[i].pid = 0;
                m_slots[i].slotStatus = Open;
                m_slots[i].downloadStatus = NotStarted;
                break;
            }
        }

        // 3. 广播协议层离开包 (W3GS_PLAYERLEAVE_OTHERS 0x07)
        QByteArray leftPacket = createW3GSPlayerLeftPacket(pidToRemove, 0x0D); // 0x0D = Left Lobby
        broadcastPacket(leftPacket, pidToRemove); // 排除掉已经断开的那个人

        // 4. 广播聊天消息：玩家离开
        MultiLangMsg leaveMsg;
        leaveMsg.add("CN", QString("玩家 [%1] 离开了游戏。").arg(nameToRemove))
            .add("EN", QString("Player [%1] has left the game.").arg(nameToRemove));

        broadcastChatMessage(leaveMsg, pidToRemove);

        // 5. 广播槽位更新 (0x09)
        broadcastSlotInfo(pidToRemove);

        LOG_INFO("📢 已广播玩家离开消息及槽位更新");
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
    if (data.size() < 4) return;
    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);
    quint8 header, msgId;
    quint16 length;
    in >> header >> msgId >> length;

    if (header != 0xF7) return;

    QString hexStr = data.toHex().toUpper();
    for(int i = 2; i < hexStr.length(); i += 3) hexStr.insert(i, " ");
    LOG_INFO(QString("📨 [UDP] 收到 %1 字节来自 %2:%3 | 内容: %4")
                 .arg(data.size()).arg(sender.toString()).arg(senderPort).arg(hexStr));

    switch (msgId) {
    case W3GS_TEST: // 自定义测试包 ID
    {
        // 读取剩余的数据作为字符串打印出来
        QByteArray payload = data.mid(4);
        QString msg = QString::fromUtf8(payload);
        LOG_INFO(QString("🧪 [UDP] 收到测试包 (0x88) | 连通性测试成功！"));
        LOG_INFO(QString("   -> 附加消息: %1").arg(msg));

        // 可选：给发送者回一个包，证明死活 (这里简单回复一个 0x88)
        m_udpSocket->writeDatagram(data, sender, senderPort);
    }
    break;
    default:
        LOG_INFO(QString("❓ [UDP] 未处理包 ID: 0x%1").arg(QString::number(msgId, 16)));
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
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out << (quint32)0;
    out.writeRawData("68XI", 4); out.writeRawData("PX3W", 4);
    out << (quint32)26; out.writeRawData("SUne", 4);
    out << localIp << (quint32)0xFFFFFE20 << (quint32)2052 << (quint32)2052;
    out.writeRawData("CHN", 3); out.writeRawData("\0", 1);
    out.writeRawData("China", 5); out.writeRawData("\0", 1);
    sendPacket(SID_AUTH_INFO, payload);
}

void Client::handleAuthCheck(const QByteArray &data)
{
    LOG_INFO("🔍 解析 Auth Challenge (0x51)...");
    if (data.size() < 24) return;
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

    unsigned long checkSum = 0;
    if (QFile::exists(m_war3ExePath)) {
        checkRevisionFlat(formulaString.constData(), m_war3ExePath.toUtf8().constData(),
                          m_stormDllPath.toUtf8().constData(), m_gameDllPath.toUtf8().constData(),
                          mpqNumber, &checkSum);
    } else {
        LOG_ERROR("War3.exe 不存在，无法计算哈希");
        return;
    }
    LOG_INFO(QString("✅ 哈希: 0x%1").arg(QString::number(checkSum, 16).toUpper()));

    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    quint32 exeVersion = 0x011a0001;
    out << m_clientToken << exeVersion << (quint32)checkSum << (quint32)1 << (quint32)0;
    out << (quint32)20 << (quint32)18 << (quint32)0 << (quint32)0;
    out.writeRawData(QByteArray(20, 0).data(), 20);

    QFileInfo fileInfo(m_war3ExePath);
    if (fileInfo.exists()) {
        QString exeInfoString = QString("%1 %2 %3").arg(fileInfo.fileName(), fileInfo.lastModified().toString("MM/dd/yy HH:mm:ss"), QString::number(fileInfo.size()));
        out.writeRawData(exeInfoString.toUtf8().constData(), exeInfoString.length());
        out << (quint8)0;
    } else {
        out.writeRawData("War3.exe 03/18/11 02:00:00 471040\0", 38);
    }
    out.writeRawData(m_user.toUtf8().constData(), m_user.toUtf8().size());
    out << (quint8)0;
    sendPacket(SID_AUTH_CHECK, response);

    LOG_INFO("发起登录请求...");
    sendLoginRequest(m_loginProtocol);
}

void Client::sendLoginRequest(LoginProtocol protocol)
{
    if (protocol == Protocol_Old_0x29 || protocol == Protocol_Logon2_0x3A) {
        LOG_INFO(QString("发送 DoubleHash 登录 (0x%1)").arg(QString::number(protocol, 16)));
        QByteArray proof = calculateOldLogonProof(m_pass, m_clientToken, m_serverToken);
        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        out << m_clientToken << m_serverToken;
        out.writeRawData(proof.data(), 20);
        out.writeRawData(m_user.toUtf8().constData(), m_user.toUtf8().size());
        out << (quint8)0;
        sendPacket(protocol == Protocol_Old_0x29 ? SID_LOGONRESPONSE : SID_LOGONRESPONSE2, payload);
    }
    else if (protocol == Protocol_SRP_0x53) {
        LOG_INFO("发送 SRP 登录 (0x53) - 步骤1");
        if (m_srp) delete m_srp;
        m_srp = new BnetSRP3(m_user, m_pass);
        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        BigInt A = m_srp->getClientSessionPublicKey();
        QByteArray A_bytes = A.toByteArray(32, 1, false);
        out.writeRawData(A_bytes.constData(), 32);
        out.writeRawData(m_user.trimmed().toUtf8().constData(), m_user.length());
        out << (quint8)0;
        sendPacket(SID_AUTH_ACCOUNTLOGON, payload);
    }
}

void Client::handleSRPLoginResponse(const QByteArray &data)
{
    if (data.size() < 68) return;
    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);
    quint32 status;
    QByteArray saltBytes(32, 0);
    QByteArray serverKeyBytes(32, 0);
    in >> status;
    in.readRawData(saltBytes.data(), 32);
    in.readRawData(serverKeyBytes.data(), 32);

    if (status != 0) {
        if (status == 0x01) {
            LOG_WARNING(QString("⚠️ 账号 %1 不存在，自动发起注册...").arg(m_user));
            createAccount();
        } else if (status == 0x05) {
            LOG_ERROR("❌ 密码错误");
        } else {
            LOG_ERROR("❌ 登录拒绝: 0x" + QString::number(status, 16));
        }
        return;
    }

    if (!m_srp) return;
    m_srp->setSalt(BigInt((const unsigned char*)saltBytes.constData(), 32, 4, false));
    BigInt B_val((const unsigned char*)serverKeyBytes.constData(), 32, 1, false);
    BigInt K = m_srp->getHashedClientSecret(B_val);
    BigInt A = m_srp->getClientSessionPublicKey();
    BigInt M1 = m_srp->getClientPasswordProof(A, B_val, K);
    QByteArray proofBytes = M1.toByteArray(20, 1, false);

    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData(proofBytes.constData(), 20);
    out.writeRawData(QByteArray(20, 0).data(), 20);
    sendPacket(SID_AUTH_ACCOUNTLOGONPROOF, response);
}

void Client::createAccount()
{
    LOG_INFO("📝 发起账号注册 (0x52)...");
    if (m_user.isEmpty() || m_pass.isEmpty()) return;
    QByteArray s_bytes(32, 0);
    for (int i = 0; i < 32; ++i) s_bytes[i] = (char)(QRandomGenerator::global()->generate() & 0xFF);
    QByteArray v_bytes(32, 0); // 明文密码模式
    QByteArray passRaw = m_pass.toLatin1();
    memcpy(v_bytes.data(), passRaw.constData(), qMin(passRaw.size(), 32));

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData(s_bytes.constData(), 32);
    out.writeRawData(v_bytes.constData(), 32);
    out.writeRawData(m_user.toLower().trimmed().toLatin1().constData(), m_user.length());
    out << (quint8)0;
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
    sendPacket(SID_ENTERCHAT, QByteArray(2, '\0'));
}

void Client::queryChannelList() {
    LOG_INFO("📜 请求频道列表...");
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out << (quint32)0;
    sendPacket(SID_GETCHANNELLIST, payload);
}

void Client::joinChannel(const QString &channelName) {
    if (channelName.isEmpty()) return;
    LOG_INFO(QString("💬 加入频道: %1").arg(channelName));
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out << (quint32)0x01; // First Join
    out.writeRawData(channelName.toUtf8().constData(), channelName.toUtf8().size());
    out << (quint8)0;
    sendPacket(SID_JOINCHANNEL, payload);
}

// =========================================================
// 7. 房间主机逻辑
// =========================================================

void Client::stopAdv() {
    LOG_INFO("🛑 停止房间广播");
    sendPacket(SID_STOPADV, QByteArray());
}

void Client::cancelGame() {
    stopAdv();
    enterChat();
    LOG_INFO("❌ 取消游戏，返回大厅");
    if (m_pingTimer->isActive()) {
        m_pingTimer->stop();
        LOG_INFO("🛑 Ping 循环已停止");
    }
}

void Client::createGame(const QString &gameName, const QString &password, ProviderVersion providerVersion, ComboGameType comboGameType, SubGameType subGameType, LadderType ladderType, CommandSource commandSource)
{
    initSlots();

    LOG_INFO(QString("🚀 广播房间: [%1]").arg(gameName));

    if (m_udpSocket->state() == QAbstractSocket::BoundState) {
        QByteArray portPayload;
        QDataStream portOut(&portPayload, QIODevice::WriteOnly);
        portOut.setByteOrder(QDataStream::LittleEndian);
        quint16 localPort = m_udpSocket->localPort();
        portOut << (quint16)localPort;
        sendPacket(SID_NETGAMEPORT, portPayload);
        LOG_INFO(QString("🔧 已向服务器发送 UDP 端口通知: %1 (SID_NETGAMEPORT)").arg(localPort));
    } else {
        LOG_ERROR("❌ 严重错误: UDP 未绑定，无法告知服务器端口！");
        return;
    }

    if (!m_war3Map.load(m_dota683dPath)) {
        LOG_ERROR("❌ 地图加载失败");
        return;
    }
    QByteArray encodedData = m_war3Map.getEncodedStatString(m_user);
    if (encodedData.isEmpty()) {
        LOG_ERROR("❌ StatString 生成失败");
        return;
    }

    m_hostCounter++;
    m_randomSeed = (quint32)QRandomGenerator::global()->generate();

    QByteArray finalStatString;

    // 1. 写入空闲槽位标识
    finalStatString.append('9');

    // 2. 写入反转的 Host Counter Hex 字符串
    QString hexCounter = QString("%1").arg(m_hostCounter, 8, 16, QChar('0'));
    for(int i = hexCounter.length() - 1; i >= 0; i--) {
        finalStatString.append(hexCounter[i].toLatin1());
    }

    // 3. 追加编码后的地图数据
    finalStatString.append(encodedData);

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    quint32 state = 0x00000010;
    if (!password.isEmpty()) state = 0x00000011;

    out << state                        /*Game State*/
        << (quint32)0                   /*Game Elapsed Time*/
        << (quint16)comboGameType       /*Game Type*/
        << (quint16)subGameType         /*Sub Game Type*/
        << (quint32)providerVersion     /*Provider Version Constant*/
        << (quint32)ladderType;         /*Ladder Type*/

    out.writeRawData(gameName.toUtf8().constData(), gameName.toUtf8().size()); out << (quint8)0;
    out.writeRawData(password.toUtf8().constData(), password.toUtf8().size()); out << (quint8)0;
    out.writeRawData(finalStatString.constData(), finalStatString.size()); out << (quint8)0;

    sendPacket(SID_STARTADVEX3, payload);
    LOG_INFO(QString("📤 来自%1端的房间创建请求发送完毕").arg(commandSource == From_Server ? "服务" : "客户"));

    if (!m_pingTimer->isActive()) {
        m_pingTimer->start(5000);
        LOG_INFO("💓 Ping 循环已启动 (间隔: 5秒)");
    }
}

// =========================================================
// 8. 游戏数据处理
// =========================================================

void Client::initSlots(quint8 maxPlayers)
{
    // 1. 清空旧数据
    m_slots.clear();
    m_slots.resize(maxPlayers);

    // 2. 清空现有玩家连接
    for (auto socket : qAsConst(m_playerSockets)) {
        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->disconnectFromHost();
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
            m_slots[i].pid = 1;                             // Bot 的 PID 固定为 1
            m_slots[i].downloadStatus = Completed;          // Bot 肯定有图
            m_slots[i].slotStatus = Occupied;               // Occupied
            m_slots[i].computer = Human;                    // Human
            m_slots[i].team = (quint8)SlotTeam::Observer;   // 裁判
            m_slots[i].race = (quint8)SlotRace::Observer;
            continue;
        }

        // --- 正常玩家槽位 ---
        m_slots[i].pid = 0;                                 // 空
        m_slots[i].slotStatus = Open;                       // Open

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

    LOG_INFO("✨ 房间初始化完成：Bot 已隐藏至 Slot 11 (裁判位)");
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

    // 2. Num Receivers (数量)
    out << (quint8)1;

    // 3. Receiver PID (接收者 ID)
    out << (quint8)toPid;

    // 4. Sender PID (发送者 ID)
    out << (quint8)senderPid;

    // 5. Flag
    // 强制转为 quint8 写入流
    out << (quint8)flag;

    // 6. Extra Data
    switch (flag) {
    case Message:
        // 无额外数据
        break;
    case TeamChange:
    case ColorChange:
    case RaceChange:
    case HandicapChange:
        out << (quint8)(extraData & 0xFF);
        break;
    case Scope:
        out << (quint32)extraData;
        break;
    default: break;
    }

    // 7. Message String (直接写入传入的二进制数据)
    out.writeRawData(rawBytes.data(), rawBytes.length());
    out << (quint8)0; // Null Terminator

    // 8. 回填长度
    quint16 totalSize = (quint16)packet.size();
    QDataStream lenStream(&packet, QIODevice::ReadWrite);
    lenStream.setByteOrder(QDataStream::LittleEndian);
    lenStream.skipRawData(2);
    lenStream << totalSize;

    LOG_INFO(QString("📦 构建聊天包: To=%1 From=%2 Flag=%3 Len=%4 PayloadHex=%5")
                 .arg(toPid)
                 .arg(senderPid)
                 .arg((int)flag)
                 .arg(totalSize)
                 .arg(QString(rawBytes.toHex().toUpper()).mid(0, 20) + "..."));

    return packet;
}

QByteArray Client::createW3GSSlotInfoJoinPacket(quint8 playerID, const QHostAddress& externalIp, quint16 localPort)
{
    LOG_INFO("=== 构建 W3GS_SLOTINFOJOIN (0x04) ===");

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian); // War3 协议统一使用小端序

    // -------------------------------------------------
    // 1. 准备数据
    // -------------------------------------------------
    QByteArray slotData = serializeSlotData();

    // 【关键】计算 SlotInfoBlock 的总长度
    // 结构包含: [SlotData (N字节)] + [RandomSeed (4)] + [LayoutStyle (1)] + [NumSlots (1)]
    // 所以长度 = slotData.size() + 6
    quint16 slotBlockSize = (quint16)slotData.size() + 6;

    // -------------------------------------------------
    // 2. 写入包头 (Header)
    // -------------------------------------------------
    out << (quint8)0xF7         // Header
        << (quint8)0x04         // ID: W3GS_SLOTINFOJOIN
        << (quint16)0;          // Total Length (稍后回填)

    // -------------------------------------------------
    // 3. 写入槽位信息块 (Slot Info Block)
    // -------------------------------------------------
    // 3.1 写入块长度 (必须包含尾部的6字节，否则客户端解析错位)
    out << slotBlockSize;

    // 3.2 写入槽位数据
    out.writeRawData(slotData.data(), slotData.size());

    // 3.3 写入尾部信息 (共6字节)
    out << (quint32)m_randomSeed;           // 随机种子
    out << (quint8)m_layoutStyle;           // 布局 (3=Fixed)
    out << (quint8)m_slots.size();          // 玩家总数

    // -------------------------------------------------
    // 4. 写入玩家连接信息 (Player Join Info)
    // -------------------------------------------------
    // 4.1 玩家 ID
    out << (quint8)playerID;

    // 4.2 网络端口与 IP
    out << (quint16)2;                      // AF_INET
    out << (quint16)qToBigEndian(localPort);// Port (网络字节序/大端)
    writeIpToStreamWithLog(out, externalIp);// IP Address

    // 4.3 填充数据 (Unknown)
    out << (quint32)0;                      // Unknown 1
    out << (quint32)0;                      // Unknown 2

    // -------------------------------------------------
    // 5. 收尾工作
    // -------------------------------------------------
    // 回填包总长度 (覆盖偏移 2-3 的位置)
    quint16 totalSize = (quint16)packet.size();
    QDataStream lenStream(&packet, QIODevice::ReadWrite);
    lenStream.setByteOrder(QDataStream::LittleEndian);
    lenStream.skipRawData(2);
    lenStream << totalSize;

    // -------------------------------------------------
    // 6. 日志记录
    // -------------------------------------------------
    LOG_INFO(QString("📦 [0x04] 生成完毕: 总长=%1, 槽位块长=%2, PID=%3")
                 .arg(totalSize).arg(slotBlockSize).arg(playerID));

    // 校验日志：打印 PID 及其前一个字节，确保没有错位
    // 偏移量计算: Header(4) + Len(2) + SlotBlock(slotBlockSize)
    // PID 应该位于: 4 + 2 + slotBlockSize 的位置
    if (packet.size() > 6 + slotBlockSize) {
        int pidOffset = 6 + slotBlockSize;
        quint8 pidInPacket = (quint8)packet.at(pidOffset);
        quint8 byteBefore = (quint8)packet.at(pidOffset - 1);

        LOG_INFO(QString("🔍 偏移校验: 预期PID位置[%1] 值=0x%2 (前一字节=0x%3)")
                     .arg(pidOffset)
                     .arg(QString::number(pidInPacket, 16).toUpper(),
                          QString::number(byteBefore, 16).toUpper())); // 前一字节应该是 NumSlots (0x0A)
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
    LOG_INFO("================================================");
    LOG_INFO("🛠️ 正在构建 W3GS_MAPCHECK (0x3D)...");

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // 1. Header
    out << (quint8)0xF7 << (quint8)0x3D << (quint16)0;

    // 2. Unknown
    out << (quint32)1;

    // 3. Map Path
    QString mapPath = "Maps\\Download\\" + m_war3Map.getMapName();
    QByteArray mapPathBytes = mapPath.toLocal8Bit();
    out.writeRawData(mapPathBytes.data(), mapPathBytes.length());
    out << (quint8)0;

    // 4. Map Stat Data
    quint32 fileSize = m_war3Map.getMapSize();
    quint32 fileInfo = m_war3Map.getMapInfo();
    quint32 fileCRC  = m_war3Map.getMapCRC();

    out << (quint32)fileSize;
    out << (quint32)fileInfo;
    out << (quint32)fileCRC;

    // 5. Map SHA1
    QByteArray sha1 = m_war3Map.getMapSHA1Bytes();

    // 如果获取失败，打印警告
    if (sha1.size() != 20) {
        LOG_WARNING(QString("⚠️ SHA1 长度异常 (%1)，已强制补零调整为 20").arg(sha1.size()));
        sha1.resize(20);
    }

    // === 打印 SHA1 内容 ===
    QString currentHex = sha1.toHex().toUpper();
    for(int i = 2; i < currentHex.length(); i += 3) currentHex.insert(i, " ");

    LOG_INFO(QString("📊 Size: %1").arg(fileSize));
    LOG_INFO(QString("ℹ️ Info: 0x%1").arg(QString::number(fileInfo, 16).toUpper()));
    LOG_INFO(QString("🔑 CRC:  0x%1").arg(QString::number(fileCRC, 16).toUpper()));
    LOG_INFO(QString("🔐 SHA1 (当前): %1").arg(currentHex));

    out.writeRawData(sha1.data(), 20);

    // 6. 回填长度
    quint16 totalSize = (quint16)packet.size();
    QDataStream lenStream(&packet, QIODevice::ReadWrite);
    lenStream.setByteOrder(QDataStream::LittleEndian);
    lenStream.skipRawData(2);
    lenStream << totalSize;

    LOG_INFO("================================================");
    return packet;
}

QByteArray Client::createW3GSStartDownloadPacket(quint8 toPid)
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // Header: F7 3F [Len]
    out << (quint8)0xF7 << (quint8)0x3F << (quint16)0;

    // Unknown (Always 1)
    out << (quint32)1;

    // Player ID (接收者)
    out << (quint8)toPid;

    // 回填长度
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
    unsigned long crc = crc32(0L, nullptr, 0);
    crc = crc32(crc, reinterpret_cast<const unsigned char*>(chunkData.constData()), chunkData.size());

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
    broadcastPacket(slotPacket, excludePid);
    LOG_INFO(QString("📢 广播槽位更新 (0x09)%1")
                 .arg(excludePid != 0 ? QString(" (排除 PID: %1)").arg(excludePid) : ""));
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
// 10. 辅助工具函数
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

void Client::checkPlayerTimeouts()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 定义超时阈值 (毫秒)
    const qint64 TIMEOUT_CONNECTION = 60000; // 60秒无响应算掉线
    const qint64 TIMEOUT_DOWNLOAD = 120000;  // 120秒下载卡住算超时

    // 使用迭代器遍历，以便安全删除
    auto it = m_players.begin();
    while (it != m_players.end()) {
        quint8 pid = it.key();
        PlayerData &playerData = it.value();

        // 跳过主机自己 (PID 1)
        if (pid == 1) {
            ++it;
            continue;
        }

        bool kick = false;
        QString kickReason = "";

        // 1. 检查心跳超时
        if ((now - playerData.lastResponseTime) > TIMEOUT_CONNECTION) {
            kick = true;
            kickReason = QString("连接超时 (%1秒无响应)").arg((now - playerData.lastResponseTime)/1000);
        }
        // 2. 检查下载超时
        // 只有当玩家正在下载状态，且距离上次请求分片已经很久了
        else if (playerData.isDownloading && (now - playerData.lastDownloadTime) > TIMEOUT_DOWNLOAD) {
            kick = true;
            kickReason = QString("下载卡死 (%1秒无进度)").arg((now - playerData.lastDownloadTime)/1000);
        }

        if (kick) {
            LOG_WARNING(QString("👢 踢出玩家 [%1] (PID:%2): %3")
                            .arg(playerData.name).arg(pid).arg(kickReason));

            if (playerData.socket) {
                playerData.socket->disconnectFromHost();
            }
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

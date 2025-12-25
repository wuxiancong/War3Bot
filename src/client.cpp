#include "client.h"
#include "logger.h"
#include "bncsutil/checkrevision.h"
#include "bnethash.h"
#include "bnetsrp3.h"

#include <QDir>
#include <QFileInfo>
#include <QDateTime>
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
    initSlots();

    m_udpSocket = new QUdpSocket(this);
    m_tcpServer = new QTcpServer(this);
    m_tcpSocket = new QTcpSocket(this);

    // 信号槽连接
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

void Client::disconnectFromHost() { m_tcpSocket->disconnectFromHost(); }
bool Client::isConnected() const { return m_tcpSocket->state() == QAbstractSocket::ConnectedState; }
void Client::onDisconnected() { LOG_WARNING("🔌 战网连接断开"); }

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

void Client::sendPacket(TCPPacketID id, const QByteArray &payload)
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out << BNET_HEADER;
    out << (quint8)id;
    out << (quint16)(payload.size() + 4);

    if (!payload.isEmpty()) {
        packet.append(payload);
    }

    m_tcpSocket->write(packet);

    QString hexStr = packet.toHex().toUpper();
    for(int i = 2; i < hexStr.length(); i += 3) hexStr.insert(i, " ");
    LOG_INFO(QString("📤 发送包 ID: 0x%1 Len:%2 Data: %3")
                 .arg(QString::number(id, 16))
                 .arg(packet.size())
                 .arg(hexStr));
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
        handleTcpPacket((TCPPacketID)packetIdVal, packetData.mid(4));
    }
}

void Client::handleTcpPacket(TCPPacketID id, const QByteArray &data)
{
    LOG_INFO(QString("📥 收到包 ID: 0x%1").arg(QString::number(id, 16)));

    switch (id) {
    case SID_PING:
        sendPacket(SID_PING, data);
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
            if (strEnd == -1) break;
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

        switch (eventId) {
        case 0x01: LOG_INFO(QString("👤 [频道用户] %1 (Ping: %2)").arg(username).arg(ping)); break;
        case 0x02: LOG_INFO(QString("➡️ %1 加入了频道").arg(username)); break;
        case 0x03: LOG_INFO(QString("⬅️ %1 离开了频道").arg(username)); break;
        case 0x04: LOG_INFO(QString("📩 [%1] 悄悄: %2").arg(username, text)); break;
        case 0x05: LOG_INFO(QString("💬 [%1]: %2").arg(username, text)); break;
        case 0x06: LOG_INFO(QString("📢 [广播]: %1").arg(text)); break;
        case 0x07: LOG_INFO(QString("🏠 已加入频道: [%1]").arg(text)); break;
        case 0x09: LOG_INFO(QString("🔧 %1 更新状态 (Flags: %2)").arg(username, QString::number(flags, 16))); break;
        case 0x0A: LOG_INFO(QString("📤 你对 [%1] 说: %2").arg(username, text)); break;
        case 0x0D: LOG_WARNING("⚠️ 频道已满"); break;
        case 0x0E: LOG_WARNING("⚠️ 频道不存在"); break;
        case 0x0F: LOG_WARNING("⚠️ 频道权限受限"); break;
        case 0x12: LOG_INFO(QString("ℹ️ [系统]: %1").arg(text)); break;
        case 0x13: LOG_ERROR(QString("❌ [错误]: %1").arg(text)); break;
        case 0x17: LOG_INFO(QString("✨ %1 %2").arg(username, text)); break;
        default:   LOG_INFO(QString("📦 未知聊天事件 ID: 0x%1").arg(QString::number(eventId, 16))); break;
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
        if (data.size() > 16) handleAuthCheck(data);
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
        if (status == 0 || status == 0x0E) {
            LOG_INFO("🎉 登录成功 (SRP)！");
            emit authenticated();
        } else {
            LOG_ERROR(QString("❌ 登录失败 (SRP): 0x%1").arg(QString::number(status, 16)));
        }
    }
    break;

    case SID_STARTADVEX3:
        LOG_INFO("✅ 房间创建成功！");
        emit gameListRegistered();
        break;

    default: break;
    }
}

void Client::onPlayerReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    // 1. 将新数据追加到该 socket 对应的缓冲区
    QByteArray &buffer = m_playerBuffers[socket];
    buffer.append(socket->readAll());

    // 2. 循环处理缓冲区中的完整包
    while (buffer.size() >= 4) {
        quint8 header = (quint8)buffer[0];
        if (header != 0xF7) {
            LOG_WARNING("❌ 非法协议头，断开连接");
            socket->disconnectFromHost();
            return;
        }

        // 解析长度 (Little Endian)
        quint16 length = (quint8)buffer[2] | ((quint8)buffer[3] << 8);

        // 如果缓冲区数据不够一个包，停止处理，等待下一次 readyRead
        if (buffer.size() < length) {
            return;
        }

        // 3. 提取完整包
        QByteArray packet = buffer.mid(0, length);

        // 4. 从缓冲区移除已处理的数据
        buffer.remove(0, length);

        // 5. 处理逻辑
        quint8 msgId = (quint8)packet[1];
        QByteArray payload = packet.mid(4);
        handleW3GSPacket(socket, msgId, payload);
    }
}

void Client::handleW3GSPacket(QTcpSocket *socket, quint8 id, const QByteArray &payload)
{
    switch (id) {
    case 0x1E: // W3GS_REQJOIN
    {
        LOG_INFO("🚪 收到加入请求 (0x1E)");

        // 1. 寻找空槽位
        int slotIndex = -1;
        for (int i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].slotStatus == 0) { // Open
                slotIndex = i;
                break;
            }
        }

        if (slotIndex == -1) {
            LOG_WARNING("⚠️ 房间已满，拒绝加入");
            // 发送 0x05 REJECT
            return;
        }

        // 2. 分配 PID (简单算法：槽位索引 + 2)
        // 注意：真实逻辑需要更严谨的 PID 分配，这里简化处理
        quint8 newPid = slotIndex + 2;

        // 3. 更新槽位状态
        m_slots[slotIndex].pid = newPid;
        m_slots[slotIndex].slotStatus = 2; // Occupied
        m_slots[slotIndex].downloadStatus = 255;
        m_slots[slotIndex].computer = 0;

        // 4. 发送 0x04 (SlotInfoJoin)
        QByteArray packet;
        QDataStream out(&packet, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);

        // 序列化当前的槽位数据
        QByteArray slotData = serializeSlotData();

        out << (quint8)0xF7 << (quint8)0x04 << (quint16)0; // Head

        out << (quint16)slotData.size();    // SlotInfo Len
        out.writeRawData(slotData.data(), slotData.size()); // Slots

        out << (quint32)12345;  // Seed
        out << (quint8)3;       // Custom Game
        out << (quint8)m_slots.size(); // Total Slots
        out << (quint8)newPid;  // **告诉玩家他是谁**
        out << (quint16)2;      // AF_INET
        out << (quint16)socket->peerPort();
        out << (quint32)socket->peerAddress().toIPv4Address();
        out << (quint32)0 << (quint32)0;

        // 回填长度
        QDataStream lenStream(&packet, QIODevice::ReadWrite);
        lenStream.setByteOrder(QDataStream::LittleEndian);
        lenStream.skipRawData(2);
        lenStream << (quint16)packet.size();

        socket->write(packet);
        LOG_INFO(QString("✅ 发送 0x04, PID: %1, Slot: %2").arg(newPid).arg(slotIndex));

        // --- 构造 0x06 (Host Info) ---
        // 告诉客户端：PID=1 的人是主机，名字叫 "War3Bot"
        QByteArray hostInfo;
        QDataStream pOut(&hostInfo, QIODevice::WriteOnly);
        pOut.setByteOrder(QDataStream::LittleEndian);

        pOut << (quint8)0xF7 << (quint8)0x06; // Header
        pOut << (quint16)0; // 长度占位

        pOut << (quint32)1; // Host PID (通常主机是 1)
        pOut << (quint8)1;  // Player Type (1=Host/Player)
        pOut << (quint8)0;  // Team (0=Sentinel/Team 1)

        QByteArray hostName = "War3Bot"; // 主机名字
        pOut.writeRawData(hostName.data(), hostName.length());
        pOut << (quint8)0;  // 字符串结束符

        pOut << (quint16)1; // External Port (随便填，不重要)
        pOut << (quint32)1; // External IP (随便填，不重要)
        pOut << (quint32)0; // Internal IP (0)
        pOut << (quint32)0; // Internal IP (0)

        // 回填长度
        QDataStream pLenOut(&hostInfo, QIODevice::ReadWrite);
        pLenOut.setByteOrder(QDataStream::LittleEndian);
        pLenOut.skipRawData(2);
        pLenOut << (quint16)hostInfo.size();

        socket->write(hostInfo);
        LOG_INFO("👤 发送主机信息 (0x06)");

        // --- 构造 0x3D (Map Check) ---
        QByteArray mapCheck;
        QDataStream mOut(&mapCheck, QIODevice::WriteOnly);
        mOut.setByteOrder(QDataStream::LittleEndian);

        mOut << (quint8)0xF7 << (quint8)0x3D; // Header
        mOut << (quint16)0; // 长度占位

        mOut << (quint32)1; // Unknown (1)

        // 地图路径 (必须和客户端本地路径匹配，或者相对路径)
        // 标准 DotA 路径示例
        QByteArray mapPath = "Maps\\Download\\DotA v6.83d.w3x";
        mOut.writeRawData(mapPath.data(), mapPath.length());
        mOut << (quint8)0; // 字符串结束符

        mOut << (quint32)0; // Map Size (如果你知道真实大小最好填对，不知道填0也行，但这会影响下载逻辑)
        mOut << (quint32)0; // Map Info (CRC32等) - 这里填0通常会让客户端显示"未知地图"但能进
        mOut << (quint32)0; // Map CRC
        mOut << (quint32)0; // Map SHA1 (部分私服协议需要)

        // 回填长度
        QDataStream mLenOut(&mapCheck, QIODevice::ReadWrite);
        mLenOut.setByteOrder(QDataStream::LittleEndian);
        mLenOut.skipRawData(2);
        mLenOut << (quint16)mapCheck.size();

        socket->write(mapCheck);
        LOG_INFO("🗺️ 发送地图验证请求 (0x3D)");

        // 7. 通知房间内 OTHER 玩家：有人进来了 (0x09 SlotInfo Update)
        // 这一步对于多人游戏是必须的，否则其他人看不到新玩家
        // 遍历 m_playerSockets 发送 0x09
    }
    break;

    case 0x21: // W3GS_LEAVEREQ (玩家离开)
        LOG_INFO("👋 玩家请求离开房间");
        socket->disconnectFromHost();
        break;

    case 0x06: // W3GS_MAPPART (地图下载/校验相关)
        LOG_INFO("🗺️ 收到地图相关请求 (0x06)，暂忽略");
        break;

    case 0x28: // W3GS_PONG_TO_HOST (TCP Ping 回复)
        LOG_INFO("💓 收到玩家 TCP Pong");
        break;

    default:
        LOG_INFO(QString("❓ 未处理的 TCP 包 ID: 0x%1").arg(QString::number(id, 16)));
        break;
    }
}

void Client::onPlayerDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        LOG_INFO("🔌 玩家断开");
        m_playerSockets.removeAll(socket);
        m_playerBuffers.remove(socket); // 清理缓存

        // TODO: 找到该 Socket 对应的 PID，将 m_slots 设置回 Open (0)
        // 并广播 0x09 给剩余玩家

        socket->deleteLater();
    }
}

// =========================================================
// 4. UDP 核心处理
// =========================================================

void Client::onUdpReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        handleUdpPacket(datagram.data(), datagram.senderAddress(), datagram.senderPort());
    }
}

void Client::handleUdpPacket(const QByteArray &data, const QHostAddress &sender, quint16 senderPort)
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

    switch ((UdpPacketID)msgId) {
    case W3GS_PING_FROM_OTHERS: // 0x35
    {
        QByteArray pong = data;
        pong[1] = (char)W3GS_PONG_TO_OTHERS; // 0x35 -> 0x36
        m_udpSocket->writeDatagram(pong, sender, senderPort);
        LOG_INFO("⚡ [UDP] 回复 P2P Ping (0x36)");
    }
    break;
    case W3GS_REQJOIN: // 0x1E
        LOG_INFO(QString("🚪 [UDP] 收到加入请求 (0x1E) Size: %1").arg(data.size()));
        break;
    case W3GS_PING_FROM_HOST: // 0x01
    {
        LOG_INFO("💓 [UDP] 收到主机 Ping (0x01) -> 回复 0x46");
        QByteArray pong = data;
        pong[1] = (char)W3GS_PONG_TO_HOST; // 0x01 -> 0x46
        m_udpSocket->writeDatagram(pong, sender, senderPort);
    }
    break;
    case W3GS_PONG_TO_OTHERS: // 0x36
        LOG_INFO("📶 [UDP] 收到 P2P Pong (0x36) | 延迟检测成功");
        break;
    case W3GS_SEARCHGAME: // 0x2F
        LOG_INFO("🔍 [UDP] 收到局域网搜房请求 (0x2F)");
        break;
    case W3GS_GAMEINFO:     // 0x30
    case W3GS_REFRESHGAME:  // 0x32
        LOG_INFO(QString("🗺️ [UDP] 收到局域网房间广播 (0x%1)").arg(QString::number(msgId, 16)));
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
}

void Client::createGame(const QString &gameName, const QString &password, ProviderVersion providerVersion, ComboGameType comboGameType, SubGameType subGameType, LadderType ladderType)
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

    out << state << (quint32)0 << (quint16)comboGameType << (quint16)subGameType
        << (quint32)providerVersion << (quint32)ladderType;
    out.writeRawData(gameName.toUtf8().constData(), gameName.toUtf8().size()); out << (quint8)0;
    out.writeRawData(password.toUtf8().constData(), password.toUtf8().size()); out << (quint8)0;
    out.writeRawData(finalStatString.constData(), finalStatString.size()); out << (quint8)0;

    sendPacket(SID_STARTADVEX3, payload);
    LOG_INFO("📤 房间创建请求发送完毕");
}

void Client::initSlots()
{
    // 1. 清空旧数据
    m_slots.clear();
    m_slots.resize(12);

    // 2. 清空现有玩家连接
    for (auto socket : qAsConst(m_playerSockets)) {
        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->disconnectFromHost();
        }
    }
    m_playerSockets.clear();
    m_playerBuffers.clear();

    // 3. 初始化槽位状态
    for (int i = 0; i < 12; ++i) {
        m_slots[i] = GameSlot();

        // 设置队伍和颜色
        m_slots[i].color = i;
        m_slots[i].team = 0;
        m_slots[i].race = 32;

        if (i == 0) {
            // === Slot 0: 主机 (Bot) ===
            m_slots[i].pid = 1;         // 主机通常 PID=1
            m_slots[i].downloadStatus = 100;
            m_slots[i].slotStatus = 2;  // Occupied
            m_slots[i].computer = 0;    // Human (Bot 假装是人)
            m_slots[i].race = 1;        // Human (或者 Random)
        } else {
            // === Slot 1-11: 空位 ===
            m_slots[i].pid = 0;         // 0 表示无人
            m_slots[i].slotStatus = 0;  // Open
        }
    }

    LOG_INFO("✨ 房间槽位已初始化 (主机在 Slot 0)");
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

// =========================================================
// 8. 辅助工具函数
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

    // 优先 6112-6119
    for (quint16 p = 6112; p <= 6119; ++p) {
        if (tryBind(p)) return true;
    }
    // 随机范围
    for (int i = 0; i < 200; ++i) {
        quint16 p = QRandomGenerator::global()->bounded(49152, 65536);
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
    // 构造 0x01 Ping 包
    // F7 01 08 00 [Timestamp(4)]
    QByteArray pingPacket;
    QDataStream out(&pingPacket, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out << (quint8)0xF7 << (quint8)0x01 << (quint16)8;
    out << (quint32)QDateTime::currentMSecsSinceEpoch();

    for (auto socket : qAsConst(m_playerSockets)) {
        socket->write(pingPacket);
    }
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

quint32 Client::ipToUint32(const QHostAddress &address) { return address.toIPv4Address(); }
quint32 Client::ipToUint32(const QString &ipAddress) { return QHostAddress(ipAddress).toIPv4Address(); }

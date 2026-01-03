#include "netmanager.h"
#include "war3map.h"
#include "logger.h"
#include <QDir>
#include <QTimer>
#include <QPointer>
#include <QFileInfo>
#include <QDateTime>
#include <QDataStream>
#include <QCoreApplication>
#include <QNetworkDatagram>
#include <QRandomGenerator>
#include <QCryptographicHash>

#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

NetManager::NetManager(QObject *parent)
    : QObject(parent)
    , m_peerTimeout(300000)
    , m_listenPort(0)
    , m_cleanupInterval(60000)
    , m_settings(nullptr)
    , m_enableBroadcast(false)
    , m_broadcastInterval(30000)
    , m_broadcastPort(6112)
    , m_isRunning(false)
    , m_cleanupTimer(nullptr)
    , m_broadcastTimer(nullptr)
    , m_udpSocket(nullptr)
    , m_tcpServer(nullptr)
    , m_nextSessionId(1000)
    , m_serverSeq(0)
{
}

NetManager::~NetManager()
{
    stopServer();
}

// ==================== Socket 管理与启动 ====================

bool NetManager::startServer(quint64 port, const QString &configFile)
{
    if (m_isRunning) return true;

    m_settings = new QSettings(configFile, QSettings::IniFormat, this);
    loadConfiguration();

    m_udpSocket = new QUdpSocket(this);
    if (!bindSocket(port)) {
        cleanupResources();
        return false;
    }
    setupSocketOptions();

    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetManager::onUDPReadyRead);

    m_tcpServer = new QTcpServer(this);
    if (!m_tcpServer->listen(QHostAddress::AnyIPv4, port)) {
        LOG_ERROR(QString("❌ TCP 启动失败: %1").arg(m_tcpServer->errorString()));
        cleanupResources();
        return false;
    }
    connect(m_tcpServer, &QTcpServer::newConnection, this, &NetManager::onNewTcpConnection);

    m_listenPort = m_udpSocket->localPort();
    m_isRunning = true;
    setupTimers();

    LOG_INFO(QString("✅ 服务端启动 - UDP/TCP端口: %1").arg(m_listenPort));
    emit serverStarted(port);
    return true;
}

void NetManager::loadConfiguration()
{
    m_peerTimeout = m_settings->value("server/peer_timeout", 300000).toInt();
    m_cleanupInterval = m_settings->value("server/cleanup_interval", 60000).toInt();
    m_enableBroadcast = m_settings->value("server/enable_broadcast", false).toBool();
}

bool NetManager::setupSocketOptions()
{
    int fd = m_udpSocket->socketDescriptor();
    if (fd == -1) return false;
    int reuse = 1;
#ifdef Q_OS_WIN
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
#else
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
    return true;
}

bool NetManager::bindSocket(quint64 port)
{
    if (!m_udpSocket->bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress)) {
        LOG_ERROR(QString("Bind Error: %1").arg(m_udpSocket->errorString()));
        return false;
    }
    return true;
}

void NetManager::setupTimers()
{
    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &NetManager::onCleanupTimeout);
    m_cleanupTimer->start(m_cleanupInterval);

    if (m_enableBroadcast) {
        m_broadcastTimer = new QTimer(this);
        connect(m_broadcastTimer, &QTimer::timeout, this, &NetManager::onBroadcastTimeout);
        m_broadcastTimer->start(30000);
    }
}

void NetManager::stopServer()
{
    if (!m_isRunning) return;
    m_isRunning = false;
    cleanupResources();
    m_registerInfos.clear();
    emit serverStopped();
}

// ==================== 二进制发送逻辑 ====================

qint64 NetManager::sendUdpPacket(const QHostAddress &target, quint64 port, PacketType type, const void *payload, quint64 payloadLen)
{
    if (!m_udpSocket) {
        LOG_ERROR("❌ [UDP] 发送失败: UDP Socket 未初始化");
        return -1;
    }

    // 1. 准备 Buffer
    int totalSize = sizeof(PacketHeader) + payloadLen;
    QByteArray buffer;
    buffer.resize(totalSize);

    // 2. 填充 Header
    PacketHeader *header = reinterpret_cast<PacketHeader*>(buffer.data());
    header->magic = PROTOCOL_MAGIC;
    header->version = PROTOCOL_VERSION;
    header->command = static_cast<quint8>(type);
    header->sessionId = 0;
    header->seq = ++m_serverSeq;
    header->payloadLen = payloadLen;
    header->checksum = 0;

    // 3. 填充 Payload
    if (payloadLen > 0 && payload != nullptr) {
        memcpy(buffer.data() + sizeof(PacketHeader), payload, payloadLen);
    }

    // 4. 计算 CRC
    header->checksum = calculateCRC16(buffer);

    // 5. 发送
    qint64 sent = m_udpSocket->writeDatagram(buffer, target, port);

    QString typeStr = packetTypeToString(type);

    if (sent < 0) {
        LOG_ERROR(QString("❌ [UDP] 发送失败 -> %1:%2 | Cmd: %3 | Error: %4")
                      .arg(target.toString()).arg(port).arg(typeStr, m_udpSocket->errorString()));
    } else {
        if (type == PacketType::C_S_HEARTBEAT || type == PacketType::S_C_PONG) {
            LOG_DEBUG(QString("📤 [UDP] %1 -> %2:%3 (Len: %4)").arg(typeStr, target.toString()).arg(port).arg(sent));
        } else {
            LOG_INFO(QString("📤 [UDP] %1 -> %2:%3 (Len: %4)").arg(typeStr, target.toString()).arg(port).arg(sent));
        }
    }
    return sent;
}

bool NetManager::sendTcpPacket(QTcpSocket *socket, PacketType type, const void *payload, quint64 payloadLen)
{
    // 0. 前置检查
    if (!socket) {
        LOG_ERROR("❌ [TCP] 发送失败: Socket 指针为空");
        return false;
    }
    if (socket->state() != QAbstractSocket::ConnectedState) {
        LOG_WARNING(QString("❌ [TCP] 发送失败: Socket 未连接 (State: %1)").arg(socket->state()));
        return false;
    }

    // 1. 准备 Buffer
    int totalSize = sizeof(PacketHeader) + payloadLen;
    QByteArray buffer;
    buffer.resize(totalSize);

    // 2. 填充 Header
    PacketHeader *header = reinterpret_cast<PacketHeader*>(buffer.data());
    header->magic = PROTOCOL_MAGIC;
    header->version = PROTOCOL_VERSION;
    header->command = static_cast<quint8>(type);

    // 获取身份信息用于日志
    quint32 sid = socket->property("sessionId").toUInt();
    QString clientId = socket->property("clientId").toString();

    header->sessionId = sid;
    header->seq = ++m_serverSeq;
    header->payloadLen = payloadLen;
    header->checksum = 0;

    // 3. 填充 Payload
    if (payloadLen > 0 && payload != nullptr) {
        memcpy(buffer.data() + sizeof(PacketHeader), payload, payloadLen);
    }

    // 4. 发送
    qint64 sent = socket->write(buffer);
    socket->flush();

    QString typeStr = packetTypeToString(type);
    QString peerInfo = QString("%1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort());

    // 5. 结果判断与日志
    if (sent == totalSize) {
        LOG_INFO(QString("🚀 [TCP] %1 -> %2 (Session: %3 | Client: %4 | Len: %5)")
                     .arg(typeStr, peerInfo).arg(sid)
                     .arg(clientId.isEmpty() ? "Unknown" : clientId.left(8))
                     .arg(sent));
        return true;
    } else {
        LOG_ERROR(QString("❌ [TCP] 发送不完整 -> %1 | Cmd: %2 | 计划: %3 / 实际: %4 | Error: %5")
                      .arg(peerInfo, typeStr)
                      .arg(totalSize).arg(sent)
                      .arg(socket->errorString()));
        return false;
    }
}

// ==================== 二进制接收逻辑 ====================

void NetManager::onUDPReadyRead()
{
    while (m_udpSocket && m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        handleIncomingDatagram(datagram);
    }
}

void NetManager::handleIncomingDatagram(const QNetworkDatagram &datagram)
{
    if (!m_watchdog.checkUdpPacket(datagram.senderAddress(), datagram.data().size())) {
        return;
    }

    QByteArray data = datagram.data();
    if (data.size() < (int)sizeof(PacketHeader)) return;

    PacketHeader *header = reinterpret_cast<PacketHeader*>(data.data());

    // 1. 基础校验
    if (header->magic != PROTOCOL_MAGIC || header->version != PROTOCOL_VERSION) {
        QString sender = QString("%1:%2").arg(datagram.senderAddress().toString()).arg(datagram.senderPort());

        LOG_WARNING(QString("❌ [协议拒绝] 来自 %1 | Magic: 0x%2 (期望: 0x%3) | Ver: %4 (期望: %5)")
                        .arg(sender,
                             QString::number(header->magic, 16).toUpper(),
                             QString::number(PROTOCOL_MAGIC, 16).toUpper())
                        .arg(header->version)
                        .arg(PROTOCOL_VERSION));
        return;
    }

    if (data.size() != static_cast<int>(sizeof(PacketHeader) + header->payloadLen)) {
        LOG_WARNING("包长度不一致，丢弃");
        return;
    }

    // 2. CRC 校验
    quint64 recvChecksum = header->checksum;
    header->checksum = 0;
    if (calculateCRC16(data) != recvChecksum) {
        LOG_WARNING("CRC 校验失败");
        return;
    }

    // 3. 分发
    char *payload = data.data() + sizeof(PacketHeader);
    QHostAddress sender = datagram.senderAddress();
    quint64 port = datagram.senderPort();

    switch (static_cast<PacketType>(header->command)) {
    case C_S_REGISTER:
        if (header->payloadLen >= sizeof(CSRegisterPacket)) {
            handleRegister(header, reinterpret_cast<CSRegisterPacket*>(payload), sender, port);
        }
        break;
    case C_S_UNREGISTER:
        handleUnregister(header);
        break;
    case C_S_HEARTBEAT:
        handleHeartbeat(header, sender, port);
        break;
    case C_S_PING:
        handlePing(header, sender, port);
        break;
    case C_S_COMMAND:
        if (header->payloadLen >= sizeof(CSCommandPacket)) {
            handleCommand(header, reinterpret_cast<CSCommandPacket*>(payload));
        }
        break;
    case C_S_CHECKMAPCRC:
        if (header->payloadLen >= sizeof(CSCheckMapCRCPacket)) {
            handleCheckMapCRC(header, reinterpret_cast<CSCheckMapCRCPacket*>(payload), sender, port);
        }
        break;
    default:
        LOG_DEBUG(QString("❓ 收到未知指令: %1 来自 %2").arg((int)header->command).arg(sender.toString()));
        break;
    }
}

// ==================== 具体业务处理器 ====================

void NetManager::handleRegister(const PacketHeader *header, const CSRegisterPacket *packet, const QHostAddress &senderAddr, quint64 senderPort)
{
    Q_UNUSED(header);

    // 提取字符串数据
    QString clientId = QString::fromUtf8(packet->clientId, strnlen(packet->clientId, sizeof(packet->clientId)));
    QString username = QString::fromUtf8(packet->username, strnlen(packet->username, sizeof(packet->username)));
    QString localIp  = QString::fromUtf8(packet->localIp, strnlen(packet->localIp, sizeof(packet->localIp)));

    // 🆕 提取客户端上报的公网IP
    QString reportedPublicIp = QString::fromUtf8(packet->publicIp, strnlen(packet->publicIp, sizeof(packet->publicIp)));

    if (clientId.isEmpty()) return;

    QWriteLocker locker(&m_registerInfosLock);

    // === Session ID 生成 ===
    quint32 newSessionId = 0;
    do {
        newSessionId = QRandomGenerator::global()->generate();
    } while (newSessionId == 0 || m_sessionIndex.contains(newSessionId));

    // === 清理旧会话 ===
    if (m_registerInfos.contains(clientId)) {
        quint32 oldSession = m_registerInfos[clientId].sessionId;
        m_sessionIndex.remove(oldSession);
        LOG_INFO(QString("♻️ 用户重连，清理旧 Session: %1").arg(oldSession));
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QString actualPublicIp = cleanAddress(senderAddr);
    QString natStr = natTypeToString(static_cast<NATType>(packet->natType));

    // 存储用户信息
    RegisterInfo info;
    info.clientId = clientId;
    info.username = username;
    info.localIp = localIp;
    info.localPort = packet->localPort;
    info.publicIp = actualPublicIp;
    info.publicPort = senderPort;
    info.sessionId = newSessionId;
    info.lastSeen = now;
    info.firstSeen = now;
    info.isRegistered = true;
    info.natType = packet->natType;

    m_registerInfos[clientId] = info;
    m_sessionIndex[newSessionId] = clientId;

    locker.unlock();

    // ✅ 打印详细日志 (包含您要求的两个字段对比)
    LOG_INFO("--------------------[ 📝 用户注册请求 ]--------------------");
    LOG_INFO(QString("   ├─ Session ID:     %1").arg(newSessionId));
    LOG_INFO(QString("   ├─ Username:       %1").arg(username));
    LOG_INFO(QString("   ├─ Client UUID:    %1").arg(clientId));
    LOG_INFO(QString("   ├─ Local Address:  %1:%2").arg(localIp).arg(packet->localPort));
    // 显示客户端自己检测到的 (Reported)
    LOG_INFO(QString("   ├─ Public(Report): %1:%2").arg(reportedPublicIp).arg(packet->publicPort));
    // 显示服务端实际看到的 (Actual)
    LOG_INFO(QString("   ├─ Public(Actual): %1:%2").arg(actualPublicIp).arg(senderPort));
    LOG_INFO(QString("   └─ NAT Type:       %1").arg(natStr));
    LOG_INFO("----------------------------------------------------------");

    // 发送响应
    SCRegisterPacket resp;
    resp.sessionId = newSessionId;
    resp.status = 1;

    sendUdpPacket(senderAddr, senderPort, S_C_REGISTER, &resp, sizeof(resp));
}

void NetManager::handleUnregister(const PacketHeader *header)
{
    // SessionID 为 0 表示无效或未注册
    if (header->sessionId == 0) return;

    QWriteLocker locker(&m_registerInfosLock);

    // ✅ 1. 利用索引快速查找 (O(1) 时间复杂度)
    if (m_sessionIndex.contains(header->sessionId)) {

        // 2. 从索引中移除，并获取对应的 Client UUID
        QString uuid = m_sessionIndex.take(header->sessionId);

        // 3. 从主信息表中移除，并获取信息用于打印日志
        if (m_registerInfos.contains(uuid)) {
            RegisterInfo info = m_registerInfos.take(uuid); // take = remove + return

            LOG_INFO("--------------------[ 👋 用户注销请求 ]--------------------");
            LOG_INFO(QString("   ├─ Username:    %1").arg(info.username));
            LOG_INFO(QString("   ├─ Client UUID: %1").arg(info.clientId));
            LOG_INFO(QString("   └─ Session ID:  %1").arg(header->sessionId));
            LOG_INFO("-------------------------------------------------------");
        }
        else {
            // 理论上不应该进这里，除非索引和主表数据不一致
            LOG_WARNING(QString("⚠️ 索引存在但主表丢失数据: %1").arg(uuid));
        }
    }
    else {
        LOG_WARNING(QString("⚠️ 收到未知 Session %1 的注销请求").arg(header->sessionId));
    }
}

void NetManager::handlePing(const PacketHeader *header, const QHostAddress &senderAddr, quint64 senderPort)
{
    // 1. 检查 SessionID 是否存在
    bool isRegistered = false;

    if (header->sessionId != 0) {
        QReadLocker locker(&m_registerInfosLock);
        if (m_sessionIndex.contains(header->sessionId)) {
            isRegistered = true;
            QString uuid = m_sessionIndex.value(header->sessionId);
            m_registerInfos[uuid].lastSeen = QDateTime::currentMSecsSinceEpoch();
            m_registerInfos[uuid].publicIp = cleanAddress(senderAddr);
            m_registerInfos[uuid].publicPort = senderPort;
        }
    }

    // 2. 构造带状态的 PONG 包
    SCPongPacket pongPkt;
    pongPkt.status = isRegistered ? 1 : 0;

    // 3. 发送
    sendUdpPacket(senderAddr, senderPort, S_C_PONG, &pongPkt, sizeof(pongPkt));

    // 4. 日志策略：高频操作，仅使用单行日志，且仅在注册时显示
    if (isRegistered) {
        // 使用紧凑格式，不占用太多版面
        LOG_DEBUG(QString("🏓 [PING] Session:%1 <-> %2:%3").arg(header->sessionId).arg(senderAddr.toString()).arg(senderPort));
    } else {
        // 未注册的 Ping 可能是过期客户端，稍微提示一下
        LOG_DEBUG(QString("⚠️ [PING] 未注册/过期请求 <-> %1:%2").arg(senderAddr.toString()).arg(senderPort));
    }
}

void NetManager::handleHeartbeat(const PacketHeader *header, const QHostAddress &senderAddr, quint64 senderPort)
{
    if (header->sessionId == 0) return;

    bool found = false;
    bool ipChanged = false;
    QString userName;
    QString oldAddr;
    QString newAddr;

    // 1. 锁定并更新
    {
        QWriteLocker locker(&m_registerInfosLock);
        if (m_sessionIndex.contains(header->sessionId)) {
            QString uuid = m_sessionIndex[header->sessionId];
            if (m_registerInfos.contains(uuid)) {
                RegisterInfo &info = m_registerInfos[uuid];

                // 更新活跃时间
                info.lastSeen = QDateTime::currentMSecsSinceEpoch();

                // 检测地址变化 (NAT 漫游检测)
                QString currentCleanIp = cleanAddress(senderAddr);
                if (info.publicIp != currentCleanIp || info.publicPort != senderPort) {
                    ipChanged = true;
                    userName = info.username;
                    oldAddr = QString("%1:%2").arg(info.publicIp).arg(info.publicPort);
                    newAddr = QString("%1:%2").arg(currentCleanIp).arg(senderPort);

                    // 更新为新地址
                    info.publicIp = currentCleanIp;
                    info.publicPort = senderPort;
                }

                found = true;
            }
        }
    }

    // 2. 响应逻辑
    if (found) {
        sendUdpPacket(senderAddr, senderPort, S_C_PONG);
        if (ipChanged) {
            qDebug().noquote() << "🔄 [网络漫游/NAT变更]";
            qDebug().noquote() << QString("   ├─ 👤 用户: %1").arg(userName);
            qDebug().noquote() << QString("   ├─ 🏚️ 旧址: %1").arg(oldAddr);
            qDebug().noquote() << QString("   └─ 🆕 新址: %1").arg(newAddr);
        }
    } else {
        // 3. 异常处理：会话失效
        SCPongPacket pongPkt;
        pongPkt.status = 0; // 0 表示让客户端重置
        sendUdpPacket(senderAddr, senderPort, S_C_PONG, &pongPkt, sizeof(pongPkt));

        qDebug().noquote() << "🛑 [心跳拒绝]";
        qDebug().noquote() << QString("   ├─ 🎯 来源: %1:%2").arg(senderAddr.toString()).arg(senderPort);
        qDebug().noquote() << QString("   ├─ 🆔 会话: %1").arg(header->sessionId);
        qDebug().noquote() << "   └─ ❌ 原因: 会话无效或已过期 (已通知客户端重连)";
    }
}

void NetManager::handleCommand(const PacketHeader *header, const CSCommandPacket *packet)
{
    QString serverRecClientId; // 服务器端记录的 ClientID

    // 1. Session 验证与查找
    {
        QReadLocker locker(&m_registerInfosLock);
        if (m_sessionIndex.contains(header->sessionId)) {
            serverRecClientId = m_sessionIndex.value(header->sessionId);

            // 数据一致性检查
            if (!m_registerInfos.contains(serverRecClientId)) {
                LOG_WARNING(QString("⚠️ [指令拒绝] 数据不一致: 索引存在 Session %1 但主表丢失 ClientInfo").arg(header->sessionId));
                return;
            }
        } else {
            // 这是最常见的非法包，用一行日志即可
            LOG_WARNING(QString("⚠️ [指令拒绝] 无效 SessionID: %1 (来自未注册或已过期的连接)").arg(header->sessionId));
            return;
        }
    }

    // 2. 提取数据包内容
    QString pktClientId = QString::fromUtf8(packet->clientId, strnlen(packet->clientId, sizeof(packet->clientId)));
    QString user = QString::fromUtf8(packet->username, strnlen(packet->username, sizeof(packet->username)));
    QString cmd  = QString::fromUtf8(packet->command, strnlen(packet->command, sizeof(packet->command)));
    QString text = QString::fromUtf8(packet->text, strnlen(packet->text, sizeof(packet->text)));

    // 3. 安全校验：防伪造检查
    if (pktClientId != serverRecClientId) {
        qDebug().noquote() << "🚫 [安全拦截] 客户端 ID 不匹配 (疑似伪造包)";
        qDebug().noquote() << QString("   ├─ 🔒 Session 绑定: %1").arg(serverRecClientId);
        qDebug().noquote() << QString("   └─ 🔓 数据包声称:   %1").arg(pktClientId);
        return;
    }

    // 4. 打印成功日志
    QString fullCmd = cmd + (text.isEmpty() ? "" : " " + text);

    qDebug().noquote() << "🤖 [收到用户指令]";
    qDebug().noquote() << QString("   ├─ 👤 用户: %1").arg(user);
    qDebug().noquote() << QString("   ├─ 💬 内容: %1").arg(fullCmd);
    qDebug().noquote() << QString("   └─ 🔑 验证: 通过 (Session: %1)").arg(header->sessionId);

    // 5. 向上层分发
    emit commandReceived(user, serverRecClientId, cmd, text);
}

void NetManager::handleCheckMapCRC(const PacketHeader *header, const CSCheckMapCRCPacket *packet, const QHostAddress &senderAddr, quint64 senderPort)
{
    Q_UNUSED(header);
    QString crcHex = QString::fromUtf8(packet->crcHex, strnlen(packet->crcHex, sizeof(packet->crcHex))).trimmed();

    QString scriptDir = QCoreApplication::applicationDirPath() + "/war3files/crc/" + crcHex;
    QDir dir(scriptDir);
    bool exists = dir.exists() && QFile::exists(scriptDir + "/common.j"); // 简化检查

    // 构造响应
    SCCheckMapCRCPacket resp;
    memset(&resp, 0, sizeof(resp));
    strncpy(resp.crcHex, crcHex.toStdString().c_str(), sizeof(resp.crcHex) - 1);
    resp.exists = exists ? 1 : 0;

    // 如果不存在，加入待上传白名单
    if (!exists) {
        QWriteLocker locker(&m_tokenLock);
        m_pendingUploadTokens.insert(crcHex, header->sessionId);
        LOG_INFO(QString("🔍 请求CRC %1 不存在，等待上传 (Session: %2)")
                     .arg(crcHex).arg(header->sessionId));
    } else {
        LOG_INFO(QString("✅ 请求CRC %1 已存在").arg(crcHex));
    }

    sendUdpPacket(senderAddr, senderPort, S_C_CHECKMAPCRC, &resp, sizeof(resp));
}

// ==================== TCP ====================

void NetManager::onTcpReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    // 1. 快速路径：如果已识别过协议，直接分发
    if (socket->property("ConnType").isValid()) {
        QString type = socket->property("ConnType").toString();
        if (type == "UPLOAD") handleTcpUploadMessage(socket);
        else if (type == "COMMAND") handleTcpCommandMessage(socket);
        return;
    }

    // 2. 数据缓冲检查
    if (socket->bytesAvailable() < 4) return;

    // 3. 预读协议头
    QByteArray head = socket->peek(4);
    QString peerInfo = QString("%1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort());
    QString headHex = head.toHex().toUpper();

    // 4. 协议识别逻辑
    if (head.startsWith("W3UP")) {
        socket->setProperty("ConnType", "UPLOAD");

        qDebug().noquote() << "🔌 [TCP 协议识别]";
        qDebug().noquote() << QString("   ├─ 👤 来源: %1").arg(peerInfo);
        qDebug().noquote() << QString("   ├─ 🏷️ 头部: %1 (ASCII: W3UP)").arg(headHex);
        qDebug().noquote() << "   └─ ✅ 类型: 文件上传通道 (UPLOAD)";

        handleTcpUploadMessage(socket);
    }
    else {
        const char *data = head.constData();
        quint32 magic = *reinterpret_cast<const quint32*>(data);
        if (magic == static_cast<quint32>(PROTOCOL_MAGIC)) {
            socket->setProperty("ConnType", "COMMAND");

            qDebug().noquote() << "🔌 [TCP 协议识别]";
            qDebug().noquote() << QString("   ├─ 👤 来源: %1").arg(peerInfo);
            qDebug().noquote() << QString("   ├─ 🏷️ 头部: 0x%1").arg(QString::number(magic, 16).toUpper());
            qDebug().noquote() << "   └─ ✅ 类型: 控制指令通道 (COMMAND)";

            handleTcpCommandMessage(socket);
        }
        else {
            qDebug().noquote() << "🛑 [TCP 协议识别失败]";
            qDebug().noquote() << QString("   ├─ 👤 来源: %1").arg(peerInfo);
            qDebug().noquote() << QString("   ├─ 🏷️ 头部: %1 (未知)").arg(headHex);
            qDebug().noquote() << "   └─ 🛡️ 动作: 断开非法连接";

            socket->disconnectFromHost();
        }
    }
}

void NetManager::handleTcpUploadMessage(QTcpSocket *socket)
{
    // 建立数据流
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_5_15);

    while (socket->bytesAvailable() > 0) {

        // ==================== 阶段 1: 解析头部 ====================
        if (!socket->property("HeaderParsed").toBool()) {

            // 基础头部长度: Magic(4) + Token(8) + NameLen(4) = 16 字节
            const int MIN_HEADER_SIZE = 16;
            if (socket->bytesAvailable() < MIN_HEADER_SIZE) return;

            // ⚡ 关键修正 1: 预读 (Peek) 检查完整性
            // 防止读了 Magic 发现后面数据不够，导致下次重入时数据错位
            QByteArray headerPeep = socket->peek(MIN_HEADER_SIZE);

            // 提取文件名长度 (最后4字节)
            QDataStream peepStream(headerPeep);
            peepStream.skipRawData(12); // 跳过 Magic(4) + Token(8)
            quint32 nameLenPreview;
            peepStream >> nameLenPreview;

            // 🛡️ 安全检查: 文件名长度过长
            if (nameLenPreview > 256) {
                LOG_WARNING("❌ TCP 拒绝: 文件名过长");
                // 此时还没读 socket，但为了发回执需要读出 token (为了逻辑简单，这里直接断开即可)
                // 或者手动读出 token 发回执
                socket->disconnectFromHost();
                return;
            }

            // ⚡ 关键修正 2: 确保【整个头部 + 文件名】都已到达才开始读取
            if (socket->bytesAvailable() < MIN_HEADER_SIZE + nameLenPreview) {
                return; // 数据不够，等待下一个包，不做任何读取操作
            }

            // ==================== 开始正式读取 ====================

            // 1. 验证 Magic
            QByteArray magic = socket->read(4);
            if (magic != "W3UP") {
                LOG_WARNING("❌ TCP 非法连接: 魔数错误");
                sendUploadResult(socket, "", "Magic not match", false, UPLOAD_ERR_MAGIC);
                socket->disconnectFromHost();
                return;
            }

            // 2. 读取并验证 CRC Token
            QByteArray tokenBytes = socket->read(8);
            QString crcToken = QString::fromLatin1(tokenBytes).trimmed();
            quint32 linkedSessionId = 0;

            {
                QReadLocker locker(&m_tokenLock);
                // 3: 逻辑合并，去掉多余的 if/else
                if (m_pendingUploadTokens.contains(crcToken)) {
                    linkedSessionId = m_pendingUploadTokens.value(crcToken);
                } else {
                    LOG_WARNING(QString("❌ TCP 拒绝上传: 未授权的 Token (%1)").arg(crcToken));
                    sendUploadResult(socket, crcToken, "Unauthorized", false, UPLOAD_ERR_TOKEN);
                    socket->disconnectFromHost();
                    return;
                }
            }

            // 保存 SessionID
            socket->setProperty("CrcToken", crcToken);
            socket->setProperty("SessionId", linkedSessionId);

            // 3. 读取文件名长度
            quint32 nameLen;
            in >> nameLen;

            // 4. 读取文件名
            QByteArray nameBytes = socket->read(nameLen);
            QString rawFileName = QString::fromUtf8(nameBytes);
            QString fileName = QFileInfo(rawFileName).fileName();

            if (!isValidFileName(fileName)) {
                LOG_WARNING(QString("❌ TCP 拒绝: 非法文件名 %1").arg(rawFileName));
                sendUploadResult(socket, crcToken, fileName, false, UPLOAD_ERR_FILENAME);
                socket->disconnectFromHost();
                return;
            }

            // 5. 读取文件大小
            if (socket->bytesAvailable() < 8) return;
            qint64 fileSize;
            in >> fileSize;

            if (fileSize <= 0 || fileSize > 20 * 1024 * 1024) {
                LOG_WARNING("❌ TCP 拒绝: 文件过大");
                sendUploadResult(socket, crcToken, fileName, false, UPLOAD_ERR_SIZE);
                socket->disconnectFromHost();
                return;
            }

            // 6. 准备文件
            QString saveDir = QCoreApplication::applicationDirPath() + "/war3files/crc/" + crcToken;
            QDir dir(saveDir);
            if (!dir.exists()) dir.mkpath(".");

            QString savePath = saveDir + "/" + fileName;
            QFile *file = new QFile(savePath);
            if (!file->open(QIODevice::WriteOnly)) {
                LOG_ERROR("❌ 无法创建文件: " + savePath);
                sendUploadResult(socket, crcToken, fileName, false, UPLOAD_ERR_IO);
                delete file;
                socket->disconnectFromHost();
                return;
            }

            socket->setProperty("FilePtr", QVariant::fromValue((void*)file));
            socket->setProperty("BytesTotal", fileSize);
            socket->setProperty("BytesWritten", (qint64)0);
            socket->setProperty("HeaderParsed", true);
            socket->setProperty("FileName", fileName);

            LOG_INFO(QString("📥 [TCP] 开始接收文件: %1 (CRC: %2)").arg(fileName, crcToken));
        }

        // ==================== 阶段 2: 接收文件内容 ====================
        if (socket->property("HeaderParsed").toBool()) {
            QFile *file = static_cast<QFile*>(socket->property("FilePtr").value<void*>());
            qint64 total = socket->property("BytesTotal").toLongLong();
            qint64 current = socket->property("BytesWritten").toLongLong();
            qint64 remaining = total - current;
            qint64 bytesToRead = qMin(remaining, socket->bytesAvailable());

            QString currentCrcToken = socket->property("CrcToken").toString();
            QString currentFileName = socket->property("FileName").toString();

            if (bytesToRead > 0) {
                QByteArray chunk = socket->read(bytesToRead);
                file->write(chunk);
                current += chunk.size();
                socket->setProperty("BytesWritten", current);

                if (current > total) {
                    // 溢出保护
                    file->remove();
                    socket->disconnectFromHost();
                    return;
                }

                if (current == total) {
                    file->close();
                    file->deleteLater();

                    // =======================================================
                    // 4: 使用 SessionID 更新状态
                    // =======================================================
                    quint32 sid = socket->property("SessionId").toUInt();
                    bool updated = false;
                    QString clientName = "Unknown";

                    {
                        QWriteLocker locker(&m_registerInfosLock);
                        // 1. 先查索引
                        if (m_sessionIndex.contains(sid)) {
                            QString uuid = m_sessionIndex.value(sid);
                            // 2. 再查主表
                            if (m_registerInfos.contains(uuid)) {
                                m_registerInfos[uuid].crcToken = currentCrcToken;
                                clientName = m_registerInfos[uuid].username;
                                updated = true;
                                LOG_INFO(QString("🗺️ 已更新用户 %1 的地图CRC: %2 (via SessionID)")
                                             .arg(clientName, currentCrcToken));
                            }
                        }

                        // 备用方案：如果 SessionID 找不到 (极少见)，才去遍历 IP
                        if (!updated) {
                            QString senderIp = cleanAddress(socket->peerAddress().toString());
                            for (auto it = m_registerInfos.begin(); it != m_registerInfos.end(); ++it) {
                                if (it.value().publicIp == senderIp) {
                                    it.value().crcToken = currentCrcToken;
                                    updated = true;
                                    LOG_INFO(QString("🗺️ 已更新用户 %1 的地图CRC (via IP)").arg(it.value().username));
                                    break;
                                }
                            }
                        }
                    }

                    if (!updated) {
                        LOG_WARNING(QString("⚠️ 文件接收完成，但找不到对应用户 (Session: %1)").arg(sid));
                    }

                    sendUploadResult(socket, currentCrcToken, currentFileName, true, UPLOAD_OK);

                    updateMostFrequentCrc();
                    LOG_INFO("✅ [TCP] 接收完成");
                    socket->disconnectFromHost();
                    return;
                }
            } else {
                break;
            }
        }
    }
}

void NetManager::handleTcpCommandMessage(QTcpSocket *socket)
{
    // 循环读取，处理粘包
    while (socket->bytesAvailable() > 0) {

        // 1. 检查头部是否完整
        if (socket->bytesAvailable() < (qint64)sizeof(PacketHeader)) return;

        // 2. 预读头部 (Peek 不会移动读取指针)
        PacketHeader header;
        socket->peek(reinterpret_cast<char*>(&header), sizeof(PacketHeader));

        // 3. 校验魔数
        if (header.magic != PROTOCOL_MAGIC) {
            QString peerInfo = QString("%1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort());
            QString badMagic = QString::number(header.magic, 16).toUpper();

            qDebug().noquote() << "🛑 [TCP 协议违规]";
            qDebug().noquote() << QString("   ├─ 🔌 来源: %1").arg(peerInfo);
            qDebug().noquote() << QString("   ├─ ❌ Magic: 0x%1 (预期: 0x%2)").arg(badMagic, QString::number(PROTOCOL_MAGIC, 16).toUpper());
            qDebug().noquote() << "   └─ 🛡️ 动作: 断开连接";

            socket->disconnectFromHost();
            return;
        }

        // 4. 检查包体是否已完全到达
        qint64 totalPacketSize = sizeof(PacketHeader) + header.payloadLen;
        if (socket->bytesAvailable() < totalPacketSize) {
            return; // 数据不够，等待下次 readyRead
        }

        // 5. 正式读取完整数据包
        QByteArray packetData = socket->read(totalPacketSize);
        const PacketHeader *pHeader = reinterpret_cast<const PacketHeader*>(packetData.constData());
        const char *payload = packetData.constData() + sizeof(PacketHeader);

        // A. 先尝试获取当前已绑定的 ID
        QString currentClientId = socket->property("clientId").toString();

        // B. 如果未绑定，且包里有 SessionID，尝试进行“首次绑定”
        if (currentClientId.isEmpty() && pHeader->sessionId != 0) {
            QWriteLocker locker(&m_registerInfosLock);
            if (m_sessionIndex.contains(pHeader->sessionId)) {
                // 查到了！
                QString clientId = m_sessionIndex.value(pHeader->sessionId);

                // 立即更新 Socket 属性和 Map
                m_tcpClients.insert(clientId, socket);
                socket->setProperty("clientId", clientId);
                socket->setProperty("sessionId", pHeader->sessionId);

                qDebug().noquote() << "🔗 [TCP 控制通道绑定]";
                qDebug().noquote() << QString("   ├─ 🆔 Session: %1").arg(pHeader->sessionId);
                qDebug().noquote() << QString("   ├─ 👤 用户ID:  %1").arg(clientId);
                qDebug().noquote() << "   └─ ✅ 状态:    绑定成功";
            } else {
                qDebug().noquote() << "⚠️ [TCP 绑定失败]";
                qDebug().noquote() << QString("   ├─ 🆔 Session: %1").arg(pHeader->sessionId);
                qDebug().noquote() << "   └─ ❌ 原因:    无效的会话 ID";
            }
        }

        // 6. 处理具体指令
        switch (static_cast<PacketType>(pHeader->command)) {

        case PacketType::C_S_HEARTBEAT:
        case PacketType::C_S_PING:
        {
            SCPongPacket pong;
            pong.status = 1;
            sendTcpPacket(socket, PacketType::S_C_PONG, &pong, sizeof(pong));
        }
        break;

        case PacketType::C_S_COMMAND:
            if (pHeader->payloadLen >= sizeof(CSCommandPacket)) {
                // 🛡️ 安全检查：如果到现在还没 ClientID，说明这是个未授权的连接发来的指令
                if (currentClientId.isEmpty()) {
                    qDebug().noquote() << "🛑 [指令拒绝]";
                    qDebug().noquote() << "   ├─ ❌ 原因: 未鉴权连接 (无有效 SessionID)";
                    qDebug().noquote() << "   └─ 🛡️ 动作: 忽略指令";
                    break;
                }

                const CSCommandPacket *cmdPkt = reinterpret_cast<const CSCommandPacket*>(payload);

                QString cmd = QString::fromUtf8(cmdPkt->command, strnlen(cmdPkt->command, sizeof(cmdPkt->command)));
                QString text = QString::fromUtf8(cmdPkt->text, strnlen(cmdPkt->text, sizeof(cmdPkt->text)));
                QString user = QString::fromUtf8(cmdPkt->username, strnlen(cmdPkt->username, sizeof(cmdPkt->username)));

                qDebug().noquote() << "🎮 [TCP 指令接收]";
                qDebug().noquote() << QString("   ├─ 👤 发送者: %1").arg(user);
                qDebug().noquote() << QString("   ├─ 🔗 来源ID: %1").arg(currentClientId);
                qDebug().noquote() << QString("   ├─ 💬 指令:   %1").arg(cmd);
                if (!text.isEmpty()) {
                    qDebug().noquote() << QString("   ├─ 📄 参数:   %1").arg(text);
                }
                qDebug().noquote() << "   └─ 🚀 动作:    分发至 BotManager";

                // 这里直接用 currentClientId，它一定是最新的
                emit commandReceived(user, currentClientId, cmd, text);
            }
            break;

        default:
            // 处理未知包
            qDebug().noquote() << "❓ [TCP 未知指令]";
            qDebug().noquote() << QString("   └─ 🔢 Command: %1").arg(pHeader->command);
            break;
        }
    }
}

void NetManager::onNewTcpConnection()
{
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();

        if (!m_watchdog.checkTcpConnection(socket->peerAddress())) {
            LOG_WARNING(QString("🛡️ 拒绝恶意 IP 连接请求: %1").arg(socket->peerAddress().toString()));
            socket->close(); // 立即关闭
            socket->deleteLater();
            continue; // 跳过这个连接
        }

        connect(socket, &QTcpSocket::readyRead, this, &NetManager::onTcpReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &NetManager::onTcpDisconnected);

        LOG_INFO(QString("📥 TCP 连接来自: %1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort()));
    }
}

void NetManager::onTcpDisconnected() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) socket->deleteLater();
}

bool NetManager::sendEnterRoomCommand(const QString &clientId, quint64 port)
{
    // 1. 检查 TCP 连接是否存在
    if (!m_tcpClients.contains(clientId)) {
        qDebug().noquote() << "🛑 [指令发送失败]";
        qDebug().noquote() << QString("   ├─ 🎯 目标: %1").arg(clientId);
        qDebug().noquote() << "   └─ ❌ 原因: 用户没有记录";
        return false;
    }

    QTcpSocket *socket = m_tcpClients[clientId];

    // 2. 检查 Socket 状态
    if (socket->state() != QAbstractSocket::ConnectedState) {
        m_tcpClients.remove(clientId); // 清理死链接
        qDebug().noquote() << "🛑 [指令发送失败]";
        qDebug().noquote() << QString("   ├─ 🎯 目标: %1").arg(clientId);
        qDebug().noquote() << "   └─ ❌ 原因: Socket 连接已断开 (清理僵尸连接)";
        return false;
    }

    // 3. 构造二进制 payload
    SCCommandPacket pkt;
    memset(&pkt, 0, sizeof(pkt));

    // 填充 ClientID (截断保护)
    strncpy(pkt.clientId, clientId.toUtf8().constData(), sizeof(pkt.clientId) - 1);

    // 填充指令
    const char *cmd = "ENTER_ROOM";
    strncpy(pkt.command, cmd, sizeof(pkt.command) - 1);

    // 填充参数 (端口)
    QString portStr = QString::number(port);
    strncpy(pkt.text, portStr.toUtf8().constData(), sizeof(pkt.text) - 1);

    // 4. 发送 PacketType::S_C_COMMAND
    bool ok = sendTcpPacket(socket, PacketType::S_C_COMMAND, &pkt, sizeof(pkt));

    // 5. 打印结果日志
    if (ok) {
        qDebug().noquote() << "🚀 [自动进入指令分发]";
        qDebug().noquote() << QString("   ├─ 👤 目标用户: %1").arg(clientId);
        qDebug().noquote() << QString("   ├─ 🚪 房间端口: %1").arg(port);
        qDebug().noquote() << "   └─ ✅ 发送状态: 成功 (TCP)";
    } else {
        qDebug().noquote() << "🛑 [指令发送失败]";
        qDebug().noquote() << QString("   ├─ 🎯 目标: %1").arg(clientId);
        qDebug().noquote() << "   └─ ❌ 原因: TCP Socket 写入错误";
    }

    return ok;
}

bool NetManager::sendToClient(const QString &clientId, const QByteArray &data)
{
    if (!m_tcpClients.contains(clientId)) {
        LOG_WARNING(QString("❌ 发送 TCP 失败: 找不到在线的 clientId %1").arg(clientId));
        return false;
    }

    QTcpSocket *socket = m_tcpClients[clientId];
    if (socket->state() != QAbstractSocket::ConnectedState) {
        m_tcpClients.remove(clientId); // 清理死链接
        return false;
    }

    qint64 written = socket->write(data);
    socket->flush();

    LOG_INFO(QString("🚀 TCP 发送 %1 字节 -> %2").arg(written).arg(clientId));
    return true;
}

void NetManager::sendUploadResult(QTcpSocket* socket, const QString& crc, const QString& fileName, bool success, UploadErrorCode reason)
{
    if (!socket || !m_udpSocket) return;

    QString senderIp = cleanAddress(socket->peerAddress().toString());
    QHostAddress targetAddr;
    quint64 targetPort = 0;
    bool found = false;
    quint32 fallbackSessionId = socket->property("SessionId").toUInt(); // 获取 SessionID

    QReadLocker locker(&m_registerInfosLock);

    // ---------------------------------------------------------
    // 策略 1: 优先尝试通过 IP 匹配
    // ---------------------------------------------------------
    for (const auto &info : qAsConst(m_registerInfos)) {
        // 如果 SessionID 存在，优先匹配 SessionID (最准确)
        if (fallbackSessionId != 0 && info.sessionId == fallbackSessionId) {
            targetAddr = QHostAddress(info.publicIp);
            targetPort = info.publicPort;
            found = true;
            // 如果 IP 变了，顺便打印个日志
            if (info.publicIp != senderIp) {
                LOG_INFO(QString("🔄 [TCP/UDP关联] IP不一致 (TCP:%1 vs UDP:%2)，使用 SessionID:%3 修正")
                             .arg(senderIp, info.publicIp).arg(fallbackSessionId));
            }
            break;
        }

        // 如果没有 SessionID，才尝试 IP 匹配
        if (fallbackSessionId == 0 && info.publicIp == senderIp) {
            targetAddr = QHostAddress(info.publicIp);
            targetPort = info.publicPort;
            found = true;
            break;
        }
    }

    // ---------------------------------------------------------
    // 策略 2: 如果遍历完还没找到
    // ---------------------------------------------------------
    if (!found && fallbackSessionId != 0) {
        // 尝试通过索引直接查找
        if (m_sessionIndex.contains(fallbackSessionId)) {
            QString uuid = m_sessionIndex.value(fallbackSessionId);
            if (m_registerInfos.contains(uuid)) {
                const RegisterInfo &info = m_registerInfos[uuid];
                targetAddr = QHostAddress(info.publicIp);
                targetPort = info.publicPort;
                found = true;
                LOG_INFO(QString("✅ 通过索引找回用户: %1 (Session: %2)").arg(uuid).arg(fallbackSessionId));
            }
        }
    }

    locker.unlock(); // 尽早解锁

    if (!found) {
        LOG_WARNING(QString("⚠️ 上传结束，无法找到 UDP 用户 (IP: %1, SessionID: %2)")
                        .arg(senderIp)
                        .arg(fallbackSessionId));
        return;
    }

    SCUploadResultPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    strncpy(pkt.crcHex, crc.toUtf8().constData(), sizeof(pkt.crcHex) - 1);
    strncpy(pkt.fileName, fileName.toUtf8().constData(), sizeof(pkt.fileName) - 1);
    pkt.status = success ? 1 : 0;
    pkt.reason = static_cast<quint8>(reason);

    sendUdpPacket(targetAddr, targetPort, S_C_UPLOADRESULT, &pkt, sizeof(pkt));

    LOG_INFO(QString("📤 发送上传回执 -> %1:%2 (Session: %5) | 文件: %3 | 结果: %4")
                 .arg(targetAddr.toString()).arg(targetPort)
                 .arg(fileName, success ? "成功" : "失败")
                 .arg(fallbackSessionId));
}

// ==================== 定时任务 ====================

void NetManager::onCleanupTimeout()
{
    cleanupExpiredClients();
}

void NetManager::onBroadcastTimeout()
{
    broadcastServerInfo();
}

void NetManager::broadcastServerInfo()
{
    if (!m_enableBroadcast || !m_udpSocket) return;
    QByteArray msg = QString("WAR3BOT_SERVER|%1").arg(m_listenPort).toUtf8();
    m_udpSocket->writeDatagram(msg, QHostAddress::Broadcast, m_broadcastPort);
}

void NetManager::updateMostFrequentCrc()
{
    m_crcCounts.clear();

    {
        QReadLocker locker(&m_registerInfosLock);
        for (const RegisterInfo &peer : qAsConst(m_registerInfos)) {
            if (!peer.crcToken.isEmpty()) {
                m_crcCounts[peer.crcToken]++;
            }
        }
    }

    // 找出最大值
    QString maxCrcToken;
    int maxCount = 0;

    QMapIterator<QString, int> i(m_crcCounts);
    while (i.hasNext()) {
        i.next();
        if (i.value() > maxCount) {
            maxCount = i.value();
            maxCrcToken = i.key();
        }
    }

    if (!maxCrcToken.isEmpty()) {
        QString path = QCoreApplication::applicationDirPath() + "/war3files/crc/" + maxCrcToken;
        QDir dir(path);

        // 确保该目录确实存在 .j 文件，否则设置了也没用
        if (dir.exists() && QFile::exists(path + "/common.j")) {
            War3Map::setPriorityCrcDirectory(path);
            LOG_INFO(QString("🔥 更新热门地图 CRC: %1 (在线人数: %2)").arg(maxCrcToken).arg(maxCount));
        } else {
            // 目录不完整，回退
            War3Map::setPriorityCrcDirectory("");
        }
    } else {
        // 没有热门地图，回退
        War3Map::setPriorityCrcDirectory("");
    }
}

// ==================== 清理函数 ====================

void NetManager::cleanupResources()
{
    if (m_cleanupTimer) m_cleanupTimer->deleteLater();
    if (m_broadcastTimer) m_broadcastTimer->deleteLater();
    if (m_udpSocket) m_udpSocket->deleteLater();
    if (m_tcpServer) m_tcpServer->deleteLater();
    if (m_settings) m_settings->deleteLater();
    m_cleanupTimer = nullptr;
    m_broadcastTimer = nullptr;
    m_udpSocket = nullptr;
    m_tcpServer = nullptr;
    m_settings = nullptr;
}

void NetManager::cleanupExpiredClients()
{
    QWriteLocker locker(&m_registerInfosLock);
    quint64 now = QDateTime::currentMSecsSinceEpoch();

    // 定义临时结构体存储待删除信息，避免在 remove 时丢失用户名等信息
    struct ExpiredClient {
        QString uuid;
        QString username;
        quint64 silenceDuration;
    };
    QList<ExpiredClient> expiredList;

    // 1. 扫描阶段
    for (auto it = m_registerInfos.begin(); it != m_registerInfos.end(); ++it) {
        quint64 silence = now - it.value().lastSeen;
        if (silence > m_peerTimeout) {
            expiredList.append({it.key(), it.value().username, silence});
        }
    }

    // 2. 如果没有过期用户，直接返回，保持日志清爽
    if (expiredList.isEmpty()) {
        return;
    }

    // 3. 执行清理并打印树状日志
    qDebug().noquote() << "🗑️ [会话超时清理任务]";
    qDebug().noquote() << QString("   ├─ ⏱️ 超时阈值: %1 ms").arg(m_peerTimeout);
    qDebug().noquote() << QString("   ├─ 📉 清理数量: %1 人").arg(expiredList.size());

    for (int i = 0; i < expiredList.size(); ++i) {
        const auto &client = expiredList.at(i);
        bool isLast = (i == expiredList.size() - 1);
        QString prefix = isLast ? "   └─ " : "   ├─ ";

        // 打印详情：显示用户名、UUID前8位、沉默秒数
        qDebug().noquote() << QString("%1🚫 移除: %2 (UUID: %3...) | 已沉默: %4s")
                                  .arg(prefix, client.username, client.uuid.left(8))
                                  .arg(client.silenceDuration / 1000.0, 0, 'f', 1);

        // 执行移除
        removeClientInternal(client.uuid);
    }
}

void NetManager::removeClientInternal(const QString& uuid)
{
    if (!m_registerInfos.contains(uuid)) return;

    // 1. 获取 SessionID
    quint32 sid = m_registerInfos[uuid].sessionId;

    // 2. 删索引
    m_sessionIndex.remove(sid);

    // 3. 删主表
    m_registerInfos.remove(uuid);
}

// ==================== 工具函数 ====================

QList<RegisterInfo> NetManager::getOnlinePlayers() const
{
    QReadLocker locker(&m_registerInfosLock);
    return m_registerInfos.values();
}

quint16 NetManager::calculateCRC16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    const char *p = data.constData();
    int len = data.size();

    for (int i = 0; i < len; i++) {
        unsigned char x = (crc >> 8) ^ (unsigned char)p[i];
        x ^= x >> 4;
        crc = (crc << 8) ^ (quint16)(x << 12) ^ (quint16)(x << 5) ^ (quint16)x;
    }
    return crc;
}

bool NetManager::isValidFileName(const QString &name)
{
    QString safe = QFileInfo(name).fileName();
    return (safe.compare("common.j", Qt::CaseInsensitive) == 0 ||
            safe.compare("blizzard.j", Qt::CaseInsensitive) == 0 ||
            safe.compare("war3map.j", Qt::CaseInsensitive) == 0);
}

bool NetManager::isClientRegistered(const QString &clientId) const
{
    if (clientId.isEmpty()) return false;
    QReadLocker locker(&m_registerInfosLock);
    return m_registerInfos.contains(clientId);
}

QString NetManager::cleanAddress(const QHostAddress &address) {
    QString ip = address.toString();
    return ip.startsWith("::ffff:") ? ip.mid(7) : ip;
}

QString NetManager::cleanAddress(const QString &address) {
    return address.startsWith("::ffff:") ? address.mid(7) : address;
}

QString NetManager::natTypeToString(NATType type)
{
    switch (type) {
    case NAT_UNKNOWN:
        return QStringLiteral("未知");
    case NAT_OPEN_INTERNET:
        return QStringLiteral("开放互联网");
    case NAT_FULL_CONE:
        return QStringLiteral("完全锥形NAT");
    case NAT_RESTRICTED_CONE:
        return QStringLiteral("限制锥形NAT");
    case NAT_PORT_RESTRICTED_CONE:
        return QStringLiteral("端口限制锥形NAT");
    case NAT_SYMMETRIC:
        return QStringLiteral("对称型NAT");
    case NAT_SYMMETRIC_UDP_FIREWALL:
        return QStringLiteral("对称型UDP防火墙");
    case NAT_BLOCKED:
        return QStringLiteral("被阻挡");
    case NAT_DOUBLE_NAT:
        return QStringLiteral("双重NAT");
    case NAT_CARRIER_GRADE:
        return QStringLiteral("运营商级NAT");
    case NAT_IP_RESTRICTED:
        return QStringLiteral("IP限制型NAT");
    default:
        return QStringLiteral("未知类型 (%1)").arg(type);
    }
}

QString NetManager::packetTypeToString(PacketType type)
{
    switch (type) {
    case PacketType::C_S_HEARTBEAT:   return "C_S_HEARTBEAT";
    case PacketType::C_S_REGISTER:    return "C_S_REGISTER";
    case PacketType::S_C_REGISTER:    return "S_C_REGISTER";
    case PacketType::C_S_UNREGISTER:  return "C_S_UNREGISTER";
    case PacketType::C_S_COMMAND:     return "C_S_COMMAND";
    case PacketType::S_C_COMMAND:     return "S_C_COMMAND";
    case PacketType::C_S_PING:        return "C_S_PING";
    case PacketType::S_C_PONG:        return "S_C_PONG";
    case PacketType::S_C_UPLOADRESULT: return "S_C_UPLOADRESULT";
    default: return QString("UNKNOWN(%1)").arg(static_cast<int>(type));
    }
}

bool NetManager::isRunning() const { return m_isRunning; }

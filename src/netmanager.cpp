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

bool NetManager::startServer(quint16 port, const QString &configFile)
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

bool NetManager::bindSocket(quint16 port)
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

qint64 NetManager::sendPacket(const QHostAddress &target, quint16 port, PacketType type, const void *payload, quint16 payloadLen)
{
    if (!m_udpSocket) return -1;

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
    if (sent < 0) {
        LOG_ERROR(QString("❌ 发送失败: %1").arg(m_udpSocket->errorString()));
    }
    return sent;
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
    quint16 recvChecksum = header->checksum;
    header->checksum = 0;
    if (calculateCRC16(data) != recvChecksum) {
        LOG_WARNING("CRC 校验失败");
        return;
    }

    // 3. 分发
    char *payload = data.data() + sizeof(PacketHeader);
    QHostAddress sender = datagram.senderAddress();
    quint16 port = datagram.senderPort();

    switch (static_cast<PacketType>(header->command)) {
    case PacketType::C_S_REGISTER:
        if (header->payloadLen >= sizeof(CSRegisterPacket)) {
            handleRegister(header, reinterpret_cast<CSRegisterPacket*>(payload), sender, port);
        }
        break;
    case PacketType::C_S_UNREGISTER:
        handleUnregister(header);
        break;
    case PacketType::C_S_HEARTBEAT:
        handleHeartbeat(header, sender, port);
        break;
    case PacketType::C_S_PING:
        handlePing(header, sender, port);
        break;
    case PacketType::C_S_COMMAND:
        if (header->payloadLen >= sizeof(CSCommandPacket)) {
            handleCommand(header, reinterpret_cast<CSCommandPacket*>(payload));
        }
        break;
    case PacketType::C_S_CHECKMAPCRC:
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

void NetManager::handleRegister(const PacketHeader *header, const CSRegisterPacket *packet, const QHostAddress &senderAddr, quint16 senderPort)
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

    sendPacket(senderAddr, senderPort, PacketType::S_C_REGISTER, &resp, sizeof(resp));
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

void NetManager::handlePing(const PacketHeader *header, const QHostAddress &senderAddr, quint16 senderPort)
{
    // 1. 检查 SessionID 是否存在
    bool isRegistered = false;

    if (header->sessionId != 0) {
        QReadLocker locker(&m_registerInfosLock);
        if (m_sessionIndex.contains(header->sessionId)) {
            isRegistered = true;
        }
    }

    // 2. 构造带状态的 PONG 包
    SCPongPacket pongPkt;
    pongPkt.status = isRegistered ? 1 : 0;

    // 3. 发送
    sendPacket(senderAddr, senderPort, PacketType::S_C_PONG, &pongPkt, sizeof(pongPkt));

    // 只有已注册才打印
    if (isRegistered) {
        LOG_DEBUG(QString("🏓 Pong -> %1 (Session: %2)").arg(senderAddr.toString()).arg(header->sessionId));
    } else {
        // LOG_DEBUG(QString("⚠️ 未注册 Ping -> %1").arg(senderAddr.toString()));
    }
}

void NetManager::handleHeartbeat(const PacketHeader *header, const QHostAddress &senderAddr, quint16 senderPort)
{
    if (header->sessionId == 0) return;

    bool found = false;
    {
        QWriteLocker locker(&m_registerInfosLock);
        if (m_sessionIndex.contains(header->sessionId)) {
            QString uuid = m_sessionIndex[header->sessionId];
            if (m_registerInfos.contains(uuid)) {
                m_registerInfos[uuid].lastSeen = QDateTime::currentMSecsSinceEpoch();
                m_registerInfos[uuid].publicIp = cleanAddress(senderAddr);
                m_registerInfos[uuid].publicPort = senderPort;
                found = true;
            }
        }
    }

    if (found) {
        sendPacket(senderAddr, senderPort, PacketType::S_C_PONG);
    } else {
        SCPongPacket pongPkt;
        pongPkt.status = 0;
        sendPacket(senderAddr, senderPort, PacketType::S_C_PONG, &pongPkt, sizeof(pongPkt));
        LOG_WARNING(QString("⚠️ 收到失效心跳 (Session %1)，已通知客户端重连").arg(header->sessionId));
    }
}

void NetManager::handleCommand(const PacketHeader *header, const CSCommandPacket *packet)
{
    QString clientId;
    {
        QReadLocker locker(&m_registerInfosLock);
        if (m_sessionIndex.contains(header->sessionId)) {
            clientId = m_sessionIndex.value(header->sessionId);
            if (!m_registerInfos.contains(clientId)) {
                LOG_WARNING(QString("⚠️ 数据不一致: 索引有 Session %1 但找不到 ClientInfo").arg(header->sessionId));
                return;
            }
        } else {
            LOG_WARNING(QString("⚠️ 收到指令，但 SessionID 无效: %1").arg(header->sessionId));
            return;
        }
    }

    QString cmd = QString::fromUtf8(packet->command, strnlen(packet->command, sizeof(packet->command)));
    QString text = QString::fromUtf8(packet->text, strnlen(packet->text, sizeof(packet->text)));
    QString user = QString::fromUtf8(packet->username, strnlen(packet->username, sizeof(packet->username)));

    emit commandReceived(user, clientId, cmd, text);
}

void NetManager::handleCheckMapCRC(const PacketHeader *header, const CSCheckMapCRCPacket *packet, const QHostAddress &senderAddr, quint16 senderPort)
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

    sendPacket(senderAddr, senderPort, PacketType::S_C_CHECKMAPCRC, &resp, sizeof(resp));
}

// ==================== TCP ====================

void NetManager::onTcpReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    if (socket->property("ConnType").isValid()) {
        QString type = socket->property("ConnType").toString();
        if (type == "UPLOAD") handleTcpUploadMessage(socket);
        else if (type == "CONTROL") handleTcpControlMessage(socket);
        return;
    }

    if (socket->bytesAvailable() < 4) return;
    QByteArray magic = socket->peek(4);

    if (magic == "W3UP") {
        socket->setProperty("ConnType", "UPLOAD");
        handleTcpUploadMessage(socket);
    } else {
        socket->setProperty("ConnType", "CONTROL");
        handleTcpControlMessage(socket);
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

void NetManager::handleTcpControlMessage(QTcpSocket *socket)
{
    while (socket->canReadLine()) {
        // 1. 读取一行数据
        QByteArray data = socket->readLine();
        QString line = QString::fromUtf8(data).trimmed();

        if (line.isEmpty()) continue;

        LOG_INFO(QString("🎮 收到指令: %1").arg(line));

        QStringList parts = line.split('|');
        if (parts.isEmpty()) continue;

        QString cmd = parts[0].toUpper();

        if (cmd == "CONTROL_LOGIN_CLIENTID") {
            QString clientId = (parts.size() > 1) ? parts[1].trimmed() : "";

            if (!clientId.isEmpty()) {
                // 1. 记录连接
                m_tcpClients.insert(clientId, socket);

                // 2. 设置属性 (用于断开时清理)
                socket->setProperty("clientId", clientId);

                LOG_INFO(QString("✅ 控制通道已绑定用户: %1").arg(clientId));

                // 3. 回复成功
                socket->write("CONTROL_LOGIN_RESPONSE|OK\n");
            } else {
                LOG_WARNING("⚠️ 登录失败: clientId 为空");
                // 4. 回复失败
                socket->write("CONTROL_LOGIN_RESPONSE|EMPTY_clientId\n");
            }
        }
        else if (cmd == "PING") {
            socket->write("PONG\n");
        }
        else {
            LOG_WARNING(QString("❓ 未知 TCP 控制指令: %1").arg(cmd));
        }

        socket->flush();
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

bool NetManager::sendControlEnterRoom(const QString &clientId, quint16 port)
{
    QString command = QString("CONTROL_ENTER_ROOM|%2\n").arg(port);

    if (sendToClient(clientId, command.toUtf8())) {
        LOG_INFO(QString("🚀 已发送自动进入指令给 [%1]: %2").arg(clientId, command.trimmed()));
        return true;
    } else {
        LOG_WARNING(QString("❌ 发送自动进入指令失败: 找不到在线的 clientId [%1]").arg(clientId));
        return false;
    }
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
    quint16 targetPort = 0;
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

    sendPacket(targetAddr, targetPort, PacketType::S_C_UPLOADRESULT, &pkt, sizeof(pkt));

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
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    QStringList timeOutUsers;
    for (auto it = m_registerInfos.begin(); it != m_registerInfos.end(); ++it) {
        if (now - it.value().lastSeen > m_peerTimeout) {
            timeOutUsers.append(it.key());
        }
    }

    for (const QString& uuid : timeOutUsers) {
        LOG_INFO(QString("🗑️ 超时移除: %1").arg(uuid));
        removeClientInternal(uuid);
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

bool NetManager::isRunning() const { return m_isRunning; }

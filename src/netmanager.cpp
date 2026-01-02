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

// ==================== Socket 管理与启动 (保持逻辑但简化日志) ====================

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

// ==================== 核心：二进制发送逻辑 ====================

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

// ==================== 核心：二进制接收逻辑 ====================

void NetManager::onUDPReadyRead()
{
    while (m_udpSocket && m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        processIncomingDatagram(datagram);
    }
}

void NetManager::processIncomingDatagram(const QNetworkDatagram &datagram)
{
    QByteArray data = datagram.data();
    if (data.size() < (int)sizeof(PacketHeader)) return;

    PacketHeader *header = reinterpret_cast<PacketHeader*>(data.data());

    // 1. 基础校验
    if (header->magic != PROTOCOL_MAGIC || header->version != PROTOCOL_VERSION) {
        LOG_WARNING("无效的协议魔数或版本");
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

            LOG_INFO("--------------------[ 👋 用户注销 ]--------------------");
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
    Q_UNUSED(header);
    sendPacket(senderAddr, senderPort, PacketType::S_C_PONG);
    LOG_DEBUG(QString("🏓 Pong -> %1").arg(senderAddr.toString()));
}

void NetManager::handleHeartbeat(const PacketHeader *header, const QHostAddress &senderAddr, quint16 senderPort)
{
    if (header->sessionId == 0) return;

    QWriteLocker locker(&m_registerInfosLock);
    for (auto &info : m_registerInfos) {
        if (info.sessionId == header->sessionId) {
            info.lastSeen = QDateTime::currentMSecsSinceEpoch();
            info.publicIp = cleanAddress(senderAddr);
            info.publicPort = senderPort;

            locker.unlock();
            sendPacket(senderAddr, senderPort, PacketType::S_C_PONG);
            return;
        }
    }
}

void NetManager::handleCommand(const PacketHeader *header, const CSCommandPacket *packet)
{
    // 验证 SessionID
    QString clientId;
    {
        QReadLocker locker(&m_registerInfosLock);
        bool found = false;
        for (const auto &info : qAsConst(m_registerInfos)) {
            if (info.sessionId == header->sessionId) {
                clientId = info.clientId;
                found = true;
                break;
            }
        }
        if (!found) {
            LOG_WARNING("⚠️ 收到 Bot指令，但 SessionID 无效");
            return;
        }
    }

    QString cmd = QString::fromUtf8(packet->command, strnlen(packet->command, sizeof(packet->command)));
    QString text = QString::fromUtf8(packet->text, strnlen(packet->text, sizeof(packet->text)));
    QString user = QString::fromUtf8(packet->username, strnlen(packet->username, sizeof(packet->username)));

    LOG_INFO(QString("🤖 指令 [%1]: %2 %3").arg(user, cmd, text));
    emit botCommandReceived(user, clientId, cmd, text);
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
        m_pendingUploadTokens.insert(crcHex);
        LOG_INFO(QString("🔍 请求CRC %1 不存在，等待上传").arg(crcHex));
    } else {
        LOG_INFO(QString("✅ 请求CRC %1 已存在").arg(crcHex));
    }

    sendPacket(senderAddr, senderPort, PacketType::S_C_CHECKMAPCRC, &resp, sizeof(resp));
}

// ==================== TCP / 辅助功能 (基本保持不变) ====================

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
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_5_15);

    while (socket->bytesAvailable() > 0) {
        if (!socket->property("HeaderParsed").toBool()) {

            if (socket->bytesAvailable() < 4 + 8 + 4) return;

            // 1. 验证 Magic "W3UP"
            QByteArray magic = socket->read(4);

            if (magic != "W3UP") {
                LOG_WARNING("❌ TCP 非法连接: 魔数错误");
                socket->disconnectFromHost();
                return;
            }

            // 2. 读取并验证 CRC Token
            QByteArray tokenBytes = socket->read(8);
            QString crcToken = QString::fromLatin1(tokenBytes).trimmed();

            {
                QReadLocker locker(&m_tokenLock);
                if (!m_pendingUploadTokens.contains(crcToken)) {
                    LOG_WARNING(QString("❌ TCP 拒绝上传: 未授权的 Token (%1)").arg(crcToken));
                    socket->disconnectFromHost();
                    return;
                }
            }

            // 3. 读取文件名长度
            quint32 nameLen;
            in >> nameLen;

            // 🛡️ 安全检查: 文件名长度限制
            if (nameLen > 256) {
                LOG_WARNING("❌ TCP 拒绝: 文件名过长");
                socket->disconnectFromHost();
                return;
            }

            // 4. 读取文件名
            if (socket->bytesAvailable() < nameLen) return;
            QByteArray nameBytes = socket->read(nameLen);
            QString rawFileName = QString::fromUtf8(nameBytes);

            // 🛡️ 安全检查: 强制使用 QFileInfo 取文件名，防止路径遍历
            QString fileName = QFileInfo(rawFileName).fileName();

            if (!isValidFileName(fileName)) {
                LOG_WARNING(QString("❌ TCP 拒绝: 非法文件名 %1").arg(rawFileName));
                socket->disconnectFromHost();
                return;
            }

            // 5. 读取文件大小
            if (socket->bytesAvailable() < 8) return;
            qint64 fileSize;
            in >> fileSize;

            // 🛡️ 安全检查: 大小限制 (例如最大 20MB)
            if (fileSize <= 0 || fileSize > 20 * 1024 * 1024) {
                LOG_WARNING("❌ TCP 拒绝: 文件过大");
                socket->disconnectFromHost();
                return;
            }

            // 6. 准备文件写入
            QString saveDir = QCoreApplication::applicationDirPath() + "/war3files/crc/" + crcToken;

            QDir dir(saveDir);
            if (!dir.exists()) {
                if (!dir.mkpath(".")) {
                    LOG_ERROR("❌ 无法创建目录: " + saveDir);
                    socket->disconnectFromHost();
                    return;
                }
            }

            QString safeFileName = QFileInfo(rawFileName).fileName();
            if (!isValidFileName(safeFileName)) {
                LOG_WARNING(QString("❌ TCP 拒绝上传: 非法文件名 (%1)").arg(fileName));
                socket->disconnectFromHost();
                return;
            }
            QString savePath = saveDir + "/" + safeFileName;

            // 把 crcToken 存到 socket 属性里，传给下一步用
            socket->setProperty("CrcToken", crcToken);

            QFile *file = new QFile(savePath);
            if (!file->open(QIODevice::WriteOnly)) {
                LOG_ERROR("❌ 无法创建文件: " + savePath);
                delete file;
                socket->disconnectFromHost();
                return;
            }

            socket->setProperty("FilePtr", QVariant::fromValue((void*)file));
            socket->setProperty("BytesTotal", fileSize);
            socket->setProperty("BytesWritten", (qint64)0);
            socket->setProperty("HeaderParsed", true);

            LOG_INFO(QString("📥 [TCP] 开始接收文件: %1 (CRC: %2)").arg(fileName, crcToken));
        }

        // 数据接收部分
        if (socket->property("HeaderParsed").toBool()) {
            QFile *file = static_cast<QFile*>(socket->property("FilePtr").value<void*>());
            qint64 total = socket->property("BytesTotal").toLongLong();
            qint64 current = socket->property("BytesWritten").toLongLong();

            // 计算还需要读多少
            qint64 remaining = total - current;

            // 只读取需要的部分，防止多读了下一个包的数据 (粘包处理)
            qint64 bytesToRead = qMin(remaining, socket->bytesAvailable());

            if (bytesToRead > 0) {
                QByteArray chunk = socket->read(bytesToRead);
                file->write(chunk);
                current += chunk.size();
                socket->setProperty("BytesWritten", current);

                // 🛡️ 安全检查: 防止超量写入
                if (current > total) {
                    LOG_ERROR("❌ 写入溢出，断开连接");
                    file->remove();
                    socket->disconnectFromHost();
                    return;
                }

                if (current == total) {
                    file->close();
                    file->deleteLater();

                    // 1. 取出保存的 CRC
                    QString uploadedCrc = socket->property("CrcToken").toString();

                    // 2. 获取发送者 IP (处理 IPv6 映射)
                    QString senderIp = cleanAddress(socket->peerAddress().toString());

                    if (!uploadedCrc.isEmpty()) {
                        QWriteLocker locker(&m_registerInfosLock);
                        bool peerFound = false;

                        // 3. 遍历查找 IP 匹配的用户
                        for (auto it = m_registerInfos.begin(); it != m_registerInfos.end(); ++it) {
                            if (it.value().publicIp == senderIp) {
                                it.value().crcToken = uploadedCrc;
                                peerFound = true;
                                LOG_INFO(QString("🗺️ 已更新用户 %1 的地图CRC: %2")
                                             .arg(it.value().clientId, uploadedCrc));
                            }
                        }

                        if (!peerFound) {
                            LOG_WARNING(QString("⚠️ 文件接收完成，但未找到 IP 为 %1 的用户来绑定 CRC").arg(senderIp));
                        }
                    }
                    // 上传完成后，重新计算热门 CRC
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

void NetManager::onNewTcpConnection() {
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &NetManager::onTcpReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &NetManager::onTcpDisconnected);
        LOG_INFO(QString("📥 TCP 连接: %1").arg(socket->peerAddress().toString()));
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
// ==================== 定时任务 ====================

void NetManager::onCleanupTimeout()
{
    cleanupExpiredPeers();
}

void NetManager::cleanupExpiredPeers()
{
    QWriteLocker locker(&m_registerInfosLock);
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = m_registerInfos.begin();
    while (it != m_registerInfos.end()) {
        if (now - it.value().lastSeen > m_peerTimeout) {
            LOG_INFO(QString("🗑️ 超时移除: %1").arg(it.key()));
            it = m_registerInfos.erase(it);
        } else {
            ++it;
        }
    }
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

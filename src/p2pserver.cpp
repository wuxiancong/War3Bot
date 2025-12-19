#include "p2pserver.h"
#include "war3map.h"
#include "logger.h"
#include <QDir>
#include <QTimer>
#include <QPointer>
#include <QDateTime>
#include <QDataStream>
#include <QCoreApplication>
#include <QRandomGenerator>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QCryptographicHash>

#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

P2PServer::P2PServer(QObject *parent)
    : QObject(parent)
    , m_peerTimeout(300000)
    , m_listenPort(0)
    , m_cleanupInterval(60000)
    , m_enableBroadcast(false)
    , m_broadcastInterval(30000)
    , m_broadcastPort(6112)
    , m_isRunning(false)
    , m_settings(nullptr)
    , m_udpSocket(nullptr)
    , m_tcpServer(nullptr)
    , m_cleanupTimer(nullptr)
    , m_broadcastTimer(nullptr)
    , m_totalRequests(0)
    , m_totalResponses(0)
    , m_nextVirtualIp(0x1A000001)
{
}

P2PServer::~P2PServer()
{
    stopServer();
}

bool P2PServer::startServer(quint16 port, const QString &configFile)
{
    if (m_isRunning) {
        LOG_WARNING("服务器已在运行中");
        return true;
    }

    // 加载配置文件
    m_settings = new QSettings(configFile, QSettings::IniFormat, this);
    loadConfiguration();

    // 创建UDP socket
    m_udpSocket = new QUdpSocket(this);

    // 先绑定socket，然后再设置选项
    if (!bindSocket(port)) {
        cleanupResources();
        return false;
    }

    // 绑定成功后再设置socket选项
    setupSocketOptions();

    connect(m_udpSocket, &QUdpSocket::readyRead, this, &P2PServer::onUDPReadyRead);

    // 创建TCP server
    m_tcpServer = new QTcpServer(this);
    if (!m_tcpServer->listen(QHostAddress::AnyIPv4, port)) {
        LOG_ERROR(QString("❌ TCP 服务器启动失败: %1").arg(m_tcpServer->errorString()));
        cleanupResources();
        return false;
    }

    connect(m_tcpServer, &QTcpServer::newConnection, this, &P2PServer::onNewTcpConnection);

    m_listenPort = m_udpSocket->localPort();

    m_isRunning = true;

    // 启动定时器
    setupTimers();

    LOG_INFO(QString("✅ P2P服务器已在端口 %1 启动").arg(m_listenPort));
    logServerConfiguration();

    emit serverStarted(port);
    return true;
}

void P2PServer::loadConfiguration()
{
    m_peerTimeout = m_settings->value("server/peer_timeout", 300000).toInt();
    m_cleanupInterval = m_settings->value("server/cleanup_interval", 60000).toInt();
    m_broadcastInterval = m_settings->value("server/broadcast_interval", 30000).toInt();
    m_enableBroadcast = m_settings->value("server/enable_broadcast", false).toBool();
    m_broadcastPort = m_settings->value("server/broadcast_port", 6112).toUInt();
}

bool P2PServer::setupSocketOptions()
{
    int fd = m_udpSocket->socketDescriptor();
    if (fd == -1) {
        LOG_ERROR("无法获取socket描述符");
        return false;
    }

    int reuse = 1;
#ifdef Q_OS_WIN
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
        LOG_WARNING("在Windows上设置SO_REUSEADDR失败");
    }
#else
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        LOG_WARNING("在Linux上设置SO_REUSEADDR失败");
    }
#ifdef SO_REUSEPORT
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        LOG_WARNING("在Linux上设置SO_REUSEPORT失败");
    }
#endif
#endif

    return true;
}

bool P2PServer::bindSocket(quint16 port)
{
    if (!m_udpSocket->bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress)) {
        LOG_ERROR(QString("绑定UDP socket到端口 %1 失败: %2")
                      .arg(port).arg(m_udpSocket->errorString()));
        return false;
    }
    return true;
}

void P2PServer::setupTimers()
{
    // 清理定时器
    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &P2PServer::onCleanupTimeout);
    m_cleanupTimer->start(m_cleanupInterval);

    // 广播定时器
    if (m_enableBroadcast) {
        m_broadcastTimer = new QTimer(this);
        connect(m_broadcastTimer, &QTimer::timeout, this, &P2PServer::onBroadcastTimeout);
        m_broadcastTimer->start(m_broadcastInterval);
    }
}

void P2PServer::logServerConfiguration()
{
    LOG_INFO(QString("对等端超时时间: %1 毫秒").arg(m_peerTimeout));
    LOG_INFO(QString("清理间隔: %1 毫秒").arg(m_cleanupInterval));
    LOG_INFO(QString("广播功能: %1").arg(m_enableBroadcast ? "启用" : "禁用"));
    if (m_enableBroadcast) {
        LOG_INFO(QString("广播端口: %1").arg(m_broadcastPort));
        LOG_INFO(QString("广播间隔: %1 毫秒").arg(m_broadcastInterval));
    }
}

void P2PServer::stopServer()
{
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;
    cleanupResources();

    m_peers.clear();
    LOG_INFO("🛑 P2P服务器已停止");
    emit serverStopped();
}

void P2PServer::cleanupResources()
{
    // 停止定时器
    if (m_cleanupTimer) {
        m_cleanupTimer->stop();
        m_cleanupTimer->deleteLater();
        m_cleanupTimer = nullptr;
    }

    if (m_broadcastTimer) {
        m_broadcastTimer->stop();
        m_broadcastTimer->deleteLater();
        m_broadcastTimer = nullptr;
    }

    // 关闭socket
    if (m_udpSocket) {
        m_udpSocket->close();
        m_udpSocket->deleteLater();
        m_udpSocket = nullptr;
    }

    if (m_tcpServer) {
        m_tcpServer->close();
        m_tcpServer->deleteLater();
        m_tcpServer = nullptr;
    }

    // 清理设置
    if (m_settings) {
        m_settings->deleteLater();
        m_settings = nullptr;
    }
}

void P2PServer::onUDPReadyRead()
{
    while (m_udpSocket && m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        if (!datagram.isValid()) {
            continue;
        }
        LOG_INFO(QString("📨 收到数据报，大小: %1 字节").arg(datagram.data().size()));
        processDatagram(datagram);
    }
}

void P2PServer::processDatagram(const QNetworkDatagram &datagram)
{
    QByteArray data = datagram.data();
    QString senderAddress = datagram.senderAddress().toString();
    quint16 senderPort = datagram.senderPort();

    LOG_INFO(QString("📨 收到 %1 字节来自 %2:%3")
                 .arg(data.size()).arg(senderAddress).arg(senderPort));

    QString message = QString::fromUtf8(data).trimmed();

    if (message.startsWith("HANDSHAKE")) {
        LOG_INFO("🔗 处理 HANDSHAKE 消息");
        processHandshake(datagram);
    } else if (message.startsWith("REGISTER")) {
        LOG_INFO("📝 处理 REGISTER 消息");
        processRegister(datagram);
    } else if (message.startsWith("UNREGISTER")) {
        LOG_INFO("👋 处理 UNREGISTER 请求");
        processUnregister(datagram);
    } else if (message.startsWith("GET_PEER_LIST")) {
        LOG_INFO("📋 处理 GET_PEER_LIST 请求");
        processGetPeerList(datagram);
    } else if (message.startsWith("GET_PEER_INFO")) {
        LOG_INFO("📋 处理 GET_PEER_INFO 请求");
        processGetPeerInfo(datagram);
    }  else if (message.startsWith("PUNCH")) {
        LOG_INFO("🚀 处理 PUNCH (P2P连接发起) 请求");
        processPunchRequest(datagram);
    } else if (message.startsWith("KEEPALIVE")) {
        LOG_DEBUG("💓 处理 KEEPALIVE 消息");
        processKeepAlive(datagram);
    } else if (message.startsWith("PEER_INFO_ACK")) {
        LOG_INFO("✅ 处理 PEER_INFO_ACK 消息");
        processPeerInfoAck(datagram);
    } else if (message.startsWith("PING")) {
        LOG_INFO("🏓 处理 PING 请求，验证客户端注册状态");
        processPingRequest(datagram);
    } else if (message.startsWith("TEST")) {
        LOG_INFO("🧪 处理 TEST 消息");
        processTestMessage(datagram);
    } else if (message.startsWith("NAT_TEST")) {
        LOG_INFO("🧪 处理 NAT_TEST 消息");
        processNATTest(datagram);
    } else if (message.startsWith("P2P_TEST")) {
        LOG_INFO("🧪 处理 P2P_TEST 消息");
        processP2PTest(datagram);
    }  else if (message.startsWith("FORWARDED")) {
        LOG_INFO("🔄 处理 FORWARDED 消息");
        processForwardedMessage(datagram);
        return;
    }else if (message.startsWith("CHECK_CRC")) {
        LOG_INFO("👀 处理 CHECK_CRC 消息");
        processCheckCrc(datagram);
    } else {
        LOG_WARNING(QString("❓ 未知消息类型来自 %1:%2: %3")
                        .arg(senderAddress).arg(senderPort).arg(message));
        sendDefaultResponse(datagram);
    }
}

void P2PServer::onTcpReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

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
                    QString senderIp = socket->peerAddress().toString();
                    if (senderIp.startsWith("::ffff:")) {
                        senderIp = senderIp.mid(7);
                    }

                    if (!uploadedCrc.isEmpty()) {
                        QWriteLocker locker(&m_peersLock);
                        bool peerFound = false;

                        // 3. 遍历查找 IP 匹配的用户
                        for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
                            if (it.value().publicIp == senderIp) {
                                it.value().crcToken = uploadedCrc;
                                peerFound = true;
                                LOG_INFO(QString("🗺️ 已更新用户 %1 的地图CRC: %2")
                                             .arg(it.value().clientUuid, uploadedCrc));
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

void P2PServer::onNewTcpConnection()
{
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &P2PServer::onTcpReadyRead);
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);

        LOG_INFO(QString("📥 TCP 连接来自: %1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort()));
    }
}

void P2PServer::processHandshake(const QNetworkDatagram &datagram)
{
    QString data = QString(datagram.data());
    QStringList parts = data.split('|');

    if (parts.size() < 7) {
        LOG_WARNING(QString("❌ 无效的握手格式: %1").arg(data));
        return;
    }

    QString clientUuid = parts[1];
    QString localIp = parts[2];
    QString localPortStr = parts[3];
    QString targetIp = parts[4];
    QString targetPortStr = parts[5];
    QString status = parts[6]; // 客户端可以传来初始状态，如 WAITING|HOSTING|CREATED|JOINED|STARTED

    // --- 安全性转换 ---
    bool localPortOk, targetPortOk;
    quint16 localPort = localPortStr.toUShort(&localPortOk);
    quint16 targetPort = targetPortStr.toUShort(&targetPortOk);
    if (!localPortOk || !targetPortOk) {
        LOG_WARNING(QString("❌ 无效的端口号: local=%1, target=%2").arg(localPortStr, targetPortStr));
        return;
    }
    QHostAddress targetAddress(targetIp);
    // ---------------------

    QString peerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());

    PeerInfo peerInfo;
    peerInfo.id = peerId;
    peerInfo.clientUuid = clientUuid;
    peerInfo.localIp = localIp;
    peerInfo.localPort = localPort;
    peerInfo.publicIp = datagram.senderAddress().toString();
    peerInfo.publicPort = datagram.senderPort();
    peerInfo.targetIp = targetIp;
    peerInfo.targetPort = targetPort;
    peerInfo.lastSeen = QDateTime::currentMSecsSinceEpoch();
    peerInfo.status = "WAITING"; // 先统一设置为WAITING，后续逻辑会修改

    {
        QWriteLocker locker(&m_peersLock);
        m_peers[clientUuid] = peerInfo;
    }

    LOG_INFO(QString("✅ 对等端已握手并注册: %1 (%2:%3) 客户端ID: %4 状态: %5")
                 .arg(peerId, peerInfo.publicIp, QString::number(peerInfo.publicPort), clientUuid,  status));

    sendHandshakeAck(datagram, peerId);

    // ★ 核心逻辑：判断是主机还是加入者
    // 如果目标IP不是一个无效/默认地址，那么它就是一个"加入者"
    if (!targetAddress.isNull() && targetIp != "0.0.0.0") {
        LOG_INFO(QString("ℹ️ 识别为 '加入者' (Guest)，正在为其查找主机..."));

        bool matched = findAndNotifyPeer(clientUuid);

        if (matched) {
            emit peerHandshaked(peerId, clientUuid, targetIp, targetPortStr);
            LOG_INFO(QString("🎉 '加入者' %1 已成功匹配到主机").arg(peerId));
        } else {
            emit peerHandshaking(peerId, clientUuid, targetIp, targetPortStr);
            LOG_INFO(QString("⏳ '加入者' %1 暂未找到主机，进入等待...").arg(peerId));
        }

    } else {
        // 目标IP是 "0.0.0.0" 或无效，那么它就是一个"主机"
        LOG_INFO(QString("ℹ️ 识别为 '主机' (Host)，已注册并等待连接。"));
        // 主机不需要做任何事，只需等待被查找
        // 可以在这里更新一下状态
        QWriteLocker locker(&m_peersLock);
        if (m_peers.contains(clientUuid)) {
            m_peers[clientUuid].status = "WAITING"; // 或 "HOSTING"
        }
    }
}

void P2PServer::processRegister(const QNetworkDatagram &datagram)
{
    QString data = QString::fromUtf8(datagram.data());
    QStringList parts = data.split('|');

    // 1. 基础格式校验
    if (parts.size() < 6) {
        qDebug() << "❌ [注册失败] 无效的格式:" << data;
        return;
    }

    // 2. 提取并校验关键数据
    QString clientUuid = parts[1].trimmed(); // 去除首尾空格
    QString localIp = parts[2];
    QString localPort = parts[3];
    QString status = parts.size() > 4 ? parts[4] : "WAITING";
    int natTypeInt = parts[5].toInt();
    NATType natType = static_cast<NATType>(natTypeInt);

    // 防止注册空数据
    if (clientUuid.isEmpty()) {
        qDebug() << "❌ [注册失败] ClientUUID 为空，拒绝注册。来源:" << datagram.senderAddress().toString();
        return;
    }

    // 3. 生成 PeerID
    QString peerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());

    QWriteLocker locker(&m_peersLock);

    PeerInfo peerInfo;

    // 4. 查重逻辑
    if (m_peers.contains(clientUuid)) {
        peerInfo = m_peers[clientUuid];
    } else {
        const quint32 VIP_START = 0x1A000001; // 26.0.0.1
        const quint32 VIP_END   = 0x1AFFFFFE; // 26.255.255.254

        int safetyCount = 0;
        int maxAttempts = 100000;

        while (m_assignedVips.contains(m_nextVirtualIp)) {
            m_nextVirtualIp++;

            if (m_nextVirtualIp > VIP_END) {
                m_nextVirtualIp = VIP_START;
            }

            safetyCount++;
            if (safetyCount > maxAttempts) {
                qDebug() << "❌ 严重错误：虚拟IP池已满，无法分配新IP！";
                return; // 拒绝注册
            }
        }

        // 找到空闲 IP
        quint32 newVip = m_nextVirtualIp;

        // 标记占用
        m_assignedVips.insert(newVip);

        // 设置 info
        peerInfo.virtualIp = ipIntToString(newVip);

        // 移动游标
        m_nextVirtualIp++;
        if (m_nextVirtualIp > VIP_END) m_nextVirtualIp = VIP_START;

        qDebug() << "🆕 为新用户" << clientUuid << "分配虚拟IP:" << peerInfo.virtualIp;
        // ==================================================================
    }

    // 5. 更新其他信息 (无论是新老用户都要更新心跳和公网地址)
    peerInfo.id = peerId;
    peerInfo.clientUuid = clientUuid;
    peerInfo.localIp = localIp;
    peerInfo.localPort = localPort.toUShort();
    peerInfo.publicIp = datagram.senderAddress().toString();
    // 处理 IPv6 映射的 IPv4 (::ffff:192.168.1.1)
    if (peerInfo.publicIp.startsWith("::ffff:")) {
        peerInfo.publicIp = peerInfo.publicIp.mid(7);
    }
    peerInfo.publicPort = datagram.senderPort();
    peerInfo.targetIp = "0.0.0.0"; // 初始化
    peerInfo.targetPort = 0;
    peerInfo.lastSeen = QDateTime::currentMSecsSinceEpoch();
    peerInfo.natType = natTypeToString(natType);
    peerInfo.status = status;

    // 6. 写入 Map
    m_peers[clientUuid] = peerInfo;

    // 🔓 锁在这里自动释放 (sendToAddress 发送网络包耗时较长，建议放在锁外面，或者拷贝一份数据发)
    locker.unlock();

    // 7. 发送响应 (放在锁外面，避免阻塞其他线程)
    qDebug() << "📝 对等端注册成功:" << clientUuid << "VIP:" << peerInfo.virtualIp;

    QByteArray response = QString("REGISTER_ACK|%1|%2|%3")
                              .arg(peerInfo.id, peerInfo.status, peerInfo.virtualIp)
                              .toUtf8();

    // 注意：sendToAddress 内部如果只用 Socket 发送，不需要 m_peersLock
    sendToAddress(datagram.senderAddress(), datagram.senderPort(), response);

    emit peerRegistered(peerId, clientUuid, m_peers.size());
}

void P2PServer::processUnregister(const QNetworkDatagram &datagram)
{
    QStringList parts = QString(datagram.data()).split('|');
    QString clientUuidToRemove;

    // 格式: UNREGISTER|CLIENT_UUID
    if (parts.size() > 1 && !parts[1].isEmpty()) {
        clientUuidToRemove = parts[1];
    } else {
        // 兼容旧客户端，通过地址查找
        clientUuidToRemove = findPeerUuidByAddress(datagram.senderAddress(), datagram.senderPort());
    }

    bool removed = false;
    if (!clientUuidToRemove.isEmpty()) {
        QWriteLocker locker(&m_peersLock);
        if (m_peers.contains(clientUuidToRemove)) {
            m_peers.remove(clientUuidToRemove);
            removed = true;
        }
    }

    if (removed) {
        LOG_INFO(QString("🗑️ 对等端主动注销并已移除: %1").arg(clientUuidToRemove));
        emit peerRemoved(clientUuidToRemove);

        QByteArray response = QString("UNREGISTER_ACK|%1|SUCCESS").arg(clientUuidToRemove).toUtf8();
        sendToAddress(datagram.senderAddress(), datagram.senderPort(), response);
        LOG_INFO(QString("✅ 已向 %1 发送注销确认").arg(clientUuidToRemove));
    } else {
        LOG_WARNING(QString("❓ 收到一个来自未注册对等端的注销请求: %1").arg(clientUuidToRemove));
        QByteArray response = QString("UNREGISTER_ACK|%1|NOT_FOUND").arg(clientUuidToRemove).toUtf8();
        sendToAddress(datagram.senderAddress(), datagram.senderPort(), response);
    }
}

void P2PServer::processGetPeerList(const QNetworkDatagram &datagram)
{
    QString dataStr = QString(datagram.data());
    QStringList parts = dataStr.split('|');

    QString clientUuid = parts[1];
    int count = -1;
    if (parts.size() > 1) {
        bool ok;
        int requestedCount = parts[2].toInt(&ok);
        if (ok) {
            count = requestedCount;
        }
    }

    QByteArray peerListResponse = getPeers(count, clientUuid);
    sendToAddress(datagram.senderAddress(), datagram.senderPort(), peerListResponse);
}

void P2PServer::processGetPeerInfo(const QNetworkDatagram &datagram)
{
    QString data = QString(datagram.data());
    QStringList parts = data.split('|');

    if (parts.size() < 4) {
        LOG_WARNING(QString("❌ 无效的 GET_PEER_INFO 格式: %1").arg(data));
        return;
    }

    QString requesterUuid = parts[1];
    QString targetIp = parts[2];
    quint16 targetPort = parts[3].toUShort();
    bool searchByIpOnly = (targetPort == 0);

    if (searchByIpOnly) {
        // --- 处理模糊查询 (仅IP) ---
        LOG_INFO(QString("🔍 收到来自 %1 的仅IP查询，目标IP: %2").arg(requesterUuid, targetIp));

        QList<PeerInfo> foundPeers;
        {
            QReadLocker locker(&m_peersLock);
            for (const PeerInfo &peer : qAsConst(m_peers)) {
                if (peer.publicIp == targetIp) {
                    foundPeers.append(peer);
                }
            }
        }

        if (!foundPeers.isEmpty()) {
            // 成功，调用新的通知函数发送 PEERS_INFO
            notifyPeerAboutPeers(requesterUuid, foundPeers);
        } else {
            // 失败，发送 NOT_FOUND
            LOG_WARNING(QString("❓ IP %1 未找到任何匹配项。").arg(targetIp));
            // 格式: PEER_INFO_ACK|TARGET_IP|TARGET_PORT|RESULT
            QString response = QString("PEER_INFO_ACK|%1|0|NOT_FOUND").arg(targetIp);
            sendToAddress(datagram.senderAddress(), datagram.senderPort(), response.toUtf8());
        }

    } else {
        // --- 处理精确查询 (IP + Port) ---
        LOG_INFO(QString("🔍 收到来自 %1 的精确查询，目标: %2:%3").arg(requesterUuid, targetIp).arg(targetPort));

        PeerInfo foundPeer;
        bool peerFound = false;
        {
            QReadLocker locker(&m_peersLock);
            for (const PeerInfo &peer : qAsConst(m_peers)) {
                if (peer.publicIp == targetIp && peer.publicPort == targetPort) {
                    foundPeer = peer;
                    peerFound = true;
                    break;
                }
            }
        }

        if (peerFound) {
            // 成功，调用 notifyPeerAboutPeer，它会发送 PEER_INFO
            LOG_INFO(QString("✅ 找到精确匹配: %1，发送单点信息。").arg(foundPeer.clientUuid));
            notifyPeerAboutPeer(requesterUuid, foundPeer);
        } else {
            // 失败，发送 NOT_FOUND
            LOG_WARNING(QString("❓ 精确目标 %1:%2 未找到。").arg(targetIp).arg(targetPort));
            // 格式: PEER_INFO_ACK|TARGET_IP|TARGET_PORT|RESULT
            QString response = QString("PEER_INFO_ACK|%1|%2|NOT_FOUND").arg(targetIp).arg(targetPort);
            sendToAddress(datagram.senderAddress(), datagram.senderPort(), response.toUtf8());
        }
    }
}

void P2PServer::sendHandshakeAck(const QNetworkDatagram &datagram, const QString &peerId)
{
    QByteArray response = QString("HANDSHAKE_ACK|%1").arg(peerId).toUtf8();
    qint64 bytesSent = sendToAddress(datagram.senderAddress(), datagram.senderPort(), response);

    if (bytesSent > 0) {
        LOG_INFO(QString("✅ 握手确认发送成功: %1 字节到 %2").arg(bytesSent).arg(peerId));
    } else {
        LOG_ERROR(QString("❌ 握手确认发送失败到: %1").arg(peerId));
    }
}

bool P2PServer::findAndNotifyPeer(const QString &guestClientUuid, bool findHost)
{
    QWriteLocker locker(&m_peersLock);

    // 1. 获取"加入者"的信息
    if (!m_peers.contains(guestClientUuid)) {
        LOG_ERROR(QString("❌ findAndNotifyPeer: 加入者 %1 不存在").arg(guestClientUuid));
        return false;
    }
    PeerInfo &guestPeer = m_peers[guestClientUuid];

    // 2. 遍历所有对等端，查找"主机"
    // 主机的标识是：其公网地址和端口与加入者的目标地址和端口匹配
    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        // 跳过自己
        if (it.key() == guestClientUuid) {
            continue;
        }

        PeerInfo &hostPeer = it.value();

        // ★ 核心匹配逻辑: 检查当前 peer 是不是 guest 想要找的 host
        if (hostPeer.publicIp == guestPeer.targetIp && hostPeer.publicPort == guestPeer.targetPort) {

            // 确保主机处于可连接状态
            if (findHost && hostPeer.status != "HOSTING") {
                LOG_WARNING(QString("⚠️ 找到主机 %1，但其状态为 %2，无法连接。").arg(hostPeer.id, hostPeer.status));
                continue; // 继续查找其他可能的匹配（例如IP和端口复用）
            }

            LOG_INFO(QString("🤝 匹配成功: 加入者 %1 -> 主机 %2").arg(guestPeer.id, hostPeer.id));

            // 更新加入者状态
            guestPeer.status = "JOINING";

            // ★ 服务器的"介绍人"角色
            // a. 告诉"加入者"关于"主机"的详细信息
            notifyPeerAboutPeer(guestPeer.clientUuid, hostPeer);
            // b. 告诉"主机"关于"加入者"的详细信息 (用于NAT打洞)
            notifyPeerAboutPeer(hostPeer.clientUuid, guestPeer);

            emit peersMatched(guestPeer.id, hostPeer.id, hostPeer.publicIp, QString::number(hostPeer.publicPort));
            return true; // 匹配成功
        }
    }

    // 3. 如果循环结束还没找到
    LOG_WARNING(QString("⏳ 未能为 %1 找到主机 (%2:%3)。可能主机还未注册。")
                    .arg(guestPeer.id, guestPeer.targetIp).arg(guestPeer.targetPort));
    // 将加入者状态置为等待，也许主机稍后就上线了
    guestPeer.status = "WAITING";
    return false;
}

void P2PServer::processPingRequest(const QNetworkDatagram &datagram)
{
    // 格式：PING|CLIENT_UUID|LOCAL_IP|LOCAL_PORT|PUBLIC_IP|PUBLIC_PORT
    QString data = QString(datagram.data());
    QStringList parts = data.split('|');

    bool isRegistered = false;

    if (parts.size() >= 3) {
        QString clientUuid = parts[1];
        QString clientLocalIp = parts[2];
        QString clientLocalPort = parts[3];
        QString clientPublicIp = parts[4];
        QString clientPublicPort = parts[5];
        QString publicIp = datagram.senderAddress().toString();
        QString publicPort = QString::number(datagram.senderPort());
        LOG_INFO(QString("🏓 PING来自 %1").arg(clientUuid));
        LOG_INFO(QString("      客户端检测本地信息-> %2:%3").arg(clientLocalIp, clientLocalPort));
        LOG_INFO(QString("      客户端检测公网信息-> %2:%3").arg(clientPublicIp, clientPublicPort));
        LOG_INFO(QString("      服务端检测公网信息-> %2:%3").arg(publicIp, publicPort));

        {
            QReadLocker locker(&m_peersLock);
            isRegistered = m_peers.contains(clientUuid);
        }

        QString status = isRegistered ? "REGISTERED" : "UNREGISTERED";
        QByteArray pongResponse = QString("PONG|%1|%2|%3").arg(publicIp, publicPort, status).toUtf8();
        qint64 bytesSent = sendToAddress(datagram.senderAddress(), datagram.senderPort(), pongResponse);

        if (bytesSent > 0) {
            LOG_INFO(QString("✅ PONG回复已发送 (状态: %1, %2 字节)").arg(status).arg(bytesSent));
            if (!isRegistered && publicIp != "0.0.0.0" && publicPort != "0") {
                LOG_INFO(QString("💡 检测到未注册客户端，建议客户端重新注册"));
            }
        } else {
            LOG_ERROR("❌ PONG回复发送失败");
        }
    } else {
        LOG_INFO("🏓 收到传统PING请求，发送PONG回复");
        QByteArray pongResponse = "PONG|War3Bot";
        sendToAddress(datagram.senderAddress(), datagram.senderPort(), pongResponse);
        LOG_INFO("✅ PONG回复已发送");
    }
}

void P2PServer::processTestMessage(const QNetworkDatagram &datagram)
{
    QByteArray data = datagram.data();
    QString message = QString(data).trimmed();
    QString senderAddress = datagram.senderAddress().toString();
    quint16 senderPort = datagram.senderPort();

    LOG_INFO(QString("🧪 处理测试消息: %1 来自 %2:%3").arg(message, senderAddress).arg(senderPort));
    if (!m_udpSocket) {
        LOG_ERROR("❌ UDP Socket 未初始化！");
        return;
    }
    if (m_udpSocket->state() != QAbstractSocket::BoundState) {
        LOG_ERROR(QString("❌ UDP Socket 未绑定状态: %1").arg(m_udpSocket->state()));
        return;
    }
    LOG_INFO(QString("📡 服务器监听在: %1:%2").arg(m_udpSocket->localAddress().toString()).arg(m_udpSocket->localPort()));

    bool isTestMessage = false;
    QString responseMessage;

    if (message.contains("TEST|CONNECTIVITY", Qt::CaseInsensitive)) {
        isTestMessage = true;
        responseMessage = "TEST|CONNECTIVITY|OK|War3Nat_Server_v3.0";
        LOG_INFO("✅ 识别为连接测试消息，准备响应");
    }

    if (isTestMessage) {
        QByteArray response = responseMessage.toUtf8();
        LOG_INFO(QString("📤 准备发送响应到: %1:%2 - 内容: %3").arg(senderAddress).arg(senderPort).arg(responseMessage));
        qint64 bytesSent = sendToAddress(datagram.senderAddress(), datagram.senderPort(), response);

        if (bytesSent > 0) {
            LOG_INFO(QString("✅ 测试响应发送成功: %1 字节").arg(bytesSent));
            m_totalResponses++;
        } else {
            QString errorStr = m_udpSocket ? m_udpSocket->errorString() : "Socket未初始化";
            LOG_ERROR(QString("❌ 测试响应发送失败: %1").arg(errorStr));
            LOG_ERROR(QString("🔧 Socket错误: %1, 状态: %2").arg(errorStr, m_udpSocket ? QString::number(m_udpSocket->state()) : "N/A"));
        }
        return;
    }

    LOG_WARNING(QString("❓ 未知测试消息格式: %1").arg(message));
    sendDefaultResponse(datagram);
}

void P2PServer::processCheckCrc(const QNetworkDatagram &datagram)
{
    QString msg = QString::fromUtf8(datagram.data()).trimmed();
    QStringList parts = msg.split('|');

    if (parts.size() < 2) return;

    QString crcHex = parts[1].trimmed().toUpper();

    // 检查本地文件
    QString scriptDir = QCoreApplication::applicationDirPath() + "/war3files/crc/" + crcHex;
    QDir dir(scriptDir);

    bool exists = dir.exists() &&
                  QFile::exists(scriptDir + "/common.j") &&
                  QFile::exists(scriptDir + "/blizzard.j") &&
                  QFile::exists(scriptDir + "/war3map.j");

    QString status;
    if (exists) {
        status = "EXIST";
    } else {
        status = "NOT_EXIST";
        QWriteLocker locker(&m_tokenLock);
        m_pendingUploadTokens.insert(crcHex);
        QPointer<P2PServer> self = this;
        QTimer::singleShot(60000, this, [self, crcHex](){
            if (self) {
                QWriteLocker locker(&self->m_tokenLock);
                self->m_pendingUploadTokens.remove(crcHex);
                qDebug() << "⏳ Token过期移除:" << crcHex;
            }
        });
    }

    QString response = QString("CHECK_CRC_ACK|%1|%2").arg(crcHex, status);
    sendToAddress(datagram.senderAddress(), datagram.senderPort(), response.toUtf8());

    LOG_INFO(QString("🔍 CRC检查: %1 -> %2").arg(crcHex, status));
}

void P2PServer::sendDefaultResponse(const QNetworkDatagram &datagram)
{
    const QByteArray originalData = datagram.data();

    const QString base64Data = QString::fromLatin1(originalData.toBase64());
    const QString stringData = QString::fromUtf8(originalData);

    const QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    const QString senderIp  = datagram.senderAddress().toString();
    const QString senderPort = QString::number(datagram.senderPort());
    const QString dataSize   = QString::number(originalData.size());

    // 格式: DEFAULT_RESPONSE|DESCRIPTION|IP|PORT|SIZE|STRING_DATA|BASE64
    QString responseMessage;
    responseMessage.reserve(256 + stringData.size() + base64Data.size());

    QTextStream stream(&responseMessage);
    stream << "DEFAULT_RESPONSE|Message received at " << timestamp
           << "|" << senderIp
           << "|" << senderPort
           << "|" << dataSize
           << "|" << stringData
           << "|" << base64Data;

    sendToAddress(datagram.senderAddress(),
                  datagram.senderPort(),
                  responseMessage.toUtf8());

    LOG_DEBUG(QString("📤 已向 %1:%2 发送默认响应，回显了 %3 字节的数据 (文本 + Base64)。")
                  .arg(senderIp, senderPort)
                  .arg(originalData.size()));
}

QByteArray P2PServer::buildSTUNTestResponse(const QNetworkDatagram &datagram)
{
    QByteArray response;
    QDataStream stream(&response, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << quint16(0x0101);
    stream << quint16(12);
    stream << quint32(0x2112A442);

    QByteArray transactionId(12, 0);
    QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(transactionId.data()), 3);
    stream.writeRawData(transactionId.constData(), 12);

    stream << quint16(0x0020);
    stream << quint16(8);

    quint16 xoredPort = datagram.senderPort() ^ (0x2112A442 >> 16);
    quint32 ipv4 = datagram.senderAddress().toIPv4Address();
    quint32 xoredIP = ipv4 ^ 0x2112A442;

    stream << quint8(0);
    stream << quint8(0x01);
    stream << xoredPort;
    stream << xoredIP;

    LOG_DEBUG(QString("🔧 STUN测试响应 - 客户端: %1:%2 -> 映射: %3:%4")
                  .arg(datagram.senderAddress().toString()).arg(datagram.senderPort())
                  .arg(datagram.senderAddress().toString()).arg(datagram.senderPort()));

    return response;
}

void P2PServer::notifyPeerAboutPeer(const QString &targetUuid, const PeerInfo &otherPeer)
{
    QHostAddress targetAddress;
    quint16 targetPort = 0;
    bool targetFound = false;

    {
        QReadLocker locker(&m_peersLock);
        if (m_peers.contains(targetUuid)) {
            const PeerInfo &targetPeer = m_peers.value(targetUuid);
            QString cleanIp = targetPeer.publicIp;
            if (cleanIp.startsWith("::ffff:")) {
                cleanIp = cleanIp.mid(7);
            }
            targetAddress = QHostAddress(cleanIp);
            targetPort = targetPeer.publicPort;
            targetFound = !targetAddress.isNull();
        } else {
            LOG_ERROR(QString("❌ 对等端不存在: %1").arg(targetUuid));
        }
    }

    if (targetFound) {
        // 格式: PEER_INFO|CLIENT_UUID|PUBLIC_IP|PUBLIC_PORT|LOCAL_IP|LOCAL_PORT
        QString message = QString("PEER_INFO|%1|%2|%3|%4|%5")
                              .arg(otherPeer.clientUuid,
                                   otherPeer.publicIp)
                              .arg(otherPeer.publicPort)
                              .arg(otherPeer.localIp)
                              .arg(otherPeer.localPort);

        qint64 bytesSent = sendToAddress(targetAddress, targetPort, message.toUtf8());

        if (bytesSent > 0) {
            LOG_INFO(QString("✅ 对等端信息发送成功: %1 -> %2 (%3 字节)")
                         .arg(otherPeer.clientUuid, targetUuid).arg(bytesSent));
        } else {
            LOG_ERROR(QString("❌ 对等端信息发送失败: %1 -> %2").arg(otherPeer.clientUuid, targetUuid));
        }
    }
}

void P2PServer::notifyPeerAboutPeers(const QString &requesterUuid, const QList<PeerInfo> &peers)
{
    // 首先，我们需要根据 requesterUuid 找到请求者的地址
    QHostAddress requesterAddress;
    quint16 requesterPort = 0;
    bool requesterFound = false;

    {
        QReadLocker locker(&m_peersLock);
        if (m_peers.contains(requesterUuid)) {
            const PeerInfo &requesterPeer = m_peers.value(requesterUuid);
            // 这里可以复用您在 notifyPeerAboutPeer 中的IP清理逻辑
            QString cleanIp = requesterPeer.publicIp;
            if (cleanIp.startsWith("::ffff:")) {
                cleanIp = cleanIp.mid(7);
            }
            requesterAddress = QHostAddress(cleanIp);
            requesterPort = requesterPeer.publicPort;
            requesterFound = !requesterAddress.isNull();
        } else {
            LOG_ERROR(QString("❌ 无法通知不存在的请求者: %1").arg(requesterUuid));
            return; // 请求者都找不到了，直接返回
        }
    }

    if (!requesterFound) {
        LOG_ERROR(QString("❌ 无法解析请求者的地址: %1").arg(requesterUuid));
        return;
    }

    // 构建 PEERS_INFO 消息
    // 格式: PEERS_INFO|PEER_DATA_1;PEER_DATA_2;...
    QStringList peerStrings;
    for(const PeerInfo& peer : peers) {
        peerStrings.append(QString("%1,%2,%3,%4,%5")
                               .arg(peer.clientUuid, peer.publicIp)
                               .arg(peer.publicPort).arg(peer.localIp)
                               .arg(peer.localPort));
    }
    QString message = QString("PEERS_INFO|%1").arg(peerStrings.join(";"));

    // 发送消息
    qint64 bytesSent = sendToAddress(requesterAddress, requesterPort, message.toUtf8());

    if (bytesSent > 0) {
        LOG_INFO(QString("✅ 对等端列表发送成功 -> %1 (%2 个Peers, %3 字节)")
                     .arg(requesterUuid).arg(peers.size()).arg(bytesSent));
    } else {
        LOG_ERROR(QString("❌ 对等端列表发送失败 -> %1").arg(requesterUuid));
    }
}

qint64 P2PServer::sendToAddress(const QHostAddress &address, quint16 port, const QByteArray &data)
{
    if (!m_udpSocket) {
        LOG_ERROR("❌ UDP Socket 未初始化");
        return -1;
    }
    return m_udpSocket->writeDatagram(data, address, port);
}

void P2PServer::processPunchRequest(const QNetworkDatagram &datagram)
{
    QString data = QString::fromUtf8(datagram.data());
    QStringList parts = data.split('|');
    if (parts.size() < 2) {
        LOG_WARNING("❌ 无效的 PUNCH 格式");
        return;
    }

    QString initiatorUuid = findPeerUuidByAddress(datagram.senderAddress(), datagram.senderPort());
    QString targetUuid  = parts[1];

    if (initiatorUuid.isEmpty()) {
        LOG_WARNING(QString("❓ 未知的打洞发起方: %1:%2").arg(datagram.senderAddress().toString()).arg(datagram.senderPort()));
        return;
    }

    LOG_INFO(QString("🔄 协调打洞: 发起方 %1 -> 目标 %2").arg(initiatorUuid, targetUuid));

    PeerInfo initiatorPeer;
    PeerInfo targetPeer;
    bool found = false;

    {
        QReadLocker locker(&m_peersLock);
        if (m_peers.contains(initiatorUuid) && m_peers.contains(targetUuid)) {
            initiatorPeer = m_peers.value(initiatorUuid);
            targetPeer = m_peers.value(targetUuid);
            found = true;
        } else {
            if (!m_peers.contains(initiatorUuid)) LOG_WARNING(QString("❓ 未知的打洞发起方: %1").arg(initiatorUuid));
            if (!m_peers.contains(targetUuid)) LOG_WARNING(QString("❓ 未知的打洞目标: %1").arg(targetUuid));
        }
    }

    if (found) {
        LOG_INFO(QString("🤝 正在通知 %1 (发起方) 关于 %2 (目标) 的信息...").arg(initiatorUuid, targetUuid));
        notifyPeerAboutPeer(initiatorUuid, targetPeer);

        LOG_INFO(QString("🤝 正在通知 %1 (目标) 关于 %2 (发起方) 的信息...").arg(targetUuid, initiatorUuid));
        notifyPeerAboutPeer(targetUuid, initiatorPeer);

        emit punchRequested(initiatorUuid, targetUuid);
    }
}

void P2PServer::processKeepAlive(const QNetworkDatagram &datagram)
{
    QString clientUuid = findPeerUuidByAddress(datagram.senderAddress(), datagram.senderPort());
    if (!clientUuid.isEmpty()) {
        QWriteLocker locker(&m_peersLock);
        if (m_peers.contains(clientUuid)) {
            m_peers[clientUuid].lastSeen = QDateTime::currentMSecsSinceEpoch();
            LOG_DEBUG(QString("💓 心跳: %1").arg(clientUuid));
        }
    }
}

void P2PServer::processPeerInfoAck(const QNetworkDatagram &datagram)
{
    QString clientUuid = findPeerUuidByAddress(datagram.senderAddress(), datagram.senderPort());
    if (!clientUuid.isEmpty()) {
        QWriteLocker locker(&m_peersLock);
        if(m_peers.contains(clientUuid)) {
            m_peers[clientUuid].lastSeen = QDateTime::currentMSecsSinceEpoch();
            LOG_INFO(QString("✅ 对等端确认: %1").arg(clientUuid));
        }
    }
}

void P2PServer::processNATTest(const QNetworkDatagram &datagram)
{
    QString data = QString(datagram.data());
    QStringList parts = data.split('|');
    QString senderAddress = datagram.senderAddress().toString();
    quint16 senderPort = datagram.senderPort();
    LOG_INFO(QString("🔍 NAT测试来自 %1:%2").arg(senderAddress).arg(senderPort));

    QByteArray response;
    if (parts.size() > 2 && parts[1] == "PORT_DETECTION") {
        QString testId = parts[2];
        response = QString("NAT_TEST|%1|%2|%3").arg(testId, senderAddress, QString::number(senderPort)).toUtf8();
    } else {
        response = QString("NAT_TEST|%1|%2").arg(senderAddress, QString::number(senderPort)).toUtf8();
    }

    qint64 bytesSent = sendToAddress(datagram.senderAddress(), datagram.senderPort(), response);
    if (bytesSent > 0) {
        LOG_DEBUG(QString("✅ NAT测试响应已发送: %1 字节").arg(bytesSent));
    }
}

void P2PServer::processP2PTest(const QNetworkDatagram &datagram)
{
    // 1. 解析收到的消息
    QString message = QString::fromUtf8(datagram.data());
    QStringList parts = message.split('|');

    // 2. 验证消息格式是否正确
    if (parts.size() < 2) {
        LOG_WARNING(QString("❌ 无效的P2P_TEST格式，来自 %1:%2: %3")
                        .arg(datagram.senderAddress().toString())
                        .arg(datagram.senderPort())
                        .arg(message));
        return;
    }

    // 3. 提取唯一标识 (Nonce)
    const QString nonce = parts[1];

    LOG_INFO(QString("🤝 收到 P2P_TEST 请求，来自 %1:%2, Nonce: %3")
                 .arg(datagram.senderAddress().toString())
                 .arg(datagram.senderPort())
                 .arg(nonce));

    // 4. 构建响应消息 "P2P_TEST_ACK|nonce"
    QByteArray responseMessage = QString("P2P_TEST_ACK|%1").arg(nonce).toUtf8();

    // 5. 将响应发送回请求方
    qint64 bytesSent = sendToAddress(datagram.senderAddress(), datagram.senderPort(), responseMessage);

    if (bytesSent > 0) {
        LOG_INFO(QString("✅ P2P_TEST_ACK 已成功发送给 %1:%2")
                     .arg(datagram.senderAddress().toString())
                     .arg(datagram.senderPort()));
    } else {
        LOG_ERROR(QString("❌ P2P_TEST_ACK 发送失败给 %1:%2")
                      .arg(datagram.senderAddress().toString())
                      .arg(datagram.senderPort()));
    }
}

void P2PServer::processForwardedMessage(const QNetworkDatagram &datagram)
{
    QString data = QString(datagram.data());
    QStringList parts = data.split('|');
    if (parts.size() < 5) {
        LOG_WARNING("❌ 无效的转发消息格式");
        return;
    }

    QString originalClientIp = parts[1];
    QString originalClientPort = parts[2];
    QString timestamp = parts[3];
    QString originalMessage = parts.mid(4).join("|");

    LOG_INFO(QString("📨 转发消息 - 原始客户端: %1:%2, 时间: %3").arg(originalClientIp, originalClientPort, timestamp));
    LOG_INFO(QString("   原始消息: %1").arg(originalMessage));

    QHostAddress originalAddr(originalClientIp);
    quint16 originalPort = originalClientPort.toUShort();
    QByteArray originalData = originalMessage.toUtf8();
    processOriginalMessage(originalData, originalAddr, originalPort);
}

void P2PServer::processOriginalMessage(const QByteArray &data, const QHostAddress &originalAddr, quint16 originalPort)
{
    QString message = QString(data).trimmed();
    LOG_INFO(QString("🔍 处理原始消息来自 %1:%2: %3").arg(originalAddr.toString()).arg(originalPort).arg(message));

    if (message.startsWith("REGISTER_RELAY|")) {
        LOG_INFO("📝 处理转发的 REGISTER_RELAY 消息");
        processRegisterRelayFromForward(data, originalAddr, originalPort);
    } else {
        LOG_WARNING(QString("❓ 未知的转发消息类型: %1").arg(message));
    }
}

void P2PServer::processRegisterRelayFromForward(const QByteArray &data, const QHostAddress &originalAddr, quint16 originalPort)
{
    QString message = QString(data);
    QStringList parts = message.split('|');
    if (parts.size() < 6) {
        LOG_WARNING(QString("❌ 无效的中继注册格式: %1").arg(message));
        return;
    }

    QString clientUuid = parts[1];
    QString relayIp = parts[2];
    QString relayPort = parts[3];
    int natTypeInt = parts[4].toInt();
    QString status = parts.size() > 5 ? parts[5] : "RELAY_WAITING";
    NATType natType = static_cast<NATType>(natTypeInt);

    QString peerId = generatePeerId(originalAddr, originalPort);

    PeerInfo peerInfo;
    peerInfo.id = peerId;
    peerInfo.clientUuid = clientUuid;
    peerInfo.localIp = relayIp;
    peerInfo.localPort = relayPort.toUShort();
    peerInfo.publicIp = originalAddr.toString();
    peerInfo.publicPort = originalPort;
    peerInfo.relayIp = relayIp;
    peerInfo.relayPort = relayPort.toUShort();
    peerInfo.natType = natTypeToString(natType);
    peerInfo.targetIp = "0.0.0.0";
    peerInfo.targetPort = 0;
    peerInfo.lastSeen = QDateTime::currentMSecsSinceEpoch();
    peerInfo.status = status;
    peerInfo.isRelayMode = true;

    {
        QWriteLocker locker(&m_peersLock);
        m_peers[clientUuid] = peerInfo;
    }

    LOG_INFO(QString("🔄 转发中继模式对等端注册: %1").arg(clientUuid));
    LOG_INFO(QString("  真实公网地址: %1:%2").arg(peerInfo.publicIp).arg(peerInfo.publicPort));
    LOG_INFO(QString("  中继地址: %1:%2").arg(relayIp, relayPort));
    LOG_INFO(QString("  NAT类型: %1").arg(peerInfo.natType));
    LOG_INFO(QString("  状态: %1").arg(status));

    QByteArray response = QString("REGISTER_RELAY_ACK|%1|%2|%3|%4").arg(peerId, relayIp, relayPort, status).toUtf8();
    qint64 bytesSent = sendToAddress(originalAddr, originalPort, response);

    if (bytesSent > 0) {
        LOG_INFO(QString("✅ 中继注册确认发送成功: %1 字节").arg(bytesSent));
    } else {
        LOG_ERROR(QString("❌ 中继注册确认发送失败"));
    }
    emit peerRegistered(peerId, clientUuid, m_peers.size());
}

void P2PServer::sendToPeer(const QString &clientUuid, const QByteArray &data)
{
    QWriteLocker locker(&m_peersLock);
    if (!m_peers.contains(clientUuid)) {
        LOG_ERROR(QString("❌ 对等端不存在: %1").arg(clientUuid));
        return;
    }

    const PeerInfo &peer = m_peers[clientUuid];
    QString cleanIp = peer.publicIp;
    if (cleanIp.startsWith("::ffff:")) {
        cleanIp = cleanIp.mid(7);
    }
    QHostAddress address(cleanIp);
    if (address.isNull()) {
        LOG_ERROR(QString("❌ 无效地址: %1").arg(cleanIp));
        return;
    }

    qint64 bytesSent = sendToAddress(address, peer.publicPort, data);
    if (bytesSent > 0) {
        LOG_DEBUG(QString("📤 发送到 %1: %2 字节").arg(clientUuid).arg(bytesSent));
    } else {
        LOG_ERROR(QString("❌ 发送失败到 %1").arg(clientUuid));
    }
}

void P2PServer::onCleanupTimeout()
{
    // 先清理格式错误的数据
    cleanupInvalidPeers();

    // 再清理超时的数据
    cleanupExpiredPeers();
}

void P2PServer::onBroadcastTimeout()
{
    broadcastServerInfo();
}

void P2PServer::broadcastServerInfo()
{
    if (!m_enableBroadcast || !m_udpSocket) return;

    QByteArray broadcastMsg = QString("WAR3BOT_SERVER|%1").arg(m_listenPort).toUtf8();
    m_udpSocket->writeDatagram(broadcastMsg, QHostAddress::Broadcast, m_broadcastPort);
    LOG_DEBUG("📢 广播服务器信息");
}

void P2PServer::cleanupInvalidPeers()
{
    // 必须加写锁
    QWriteLocker locker(&m_peersLock);

    QList<QString> invalidKeys;

    // 1. 扫描无效节点
    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        const PeerInfo &info = it.value();

        // 判定条件：Key为空，或者 UUID为空，或者公网IP无效
        if (it.key().isEmpty() || info.clientUuid.isEmpty() || info.publicIp == "0.0.0.0" || info.publicPort == 0) {
            invalidKeys.append(it.key());
        }
    }

    // 2. 执行删除
    for (const QString &key : invalidKeys) {
        // 先获取 info (引用)，用于释放资源
        // 注意：如果是空 Key，可能取不到完整的 info，但尝试释放 IP 总是安全的
        PeerInfo info = m_peers.value(key);

        LOG_INFO(QString("🧹 清理无效/空数据节点, Key: '%1'").arg(key));

        // 释放虚拟 IP (如果有的话)
        if (!info.virtualIp.isEmpty()) {
            QHostAddress addr(info.virtualIp);
            quint32 vipInt = addr.toIPv4Address();
            if (vipInt != 0) {
                m_assignedVips.remove(vipInt);
                qDebug() << "♻️ 回收虚拟IP:" << info.virtualIp;
            }
        }

        // 从 Map 中彻底移除
        m_peers.remove(key);
    }
}

void P2PServer::cleanupExpiredPeers()
{
    {
        QWriteLocker locker(&m_peersLock);
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        QList<QString> expiredPeers;

        // 1. 扫描过期节点
        for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
            if (currentTime - it.value().lastSeen > m_peerTimeout) {
                expiredPeers.append(it.key());
            }
        }

        // 2. 执行删除
        for (const QString &clientUuid : expiredPeers) {
            // 必须在 remove 之前获取 PeerInfo！
            // 否则 remove 后再用 [] 访问，会重新插入一个空的 PeerInfo！
            PeerInfo info = m_peers.value(clientUuid);

            LOG_INFO(QString("🗑️ 移除过期对等端: %1").arg(clientUuid));

            // 释放虚拟 IP
            if (!info.virtualIp.isEmpty()) {
                QHostAddress addr(info.virtualIp);
                quint32 vipInt = addr.toIPv4Address();
                if (vipInt != 0) {
                    m_assignedVips.remove(vipInt);
                    qDebug() << "♻️ 回收过期用户的虚拟IP:" << info.virtualIp;
                }
            }

            // 这里的 remove 才是安全的
            m_peers.remove(clientUuid);
            emit peerRemoved(clientUuid);
        }

        if (!expiredPeers.isEmpty()) {
            LOG_INFO(QString("🧹 已清理 %1 个过期对等端").arg(expiredPeers.size()));
        }
    }
    updateMostFrequentCrc();
}

QString P2PServer::generatePeerId(const QHostAddress &address, quint16 port)
{
    QString ipString = address.toString();
    if (ipString.startsWith("::ffff:")) {
        ipString = ipString.mid(7);
    }
    return QString("%1:%2").arg(ipString).arg(port);
}

QString P2PServer::findPeerUuidByAddress(const QHostAddress &address, quint16 port)
{
    QReadLocker locker(&m_peersLock);
    QString peerIdToFind = generatePeerId(address, port);
    for (auto it = m_peers.constBegin(); it != m_peers.constEnd(); ++it) {
        // PeerInfo.id 中仍然存储着 ip:port
        if (it.value().id == peerIdToFind) {
            return it.key(); // it.key() 现在是 clientUuid
        }
    }
    return QString();
}

void P2PServer::removePeer(const QString &clientUuid)
{
    if (m_peers.contains(clientUuid)) {
        {
            QWriteLocker locker(&m_peersLock);
            const PeerInfo &peer = m_peers[clientUuid];

            // 释放虚拟 IP
            if (!peer.virtualIp.isEmpty()) {
                QHostAddress addr(peer.virtualIp);
                quint32 vipInt = addr.toIPv4Address(); // 转回整数
                m_assignedVips.remove(vipInt);         // 从占用集合中移除

                qDebug() << "♻️ 释放虚拟IP:" << peer.virtualIp;
            }

            m_peers.remove(clientUuid);
            emit peerRemoved(clientUuid);
        }
        updateMostFrequentCrc();
    }
}

QString P2PServer::ipIntToString(quint32 ip) {
    return QHostAddress(ip).toString();
}

QString P2PServer::natTypeToString(NATType type)
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

QByteArray P2PServer::getPeers(int maxCount, const QString &excludeClientUuid)
{
    QReadLocker locker(&m_peersLock);

    QList<PeerInfo> peerList = m_peers.values();

    // 修正计数逻辑：
    // 1. 计算真正可用的对等端数量（总数 - 请求者自己）
    int availablePeers = peerList.size() > 1 ? peerList.size() - 1 : 0;
    // 2. 确定最终要发送的数量：在客户端请求数和我们实际可提供的数量之间取较小值
    int desiredCount = (maxCount < 0) ? availablePeers : qMin(maxCount, availablePeers);

    LOG_INFO(QString("🔍 正在准备对等端列表... 请求数量: %1, 排除UUID: %2, 总对等端数: %3, 最终发送数(上限): %4")
                 .arg(maxCount).arg(excludeClientUuid).arg(peerList.size()).arg(desiredCount));

    QByteArray response = "PEER_LIST|";
    int peersAdded = 0;

    // --- 用于详细日志记录 ---
    QStringList peersLogList;
    if (desiredCount > 0) {
        peersLogList << QString("--- 准备发送给 %1 的对等端列表 (最多 %2 个) ---")
                            .arg(excludeClientUuid)
                            .arg(desiredCount);
    }
    // -----------------------

    for (const PeerInfo &peer : qAsConst(peerList)) {
        // 如果已达到期望的数量，则停止
        if (peersAdded >= desiredCount) {
            break;
        }

        // 跳过请求者自身
        if (peer.clientUuid == excludeClientUuid) {
            continue;
        }

        QString peerData = formatPeerData(peer);

        response.append(peerData.toUtf8());
        response.append("|");
        peersAdded++;

        // --- 添加到详细日志 ---
        peersLogList << QString("  [%1/%2] Adding peer:").arg(peersAdded).arg(desiredCount);
        peersLogList << formatPeerLog(peer);
    }

    // 移除末尾多余的'|'
    if (response.endsWith('|')) {
        response.chop(1);
    }

    // --- 打印日志 ---
    if (peersAdded > 0) {
        LOG_INFO(peersLogList.join("\n"));
    } else {
        LOG_INFO(QString("ℹ️ 没有找到其他符合条件的可发送对等端给 %1 (总在线数: %2)")
                     .arg(excludeClientUuid).arg(peerList.size()));
    }
    // -----------------

    LOG_INFO(QString("✅ 对等端列表准备完成，共发送 %1 个对等端给请求者。").arg(peersAdded));
    return response;
}

QString P2PServer::formatPeerData(const PeerInfo &peer) const
{
    return QString("id=%1;cid=%2;lip=%3;lport=%4;pip=%5;pport=%6;rip=%7;rport=%8;tip=%9;tport=%10;nat=%11;seen=%12;stat=%13;relay=%14;vip=%15;crc=%16")
    .arg(peer.id,
         peer.clientUuid,
         peer.localIp,
         QString::number(peer.localPort),
         peer.publicIp,
         QString::number(peer.publicPort),
         peer.relayIp,
         QString::number(peer.relayPort),
         peer.targetIp,
         QString::number(peer.targetPort),
         peer.natType,
         QString::number(peer.lastSeen),
         peer.status,
         peer.isRelayMode ? "1" : "0",
         peer.virtualIp,
         peer.crcToken);
}

QString P2PServer::formatPeerLog(const PeerInfo &peer) const
{
    // 使用 QStringList 来构建多行日志，更清晰
    QStringList logLines;
    logLines << QString("    ID: %1").arg(peer.id, -22); // 左对齐
    logLines << QString("    UUID: %1").arg(peer.clientUuid);
    logLines << QString("    Status: %1").arg(peer.status);
    logLines << QString("    Virtual Addr: %1").arg(peer.virtualIp);
    logLines << QString("    Public Addr: %1:%2").arg(peer.publicIp).arg(peer.publicPort);
    logLines << QString("    Local Addr: %1:%2").arg(peer.localIp).arg(peer.localPort);

    // 只在有目标时记录目标信息
    if (peer.targetIp != "0.0.0.0" && peer.targetPort != 0) {
        logLines << QString("    Target Addr: %1:%2").arg(peer.targetIp).arg(peer.targetPort);
    }

    logLines << QString("    NAT Type: %1").arg(peer.natType);
    logLines << QString("    Last Seen: %1").arg(QDateTime::fromMSecsSinceEpoch(peer.lastSeen).toString("yyyy-MM-dd hh:mm:ss"));

    // 只在中继模式时记录中继信息
    if (peer.isRelayMode) {
        logLines << QString("    Relay Mode: Yes (via %1:%2)").arg(peer.relayIp).arg(peer.relayPort);
    } else {
        logLines << QString("    Relay Mode: No");
    }

    logLines << QString("    Crc Token: %1").arg(peer.crcToken);

    // 将所有行合并为一个字符串，每行前加缩进
    return "\n" + logLines.join("\n");
}

void P2PServer::updateMostFrequentCrc()
{
    m_crcCounts.clear();

    {
        QReadLocker locker(&m_peersLock);
        for (const PeerInfo &peer : qAsConst(m_peers)) {
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

bool P2PServer::isValidFileName(const QString &name)
{
    // 强制剥离路径，只取文件名
    QString safeName = QFileInfo(name).fileName();
    if (safeName != name) return false;
    QString lower = safeName.toLower();
    // 白名单
    return lower == "common.j" ||
           lower == "blizzard.j" ||
           lower == "war3map.j" ||
           lower == "war3map.lua";
}

bool P2PServer::isRunning() const
{
    return m_isRunning;
}

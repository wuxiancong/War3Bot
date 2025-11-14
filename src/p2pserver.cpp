#include "p2pserver.h"
#include "logger.h"
#include <QTimer>
#include <QDateTime>
#include <QDataStream>
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
    , m_cleanupTimer(nullptr)
    , m_broadcastTimer(nullptr)
    , m_totalRequests(0)
    , m_totalResponses(0)
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

    connect(m_udpSocket, &QUdpSocket::readyRead, this, &P2PServer::onReadyRead);

    m_listenPort = port;
    m_isRunning = true;

    // 启动定时器
    setupTimers();

    LOG_INFO(QString("✅ P2P服务器已在端口 %1 启动").arg(port));
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

    // 清理设置
    if (m_settings) {
        m_settings->deleteLater();
        m_settings = nullptr;
    }
}

void P2PServer::onReadyRead()
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
        LOG_INFO("👋 处理 UNREGISTER (注销) 请求");
        processUnregister(datagram);
    } else if (message.startsWith("GET_PEERS")) {
        LOG_INFO("📋 处理 GET_PEERS 请求");
        processGetPeers(datagram);
    } else if (message.startsWith("PUNCH")) {
        LOG_INFO("🚀 处理 PUNCH (P2P连接发起) 请求");
        processPunchRequest(datagram);
    } else if (message.startsWith("KEEPALIVE")) {
        LOG_DEBUG("💓 处理 KEEPALIVE 消息");
        processKeepAlive(datagram);
    } else if (message.startsWith("PEER_INFO_ACK")) {
        LOG_INFO("✅ 处理 PEER_INFO_ACK 消息");
        processPeerInfoAck(datagram);
    } else if (message.startsWith("PING")) {
        LOG_INFO("🏓 处理PING请求，验证客户端注册状态");
        processPingRequest(datagram);
    } else if (message.startsWith("TEST")) {
        LOG_INFO("🧪 处理测试消息");
        processTestMessage(datagram);
    } else if (message.startsWith("NAT_TEST")) {
        LOG_INFO("🔍 处理NAT测试消息");
        processNATTest(datagram);
    } else if (message.startsWith("FORWARDED")) {
        LOG_INFO("🔄 处理转发消息");
        processForwardedMessage(datagram);
        return;
    } else {
        LOG_WARNING(QString("❓ 未知消息类型来自 %1:%2: %3")
                        .arg(senderAddress).arg(senderPort).arg(message));
        sendDefaultResponse(datagram);
    }
}

void P2PServer::processHandshake(const QNetworkDatagram &datagram)
{
    QString data = QString(datagram.data());
    QStringList parts = data.split('|');

    if (parts.size() < 7) {
        LOG_WARNING(QString("❌ 无效的注册格式: %1").arg(data));
        LOG_WARNING(QString("期望 7 个部分，实际收到: %1 个部分").arg(parts.size()));
        for (int i = 0; i < parts.size(); ++i) {
            LOG_WARNING(QString("  部分[%1]: %2").arg(i).arg(parts[i]));
        }
        return;
    }

    QString clientUuid = parts[1];
    QString localIp = parts[2];
    QString localPort = parts[3];
    QString targetIp = parts[4];
    QString targetPort = parts[5];
    QString status = parts[6];

    QString peerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());

    PeerInfo peerInfo;
    peerInfo.id = peerId;
    peerInfo.clientUuid = clientUuid;
    peerInfo.localIp = localIp;
    peerInfo.localPort = localPort.toUShort();
    peerInfo.publicIp = datagram.senderAddress().toString();
    peerInfo.publicPort = datagram.senderPort();
    peerInfo.targetIp = targetIp;
    peerInfo.targetPort = targetPort.toUShort();
    peerInfo.lastSeen = QDateTime::currentMSecsSinceEpoch();
    peerInfo.status = status;

    m_peers[peerId] = peerInfo;

    LOG_INFO(QString("✅ 对等端已注册: %1 (%2) 客户端ID: %3")
                 .arg(peerId, peerInfo.publicIp, clientUuid));

    sendHandshakeAck(datagram, peerId);

    bool matched = findAndConnectPeers(peerId, clientUuid, targetIp, targetPort);

    if (matched) {
        emit peerHandshaked(peerId, clientUuid, targetIp, targetPort);
        LOG_INFO(QString("🎉 握手完成: %1 成功匹配到目标").arg(peerId));
    } else {
        emit peerHandshaking(peerId, clientUuid, targetIp, targetPort);
        LOG_INFO(QString("⏳ 握手进行中: %1 等待目标对等端连接").arg(peerId));
    }
}

void P2PServer::processRegister(const QNetworkDatagram &datagram)
{
    QString data = QString(datagram.data());
    QStringList parts = data.split('|');

    if (parts.size() < 6) {
        LOG_WARNING(QString("❌ 无效的注册格式: %1").arg(data));
        LOG_WARNING(QString("期望 6个部分，实际收到: %1 个部分").arg(parts.size()));
        for (int i = 0; i < parts.size(); ++i) {
            LOG_WARNING(QString("  部分[%1]: %2").arg(i).arg(parts[i]));
        }
        return;
    }

    QString clientUuid  = parts[1];
    QString localIp = parts[2];
    QString localPort = parts[3];
    QString status = parts.size() > 4 ? parts[4] : "WAITING";
    int natTypeInt = parts[5].toInt();
    NATType natType = static_cast<NATType>(natTypeInt);

    QString peerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());

    PeerInfo peerInfo;
    peerInfo.id = peerId;
    peerInfo.clientUuid = clientUuid;
    peerInfo.localIp = localIp;
    peerInfo.localPort = localPort.toUShort();
    peerInfo.publicIp = datagram.senderAddress().toString();
    peerInfo.publicPort = datagram.senderPort();
    peerInfo.targetIp = "0.0.0.0";
    peerInfo.targetPort = 0;
    peerInfo.lastSeen = QDateTime::currentMSecsSinceEpoch();
    peerInfo.status = status;
    peerInfo.natType = natType;

    {
        QWriteLocker locker(&m_peersLock);
        m_peers[peerId] = peerInfo;
    }

    LOG_INFO(QString("📝 对等端注册: %1").arg(peerId));
    LOG_INFO(QString("  公网地址: %1:%2").arg(peerInfo.publicIp).arg(peerInfo.publicPort));
    LOG_INFO(QString("  内网地址: %1:%2").arg(localIp, localPort));
    LOG_INFO(QString("  状态: %1").arg(status));
    LOG_INFO(QString("  NAT类型: %1").arg(natTypeToString(natType)));

    QByteArray response = QString("REGISTER_ACK|%1|%2").arg(peerId, status).toUtf8();
    sendToAddress(datagram.senderAddress(), datagram.senderPort(), response);

    emit peerRegistered(peerId, clientUuid);
}

void P2PServer::processUnregister(const QNetworkDatagram &datagram)
{
    QString peerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());

    bool removed = false;
    {
        QWriteLocker locker(&m_peersLock);
        if (m_peers.contains(peerId)) {
            m_peers.remove(peerId);
            removed = true;
        }
    }

    if (removed) {
        LOG_INFO(QString("🗑️ 对等端主动注销并已移除: %1").arg(peerId));
        emit peerRemoved(peerId);

        QByteArray response = QString("UNREGISTER_ACK|%1|SUCCESS").arg(peerId).toUtf8();
        sendToAddress(datagram.senderAddress(), datagram.senderPort(), response);
        LOG_INFO(QString("✅ 已向 %1 发送注销确认").arg(peerId));
    } else {
        LOG_WARNING(QString("❓ 收到一个来自未注册对等端的注销请求: %1").arg(peerId));
        QByteArray response = QString("UNREGISTER_ACK|%1|NOT_FOUND").arg(peerId).toUtf8();
        sendToAddress(datagram.senderAddress(), datagram.senderPort(), response);
    }
}

void P2PServer::processGetPeers(const QNetworkDatagram &datagram)
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

bool P2PServer::findAndConnectPeers(const QString &peerId, const QString &targetClientUuid, const QString &targetIp, const QString &targetPort)
{
    QWriteLocker locker(&m_peersLock);
    LOG_INFO(QString("🎯 开始查找特定匹配对等端: %1 正在寻找 客户端ID %2 (%3:%4)")
                 .arg(peerId, targetClientUuid, targetIp, targetPort));

    if (!m_peers.contains(peerId)) {
        LOG_ERROR(QString("❌ 发起方对等端不存在: %1").arg(peerId));
        return false;
    }
    PeerInfo &currentPeer = m_peers[peerId];

    LOG_INFO("=== 当前服务器上的所有对等端 ===");
    if (m_peers.isEmpty()) {
        LOG_WARNING("📭 对等端列表为空！");
    } else {
        for (const auto &peer : qAsConst(m_peers)) {
            LOG_INFO(QString("  - 对等端ID: %1, 客户端ID: %2, 状态: %3, 地址: %4:%5")
                         .arg(peer.id, peer.clientUuid, peer.status, peer.publicIp)
                         .arg(peer.publicPort));
        }
    }
    LOG_INFO("=== 结束对等端列表 ===");

    PeerInfo matchedPeer;
    bool foundMatch = false;

    bool ok;
    quint16 targetPortNum = targetPort.toUShort(&ok);
    if (!ok) {
        LOG_ERROR(QString("❌ 无效的目标端口号: '%1'").arg(targetPort));
        return false;
    }

    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        const PeerInfo &otherPeer = it.value();
        if (otherPeer.id == peerId) continue;

        bool uuidMatch = (otherPeer.clientUuid == targetClientUuid);
        bool ipMatch = (otherPeer.publicIp == targetIp);
        bool portMatch = (otherPeer.publicPort == targetPortNum);

        if (uuidMatch && ipMatch && portMatch) {
            if (otherPeer.status != "WAITING") {
                LOG_WARNING(QString("⚠️ 找到目标对等端 %1, 但其状态为 '%2', 而不是 'WAITING'. 无法建立连接。")
                                .arg(otherPeer.id, otherPeer.status));
                foundMatch = false;
                break;
            }

            LOG_INFO(QString("✅ 找到完全匹配且状态为WAITING的目标对等端: %1").arg(otherPeer.id));
            matchedPeer = otherPeer;
            foundMatch = true;
            break;
        }
    }

    if (foundMatch) {
        LOG_INFO(QString("🤝 建立匹配对: %1 <-> %2").arg(peerId, matchedPeer.id));
        currentPeer.status = "CONNECTING";
        m_peers[matchedPeer.id].status = "CONNECTING";

        notifyPeerAboutPeer(peerId, matchedPeer);
        notifyPeerAboutPeer(matchedPeer.id, currentPeer);

        emit peersMatched(peerId, matchedPeer.id, matchedPeer.publicIp, QString::number(matchedPeer.publicPort));
        return true;
    } else {
        LOG_WARNING(QString("⏳ 未能找到指定的目标对等端，发起方 %1 将保持/回到等待状态").arg(peerId));
        currentPeer.status = "WAITING";
        LOG_INFO(QString("💡 诊断: 未能在服务器上找到目标 [客户端ID: %1, Addr: %2:%3] 或者该目标当前不是'WAITING'状态。")
                     .arg(targetClientUuid, targetIp, targetPort));
        return false;
    }
}

void P2PServer::processPingRequest(const QNetworkDatagram &datagram)
{
    QString data = QString(datagram.data());
    QStringList parts = data.split('|');


    QString peerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());

    if (parts.size() >= 3) {
        QString publicIp = parts[1];
        QString publicPort = parts[2];
        LOG_INFO(QString("🏓 PING来自 %1, 公网信息: %2:%3").arg(peerId, publicIp, publicPort));

        bool isRegistered = false;
        {
            QReadLocker locker(&m_peersLock);
            isRegistered = m_peers.contains(peerId);
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

void P2PServer::sendDefaultResponse(const QNetworkDatagram &datagram)
{
    QByteArray response = "DEFAULT_RESPONSE|Message received at " +
                          QDateTime::currentDateTime().toString("hh:mm:ss.zzz").toUtf8();
    sendToAddress(datagram.senderAddress(), datagram.senderPort(), response);
    LOG_DEBUG(QString("📤 发送默认响应到 %1:%2").arg(datagram.senderAddress().toString()).arg(datagram.senderPort()));
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

void P2PServer::notifyPeerAboutPeer(const QString &peerId, const PeerInfo &otherPeer)
{
    QHostAddress targetAddress;
    quint16 targetPort = 0;
    bool targetFound = false;

    {
        QReadLocker locker(&m_peersLock);
        if (m_peers.contains(peerId)) {
            const PeerInfo &targetPeer = m_peers.value(peerId);
            QString cleanIp = targetPeer.publicIp;
            if (cleanIp.startsWith("::ffff:")) {
                cleanIp = cleanIp.mid(7);
            }
            targetAddress = QHostAddress(cleanIp);
            targetPort = targetPeer.publicPort;
            targetFound = !targetAddress.isNull();
        } else {
            LOG_ERROR(QString("❌ 对等端不存在: %1").arg(peerId));
        }
    }

    if (targetFound) {
        QString message = QString("PEER_INFO|%1|%2|%3|%4")
        .arg(otherPeer.publicIp)
            .arg(otherPeer.publicPort)
            .arg(otherPeer.localIp)
            .arg(otherPeer.localPort);

        qint64 bytesSent = sendToAddress(targetAddress, targetPort, message.toUtf8());

        if (bytesSent > 0) {
            LOG_INFO(QString("✅ 对等端信息发送成功: %1 -> %2 (%3 字节)")
                         .arg(otherPeer.id, peerId).arg(bytesSent));
        } else {
            LOG_ERROR(QString("❌ 对等端信息发送失败: %1 -> %2").arg(otherPeer.id, peerId));
        }
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

    // *** 修复 ***：使用正确的函数生成 initiatorId
    QString initiatorId = generatePeerId(datagram.senderAddress(), datagram.senderPort());
    QString targetId = parts[1];

    LOG_INFO(QString("🔄 协调打洞: 发起方 %1 -> 目标 %2").arg(initiatorId, targetId));

    PeerInfo initiatorPeer;
    PeerInfo targetPeer;
    bool found = false;

    {
        QReadLocker locker(&m_peersLock);
        if (m_peers.contains(initiatorId) && m_peers.contains(targetId)) {
            initiatorPeer = m_peers.value(initiatorId);
            targetPeer = m_peers.value(targetId);
            found = true;
        } else {
            if (!m_peers.contains(initiatorId)) LOG_WARNING(QString("❓ 未知的打洞发起方: %1").arg(initiatorId));
            if (!m_peers.contains(targetId)) LOG_WARNING(QString("❓ 未知的打洞目标: %1").arg(targetId));
        }
    }

    if (found) {
        LOG_INFO(QString("🤝 正在通知 %1 (发起方) 关于 %2 (目标) 的信息...").arg(initiatorId, targetId));
        notifyPeerAboutPeer(initiatorId, targetPeer);

        LOG_INFO(QString("🤝 正在通知 %1 (目标) 关于 %2 (发起方) 的信息...").arg(targetId, initiatorId));
        notifyPeerAboutPeer(targetId, initiatorPeer);

        emit punchRequested(initiatorId, targetId);
    }
}

void P2PServer::processKeepAlive(const QNetworkDatagram &datagram)
{
    QWriteLocker locker(&m_peersLock);

    QString peerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());

    if (m_peers.contains(peerId)) {
        m_peers[peerId].lastSeen = QDateTime::currentMSecsSinceEpoch();
        LOG_DEBUG(QString("💓 心跳: %1").arg(peerId));
    }
}

void P2PServer::processPeerInfoAck(const QNetworkDatagram &datagram)
{
    QWriteLocker locker(&m_peersLock);

    QString peerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());

    if (m_peers.contains(peerId)) {
        m_peers[peerId].lastSeen = QDateTime::currentMSecsSinceEpoch();
        LOG_INFO(QString("✅ 对等端确认: %1").arg(peerId));
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
    QString natType = parts[4];
    QString status = parts.size() > 5 ? parts[5] : "RELAY_WAITING";


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
    peerInfo.natType = natType;
    peerInfo.targetIp = "0.0.0.0";
    peerInfo.targetPort = 0;
    peerInfo.lastSeen = QDateTime::currentMSecsSinceEpoch();
    peerInfo.status = status;
    peerInfo.isRelayMode = true;

    {
        QWriteLocker locker(&m_peersLock);
        m_peers[peerId] = peerInfo;
    }

    LOG_INFO(QString("🔄 转发中继模式对等端注册: %1").arg(peerId));
    LOG_INFO(QString("  真实公网地址: %1:%2").arg(peerInfo.publicIp).arg(peerInfo.publicPort));
    LOG_INFO(QString("  中继地址: %1:%2").arg(relayIp, relayPort));
    LOG_INFO(QString("  NAT类型: %1").arg(natType));
    LOG_INFO(QString("  状态: %1").arg(status));

    QByteArray response = QString("REGISTER_RELAY_ACK|%1|%2|%3|%4").arg(peerId, relayIp, relayPort, status).toUtf8();
    qint64 bytesSent = sendToAddress(originalAddr, originalPort, response);

    if (bytesSent > 0) {
        LOG_INFO(QString("✅ 中继注册确认发送成功: %1 字节").arg(bytesSent));
    } else {
        LOG_ERROR(QString("❌ 中继注册确认发送失败"));
    }
    emit peerRegistered(peerId, clientUuid);
}

void P2PServer::sendToPeer(const QString &peerId, const QByteArray &data)
{
    QWriteLocker locker(&m_peersLock);
    if (!m_peers.contains(peerId)) {
        LOG_ERROR(QString("❌ 对等端不存在: %1").arg(peerId));
        return;
    }

    const PeerInfo &peer = m_peers[peerId];
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
        LOG_DEBUG(QString("📤 发送到 %1: %2 字节").arg(peerId).arg(bytesSent));
    } else {
        LOG_ERROR(QString("❌ 发送失败到 %1").arg(peerId));
    }
}

void P2PServer::onCleanupTimeout()
{
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

void P2PServer::cleanupExpiredPeers()
{
    QWriteLocker locker(&m_peersLock);
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    QList<QString> expiredPeers;

    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        if (currentTime - it.value().lastSeen > m_peerTimeout) {
            expiredPeers.append(it.key());
        }
    }

    for (const QString &peerId : expiredPeers) {
        LOG_INFO(QString("🗑️ 移除过期对等端: %1").arg(peerId));
        m_peers.remove(peerId);
        emit peerRemoved(peerId);
    }

    if (!expiredPeers.isEmpty()) {
        LOG_INFO(QString("🧹 已清理 %1 个过期对等端").arg(expiredPeers.size()));
    }
}

QString P2PServer::generatePeerId(const QHostAddress &address, quint16 port)
{
    QString ipString = address.toString();
    if (ipString.startsWith("::ffff:")) {
        ipString = ipString.mid(7);
    }
    return QString("%1:%2").arg(ipString).arg(port);
}

void P2PServer::removePeer(const QString &peerId)
{
    QWriteLocker locker(&m_peersLock);
    if (m_peers.contains(peerId)) {
        m_peers.remove(peerId);
        LOG_INFO(QString("🗑️ 已移除对等端: %1").arg(peerId));
        emit peerRemoved(peerId);
    }
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
    int count = (maxCount < 0 || maxCount > peerList.size() - 1) ? peerList.size() : maxCount;
    if (count > peerList.size() - 1) {
        count = peerList.size() - 1;
    }

    LOG_INFO(QString("🔍 正在准备对等端列表... 请求数量: %1, 排除UUID: %2, 总对等端数: %3")
                 .arg(maxCount).arg(excludeClientUuid).arg(peerList.size()));

    QByteArray response = "PEER_LIST|";
    int peersAdded = 0;
    QStringList peersLogList;
    peersLogList << QString("--- 将要发送给 %1 的对等端列表 (最多 %2 个) ---").arg(excludeClientUuid).arg(count);

    for (const PeerInfo &peer : qAsConst(peerList)) {
        if (peersAdded >= count) break;
        if (peer.clientUuid == excludeClientUuid) continue;

        QString peerData = QString("id=%1;cid=%2;lip=%3;lport=%4;pip=%5;pport=%6;rip=%7;rport=%8;tip=%9;tport=%10;nat=%11;seen=%12;stat=%13;relay=%14")
                               .arg(peer.id, peer.clientUuid, peer.localIp).arg(peer.localPort)
                               .arg(peer.publicIp).arg(peer.publicPort)
                               .arg(peer.relayIp).arg(peer.relayPort)
                               .arg(peer.targetIp).arg(peer.targetPort)
                               .arg(peer.natType, peer.lastSeen)
                               .arg(peer.status, peer.isRelayMode ? "1" : "0");

        response.append(peerData.toUtf8());
        response.append("|");
        peersAdded++;

        peersLogList << QString("  [%1/%2] ID: %3, UUID: %4, 状态: %5")
                            .arg(peersAdded, 2, 10, QChar(' ')).arg(count, 2, 10, QChar(' '))
                            .arg(peer.id, -22).arg(peer.clientUuid, peer.status);
    }

    if (response.endsWith('|')) {
        response.chop(1);
    }
    if (peersAdded > 0) {
        LOG_INFO(peersLogList.join("\n"));
    } else {
        LOG_INFO(QString("ℹ️ 没有找到符合条件的可发送对等端给 %1").arg(excludeClientUuid));
    }

    LOG_INFO(QString("✅ 对等端列表准备完成，共发送 %1 个对等端给请求者。").arg(peersAdded));
    return response;
}

bool P2PServer::isRunning() const
{
    return m_isRunning;
}

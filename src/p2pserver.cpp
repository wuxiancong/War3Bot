#include "p2pserver.h"
#include "logger.h"
#include <QNetworkInterface>
#include <QDateTime>

P2PServer::P2PServer(QObject *parent)
    : QObject(parent)
    , m_udpSocket(nullptr)
    , m_cleanupTimer(nullptr)
    , m_broadcastTimer(nullptr)
    , m_settings(nullptr)
    , m_listenPort(0)
    , m_isRunning(false)
    , m_peerTimeout(300000) // 5分钟
    , m_cleanupInterval(60000) // 1分钟
    , m_broadcastInterval(30000) // 30秒
    , m_enableBroadcast(false)
    , m_broadcastPort(6112)
{
}

P2PServer::~P2PServer()
{
    stopServer();
}

bool P2PServer::startServer(quint16 port, const QString &configFile)
{
    if (m_isRunning) {
        return true;
    }

    // 加载配置文件
    m_settings = new QSettings(configFile, QSettings::IniFormat, this);

    m_peerTimeout = m_settings->value("server/peer_timeout", 300000).toInt();
    m_cleanupInterval = m_settings->value("server/cleanup_interval", 60000).toInt();
    m_broadcastInterval = m_settings->value("server/broadcast_interval", 30000).toInt();
    m_enableBroadcast = m_settings->value("server/enable_broadcast", false).toBool();
    m_broadcastPort = m_settings->value("server/broadcast_port", 6112).toUInt();

    // 创建UDP socket
    m_udpSocket = new QUdpSocket(this);
    if (!m_udpSocket->bind(QHostAddress::AnyIPv4, port)) {
        LOG_ERROR(QString("Failed to bind UDP socket to port %1: %2")
                      .arg(port).arg(m_udpSocket->errorString()));
        return false;
    }

    connect(m_udpSocket, &QUdpSocket::readyRead, this, &P2PServer::onReadyRead);

    m_listenPort = port;
    m_isRunning = true;

    // 启动清理定时器
    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &P2PServer::onCleanupTimeout);
    m_cleanupTimer->start(m_cleanupInterval);

    // 启动广播定时器（如果启用）
    if (m_enableBroadcast) {
        m_broadcastTimer = new QTimer(this);
        connect(m_broadcastTimer, &QTimer::timeout, this, &P2PServer::onBroadcastTimeout);
        m_broadcastTimer->start(m_broadcastInterval);
    }

    LOG_INFO(QString("P2P server started on port %1").arg(port));
    LOG_INFO(QString("Peer timeout: %1 ms").arg(m_peerTimeout));
    LOG_INFO(QString("Cleanup interval: %1 ms").arg(m_cleanupInterval));
    LOG_INFO(QString("Broadcast enabled: %1").arg(m_enableBroadcast ? "true" : "false"));

    emit serverStarted(port);

    return true;
}

void P2PServer::stopServer()
{
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;

    if (m_cleanupTimer) {
        m_cleanupTimer->stop();
        delete m_cleanupTimer;
        m_cleanupTimer = nullptr;
    }

    if (m_broadcastTimer) {
        m_broadcastTimer->stop();
        delete m_broadcastTimer;
        m_broadcastTimer = nullptr;
    }

    if (m_udpSocket) {
        m_udpSocket->close();
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }

    if (m_settings) {
        delete m_settings;
        m_settings = nullptr;
    }

    m_peers.clear();
    LOG_INFO("P2P server stopped");
    emit serverStopped();
}

void P2PServer::onReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        if (!datagram.isValid()) {
            continue;
        }

        QByteArray data = datagram.data();
        QString senderAddress = datagram.senderAddress().toString();
        quint16 senderPort = datagram.senderPort();

        LOG_DEBUG(QString("Received %1 bytes from %2:%3: %4")
                      .arg(data.size()).arg(senderAddress).arg(senderPort)
                      .arg(QString(data)));

        // 解析消息类型
        if (data.startsWith("HANDSHAKE")) {
            processHandshake(datagram);
        } else if (data.startsWith("PUNCH")) {
            processPunchRequest(datagram);
        } else if (data.startsWith("KEEPALIVE")) {
            processKeepAlive(datagram);
        } else {
            LOG_WARNING(QString("Unknown message type from %1:%2: %3")
                            .arg(senderAddress).arg(senderPort).arg(QString(data)));
        }
    }
}

void P2PServer::processHandshake(const QNetworkDatagram &datagram)
{
    // 格式: HANDSHAKE|GAME_ID|A_LOCAL_PORT|A_PUBLIC_IP|A_PUBLIC_PORT|B_PUBLIC_IP|B_PUBLIC_PORT
    QStringList parts = QString(datagram.data()).split('|');
    if (parts.size() < 7) {  // 确保有足够的字段
        LOG_WARNING("Invalid handshake format");
        return;
    }

    QString gameId = parts[1];
    quint16 aLocalPort = parts[2].toUShort();
    QString aPublicIp = parts[3];
    quint16 aPublicPort = parts[4].toUShort();
    QString bPublicIp = parts[5];
    quint16 bPublicPort = parts[6].toUShort();

    QString peerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());

    PeerInfo peer;
    peer.publicAddress = datagram.senderAddress();
    peer.publicPort = datagram.senderPort();
    peer.localAddress = datagram.destinationAddress();  // 本地地址
    peer.localPort = aLocalPort;                         // A 的本地端口
    peer.aPublicIp = QHostAddress(aPublicIp);            // A 的公网 IP
    peer.aPublicPort = aPublicPort;                      // A 的公网端口
    peer.bPublicIp = QHostAddress(bPublicIp);            // B 的公网 IP
    peer.bPublicPort = bPublicPort;                      // B 的公网端口
    peer.lastSeen = QDateTime::currentMSecsSinceEpoch();
    peer.gameId = gameId;

    m_peers[peerId] = peer;

    LOG_INFO(QString("Peer registered: %1 (%2:%3) game: %4")
                 .arg(peerId, peer.publicAddress.toString(), QString::number(peer.publicPort), gameId));

    // 发送确认
    QByteArray response = QString("HANDSHAKE_OK|%1").arg(peerId).toUtf8();
    m_udpSocket->writeDatagram(response, datagram.senderAddress(), datagram.senderPort());

    emit peerRegistered(peerId, gameId);
}

void P2PServer::processPunchRequest(const QNetworkDatagram &datagram)
{
    // 格式: PUNCH|TARGET_PEER_ID
    QStringList parts = QString(datagram.data()).split('|');
    if (parts.size() < 2) {
        LOG_WARNING("Invalid punch request format");
        return;
    }

    QString sourcePeerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());
    QString targetPeerId = parts[1];

    if (!m_peers.contains(sourcePeerId)) {
        LOG_WARNING(QString("Unknown source peer: %1").arg(sourcePeerId));
        return;
    }

    if (!m_peers.contains(targetPeerId)) {
        LOG_WARNING(QString("Unknown target peer: %1").arg(targetPeerId));
        return;
    }

    PeerInfo &sourcePeer = m_peers[sourcePeerId];
    PeerInfo &targetPeer = m_peers[targetPeerId];

    // 向双方发送对方的公网信息
    sendPunchInfo(sourcePeerId, targetPeer);
    sendPunchInfo(targetPeerId, sourcePeer);

    LOG_INFO(QString("Punch request: %1 -> %2").arg(sourcePeerId, targetPeerId));
    emit punchRequested(sourcePeerId, targetPeerId);
}

void P2PServer::processKeepAlive(const QNetworkDatagram &datagram)
{
    QString peerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());

    if (m_peers.contains(peerId)) {
        m_peers[peerId].lastSeen = QDateTime::currentMSecsSinceEpoch();
        LOG_DEBUG(QString("Keepalive from peer: %1").arg(peerId));
    } else {
        LOG_WARNING(QString("Keepalive from unknown peer: %1").arg(peerId));
    }
}

void P2PServer::sendPunchInfo(const QString &peerId, const PeerInfo &peer)
{
    if (!m_peers.contains(peerId)) {
        return;
    }

    PeerInfo &targetPeer = m_peers[peerId];

    // 发送 A 端的公网地址信息
    QString aPublicInfo = QString("PUNCH_INFO|A_PUBLIC|%1|%2|%3")
                              .arg(peer.aPublicIp.toString())
                              .arg(peer.aPublicPort)
                              .arg(peer.gameId);

    m_udpSocket->writeDatagram(aPublicInfo.toUtf8(),
                               targetPeer.publicAddress,
                               targetPeer.publicPort);

    // 发送 B 端的公网地址信息
    QString bPublicInfo = QString("PUNCH_INFO|B_PUBLIC|%1|%2|%3")
                              .arg(peer.bPublicIp.toString())
                              .arg(peer.bPublicPort)
                              .arg(peer.gameId);

    m_udpSocket->writeDatagram(bPublicInfo.toUtf8(),
                               targetPeer.publicAddress,
                               targetPeer.publicPort);

    LOG_DEBUG(QString("Sent punch info for %1 to %2").arg(peerId, targetPeer.publicAddress.toString()));
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
    if (!m_enableBroadcast) {
        return;
    }

    QByteArray broadcastMsg = QString("WAR3BOT_SERVER|%1").arg(m_listenPort).toUtf8();

    // 广播到局域网
    m_udpSocket->writeDatagram(broadcastMsg, QHostAddress::Broadcast, m_broadcastPort);

    LOG_DEBUG("Broadcast server info to LAN");
}

void P2PServer::cleanupExpiredPeers()
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    QList<QString> expiredPeers;

    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        if (currentTime - it.value().lastSeen > m_peerTimeout) {
            expiredPeers.append(it.key());
        }
    }

    for (const QString &peerId : expiredPeers) {
        LOG_INFO(QString("Removing expired peer: %1").arg(peerId));
        m_peers.remove(peerId);
        emit peerRemoved(peerId);
    }

    if (!expiredPeers.isEmpty()) {
        LOG_INFO(QString("Cleaned up %1 expired peers").arg(expiredPeers.size()));
    }
}

QString P2PServer::generatePeerId(const QHostAddress &address, quint16 port)
{
    return QString("%1:%2").arg(address.toString()).arg(port);
}

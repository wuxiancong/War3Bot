#include "p2pserver.h"
#include "logger.h"
#include <QNetworkInterface>
#include <QNetworkDatagram>
#include <QDateTime>

P2PServer::P2PServer(QObject *parent)
    : QObject(parent)
    , m_isRunning(false)
    , m_peerTimeout(300000)
    , m_listenPort(0)
    , m_cleanupInterval(60000)
    , m_settings(nullptr)
    , m_cleanupTimer(nullptr)
    , m_enableBroadcast(false) // 5分钟
    , m_broadcastInterval(30000) // 1分钟
    , m_broadcastPort(6112) // 30秒
    , m_udpSocket(nullptr)
    , m_broadcastTimer(nullptr)
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
        if (data.startsWith("HANDSHAKE|")) {
            processHandshake(datagram);
        } else if (data.startsWith("PUNCH")) {
            processPunchRequest(datagram);
        } else if (data.startsWith("KEEPALIVE")) {
            processKeepAlive(datagram);
        } else if (data.startsWith("PEER_INFO_ACK")) {
            processPeerInfoAck(datagram);
        } else {
            LOG_WARNING(QString("Unknown message type from %1:%2: %3")
                            .arg(senderAddress).arg(senderPort).arg(QString(data)));
        }
    }
}

void P2PServer::processHandshake(const QNetworkDatagram &datagram)
{
    // 格式: HANDSHAKE|GAME_ID|LOCAL_PORT|LOCAL_IP|LOCAL_PORT|TARGET_IP|TARGET_PORT
    QString data = QString(datagram.data());
    QStringList parts = data.split('|');

    if (parts.size() < 7) {
        LOG_WARNING(QString("Invalid handshake format from %1:%2: %3")
                        .arg(datagram.senderAddress().toString())
                        .arg(datagram.senderPort())
                        .arg(data));
        return;
    }

    QString gameId = parts[1];
    QString localPort = parts[2];
    QString localIp = parts[3];
    QString localPublicPort = parts[4];
    QString targetIp = parts[5];
    QString targetPort = parts[6];

    QString peerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());

    // 创建对等端信息
    PeerInfo peerInfo;
    peerInfo.id = peerId;
    peerInfo.gameId = gameId;
    peerInfo.localIp = localIp;
    peerInfo.localPort = localPort.toUShort();
    peerInfo.publicIp = datagram.senderAddress().toString();
    peerInfo.publicPort = datagram.senderPort();
    peerInfo.targetIp = targetIp;
    peerInfo.targetPort = targetPort.toUShort();
    peerInfo.lastSeen = QDateTime::currentMSecsSinceEpoch();

    // 存储对等端信息
    m_peers[peerId] = peerInfo;

    LOG_INFO(QString("Peer registered: %1 (%2) game: %3")
                 .arg(peerId, peerInfo.publicIp, gameId));
    LOG_INFO(QString("Handshake details - Local: %1:%2 localPublicPort: %3, Target: %4:%5")
                 .arg(localIp, localPort, localPublicPort, targetIp, targetPort));

    // 发送握手确认
    QByteArray response = QString("HANDSHAKE_OK|%1").arg(peerId).toUtf8();
    m_udpSocket->writeDatagram(response, datagram.senderAddress(), datagram.senderPort());

    // 查找匹配的对等端并转发信息
    findAndConnectPeers(peerId, targetIp, targetPort);

    emit peerRegistered(peerId, gameId);
}

void P2PServer::findAndConnectPeers(const QString &peerId, const QString &targetIp, const QString &targetPort)
{
    // 查找所有想要连接到相同目标的对等端
    QList<PeerInfo> matchingPeers;

    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        const PeerInfo &otherPeer = it.value();
        if (otherPeer.id != peerId &&
            otherPeer.targetIp == targetIp &&
            otherPeer.targetPort == targetPort.toUShort()) {
            matchingPeers.append(otherPeer);
        }
    }

    if (!matchingPeers.isEmpty()) {
        // 找到匹配的对等端，互相通知
        for (const PeerInfo &otherPeer : matchingPeers) {
            // 通知当前对等端关于另一个对等端的信息
            notifyPeerAboutPeer(peerId, otherPeer);
            // 通知另一个对等端关于当前对等端的信息
            notifyPeerAboutPeer(otherPeer.id, m_peers[peerId]);

            LOG_INFO(QString("Matched peers: %1 <-> %2 for target %3:%4")
                         .arg(peerId, otherPeer.id, targetIp, targetPort));

            emit peersMatched(peerId, otherPeer.id, targetIp, targetPort);
        }
    } else {
        LOG_INFO(QString("No matching peers found for %1, waiting for other peers...").arg(peerId));
    }
}

void P2PServer::notifyPeerAboutPeer(const QString &peerId, const PeerInfo &otherPeer)
{
    // 构造通知消息：PEER_INFO|PEER_PUBLIC_IP|PEER_PUBLIC_PORT|PEER_LOCAL_IP|PEER_LOCAL_PORT
    QString message = QString("PEER_INFO|%1|%2|%3|%4")
                          .arg(otherPeer.publicIp)
                          .arg(otherPeer.publicPort)
                          .arg(otherPeer.localIp)
                          .arg(otherPeer.localPort);

    // 发送给对等端
    QHostAddress peerAddress(otherPeer.publicIp);
    sendToPeer(peerId, message.toUtf8());

    LOG_INFO(QString("Sent peer info to %1: %2 peerAddress: %3").arg(peerId, message, peerAddress.toString()));
}

void P2PServer::sendToPeer(const QString &peerId, const QByteArray &data)
{
    if (!m_peers.contains(peerId)) {
        LOG_WARNING(QString("Cannot send to unknown peer: %1").arg(peerId));
        return;
    }

    const PeerInfo &peer = m_peers[peerId];
    QHostAddress address(peer.publicIp);
    m_udpSocket->writeDatagram(data, address, peer.publicPort);
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

    // 更新最后活跃时间
    sourcePeer.lastSeen = QDateTime::currentMSecsSinceEpoch();

    // 向目标对等端发送打洞请求通知
    QString punchNotify = QString("PUNCH_REQUEST|%1|%2|%3|%4|%5|%6")
                              .arg(sourcePeer.publicIp,
                                   QString::number(sourcePeer.publicPort),
                                   sourcePeer.localIp,
                                   targetPeer.publicIp,
                                   QString::number(targetPeer.publicPort),
                                   targetPeer.localIp);

    sendToPeer(targetPeerId, punchNotify.toUtf8());

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

void P2PServer::processPeerInfoAck(const QNetworkDatagram &datagram)
{
    QString peerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());

    if (m_peers.contains(peerId)) {
        m_peers[peerId].lastSeen = QDateTime::currentMSecsSinceEpoch();
        LOG_INFO(QString("Peer %1 acknowledged peer info").arg(peerId));
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

// 添加头文件中需要的新方法声明
void P2PServer::removePeer(const QString &peerId)
{
    if (m_peers.contains(peerId)) {
        m_peers.remove(peerId);
        LOG_INFO(QString("Removed peer: %1").arg(peerId));
        emit peerRemoved(peerId);
    }
}

QList<QString> P2PServer::getConnectedPeers() const
{
    return m_peers.keys();
}

int P2PServer::getPeerCount() const
{
    return m_peers.size();
}

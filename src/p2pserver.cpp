#include "p2pserver.h"
#include "logger.h"
#include <QNetworkInterface>
#include <QNetworkDatagram>
#include <QDateTime>
#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

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

    // 跨平台端口重用设置
    int fd = m_udpSocket->socketDescriptor();
    if (fd != -1) {
        int reuse = 1;

#ifdef Q_OS_WIN
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
            LOG_WARNING("Failed to set SO_REUSEADDR on Windows");
        }
#else
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            LOG_WARNING("Failed to set SO_REUSEADDR on Linux");
        }
#ifdef SO_REUSEPORT
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
            LOG_WARNING("Failed to set SO_REUSEPORT on Linux");
        }
#endif
#endif
    }

    // 使用 ShareAddress 选项允许端口重用
    if (!m_udpSocket->bind(QHostAddress::Any, port, QUdpSocket::ShareAddress)) {
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
    QString senderAddress = datagram.senderAddress().toString();
    quint16 senderPort = datagram.senderPort();

    LOG_INFO(QString("=== 收到握手请求 ==="));
    LOG_INFO(QString("来自: %1:%2").arg(senderAddress).arg(senderPort));
    LOG_INFO(QString("内容: %1").arg(data));

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

    // 发送确认前也记录日志
    LOG_INFO(QString("发送握手确认到: %1:%2, PeerID: %3")
                 .arg(datagram.senderAddress().toString())
                 .arg(datagram.senderPort())
                 .arg(peerId));

    QByteArray response = QString("HANDSHAKE_OK|%1").arg(peerId).toUtf8();
    qint64 bytesSent = m_udpSocket->writeDatagram(response, datagram.senderAddress(), datagram.senderPort());

    LOG_INFO(QString("确认消息发送结果: %1 字节").arg(bytesSent));

    if (targetIp == "8.135.235.206" && targetPort == "6112") {
        LOG_INFO("🎯 检测到A端想要连接B端，直接发送PEER_INFO给B端");
        sendPeerInfoToBDirectly();
    }

    // 查找匹配的对等端并转发信息
    findAndConnectPeers(peerId, targetIp, targetPort);

    emit peerRegistered(peerId, gameId);
}

void P2PServer::findAndConnectPeers(const QString &peerId, const QString &targetIp, const QString &targetPort)
{
    LOG_INFO(QString("🎯 === 开始查找匹配对等端 ==="));
    LOG_INFO(QString("📡 当前对等端: %1").arg(peerId));
    LOG_INFO(QString("🎯 目标地址: %1:%2").arg(targetIp, targetPort));
    LOG_INFO(QString("👥 当前已连接对等端总数: %1").arg(m_peers.size()));

    // 显示所有对等端的详细信息
    LOG_INFO(QString("📋 === 所有对等端列表 ==="));
    int counter = 0;
    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        const PeerInfo &peer = it.value();
        counter++;
        LOG_INFO(QString("  对等端 #%1: ID=%2").arg(counter).arg(peer.id));
        LOG_INFO(QString("    公网: %1:%2").arg(peer.publicIp).arg(peer.publicPort));
        LOG_INFO(QString("    内网: %1:%2").arg(peer.localIp).arg(peer.localPort));
        LOG_INFO(QString("    目标: %1:%2").arg(peer.targetIp).arg(peer.targetPort));
        LOG_INFO(QString("    游戏: %1").arg(peer.gameId));
    }
    LOG_INFO(QString("📋 === 列表结束 ==="));

    // 查找匹配的对等端
    QList<PeerInfo> matchingPeers;
    int matchCounter = 0;

    LOG_INFO(QString("🔍 === 开始匹配检查 ==="));
    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        const PeerInfo &otherPeer = it.value();
        matchCounter++;

        LOG_INFO(QString("  检查对等端 #%1: %2").arg(matchCounter).arg(otherPeer.id));

        bool isNotSelf = (otherPeer.id != peerId);
        bool targetIpMatch = (otherPeer.targetIp == targetIp);
        bool targetPortMatch = (otherPeer.targetPort == targetPort.toUShort());

        LOG_INFO(QString("    检查条件:"));
        LOG_INFO(QString("      ID不同: %1").arg(isNotSelf ? "✅" : "❌"));
        LOG_INFO(QString("      目标IP相同: %1 (%2 vs %3)").arg(targetIpMatch ? "✅" : "❌", otherPeer.targetIp, targetIp));
        LOG_INFO(QString("      目标端口相同: %1 (%2 vs %3)").arg(targetPortMatch ? "✅" : "❌").arg(otherPeer.targetPort).arg(targetPort));

        if (isNotSelf && targetIpMatch && targetPortMatch) {
            matchingPeers.append(otherPeer);
            LOG_INFO(QString("    🎉 找到匹配对等端!"));
        } else {
            LOG_INFO(QString("    ❌ 不匹配"));
        }
    }

    LOG_INFO(QString("🔍 === 匹配检查完成 ==="));
    LOG_INFO(QString("📊 匹配结果: 找到 %1 个匹配对等端").arg(matchingPeers.size()));

    if (!matchingPeers.isEmpty()) {
        LOG_INFO(QString("🔄 === 开始双向通知 ==="));

        for (const PeerInfo &otherPeer : matchingPeers) {
            LOG_INFO(QString("  🤝 匹配对: %1 <-> %2").arg(peerId, otherPeer.id));

            // 通知当前对等端关于另一个对等端的信息
            LOG_INFO(QString("  1. 通知 %1 关于 %2").arg(peerId, otherPeer.id));
            notifyPeerAboutPeer(peerId, otherPeer);

            // 通知另一个对等端关于当前对等端的信息
            LOG_INFO(QString("  2. 通知 %1 关于 %2").arg(otherPeer.id, peerId));
            notifyPeerAboutPeer(otherPeer.id, m_peers[peerId]);

            LOG_INFO(QString("  ✅ 双向通知完成"));

            emit peersMatched(peerId, otherPeer.id, targetIp, targetPort);
        }

        LOG_INFO(QString("🔄 === 双向通知完成 ==="));
    } else {
        LOG_INFO(QString("⏳ 没有找到匹配的对等端，继续等待..."));
        LOG_INFO(QString("💡 提示: 需要另一个客户端连接到相同目标 %1:%2").arg(targetIp, targetPort));
    }

    LOG_INFO(QString("🎯 === 匹配查找完成 ==="));
}

void P2PServer::notifyPeerAboutPeer(const QString &peerId, const PeerInfo &otherPeer)
{
    LOG_INFO(QString("📤 === 开始通知对等端 ==="));
    LOG_INFO(QString("  接收方: %1").arg(peerId));
    LOG_INFO(QString("  通知内容 - 对方信息:"));
    LOG_INFO(QString("    公网地址: %1:%2").arg(otherPeer.publicIp).arg(otherPeer.publicPort));
    LOG_INFO(QString("    内网地址: %1:%2").arg(otherPeer.localIp).arg(otherPeer.localPort));
    LOG_INFO(QString("    目标地址: %1:%2").arg(otherPeer.targetIp).arg(otherPeer.targetPort));

    // 构造通知消息
    QString message = QString("PEER_INFO|%1|%2|%3|%4")
                          .arg(otherPeer.publicIp)
                          .arg(otherPeer.publicPort)
                          .arg(otherPeer.localIp)
                          .arg(otherPeer.localPort);

    LOG_INFO(QString("  构造的消息: %1").arg(message));
    LOG_INFO(QString("  消息长度: %1 字节").arg(message.toUtf8().size()));

    // 检查接收方是否存在
    if (!m_peers.contains(peerId)) {
        LOG_ERROR(QString("  ❌ 错误: 对等端 %1 不存在!").arg(peerId));
        return;
    }

    const PeerInfo &targetPeer = m_peers[peerId];

    // 处理IPv6格式地址
    QString cleanIp = targetPeer.publicIp;
    if (cleanIp.startsWith("::ffff:")) {
        cleanIp = cleanIp.mid(7);
        LOG_INFO(QString("  清理IPv6地址: %1 -> %2").arg(targetPeer.publicIp, cleanIp));
    }

    QHostAddress peerAddress(cleanIp);
    if (peerAddress.isNull()) {
        LOG_ERROR(QString("  ❌ 错误: 无效的对等端地址: %1").arg(cleanIp));
        return;
    }

    LOG_INFO(QString("  发送到: %1:%2").arg(cleanIp).arg(targetPeer.publicPort));

    // 发送消息
    qint64 bytesSent = m_udpSocket->writeDatagram(message.toUtf8(), peerAddress, targetPeer.publicPort);

    if (bytesSent == -1) {
        LOG_ERROR(QString("  ❌ 发送失败: %1").arg(m_udpSocket->errorString()));
    } else {
        LOG_INFO(QString("  ✅ 发送成功: %1 字节").arg(bytesSent));
    }

    LOG_INFO(QString("📤 === 通知完成 ==="));
}

void P2PServer::sendToPeer(const QString &peerId, const QByteArray &data)
{
    LOG_INFO(QString("🚀 === 直接发送消息 ==="));
    LOG_INFO(QString("  接收方: %1").arg(peerId));
    LOG_INFO(QString("  消息内容: %1").arg(QString(data)));
    LOG_INFO(QString("  消息长度: %1 字节").arg(data.size()));

    if (!m_peers.contains(peerId)) {
        LOG_ERROR(QString("  ❌ 错误: 对等端 %1 不存在").arg(peerId));
        return;
    }

    const PeerInfo &peer = m_peers[peerId];

    // 处理IPv6格式地址
    QString cleanIp = peer.publicIp;
    if (cleanIp.startsWith("::ffff:")) {
        cleanIp = cleanIp.mid(7);
        LOG_INFO(QString("  清理IPv6地址: %1 -> %2").arg(peer.publicIp, cleanIp));
    }

    QHostAddress address(cleanIp);
    if (address.isNull()) {
        LOG_ERROR(QString("  ❌ 错误: 无效地址: %1").arg(cleanIp));
        return;
    }

    LOG_INFO(QString("  目标地址: %1:%2").arg(cleanIp).arg(peer.publicPort));

    qint64 bytesSent = m_udpSocket->writeDatagram(data, address, peer.publicPort);

    if (bytesSent == -1) {
        LOG_ERROR(QString("  ❌ 发送失败: %1").arg(m_udpSocket->errorString()));
    } else {
        LOG_INFO(QString("  ✅ 发送成功: %1 字节").arg(bytesSent));
    }

    LOG_INFO(QString("🚀 === 发送完成 ==="));
}

void P2PServer::sendPeerInfoToBDirectly()
{
    LOG_INFO("🔄 === 开始直接发送PEER_INFO给B端 ===");

    // B端的地址（从A端握手消息中得知）
    QString bPublicIp = "8.135.235.206";
    unsigned short bPublicPort = 6112;

    // A端的地址（当前连接的客户端）
    QString aPeerId = "::ffff:207.90.238.225:33428";

    if (!m_peers.contains(aPeerId)) {
        LOG_ERROR("❌ A端对等端不存在");
        return;
    }

    const PeerInfo &aPeerInfo = m_peers[aPeerId];

    LOG_INFO(QString("📤 发送A端信息给B端:"));
    LOG_INFO(QString("  A端: %1:%2").arg(aPeerInfo.publicIp).arg(aPeerInfo.publicPort));
    LOG_INFO(QString("  B端: %1:%2").arg(bPublicIp).arg(bPublicPort));

    // 构造PEER_INFO消息
    QString message = QString("PEER_INFO|%1|%2|%3|%4")
                          .arg(aPeerInfo.publicIp)
                          .arg(aPeerInfo.publicPort)
                          .arg(aPeerInfo.localIp)
                          .arg(aPeerInfo.localPort);

    LOG_INFO(QString("  消息内容: %1").arg(message));

    // 直接发送给B端，不需要B端先连接
    QHostAddress bAddress(bPublicIp);
    qint64 bytesSent = m_udpSocket->writeDatagram(message.toUtf8(), bAddress, bPublicPort);

    if (bytesSent == -1) {
        LOG_ERROR(QString("❌ 发送失败: %1").arg(m_udpSocket->errorString()));
    } else {
        LOG_INFO(QString("✅ 发送成功: %1 字节到 %2:%3").arg(bytesSent).arg(bPublicIp).arg(bPublicPort));
    }

    LOG_INFO("🔄 === 直接发送完成 ===");
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

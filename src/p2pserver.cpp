#include "p2pserver.h"
#include "logger.h"
#include <QTimer>
#include <QDateTime>
#include <QNetworkDatagram>
#include <QNetworkInterface>

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

    // 解析消息类型
    if (data.startsWith("HANDSHAKE|")) {
        LOG_INFO("🔗 处理 HANDSHAKE 消息");
        processHandshake(datagram);
    } else if (data.startsWith("REGISTER|")) {
        LOG_INFO("📝 处理 REGISTER 消息");
        processRegister(datagram);
    } else if (data.startsWith("PUNCH")) {
        LOG_INFO("🔄 处理 PUNCH 消息");
        processPunchRequest(datagram);
    } else if (data.startsWith("KEEPALIVE")) {
        LOG_DEBUG("💓 处理 KEEPALIVE 消息");
        processKeepAlive(datagram);
    } else if (data.startsWith("PEER_INFO_ACK")) {
        LOG_INFO("✅ 处理 PEER_INFO_ACK 消息");
        processPeerInfoAck(datagram);
    } else if (data.startsWith("PING|")) {
        LOG_INFO("🏓 收到PING请求，发送PONG回复");
        QByteArray pongResponse = "PONG|War3BotServer";
        sendToAddress(datagram.senderAddress(), datagram.senderPort(), pongResponse);
        LOG_INFO("✅ PONG回复已发送");
    } else if (data.startsWith("TEST|")) {
        LOG_INFO("🧪 处理测试消息");
        QByteArray testResponse = "TEST_RESPONSE|Hello from War3Bot Server";
        sendToAddress(datagram.senderAddress(), datagram.senderPort(), testResponse);
        LOG_INFO("✅ 测试回复已发送");
    } else {
        LOG_WARNING(QString("❓ 未知消息类型来自 %1:%2: %3")
                        .arg(senderAddress).arg(senderPort).arg(QString(data)));
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

    QString gameId = parts[1];
    QString localIp = parts[2];
    QString localPort = parts[3];
    QString targetIp = parts[4];
    QString targetPort = parts[5];
    QString status = parts[6];

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
    peerInfo.status = status;

    // 存储对等端信息
    m_peers[peerId] = peerInfo;

    LOG_INFO(QString("✅ 对等端已注册: %1 (%2) 游戏: %3")
                 .arg(peerId, peerInfo.publicIp, gameId));

    // 发送握手确认
    sendHandshakeAck(datagram, peerId);

    // 查找匹配的对等端，并根据结果发送不同信号
    bool matched = findAndConnectPeers(peerId, targetIp, targetPort);

    if (matched) {
        // 匹配成功，发送握手完成信号
        emit peerHandshaked(peerId, gameId, targetIp, targetPort);
        LOG_INFO(QString("🎉 握手完成: %1 成功匹配到目标").arg(peerId));
    } else {
        // 匹配中，发送握手进行中信号
        emit peerHandshaking(peerId, gameId, targetIp, targetPort);
        LOG_INFO(QString("⏳ 握手进行中: %1 等待目标对等端连接").arg(peerId));
    }
}

void P2PServer::processRegister(const QNetworkDatagram &datagram)
{
    QString data = QString(datagram.data());
    QStringList parts = data.split('|');

    // 格式: REGISTER|GAME_ID|LOCAL_PORT|LOCAL_IP|STATUS
    if (parts.size() < 5) {
        LOG_WARNING(QString("❌ 无效的注册格式: %1").arg(data));
        LOG_WARNING(QString("期望 5个部分，实际收到: %1 个部分").arg(parts.size()));
        for (int i = 0; i < parts.size(); ++i) {
            LOG_WARNING(QString("  部分[%1]: %2").arg(i).arg(parts[i]));
        }
        return;
    }

    QString gameId = parts[1];
    QString localIp = parts[2];
    QString localPort = parts[3];
    QString status = parts.size() > 4 ? parts[4] : "WAITING";

    QString peerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());

    // 创建对等端信息
    PeerInfo peerInfo;
    peerInfo.id = peerId;
    peerInfo.gameId = gameId;
    peerInfo.localIp = localIp;                                 // 内网IP
    peerInfo.localPort = localPort.toUShort();                  // 内网端口
    peerInfo.publicIp = datagram.senderAddress().toString();    // 公网IP
    peerInfo.publicPort = datagram.senderPort();                // 公网端口
    peerInfo.targetIp = "0.0.0.0";
    peerInfo.targetPort = 0;
    peerInfo.lastSeen = QDateTime::currentMSecsSinceEpoch();
    peerInfo.status = status;

    // 存储对等端信息
    {
        QWriteLocker locker(&m_peersLock);
        m_peers[peerId] = peerInfo;
    }

    LOG_INFO(QString("📝 对等端注册: %1").arg(peerId));
    LOG_INFO(QString("  公网地址: %1:%2").arg(peerInfo.publicIp, peerInfo.publicPort));
    LOG_INFO(QString("  内网地址: %1:%2").arg(localIp, localPort));
    LOG_INFO(QString("  状态: %1").arg(status));

    // 发送注册确认
    QByteArray response = QString("REGISTER_ACK|%1|%2").arg(peerId, status).toUtf8();
    sendToAddress(datagram.senderAddress(), datagram.senderPort(), response);

    emit peerRegistered(peerId, gameId);
}

void P2PServer::sendHandshakeAck(const QNetworkDatagram &datagram, const QString &peerId)
{
    QByteArray response = QString("HANDSHAKE_ACK|%1").arg(peerId).toUtf8();
    qint64 bytesSent = sendToAddress(datagram.senderAddress(), datagram.senderPort(), response);

    if (bytesSent > 0) {
        LOG_INFO(QString("✅ 握手确认发送成功: %1 字节到 %2")
                     .arg(bytesSent).arg(peerId));
    } else {
        LOG_ERROR(QString("❌ 握手确认发送失败到: %1").arg(peerId));
    }
}

bool P2PServer::findAndConnectPeers(const QString &peerId, const QString &targetIp, const QString &targetPort)
{
    QWriteLocker locker(&m_peersLock);
    LOG_INFO(QString("🎯 开始查找匹配对等端: %1 -> %2:%3")
                 .arg(peerId, targetIp, targetPort));

    quint16 targetPortNum = targetPort.toUShort();
    PeerInfo matchedPeer;
    bool foundMatch = false;

    // 详细日志：显示当前所有对等端
    LOG_INFO("=== 当前服务器上的所有对等端 ===");
    if (m_peers.isEmpty()) {
        LOG_WARNING("📭 对等端列表为空！");
    } else {
        for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
            const PeerInfo &peer = it.value();
            LOG_INFO(QString("对等端: %1").arg(peer.id));
            LOG_INFO(QString("  公网地址: %2:%3").arg(peer.publicIp).arg(peer.publicPort));
            LOG_INFO(QString("  目标地址: %2:%3").arg(peer.targetIp).arg(peer.targetPort));
        }
    }
    LOG_INFO("=== 结束对等端列表 ===");

    // 详细匹配过程
    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        const PeerInfo &otherPeer = it.value();

        // 跳过自己
        if (otherPeer.id == peerId) {
            LOG_INFO(QString("⏭️  跳过自身: %1").arg(otherPeer.id));
            continue;
        }

        // 检查目标匹配
        bool ipMatch = (otherPeer.publicIp == targetIp);
        bool portMatch = (otherPeer.publicPort == targetPortNum);

        LOG_INFO(QString("🔍 检查对等端 %1:").arg(otherPeer.id));
        LOG_INFO(QString("  公网IP匹配: %1 == %2 -> %3").arg(otherPeer.publicIp, targetIp).arg(ipMatch));
        LOG_INFO(QString("  公网端口匹配: %1 == %2 -> %3").arg(otherPeer.publicPort).arg(targetPortNum).arg(portMatch));

        if (ipMatch && portMatch) {
            LOG_INFO(QString("✅ 找到匹配对等端: %1").arg(otherPeer.id));
            matchedPeer = otherPeer;
            foundMatch = true;
            break;
        } else {
            LOG_INFO(QString("❌ 不匹配"));
        }
    }
    if (foundMatch) {
        LOG_INFO(QString("🤝 建立匹配对: %1 <-> %2").arg(peerId, matchedPeer.id));

        // 双向通知
        notifyPeerAboutPeer(peerId, matchedPeer);
        notifyPeerAboutPeer(matchedPeer.id, m_peers[peerId]);

        emit peersMatched(peerId, matchedPeer.id, targetIp, targetPort);
        return true;
    } else {
        LOG_WARNING(QString("⏳ 没有找到匹配的对等端"));

        // 提供诊断建议
        if (m_peers.size() == 1) {
            LOG_WARNING("💡 诊断: 服务器上只有一个对等端，需要等待另一个对等端注册到服务器");
        } else {
            LOG_WARNING("💡 诊断: 目标对等端可能使用了不同的地址或端口");
        }
        return false;
    }
}

void P2PServer::notifyPeerAboutPeer(const QString &peerId, const PeerInfo &otherPeer)
{
    QWriteLocker locker(&m_peersLock);
    if (!m_peers.contains(peerId)) {
        LOG_ERROR(QString("❌ 对等端不存在: %1").arg(peerId));
        return;
    }

    const PeerInfo &targetPeer = m_peers[peerId];

    // 构造通知消息 - 包含完整的地址信息
    QString message = QString("PEER_INFO|%1|%2|%3|%4")
                          .arg(otherPeer.publicIp)      // 对等端公网IP
                          .arg(otherPeer.publicPort)    // 对等端公网端口
                          .arg(otherPeer.localIp)       // 对等端内网IP
                          .arg(otherPeer.localPort);    // 对等端内网端口

    // 处理IPv6格式地址
    QString cleanIp = targetPeer.publicIp;
    if (cleanIp.startsWith("::ffff:")) {
        cleanIp = cleanIp.mid(7);
    }

    QHostAddress peerAddress(cleanIp);
    if (peerAddress.isNull()) {
        LOG_ERROR(QString("❌ 无效的对等端地址: %1").arg(cleanIp));
        return;
    }

    qint64 bytesSent = sendToAddress(peerAddress, targetPeer.publicPort, message.toUtf8());

    if (bytesSent > 0) {
        LOG_INFO(QString("✅ 对等端信息发送成功: %1 -> %2 (%3 字节)")
                     .arg(otherPeer.id, peerId).arg(bytesSent));
        LOG_INFO(QString("  公网地址: %1:%2").arg(otherPeer.publicIp).arg(otherPeer.publicPort));
        LOG_INFO(QString("  内网地址: %1:%3").arg(otherPeer.localIp).arg(otherPeer.localPort));
    } else {
        LOG_ERROR(QString("❌ 对等端信息发送失败: %1 -> %2").arg(otherPeer.id, peerId));
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
    QWriteLocker locker(&m_peersLock);
    QStringList parts = QString(datagram.data()).split('|');
    if (parts.size() < 2) {
        LOG_WARNING("❌ 无效的打洞请求格式");
        return;
    }

    QString sourcePeerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());
    QString targetPeerId = parts[1];

    if (!m_peers.contains(sourcePeerId)) {
        LOG_WARNING(QString("❓ 未知的源对等端: %1").arg(sourcePeerId));
        return;
    }

    if (!m_peers.contains(targetPeerId)) {
        LOG_WARNING(QString("❓ 未知的目标对等端: %1").arg(targetPeerId));
        return;
    }

    PeerInfo &sourcePeer = m_peers[sourcePeerId];
    //PeerInfo &targetPeer = m_peers[targetPeerId];

    // 更新最后活跃时间
    sourcePeer.lastSeen = QDateTime::currentMSecsSinceEpoch();

    // 向目标对等端发送打洞请求通知
    QString punchNotify = QString("PUNCH_REQUEST|%1|%2|%3")
                              .arg(sourcePeer.publicIp,
                                   QString::number(sourcePeer.publicPort),
                                   sourcePeer.localIp);

    sendToPeer(targetPeerId, punchNotify.toUtf8());

    LOG_INFO(QString("🔄 打洞请求: %1 -> %2").arg(sourcePeerId, targetPeerId));
    emit punchRequested(sourcePeerId, targetPeerId);
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

void P2PServer::sendToPeer(const QString &peerId, const QByteArray &data)
{
    QWriteLocker locker(&m_peersLock);
    if (!m_peers.contains(peerId)) {
        LOG_ERROR(QString("❌ 对等端不存在: %1").arg(peerId));
        return;
    }

    const PeerInfo &peer = m_peers[peerId];

    // 处理IPv6格式地址
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
    if (!m_enableBroadcast || !m_udpSocket) {
        return;
    }

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
    return QString("%1:%2").arg(address.toString()).arg(port);
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

QList<QString> P2PServer::getConnectedPeers() const
{
    return m_peers.keys();
}

int P2PServer::getPeerCount() const
{
    return m_peers.size();
}

bool P2PServer::isRunning() const
{
    return m_isRunning;
}

quint16 P2PServer::getListenPort() const
{
    return m_listenPort;
}

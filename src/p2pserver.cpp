#include "p2pserver.h"
#include "logger.h"
#include <QTimer>
#include <QDateTime>
#include <QDataStream>
#include <QRandomGenerator>
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

    // ====================== 关键修改 ======================
    // 在函数开头就进行一次字符串转换，并用这个QString进行所有判断
    QString message = QString::fromUtf8(data).trimmed();
    // ====================================================

    // 解析消息类型 (现在使用 message 而不是 data)
    if (message.startsWith("HANDSHAKE|")) {
        LOG_INFO("🔗 处理 HANDSHAKE 消息");
        processHandshake(datagram);
    } else if (message.startsWith("REGISTER|")) {
        LOG_INFO("📝 处理 REGISTER 消息");
        processRegister(datagram);
    } else if (message.startsWith("GET_PEERS")) {
        LOG_INFO("📋 处理 GET_PEERS 请求");
        processGetPeers(datagram);
    } else if (message.startsWith("PUNCH")) {
        LOG_INFO("🔄 处理 PUNCH 消息");
        processPunchRequest(datagram);
    } else if (message.startsWith("KEEPALIVE")) {
        LOG_DEBUG("💓 处理 KEEPALIVE 消息");
        processKeepAlive(datagram);
    } else if (message.startsWith("PEER_INFO_ACK")) {
        LOG_INFO("✅ 处理 PEER_INFO_ACK 消息");
        processPeerInfoAck(datagram);
    } else if (message.startsWith("PING|")) {
        LOG_INFO("🏓 处理PING请求，验证客户端注册状态");
        processPingRequest(datagram);
    } else if (message.startsWith("TEST|")) {
        LOG_INFO("🧪 处理测试消息");
        processTestMessage(datagram);
    } else if (message.startsWith("NAT_TEST")) {
        LOG_INFO("🔍 处理NAT测试消息");
        processNATTest(datagram);
    } else if (message.startsWith("FORWARDED|")) {
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

    // 格式: REGISTER|GAME_ID|LOCAL_IP|LOCAL_PORT|STATUS
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

    // 修复：正确的字符串格式化方式
    LOG_INFO(QString("  公网地址: %1:%2")
                 .arg(peerInfo.publicIp)
                 .arg(peerInfo.publicPort));

    LOG_INFO(QString("  内网地址: %1:%2")
                 .arg(localIp, localPort));

    LOG_INFO(QString("  状态: %1").arg(status));

    // 发送注册确认
    QByteArray response = QString("REGISTER_ACK|%1|%2").arg(peerId, status).toUtf8();
    sendToAddress(datagram.senderAddress(), datagram.senderPort(), response);

    emit peerRegistered(peerId, gameId);
}

void P2PServer::processGetPeers(const QNetworkDatagram &datagram)
{

    QString dataStr = QString(datagram.data());
    QStringList parts = dataStr.split('|');

    // 格式: GET_PEERS 或 GET_PEERS|COUNT
    int count = -1; // 默认获取全部
    if (parts.size() > 1) {
        bool ok;
        int requestedCount = parts[1].toInt(&ok);
        if (ok) {
            count = requestedCount;
        }
    }

    QString requesterId = generatePeerId(datagram.senderAddress(), datagram.senderPort());
    QByteArray peerListResponse = getPeers(count, requesterId);
    sendToAddress(datagram.senderAddress(), datagram.senderPort(), peerListResponse);
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

    // 获取当前对等端信息
    if (!m_peers.contains(peerId)) {
        LOG_ERROR(QString("❌ 当前对等端不存在: %1").arg(peerId));
        return false;
    }

    PeerInfo &currentPeer = m_peers[peerId];
    QString currentGameId = currentPeer.gameId;

    LOG_INFO(QString("🔍 查找游戏 '%1' 的匹配对等端").arg(currentGameId));

    // 显示当前所有对等端
    LOG_INFO("=== 当前服务器上的所有对等端 ===");
    if (m_peers.isEmpty()) {
        LOG_WARNING("📭 对等端列表为空！");
    } else {
        for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
            const PeerInfo &peer = it.value();
            LOG_INFO(QString("对等端: %1, 游戏: %2, 状态: %3")
                         .arg(peer.id, peer.gameId, peer.status));
            LOG_INFO(QString("  公网地址: %1:%2").arg(peer.publicIp).arg(peer.publicPort));
            LOG_INFO(QString("  内网地址: %1:%2").arg(peer.localIp).arg(peer.localPort));
        }
    }
    LOG_INFO("=== 结束对等端列表 ===");

    PeerInfo matchedPeer;
    bool foundMatch = false;

    // 新的匹配逻辑：基于游戏标识符和状态
    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        const PeerInfo &otherPeer = it.value();

        // 跳过自己
        if (otherPeer.id == peerId) {
            LOG_INFO(QString("⏭️  跳过自身: %1").arg(otherPeer.id));
            continue;
        }

        // 匹配条件：
        // 1. 相同的游戏ID
        // 2. 对方状态为WAITING（等待连接）
        // 3. 不检查公网端口（因为对称NAT会导致端口不同）
        bool gameMatch = (otherPeer.gameId == currentGameId);
        bool statusMatch = (otherPeer.status == "WAITING");

        LOG_INFO(QString("🔍 检查对等端 %1:").arg(otherPeer.id));
        LOG_INFO(QString("  游戏匹配: %1 == %2 -> %3").arg(otherPeer.gameId, currentGameId).arg(gameMatch));
        LOG_INFO(QString("  状态匹配: %1 == WAITING -> %2").arg(otherPeer.status).arg(statusMatch));

        if (gameMatch && statusMatch) {
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

        // 更新双方状态
        currentPeer.status = "CONNECTING";
        m_peers[matchedPeer.id].status = "CONNECTING";

        // 双向通知 - 交换完整的地址信息
        notifyPeerAboutPeer(peerId, matchedPeer);
        notifyPeerAboutPeer(matchedPeer.id, currentPeer);

        emit peersMatched(peerId, matchedPeer.id, matchedPeer.publicIp, QString::number(matchedPeer.publicPort));
        return true;
    } else {
        LOG_WARNING(QString("⏳ 没有找到匹配的对等端，当前对等端保持等待状态"));

        // 更新当前对等端状态为等待
        currentPeer.status = "WAITING";

        // 提供诊断建议
        int waitingPeers = 0;
        for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
            if (it.value().gameId == currentGameId && it.value().status == "WAITING") {
                waitingPeers++;
            }
        }

        LOG_INFO(QString("💡 诊断: 游戏 '%1' 当前有 %2 个等待连接的对等端").arg(currentGameId).arg(waitingPeers));
        return false;
    }
}

void P2PServer::processPingRequest(const QNetworkDatagram &datagram)
{
    QString data = QString(datagram.data());
    QStringList parts = data.split('|');

    QString peerId = generatePeerId(datagram.senderAddress(), datagram.senderPort());

    if (parts.size() >= 3) {
        // 格式: PING|PUBLIC_IP|PUBLIC_PORT
        QString publicIp = parts[1];
        QString publicPort = parts[2];

        LOG_INFO(QString("🏓 PING来自 %1, 公网信息: %2:%3")
                     .arg(peerId, publicIp, publicPort));

        // 验证客户端是否已注册
        bool isRegistered = false;
        {
            QReadLocker locker(&m_peersLock);
            isRegistered = m_peers.contains(peerId);
        }

        QString status = isRegistered ? "REGISTERED" : "UNREGISTERED";
        QByteArray pongResponse = QString("PONG|%1|%2|%3")
                                      .arg(publicIp, publicPort, status)
                                      .toUtf8();

        qint64 bytesSent = sendToAddress(datagram.senderAddress(), datagram.senderPort(), pongResponse);

        if (bytesSent > 0) {
            LOG_INFO(QString("✅ PONG回复已发送 (状态: %1, %2 字节)").arg(status).arg(bytesSent));

            // 如果客户端未注册但提供了公网信息，可以尝试自动注册
            if (!isRegistered && publicIp != "0.0.0.0" && publicPort != "0") {
                LOG_INFO(QString("💡 检测到未注册客户端，建议客户端重新注册"));
            }
        } else {
            LOG_ERROR("❌ PONG回复发送失败");
        }
    } else {
        // 传统PING格式
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

    LOG_INFO(QString("🧪 处理测试消息: %1 来自 %2:%3")
                 .arg(message, senderAddress).arg(senderPort));

    // 添加详细的socket状态日志
    if (!m_udpSocket) {
        LOG_ERROR("❌ UDP Socket 未初始化！");
        return;
    }

    if (m_udpSocket->state() != QAbstractSocket::BoundState) {
        LOG_ERROR(QString("❌ UDP Socket 未绑定状态: %1").arg(m_udpSocket->state()));
        return;
    }

    LOG_INFO(QString("📡 服务器监听在: %1:%2")
                 .arg(m_udpSocket->localAddress().toString())
                 .arg(m_udpSocket->localPort()));

    bool isTestMessage = false;
    QString responseMessage;

    if (message.contains("TEST|CONNECTIVITY", Qt::CaseInsensitive)) {
        isTestMessage = true;
        responseMessage = "TEST|CONNECTIVITY|OK|War3Nat_Server_v3.0";
        LOG_INFO("✅ 识别为连接测试消息，准备响应");
    }

    if (isTestMessage) {
        QByteArray response = responseMessage.toUtf8();

        // 记录发送详情
        LOG_INFO(QString("📤 准备发送响应到: %1:%2 - 内容: %3")
                     .arg(senderAddress).arg(senderPort).arg(responseMessage));

        qint64 bytesSent = sendToAddress(datagram.senderAddress(), datagram.senderPort(), response);

        if (bytesSent > 0) {
            LOG_INFO(QString("✅ 测试响应发送成功: %1 字节").arg(bytesSent));
            m_totalResponses++;
        } else {
            QString errorStr = m_udpSocket ? m_udpSocket->errorString() : "Socket未初始化";
            LOG_ERROR(QString("❌ 测试响应发送失败: %1").arg(errorStr));
            LOG_ERROR(QString("🔧 Socket错误: %1, 状态: %2")
                          .arg(errorStr, m_udpSocket ? QString::number(m_udpSocket->state()) : "N/A"));
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

    // STUN Binding Response头部
    stream << quint16(0x0101);  // Binding Response
    stream << quint16(12);      // 消息长度
    stream << quint32(0x2112A442); // Magic Cookie

    // 生成事务ID（使用固定值便于测试）
    QByteArray transactionId(12, 0);
    QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(transactionId.data()), 3);
    stream.writeRawData(transactionId.constData(), 12);

    // XOR-MAPPED-ADDRESS属性
    stream << quint16(0x0020);  // XOR-MAPPED-ADDRESS
    stream << quint16(8);       // 属性长度

    quint16 xoredPort = datagram.senderPort() ^ (0x2112A442 >> 16);
    quint32 ipv4 = datagram.senderAddress().toIPv4Address();
    quint32 xoredIP = ipv4 ^ 0x2112A442;

    stream << quint8(0);        // 保留
    stream << quint8(0x01);     // IPv4家族
    stream << xoredPort;        // XORed端口
    stream << xoredIP;          // XORed IP地址

    LOG_DEBUG(QString("🔧 STUN测试响应 - 客户端: %1:%2 -> 映射: %3:%4")
                  .arg(datagram.senderAddress().toString()).arg(datagram.senderPort())
                  .arg(datagram.senderAddress().toString()).arg(datagram.senderPort()));

    return response;
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

void P2PServer::processNATTest(const QNetworkDatagram &datagram)
{
    QString data = QString(datagram.data());
    QStringList parts = data.split('|');

    QString senderAddress = datagram.senderAddress().toString();
    quint16 senderPort = datagram.senderPort();

    LOG_INFO(QString("🔍 NAT测试来自 %1:%2").arg(senderAddress).arg(senderPort));

    // 总是发送响应，包含测试ID（如果有的话）
    QByteArray response;
    if (parts.size() > 2 && parts[1] == "PORT_DETECTION") {
        // 响应端口检测测试，包含测试ID
        QString testId = parts[2];
        response = QString("NAT_TEST_RESPONSE|%1|%2|%3").arg(testId, senderAddress, QString::number(senderPort)).toUtf8();
    } else {
        // 普通响应
        response = QString("NAT_TEST_RESPONSE|%1|%2").arg(senderAddress, QString::number(senderPort)).toUtf8();
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

    // 解析转发信息
    QString originalClientIp = parts[1];
    QString originalClientPort = parts[2];
    QString timestamp = parts[3];
    QString originalMessage = parts.mid(4).join("|");

    LOG_INFO(QString("📨 转发消息 - 原始客户端: %1:%2, 时间: %3")
                 .arg(originalClientIp, originalClientPort, timestamp));
    LOG_INFO(QString("   原始消息: %1").arg(originalMessage));

    // 创建虚拟数据报，模拟原始客户端发送
    QHostAddress originalAddr(originalClientIp);
    quint16 originalPort = originalClientPort.toUShort();

    QByteArray originalData = originalMessage.toUtf8();

    // 使用虚拟数据报处理原始消息
    processOriginalMessage(originalData, originalAddr, originalPort);
}

void P2PServer::processOriginalMessage(const QByteArray &data, const QHostAddress &originalAddr, quint16 originalPort)
{
    QString message = QString(data).trimmed();

    LOG_INFO(QString("🔍 处理原始消息来自 %1:%2: %3")
                 .arg(originalAddr.toString()).arg(originalPort).arg(message));

    // 根据消息类型调用相应的处理函数
    if (message.startsWith("REGISTER_RELAY|")) {
        LOG_INFO("📝 处理转发的 REGISTER_RELAY 消息");
        processRegisterRelayFromForward(data, originalAddr, originalPort);
    } else {
        LOG_WARNING(QString("❓ 未知的转发消息类型: %1").arg(message));
    }
}

void P2PServer::processRegisterRelayFromForward(const QByteArray &data, const QHostAddress &originalAddr, quint16 originalPort)
{
    // 复用原有的 processRegisterRelay 逻辑，但使用原始地址
    QString message = QString(data);
    QStringList parts = message.split('|');

    if (parts.size() < 6) {
        LOG_WARNING(QString("❌ 无效的中继注册格式: %1").arg(message));
        return;
    }

    QString gameId = parts[1];
    QString relayIp = parts[2];
    QString relayPort = parts[3];
    QString natType = parts[4];
    QString status = parts.size() > 5 ? parts[5] : "RELAY_WAITING";

    QString peerId = generatePeerId(originalAddr, originalPort);

    // 创建对等端信息（中继模式）
    PeerInfo peerInfo;
    peerInfo.id = peerId;
    peerInfo.gameId = gameId;
    peerInfo.localIp = relayIp;                               // 中继IP
    peerInfo.localPort = relayPort.toUShort();                // 中继端口
    peerInfo.publicIp = originalAddr.toString();              // 客户端真实公网IP
    peerInfo.publicPort = originalPort;                       // 客户端真实公网端口
    peerInfo.relayIp = relayIp;                               // 中继服务器IP
    peerInfo.relayPort = relayPort.toUShort();                // 中继服务器端口
    peerInfo.natType = natType;                               // NAT类型
    peerInfo.targetIp = "0.0.0.0";
    peerInfo.targetPort = 0;
    peerInfo.lastSeen = QDateTime::currentMSecsSinceEpoch();
    peerInfo.status = status;
    peerInfo.isRelayMode = true;                              // 标记为中继模式

    // 存储对等端信息
    {
        QWriteLocker locker(&m_peersLock);
        m_peers[peerId] = peerInfo;
    }

    LOG_INFO(QString("🔄 转发中继模式对等端注册: %1").arg(peerId));
    LOG_INFO(QString("  真实公网地址: %1:%2").arg(peerInfo.publicIp).arg(peerInfo.publicPort));
    LOG_INFO(QString("  中继地址: %1:%2").arg(relayIp, relayPort));
    LOG_INFO(QString("  NAT类型: %1").arg(natType));
    LOG_INFO(QString("  状态: %1").arg(status));

    // 发送中继注册确认（发送到原始客户端地址）
    QByteArray response = QString("REGISTER_RELAY_ACK|%1|%2|%3|%4")
                              .arg(peerId, relayIp, relayPort, status)
                              .toUtf8();

    qint64 bytesSent = sendToAddress(originalAddr, originalPort, response);

    if (bytesSent > 0) {
        LOG_INFO(QString("✅ 中继注册确认发送成功: %1 字节").arg(bytesSent));
    } else {
        LOG_ERROR(QString("❌ 中继注册确认发送失败"));
    }

    emit peerRegistered(peerId, gameId);
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

QByteArray P2PServer::getPeers(int maxCount, const QString &excludePeerId)
{
    QReadLocker locker(&m_peersLock);

    QList<PeerInfo> peerList = m_peers.values();

    // 如果请求的数量小于0或大于总数，则获取全部
    int count = (maxCount < 0 || maxCount > peerList.size()) ? peerList.size() : maxCount;

    LOG_INFO(QString("🔍 正在准备对等端列表... 请求数量: %1, 排除ID: %2, 总对等端数: %3")
                 .arg(maxCount).arg(excludePeerId).arg(peerList.size()));

    QByteArray response = "PEER_LIST|";
    int peersAdded = 0;

    for (const PeerInfo &peer : qAsConst(peerList)) {
        // 如果已达到请求数量，则停止
        if (peersAdded >= count) {
            break;
        }

        // 跳过请求者自身
        if (peer.id == excludePeerId) {
            continue;
        }

        // 使用键值对格式序列化所有字段，分号分隔
        QString peerData = QString("id=%1;gid=%2;lip=%3;lport=%4;pip=%5;pport=%6;rip=%7;rport=%8;tip=%9;tport=%10;nat=%11;seen=%12;stat=%13;relay=%14")
                               .arg(peer.id, peer.gameId, peer.localIp)
                               .arg(peer.localPort)
                               .arg(peer.publicIp)
                               .arg(peer.publicPort)
                               .arg(peer.relayIp)
                               .arg(peer.relayPort)
                               .arg(peer.targetIp)
                               .arg(peer.targetPort)
                               .arg(peer.natType)
                               .arg(peer.lastSeen)
                               .arg(peer.status, peer.isRelayMode ? "1" : "0");

        response.append(peerData.toUtf8());
        response.append("|"); // 使用'|'作为不同peer之间的分隔符
        peersAdded++;
    }

    // 移除末尾多余的'|'
    if (response.endsWith('|')) {
        response.chop(1);
    }

    LOG_INFO(QString("✅ 对等端列表准备完成，包含 %1 个对等端。").arg(peersAdded));
    return response;
}

bool P2PServer::isRunning() const
{
    return m_isRunning;
}

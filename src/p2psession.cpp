#include "logger.h"
#include "p2psession.h"
#include "stunclientmanager.h"
#include <QThread>
#include <QNetworkDatagram>

P2PSession::P2PSession(const QString &sessionId, QObject *parent)
    : QObject(parent)
    , m_sessionId(sessionId)
    , m_udpSocket(nullptr)
    , m_w3gsProtocol(nullptr)
    , m_state(StateInit)
    , m_publicPort(0)
    , m_peerPublicPort(0)
    , m_holePunchTimer(nullptr)
    , m_keepAliveTimer(nullptr)
    , m_holePunchAttempts(0)
{
    m_udpSocket = new QUdpSocket(this);
    m_w3gsProtocol = new W3GSProtocol(this);
    m_holePunchTimer = new QTimer(this);
    m_keepAliveTimer = new QTimer(this);

    connect(m_udpSocket, &QUdpSocket::readyRead, this, &P2PSession::onSocketReadyRead);
    connect(m_holePunchTimer, &QTimer::timeout, this, &P2PSession::onHolePunchTimeout);
    connect(m_keepAliveTimer, &QTimer::timeout, this, &P2PSession::onKeepAliveTimeout);

    m_holePunchTimer->setSingleShot(true);
    m_keepAliveTimer->setInterval(30000); // 30秒保活

    // 连接共享的 STUN 客户端管理器
    connect(STUNClientManager::instance(), &STUNClientManager::publicAddressDiscovered,
            this, &P2PSession::onPublicAddressDiscovered);
    connect(STUNClientManager::instance(), &STUNClientManager::discoveryFailed,
            this, &P2PSession::onDiscoveryFailed);

    LOG_DEBUG(QString("Session %1: Created").arg(m_sessionId));
}

P2PSession::~P2PSession()
{
    stopSession();
    LOG_DEBUG(QString("Session %1: Destroyed").arg(m_sessionId));
}

void P2PSession::stopSession()
{
    if (m_holePunchTimer) {
        m_holePunchTimer->stop();
        m_holePunchTimer->deleteLater();
        m_holePunchTimer = nullptr;
    }

    if (m_keepAliveTimer) {
        m_keepAliveTimer->stop();
        m_keepAliveTimer->deleteLater();
        m_keepAliveTimer = nullptr;
    }

    if (m_udpSocket && m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
        m_udpSocket->close();
        m_udpSocket->deleteLater();
        m_udpSocket = nullptr;
    }

    // 取消 STUN 发现
    STUNClientManager::instance()->cancelDiscovery(m_sessionId);

    changeState(StateFailed);

    LOG_INFO(QString("Session %1: Stopped and resources released").arg(m_sessionId));
}

bool P2PSession::startSession()
{
    if (m_state != StateInit) {
        LOG_WARNING(QString("Session %1: Cannot start, already in state %2")
                        .arg(m_sessionId).arg(m_state));
        return false;
    }

    // 检查系统资源
    if (!checkSocketResources()) {
        LOG_ERROR(QString("Session %1: System socket resources unavailable").arg(m_sessionId));
        changeState(StateFailed);
        return false;
    }

    LOG_INFO(QString("Session %1: Starting P2P session").arg(m_sessionId));

    // 绑定UDP socket
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (m_udpSocket->bind(QHostAddress::Any, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            LOG_INFO(QString("Session %1: UDP socket bound to port %2")
                         .arg(m_sessionId).arg(m_udpSocket->localPort()));

            // 开始发现公网地址
            changeState(StateDiscovering);
            STUNClientManager::instance()->discoverPublicAddress(m_sessionId);
            return true;
        }

        LOG_WARNING(QString("Session %1: Bind attempt %2 failed: %3")
                        .arg(m_sessionId).arg(attempt + 1).arg(m_udpSocket->errorString()));
        QThread::msleep(100);
    }

    LOG_ERROR(QString("Session %1: Failed to bind UDP socket after 3 attempts: %2")
                  .arg(m_sessionId).arg(m_udpSocket->errorString()));
    changeState(StateFailed);
    return false;
}

bool P2PSession::checkSocketResources()
{
    // 简单的资源检查
    int socketCount = 0;

    // 这里可以添加更复杂的系统资源检查
    // 例如检查 /proc/sys/net/ipv4/ip_local_port_range 等

    if (socketCount > 1000) { // 假设的系统限制
        LOG_WARNING(QString("Session %1: System socket resources low").arg(m_sessionId));
        return false;
    }

    return true;
}

void P2PSession::setPeerAddress(const QHostAddress &address, quint16 port)
{
    if (address == m_peerPublicAddress && port == m_peerPublicPort) {
        return;
    }

    m_peerPublicAddress = address;
    m_peerPublicPort = port;

    LOG_INFO(QString("Session %1: Peer address set to %2:%3")
                 .arg(m_sessionId).arg(address.toString()).arg(port));

    // 如果已经获得公网地址，开始打洞
    if (m_state == StateExchanging && !m_publicAddress.isNull()) {
        startHolePunching();
    }
}

void P2PSession::sendW3GSData(const QByteArray &data)
{
    if (m_state != StateConnected) {
        LOG_WARNING(QString("Session %1: Cannot send data, not connected").arg(m_sessionId));
        return;
    }

    if (m_peerPublicAddress.isNull() || m_peerPublicPort == 0) {
        LOG_ERROR(QString("Session %1: No peer address to send data").arg(m_sessionId));
        return;
    }

    // 提取玩家信息（如果是REQJOIN包）
    extractPlayerInfoFromPacket(data);

    qint64 sent = m_udpSocket->writeDatagram(data, m_peerPublicAddress, m_peerPublicPort);
    if (sent == -1) {
        LOG_ERROR(QString("Session %1: Failed to send W3GS data: %2")
                      .arg(m_sessionId).arg(m_udpSocket->errorString()));
    } else if (sent != data.size()) {
        LOG_WARNING(QString("Session %1: Partial W3GS data sent: %2/%3 bytes")
                        .arg(m_sessionId).arg(sent).arg(data.size()));
    } else {
        LOG_DEBUG(QString("Session %1: Sent W3GS packet to peer, type: 0x%2, size: %3")
                      .arg(m_sessionId)
                      .arg(static_cast<uint8_t>(data[1]), 2, 16, QLatin1Char('0'))
                      .arg(sent));
    }
}

void P2PSession::onPublicAddressDiscovered(const QString &sessionId, const QHostAddress &address, quint16 port)
{
    if (sessionId != m_sessionId) return;

    m_publicAddress = address;
    m_publicPort = port;

    LOG_INFO(QString("Session %1: Public address ready: %2:%3")
                 .arg(m_sessionId).arg(address.toString()).arg(port));

    emit publicAddressReady(address, port);

    // 如果有对端地址，开始打洞
    if (!m_peerPublicAddress.isNull() && m_peerPublicPort != 0) {
        startHolePunching();
    } else {
        changeState(StateExchanging);
    }
}

void P2PSession::onDiscoveryFailed(const QString &sessionId, const QString &error)
{
    if (sessionId != m_sessionId) return;

    LOG_ERROR(QString("Session %1: STUN discovery failed: %2").arg(m_sessionId).arg(error));
    changeState(StateFailed);
    emit sessionFailed(m_sessionId, "STUN discovery failed: " + error);
}

void P2PSession::onSocketReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(m_udpSocket->pendingDatagramSize());

        QHostAddress senderAddr;
        quint16 senderPort;

        qint64 bytesRead = m_udpSocket->readDatagram(data.data(), data.size(), &senderAddr, &senderPort);

        if (bytesRead == -1) {
            LOG_ERROR(QString("Session %1: Failed to read datagram: %2")
                          .arg(m_sessionId).arg(m_udpSocket->errorString()));
            continue;
        }

        data.resize(bytesRead);

        // 检查是否来自对端
        if (senderAddr == m_peerPublicAddress && senderPort == m_peerPublicPort) {
            if (m_state == StateHolePunching) {
                LOG_INFO(QString("Session %1: Hole punching successful! Connection established")
                             .arg(m_sessionId));
                changeState(StateConnected);

                // 启动保活定时器
                m_keepAliveTimer->start();
            }

            LOG_DEBUG(QString("Session %1: Received %2 bytes from peer")
                          .arg(m_sessionId).arg(data.size()));

            // 处理W3GS数据包
            processW3GSPacket(data);

        } else {
            LOG_DEBUG(QString("Session %1: Received data from unknown source %2:%3")
                          .arg(m_sessionId).arg(senderAddr.toString()).arg(senderPort));
        }
    }
}

void P2PSession::processW3GSPacket(const QByteArray &data)
{
    // 验证W3GS数据包
    W3GSHeader header;
    if (!m_w3gsProtocol->parsePacket(data, header)) {
        LOG_WARNING(QString("Session %1: Invalid W3GS packet received").arg(m_sessionId));
        return;
    }

    LOG_DEBUG(QString("Session %1: Received valid W3GS packet - type: 0x%2, size: %3")
                  .arg(m_sessionId)
                  .arg(header.type, 4, 16, QLatin1Char('0'))
                  .arg(data.size()));

    // 提取玩家信息（如果是特定类型的包）
    extractPlayerInfoFromPacket(data);

    // 转发给上层处理
    emit w3gsDataReceived(data);
}

void P2PSession::extractPlayerInfoFromPacket(const QByteArray &data)
{
    W3GSHeader header;
    if (!m_w3gsProtocol->parsePacket(data, header)) {
        return;
    }

    // 如果是REQJOIN包，提取玩家信息
    if (header.type == W3GSProtocol::S_TO_C_REQ_JOIN) {
        PlayerInfo w3gsPlayer;  // 明确使用协议层的类型
        if (m_w3gsProtocol->parseSlotInfoJoin(data.mid(6), w3gsPlayer)) {
            // 转换为P2PSession的PlayerInfo
            PlayerInfo player;
            player.playerId = w3gsPlayer.playerId;
            player.name = w3gsPlayer.name;
            player.address = w3gsPlayer.address;
            player.port = w3gsPlayer.port;
            player.externalIP = w3gsPlayer.externalIP;
            player.externalPort = w3gsPlayer.externalPort;

            m_playerInfo = player;
            LOG_INFO(QString("Session %1: Extracted player info - ID: %2, Name: %3, Addr: %4:%5")
                         .arg(m_sessionId)
                         .arg(player.playerId)
                         .arg(player.name)
                         .arg(player.address.toString())
                         .arg(player.port));
            emit playerInfoUpdated(m_sessionId, player);
        }
    }
}

void P2PSession::onHolePunchTimeout()
{
    m_holePunchAttempts++;

    if (m_holePunchAttempts >= 10) {
        LOG_ERROR(QString("Session %1: Hole punching failed after %2 attempts")
                      .arg(m_sessionId).arg(m_holePunchAttempts));
        changeState(StateFailed);
        emit sessionFailed(m_sessionId, "Hole punching timeout");
        return;
    }

    LOG_DEBUG(QString("Session %1: Hole punch attempt %2")
                  .arg(m_sessionId).arg(m_holePunchAttempts));

    sendHolePunchPacket();
    m_holePunchTimer->start(1000); // 1秒后重试
}

void P2PSession::onKeepAliveTimeout()
{
    if (m_state == StateConnected) {
        sendKeepAlive();
    }
}

void P2PSession::changeState(SessionState newState)
{
    if (m_state == newState) return;

    LOG_DEBUG(QString("Session %1: State change %2 -> %3")
                  .arg(m_sessionId).arg(m_state).arg(newState));

    m_state = newState;

    if (newState == StateConnected) {
        emit sessionConnected(m_sessionId);
    }
}

void P2PSession::startHolePunching()
{
    if (m_state == StateHolePunching || m_state == StateConnected) {
        return;
    }

    LOG_INFO(QString("Session %1: Starting hole punching to %2:%3")
                 .arg(m_sessionId).arg(m_peerPublicAddress.toString()).arg(m_peerPublicPort));

    changeState(StateHolePunching);
    m_holePunchAttempts = 0;

    sendHolePunchPacket();
    m_holePunchTimer->start(1000);
}

void P2PSession::sendHolePunchPacket()
{
    if (m_peerPublicAddress.isNull() || m_peerPublicPort == 0) {
        return;
    }

    // 发送打洞包（使用W3GS PING包作为打洞包）
    QByteArray punchPacket = m_w3gsProtocol->createPingPacket();
    m_udpSocket->writeDatagram(punchPacket, m_peerPublicAddress, m_peerPublicPort);

    LOG_DEBUG(QString("Session %1: Sent hole punch packet (W3GS PING) to %2:%3")
                  .arg(m_sessionId).arg(m_peerPublicAddress.toString()).arg(m_peerPublicPort));
}

void P2PSession::sendKeepAlive()
{
    if (m_state != StateConnected) return;

    // 使用W3GS PING包作为保活包
    QByteArray keepAlivePacket = m_w3gsProtocol->createPingPacket();
    m_udpSocket->writeDatagram(keepAlivePacket, m_peerPublicAddress, m_peerPublicPort);

    LOG_DEBUG(QString("Session %1: Sent keep-alive packet").arg(m_sessionId));
}

#include "gamesession.h"
#include "logger.h"
#include <QNetworkDatagram>

GameSession::GameSession(const QString &sessionId, QObject *parent)
    : QObject(parent)
    , m_sessionId(sessionId)
    , m_socket(nullptr)
    , m_pingTimer(nullptr)
    , m_isRunning(false)
    , m_hostPort(0)
{
    m_socket = new QUdpSocket(this);
    m_pingTimer = new QTimer(this);

    connect(m_socket, &QUdpSocket::readyRead, this, &GameSession::onSocketReadyRead);
    connect(m_pingTimer, &QTimer::timeout, this, &GameSession::onPingTimeout);
}

GameSession::~GameSession()
{
    stopSession();
}

bool GameSession::startSession(const QHostAddress &hostAddr, quint16 hostPort) {
    m_hostAddress = hostAddr;
    m_hostPort = hostPort;

    if (m_socket->bind(QHostAddress::Any, 0, QUdpSocket::ShareAddress)) {
        m_isRunning = true;
        m_pingTimer->start(30000);
        LOG_DEBUG(QString("Session %1 started, bound to port %2").arg(m_sessionId).arg(m_socket->localPort()));
        return true;
    }

    LOG_ERROR(QString("Session %1 failed to bind UDP socket: %2").arg(m_sessionId).arg(m_socket->errorString()));
    return false;
}

void GameSession::stopSession()
{
    if (m_isRunning) {
        m_isRunning = false;
        m_pingTimer->stop();
        m_socket->close();

        m_clientAddresses.clear();
        m_players.clear();

        LOG_INFO(QString("Session %1 stopped").arg(m_sessionId));
        emit sessionEnded(m_sessionId);
    }
}

void GameSession::addPlayer(const PlayerInfo &player)
{
    m_players[player.playerId] = player;
    LOG_INFO(QString("Session %1: Player %2 (%3) added at %4:%5")
                 .arg(m_sessionId).arg(player.playerId).arg(player.name).arg(player.address.toString()).arg(player.port));
    emit playerJoined(player);
}

void GameSession::removePlayer(uint8_t playerId, uint32_t reason)
{
    if (m_players.contains(playerId)) {
        PlayerInfo player = m_players[playerId];
        m_players.remove(playerId);
        m_clientAddresses.remove(playerId);

        LOG_INFO(QString("Session %1: Player %2 (%3) removed, reason: %4")
                     .arg(m_sessionId).arg(playerId).arg(player.name).arg(reason));
        emit playerLeft(playerId, reason);

        if (m_players.isEmpty()) {
            LOG_INFO(QString("Session %1: No players left, stopping session").arg(m_sessionId));
            stopSession();
        }
    }
}

QString GameSession::getSessionInfo() const
{
    return QString("Session %1: Running=%2, Players=%3, Host=%4:%5")
    .arg(m_sessionId).arg(m_isRunning ? "Yes" : "No").arg(m_players.size())
        .arg(m_hostAddress.toString()).arg(m_hostPort);
}

QVector<PlayerInfo> GameSession::getPlayers() const
{
    QVector<PlayerInfo> players;
    for (const PlayerInfo &player : m_players) {
        players.append(player);
    }
    return players;
}

PlayerInfo GameSession::getPlayer(uint8_t playerId) const
{
    return m_players.value(playerId, PlayerInfo());
}

bool GameSession::hasPlayer(uint8_t playerId) const
{
    return m_players.contains(playerId);
}

void GameSession::forwardData(const QByteArray &data, const QHostAddress &from, quint16 port)
{
    if (!m_isRunning) {
        LOG_WARNING(QString("Session %1: Cannot forward data, session not running").arg(m_sessionId));
        return;
    }

    W3GSHeader header;
    if (!m_protocol.parsePacket(data, header)) {
        LOG_ERROR(QString("Session %1: Failed to parse packet header").arg(m_sessionId));
        return;
    }

    LOG_DEBUG(QString("Session %1 forwarding C->S packet type: 0x%2, size: %3 from %4:%5")
                  .arg(m_sessionId).arg(header.type, 4, 16, QLatin1Char('0')).arg(data.size())
                  .arg(from.toString()).arg(port));

    processClientToServerPacket(data, header, from, port);

    // 如果是第一个数据包，可以尝试从数据包中提取目标信息
    if (m_hostAddress.isNull() || m_hostPort == 0) {
        learnTargetFromFirstPacket(data, from, port);
    }

    qint64 bytesWritten = m_socket->writeDatagram(data, m_hostAddress, m_hostPort);
    if (bytesWritten == -1) {
        LOG_ERROR(QString("Session %1 failed to forward data to %2:%3: %4")
                      .arg(m_sessionId).arg(m_hostAddress.toString()).arg(m_hostPort).arg(m_socket->errorString()));
    } else {
        LOG_DEBUG(QString("Session %1 forwarded %2 bytes to %3:%4")
                      .arg(m_sessionId).arg(bytesWritten).arg(m_hostAddress.toString()).arg(m_hostPort));
    }
}

void GameSession::learnTargetFromFirstPacket(const QByteArray &data, const QHostAddress &from, quint16 port)
{
    Q_UNUSED(data)
    Q_UNUSED(from)
    Q_UNUSED(port)

    if (m_hostAddress.isNull()) {
        LOG_INFO(QString("Session %1: Learning target address from configuration").arg(m_sessionId));
    }
}

void GameSession::setTargetAddress(const QHostAddress &hostAddr, quint16 hostPort)
{
    if (m_hostAddress != hostAddr || m_hostPort != hostPort) {
        m_hostAddress = hostAddr;
        m_hostPort = hostPort;

        LOG_INFO(QString("Session %1: Target address updated to %2:%3")
                     .arg(m_sessionId).arg(hostAddr.toString()).arg(hostPort));

        if (m_isRunning) {
            emit targetAddressChanged(hostAddr, hostPort);
        }
    }
}

void GameSession::sendToClient(const QByteArray &data, const QHostAddress &clientAddr, quint16 clientPort)
{
    if (!m_isRunning) return;

    qint64 bytesWritten = m_socket->writeDatagram(data, clientAddr, clientPort);
    if (bytesWritten == -1) {
        LOG_ERROR(QString("Session %1: Failed to send data to client %2:%3: %4")
                      .arg(m_sessionId).arg(clientAddr.toString()).arg(clientPort).arg(m_socket->errorString()));
    } else {
        LOG_DEBUG(QString("Session %1: Sent %2 bytes to client %3:%4")
                      .arg(m_sessionId).arg(bytesWritten).arg(clientAddr.toString()).arg(clientPort));
    }
}

void GameSession::broadcastToClients(const QByteArray &data)
{
    sendToAllClients(data);
}

void GameSession::updateTargetAddress(const QHostAddress &newAddr, quint16 newPort)
{
    if (m_hostAddress != newAddr || m_hostPort != newPort) {
        m_hostAddress = newAddr;
        m_hostPort = newPort;

        LOG_INFO(QString("Session %1: Target address updated to %2:%3")
                     .arg(m_sessionId).arg(newAddr.toString()).arg(newPort));

        emit targetAddressChanged(newAddr, newPort);
    }
}

void GameSession::onSocketReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        if (datagram.isValid()) {
            W3GSHeader header;
            if (m_protocol.parsePacket(datagram.data(), header)) {
                processServerToClientPacket(datagram.data(), header);
            }
        }
    }
}

void GameSession::onPingTimeout()
{
    if (!m_isRunning) return;

    QByteArray pingPacket = m_protocol.createPingPacket();
    qint64 bytesWritten = m_socket->writeDatagram(pingPacket, m_hostAddress, m_hostPort);
    if (bytesWritten == -1) {
        LOG_ERROR(QString("Session %1: Failed to send ping packet").arg(m_sessionId));
    } else {
        LOG_DEBUG(QString("Session %1: Sent ping packet to game host").arg(m_sessionId));
    }
}

void GameSession::processClientToServerPacket(const QByteArray &data, const W3GSHeader &header,
                                              const QHostAddress &from, quint16 port)
{
    QByteArray payload = data.mid(6);

    switch (header.type) {
    case W3GSProtocol::C_TO_S_SLOT_INFOJOIN:
        handleSlotInfoJoin(payload, from, port);
        break;
    case W3GSProtocol::C_TO_S_CHAT_TO_HOST:
        handleChatToHost(payload);
        break;
    case W3GSProtocol::C_TO_S_LEAVE_GAME:
        handleLeaveGame(payload, from, port);
        break;
    case W3GSProtocol::C_TO_S_INCOMING_ACTION:
        handleIncomingAction(payload);
        break;
    case W3GSProtocol::C_TO_S_MAP_CHECK:
        handleMapCheck(payload);
        break;
    case W3GSProtocol::C_TO_S_PING_FROM_HOST:
        LOG_DEBUG(QString("Session %1: Received PING from client").arg(m_sessionId));
        break;
    default:
        LOG_DEBUG(QString("Session %1: Unhandled C->S packet type: 0x%2")
                      .arg(m_sessionId).arg(header.type, 4, 16, QLatin1Char('0')));
        break;
    }
}

void GameSession::processServerToClientPacket(const QByteArray &data, const W3GSHeader &header)
{
    QByteArray payload = data.mid(6);

    switch (header.type) {
    case W3GSProtocol::S_TO_C_PONG_TO_HOST:
        LOG_DEBUG(QString("Session %1: Received PONG from game host").arg(m_sessionId));
        break;
    case W3GSProtocol::S_TO_C_PLAYER_LEFT:
        handlePlayerLeft(payload);
        break;
    case W3GSProtocol::S_TO_C_CHAT_FROM_HOST:
        handleChatFromHost(payload);
        break;
    case W3GSProtocol::S_TO_C_SLOT_INFO:
        handleSlotInfo(payload);
        break;
    default:
        LOG_DEBUG(QString("Session %1: Unhandled S->C packet type: 0x%2")
                      .arg(m_sessionId).arg(header.type, 4, 16, QLatin1Char('0')));
        break;
    }

    sendToAllClients(data);
}

void GameSession::handleSlotInfoJoin(const QByteArray &payload, const QHostAddress &from, quint16 port)
{
    PlayerInfo player;
    if (m_protocol.parseSlotInfoJoin(payload, player)) {
        player.address = from;
        player.port = port;
        addPlayer(player);
        updateClientAddress(player.playerId, from, port);
    }
}

void GameSession::handleChatToHost(const QByteArray &payload)
{
    uint8_t fromPlayer, toPlayer;
    QString message;
    if (m_protocol.parseChatToHost(payload, fromPlayer, toPlayer, message)) {
        LOG_INFO(QString("Session %1: Chat from player %2 to %3: %4")
                     .arg(m_sessionId).arg(fromPlayer).arg(toPlayer).arg(message));
        emit chatMessage(fromPlayer, toPlayer, message);
    }
}

void GameSession::handleLeaveGame(const QByteArray &payload, const QHostAddress &from, quint16 port)
{
    Q_UNUSED(from)
    Q_UNUSED(port)

    if (payload.size() >= 1) {
        uint8_t playerId = static_cast<uint8_t>(payload[0]);
        removePlayer(playerId, 0x01);
    }
}

void GameSession::handleIncomingAction(const QByteArray &payload)
{
    LOG_DEBUG(QString("Session %1: Received game actions, size: %2")
                  .arg(m_sessionId).arg(payload.size()));
}

void GameSession::handleMapCheck(const QByteArray &payload)
{
    LOG_DEBUG(QString("Session %1: Received map check, size: %2")
                  .arg(m_sessionId).arg(payload.size()));
}

void GameSession::handlePlayerLeft(const QByteArray &payload)
{
    if (payload.size() >= 5) {
        uint8_t playerId = static_cast<uint8_t>(payload[0]);
        uint32_t reason = *reinterpret_cast<const uint32_t*>(payload.constData() + 1);
        removePlayer(playerId, reason);
    }
}

void GameSession::handleChatFromHost(const QByteArray &payload)
{
    uint8_t fromPlayer, toPlayer;
    QString message;
    if (m_protocol.parseChatToHost(payload, fromPlayer, toPlayer, message)) {
        LOG_INFO(QString("Session %1: Chat from host: player %2 to %3: %4")
                     .arg(m_sessionId).arg(fromPlayer).arg(toPlayer).arg(message));
        emit chatMessage(fromPlayer, toPlayer, message);
    }
}

void GameSession::handleSlotInfo(const QByteArray &payload)
{
    LOG_DEBUG(QString("Session %1: Received slot info update, size: %2")
                  .arg(m_sessionId).arg(payload.size()));
}

void GameSession::updateClientAddress(uint8_t playerId, const QHostAddress &addr, quint16 port)
{
    m_clientAddresses[playerId] = qMakePair(addr, port);
    LOG_DEBUG(QString("Session %1: Updated client address for player %2: %3:%4")
                  .arg(m_sessionId).arg(playerId).arg(addr.toString()).arg(port));
}

QPair<QHostAddress, quint16> GameSession::getClientAddress(uint8_t playerId) const
{
    return m_clientAddresses.value(playerId, qMakePair(QHostAddress(), 0));
}

void GameSession::sendToAllClients(const QByteArray &data, uint8_t excludePlayerId)
{
    for (auto it = m_clientAddresses.constBegin(); it != m_clientAddresses.constEnd(); ++it) {
        if (it.key() != excludePlayerId) {
            sendToClient(data, it.value().first, it.value().second);
        }
    }
}

void GameSession::processW3GSPacket(const QByteArray &data, const QHostAddress &from, quint16 port)
{
    Q_UNUSED(data)
    Q_UNUSED(from)
    Q_UNUSED(port)
}

void GameSession::sendToAll(const QByteArray &data, const QHostAddress &exclude)
{
    Q_UNUSED(exclude)
    sendToAllClients(data);
}

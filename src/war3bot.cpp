// war3bot.cpp
#include "war3bot.h"
#include "gamesession.h"
#include "logger.h"
#include <QNetworkDatagram>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDataStream>

// 跨平台字节序支持
#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

War3Bot::War3Bot(QObject *parent) : QObject(parent)
    , m_tcpServer(nullptr)
    , m_settings(nullptr)
{
    m_tcpServer = new QTcpServer(this);
    m_settings = new QSettings("war3bot.ini", QSettings::IniFormat, this);

    connect(m_tcpServer, &QTcpServer::newConnection, this, &War3Bot::onNewConnection);
}

War3Bot::~War3Bot()
{
    stopServer();
    delete m_settings;
}

bool War3Bot::startServer(quint16 port) {
    if (m_tcpServer->isListening()) {
        return true;
    }

    if (m_tcpServer->listen(QHostAddress::Any, port)) {
        LOG_INFO(QString("War3Bot TCP server started on port %1").arg(port));
        return true;
    }

    LOG_ERROR(QString("Failed to start War3Bot TCP server on port %1: %2")
                  .arg(port).arg(m_tcpServer->errorString()));
    return false;
}

void War3Bot::stopServer()
{
    if (m_tcpServer) {
        m_tcpServer->close();
    }

    // 清理所有客户端连接
    for (QTcpSocket *client : m_clientSessions.keys()) {
        client->disconnectFromHost();
        client->deleteLater();
    }
    m_clientSessions.clear();

    // 清理所有会话
    for (GameSession *session : m_sessions) {
        session->deleteLater();
    }
    m_sessions.clear();
}

void War3Bot::onNewConnection()
{
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *clientSocket = m_tcpServer->nextPendingConnection();
        QString sessionKey = generateSessionId();

        LOG_INFO(QString("New TCP connection from %1:%2, session: %3")
                     .arg(clientSocket->peerAddress().toString())
                     .arg(clientSocket->peerPort())
                     .arg(sessionKey));

        // 存储客户端连接
        m_clientSessions.insert(clientSocket, sessionKey);

        // 连接信号
        connect(clientSocket, &QTcpSocket::readyRead, this, &War3Bot::onClientDataReady);
        connect(clientSocket, &QTcpSocket::disconnected, this, &War3Bot::onClientDisconnected);

        // 创建游戏会话（但不立即连接）
        GameSession *session = new GameSession(sessionKey, this);
        connect(session, &GameSession::sessionEnded, this, &War3Bot::onSessionEnded);
        connect(session, &GameSession::dataToClient, this, [clientSocket](const QByteArray &data) {
            if (clientSocket->state() == QAbstractSocket::ConnectedState) {
                clientSocket->write(data);
            }
        });

        m_sessions.insert(sessionKey, session);

        // 设置客户端套接字
        session->setClientSocket(clientSocket);

        // 配置目标地址（但不立即连接）
        QString host = m_settings->value("game/host", "127.0.0.1").toString();
        quint16 gamePort = m_settings->value("game/port", 6112).toUInt();

        LOG_INFO(QString("Session %1: Target configured as %2:%3 (connection deferred until first W3GS packet)")
                     .arg(sessionKey).arg(host).arg(gamePort));

        // 只配置目标，不立即连接
        session->startSession(QHostAddress(host), gamePort);
    }
}

void War3Bot::onClientDataReady()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    QString sessionKey = m_clientSessions.value(clientSocket);
    GameSession *session = m_sessions.value(sessionKey);

    if (!session) {
        LOG_ERROR(QString("No session found for client: %1").arg(sessionKey));
        return;
    }

    QByteArray data = clientSocket->readAll();

    LOG_DEBUG(QString("Received %1 bytes from client %2, first bytes: %3 %4 %5 %6")
                  .arg(data.size()).arg(sessionKey)
                  .arg(static_cast<quint8>(data[0]), 2, 16, QLatin1Char('0'))
                  .arg(static_cast<quint8>(data[1]), 2, 16, QLatin1Char('0'))
                  .arg(static_cast<quint8>(data[2]), 2, 16, QLatin1Char('0'))
                  .arg(static_cast<quint8>(data[3]), 2, 16, QLatin1Char('0')));

    // 处理客户端数据包
    processClientPacket(clientSocket, data);
}

void War3Bot::processClientPacket(QTcpSocket *clientSocket, const QByteArray &data)
{
    QString sessionKey = m_clientSessions.value(clientSocket);
    GameSession *session = m_sessions.value(sessionKey);

    if (!session) {
        LOG_ERROR(QString("No session found for client: %1").arg(sessionKey));
        return;
    }

    // 首先检查是否是有效的W3GS数据包
    if (!isValidW3GSPacket(data)) {
        LOG_WARNING(QString("Session %1: Received non-W3GS data (%2 bytes)")
                        .arg(sessionKey)
                        .arg(data.size()));

        // 使用分析函数来识别数据包类型
        analyzeUnknownPacket(data, sessionKey);

        // 对于非W3GS数据，我们仍然尝试转发，但记录警告
        // 因为可能是其他合法的游戏协议数据
        if (session->isRunning()) {
            LOG_DEBUG(QString("Session %1: Forwarding non-W3GS data to game server anyway")
                          .arg(sessionKey));
            session->forwardToGame(data);
        } else {
            LOG_DEBUG(QString("Session %1: Game session not running, discarding non-W3GS data")
                          .arg(sessionKey));
        }
        return;
    }

    LOG_DEBUG(QString("Session %1: Received valid W3GS packet (%2 bytes)")
                  .arg(sessionKey).arg(data.size()));

    // 转发有效的W3GS数据到游戏服务器
    if (session->isRunning()) {
        session->forwardToGame(data);
    } else {
        LOG_WARNING(QString("Session %1: Game session not running, cannot forward W3GS data")
                        .arg(sessionKey));
    }
}

bool War3Bot::isValidW3GSPacket(const QByteArray &data)
{
    if (data.size() < 4) {
        return false;
    }

    // 检查W3GS协议标识 (0xF7)
    uint8_t protocol = static_cast<uint8_t>(data[0]);
    return (protocol == 0xF7);
}

void War3Bot::analyzeUnknownPacket(const QByteArray &data, const QString &sessionKey)
{
    LOG_DEBUG(QString("=== Session %1: Analyzing 41-byte unknown packet ===").arg(sessionKey));

    // 显示完整的41字节数据
    QByteArray hexData = data.toHex();
    LOG_DEBUG(QString("Full packet (hex): %1").arg(QString(hexData)));

    // 构建详细的字节分析
    QString byteAnalysis;
    for (int i = 0; i < data.size(); ++i) {
        byteAnalysis += QString("%1 ").arg(static_cast<uint8_t>(data[i]), 2, 16, QLatin1Char('0'));
        if ((i + 1) % 8 == 0) byteAnalysis += "  ";
    }
    LOG_DEBUG(QString("Bytes: %1").arg(byteAnalysis));

    // 检查第一个字节
    uint8_t firstByte = static_cast<uint8_t>(data[0]);
    LOG_DEBUG(QString("First byte: 0x%1").arg(firstByte, 2, 16, QLatin1Char('0')));

    // 检查是否是某种已知的游戏协议
    if (firstByte == 0xF7) {
        LOG_DEBUG("This IS a W3GS packet! But our validation is too strict.");
        uint8_t packetType = static_cast<uint8_t>(data[1]);
        LOG_DEBUG(QString("Packet type: 0x%1").arg(packetType, 2, 16, QLatin1Char('0')));
    }

    // 检查是否是其他常见协议
    if (data.size() >= 2) {
        uint8_t secondByte = static_cast<uint8_t>(data[1]);

        if (firstByte == 0x00 && secondByte == 0x00) {
            LOG_DEBUG("Possible: Battle.net protocol or padding");
        }
        else if (firstByte == 0xFF && secondByte == 0xFF) {
            LOG_DEBUG("Possible: Game query protocol (like Source engine)");
        }
        else if (firstByte == 0xFE && secondByte == 0xFD) {
            LOG_DEBUG("Possible: Minecraft query protocol");
        }
        else {
            LOG_DEBUG("Unknown protocol signature");
        }
    }

    // 尝试解析为W3GS包，即使第一个字节不是0xF7
    if (data.size() >= 4) {
        uint16_t possibleSize = (static_cast<uint8_t>(data[2]) << 8) | static_cast<uint8_t>(data[3]);
        LOG_DEBUG(QString("Possible size field (big-endian): %1").arg(possibleSize));

        uint16_t possibleSizeLE = (static_cast<uint8_t>(data[3]) << 8) | static_cast<uint8_t>(data[2]);
        LOG_DEBUG(QString("Possible size field (little-endian): %1").arg(possibleSizeLE));

        if (possibleSize == 41 || possibleSizeLE == 41) {
            LOG_DEBUG("Size field matches packet size (41 bytes)!");
        }
    }

    LOG_DEBUG("=== End of packet analysis ===");
}

void War3Bot::onClientDisconnected()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    QString sessionKey = m_clientSessions.value(clientSocket);

    LOG_INFO(QString("Client disconnected: %1, session: %2")
                 .arg(clientSocket->peerAddress().toString())
                 .arg(sessionKey));

    m_clientSessions.remove(clientSocket);
    clientSocket->deleteLater();

    // 清理对应的游戏会话
    if (m_sessions.contains(sessionKey)) {
        GameSession *session = m_sessions.take(sessionKey);
        session->deleteLater();
    }
}

void War3Bot::onSessionEnded(const QString &sessionId) {
    GameSession *session = m_sessions.take(sessionId);
    if (session) {
        session->deleteLater();
        LOG_INFO(QString("Game session ended: %1").arg(sessionId));
    }
}

QString War3Bot::generateSessionId() {
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    QByteArray hash = QCryptographicHash::hash(timestamp.toUtf8(), QCryptographicHash::Md5);
    return hash.toHex().left(8);
}

QString War3Bot::getServerStatus() const
{
    return QString("War3Bot TCP Server - Active sessions: %1, Clients: %2, Port: %3")
    .arg(m_sessions.size())
        .arg(m_clientSessions.size())
        .arg(m_tcpServer->serverPort());
}

QPair<QHostAddress, quint16> War3Bot::parseTargetFromPacket(const QByteArray &data, bool isFromClient)
{
    QHostAddress targetAddr;
    quint16 targetPort = 0;

    LOG_DEBUG(QString("=== Starting packet parsing ==="));
    LOG_DEBUG(QString("Packet size: %1 bytes, Direction: %2, First 8 bytes: %3")
                  .arg(data.size())
                  .arg(isFromClient ? "C->S" : "S->C")
                  .arg(QString(data.left(8).toHex())));
    LOG_DEBUG(QString("Packet hex: %1").arg(QString(data.toHex())));

    if (data.size() < 4) {
        LOG_WARNING(QString("Packet too small for header parsing: %1 bytes").arg(data.size()));
        return qMakePair(targetAddr, targetPort);
    }

    try {
        // 重要：War3协议头部使用大端序，但字段顺序需要调整！
        QDataStream stream(data);
        stream.setByteOrder(QDataStream::LittleEndian);

        // 解析 W3GS 头部 - 正确的顺序应该是：
        // 字节0: 协议ID (0xF7)
        // 字节1: 包类型 (0x1E)
        // 字节2-3: 包大小 (0x0029 = 41字节)
        uint8_t protocol;
        uint8_t packetType;      // 第二个字节是包类型
        uint16_t packetSize;     // 第三、四个字节是包大小

        stream >> protocol;
        stream >> packetType;
        stream >> packetSize;

        LOG_DEBUG(QString("Header parsed - Protocol: 0x%1, Type: 0x%2, Size: %3")
                      .arg(protocol, 2, 16, QLatin1Char('0'))
                      .arg(packetType, 2, 16, QLatin1Char('0'))
                      .arg(packetSize));

        // 验证包大小
        if (packetSize != static_cast<uint16_t>(data.size())) {
            LOG_WARNING(QString("Packet size mismatch: header says %1, actual is %2")
                            .arg(packetSize).arg(data.size()));
        }

        // 根据文档和方向检查包类型
        LOG_DEBUG(QString("Packet type identification: 0x%1, Direction: %2")
                      .arg(packetType, 2, 16, QLatin1Char('0'))
                      .arg(isFromClient ? "C->S" : "S->C"));

        // 处理所有包类型，考虑方向
        switch (packetType) {
        // Server to Client packets (S>C)
        case 0x01:
            if (!isFromClient) LOG_DEBUG("W3GS_PING_FROM_HOST - Host keepalive");
            else LOG_DEBUG("W3GS_PING_FROM_HOST (unexpected direction)");
            break;
        case 0x04:
            if (!isFromClient) LOG_DEBUG("W3GS_SLOTINFOJOIN - Game slots info on lobby entry");
            else LOG_DEBUG("W3GS_SLOTINFOJOIN (unexpected direction)");
            break;
        case 0x05:
            if (!isFromClient) LOG_DEBUG("W3GS_REJECTJOIN - Join request denied");
            else LOG_DEBUG("W3GS_REJECTJOIN (unexpected direction)");
            break;
        case 0x06:
            if (!isFromClient) LOG_DEBUG("W3GS_PLAYERINFO - Player information");
            else LOG_DEBUG("W3GS_PLAYERINFO (unexpected direction)");
            break;
        case 0x07:
            if (!isFromClient) LOG_DEBUG("W3GS_PLAYERLEFT - Player left game");
            else LOG_DEBUG("W3GS_PLAYERLEFT (unexpected direction)");
            break;
        case 0x08:
            if (!isFromClient) LOG_DEBUG("W3GS_PLAYERLOADED - Player finished loading");
            else LOG_DEBUG("W3GS_PLAYERLOADED (unexpected direction)");
            break;
        case 0x09:
            if (!isFromClient) LOG_DEBUG("W3GS_SLOTINFO - Slot updates");
            else LOG_DEBUG("W3GS_SLOTINFO (unexpected direction)");
            break;
        case 0x0A:
            if (!isFromClient) LOG_DEBUG("W3GS_COUNTDOWN_START - Game countdown started");
            else LOG_DEBUG("W3GS_COUNTDOWN_START (unexpected direction)");
            break;
        case 0x0B:
            if (!isFromClient) LOG_DEBUG("W3GS_COUNTDOWN_END - Game started");
            else LOG_DEBUG("W3GS_COUNTDOWN_END (unexpected direction)");
            break;
        case 0x0C:
            if (!isFromClient) LOG_DEBUG("W3GS_INCOMING_ACTION - In-game action");
            else LOG_DEBUG("W3GS_INCOMING_ACTION (unexpected direction)");
            break;
        case 0x0F:
            if (!isFromClient) LOG_DEBUG("W3GS_CHAT_FROM_HOST - Chat from host");
            else LOG_DEBUG("W3GS_CHAT_FROM_HOST (unexpected direction)");
            break;
        case 0x10:
            if (!isFromClient) LOG_DEBUG("W3GS_START_LAG - Players start lagging");
            else LOG_DEBUG("W3GS_START_LAG (unexpected direction)");
            break;
        case 0x11:
            if (!isFromClient) LOG_DEBUG("W3GS_STOP_LAG - Player returned from lag");
            else LOG_DEBUG("W3GS_STOP_LAG (unexpected direction)");
            break;
        case 0x1B:
            if (!isFromClient) LOG_DEBUG("W3GS_LEAVERS - Response to leave request");
            else LOG_DEBUG("W3GS_LEAVERS (unexpected direction)");
            break;
        case 0x1C:
            if (!isFromClient) LOG_DEBUG("W3GS_HOST_KICK_PLAYER - Host kick player");
            else LOG_DEBUG("W3GS_HOST_KICK_PLAYER (unexpected direction)");
            break;
        case 0x30:
            if (!isFromClient) LOG_DEBUG("W3GS_GAMEINFO - Game info broadcast");
            else LOG_DEBUG("W3GS_GAMEINFO (unexpected direction)");
            break;
        case 0x31:
            if (!isFromClient) LOG_DEBUG("W3GS_CREATEGAME - Game created notification");
            else LOG_DEBUG("W3GS_CREATEGAME (unexpected direction)");
            break;
        case 0x32:
            if (!isFromClient) LOG_DEBUG("W3GS_REFRESHGAME - Game refresh broadcast");
            else LOG_DEBUG("W3GS_REFRESHGAME (unexpected direction)");
            break;
        case 0x33:
            if (!isFromClient) LOG_DEBUG("W3GS_DECREATEGAME - Game no longer hosted");
            else LOG_DEBUG("W3GS_DECREATEGAME (unexpected direction)");
            break;
        case 0x3D:
            if (!isFromClient) LOG_DEBUG("W3GS_MAPCHECK - Map check request");
            else LOG_DEBUG("W3GS_MAPCHECK (unexpected direction)");
            break;
        case 0x43:
            if (!isFromClient) LOG_DEBUG("W3GS_MAPPART - Map part download");
            else LOG_DEBUG("W3GS_MAPPART (unexpected direction)");
            break;
        case 0x48:
            if (!isFromClient) LOG_DEBUG("W3GS_INCOMING_ACTION2 - In-game action");
            else LOG_DEBUG("W3GS_INCOMING_ACTION2 (unexpected direction)");
            break;

        // Client to Server packets (C>S)
        case 0x1E:
            if (isFromClient) {
                LOG_DEBUG("W3GS_REQJOIN - Client request to join game lobby (THIS IS WHAT WE NEED!)");
                // 只有 REQJOIN 包才包含目标地址信息
                // 切换到小端序解析载荷
                stream.setByteOrder(QDataStream::LittleEndian);
                return parseReqJoinPacket(stream, data.size() - stream.device()->pos());
            } else {
                LOG_DEBUG("W3GS_REQJOIN (unexpected direction)");
            }
            break;

        case 0x21:
            if (isFromClient) LOG_DEBUG("W3GS_LEAVEREQ - Client leave request");
            else LOG_DEBUG("W3GS_LEAVEREQ (unexpected direction)");
            break;
        case 0x23:
            if (isFromClient) LOG_DEBUG("W3GS_GAMELOADED_SELF - Client finished loading map");
            else LOG_DEBUG("W3GS_GAMELOADED_SELF (unexpected direction)");
            break;
        case 0x26:
            if (isFromClient) LOG_DEBUG("W3GS_OUTGOING_ACTION - Client game action");
            else LOG_DEBUG("W3GS_OUTGOING_ACTION (unexpected direction)");
            break;
        case 0x27:
            if (isFromClient) LOG_DEBUG("W3GS_OUTGOING_KEEPALIVE - Client keepalive");
            else LOG_DEBUG("W3GS_OUTGOING_KEEPALIVE (unexpected direction)");
            break;
        case 0x28:
            if (isFromClient) LOG_DEBUG("W3GS_CHAT_TO_HOST - Client chat to host");
            else LOG_DEBUG("W3GS_CHAT_TO_HOST (unexpected direction)");
            break;
        case 0x29:
            if (isFromClient) LOG_DEBUG("W3GS_DROPREQ - Drop request");
            else LOG_DEBUG("W3GS_DROPREQ (unexpected direction)");
            break;
        case 0x42:
            if (isFromClient) LOG_DEBUG("W3GS_MAPSIZE - Client map file info");
            else LOG_DEBUG("W3GS_MAPSIZE (unexpected direction)");
            break;
        case 0x44:
            if (isFromClient) LOG_DEBUG("W3GS_MAPPARTOK - Map part received OK");
            else LOG_DEBUG("W3GS_MAPPARTOK (unexpected direction)");
            break;
        case 0x45:
            if (isFromClient) LOG_DEBUG("W3GS_MAPPARTNOTOK - Map part not OK");
            else LOG_DEBUG("W3GS_MAPPARTNOTOK (unexpected direction)");
            break;
        case 0x46:
            if (isFromClient) LOG_DEBUG("W3GS_PONG_TO_HOST - Response to host echo");
            else LOG_DEBUG("W3GS_PONG_TO_HOST (unexpected direction)");
            break;

        // 双向包类型 (需要在两个方向都处理)
        case 0x2F:
            if (isFromClient) {
                LOG_DEBUG("W3GS_SEARCHGAME - Client game search");
            } else {
                LOG_DEBUG("W3GS_SEARCHGAME - Reply to game search");
            }
            break;

        case 0x3F:
            if (isFromClient) {
                LOG_DEBUG("W3GS_STARTDOWNLOAD - Client initiate map download");
            } else {
                LOG_DEBUG("W3GS_STARTDOWNLOAD - Start download state");
            }
            break;

        // P2P packets (可以在任何方向)
        case 0x35: LOG_DEBUG("W3GS_PING_FROM_OTHERS - Client echo request"); break;
        case 0x36: LOG_DEBUG("W3GS_PONG_TO_OTHERS - Client echo response"); break;
        case 0x37: LOG_DEBUG("W3GS_CLIENTINFO - Client info request"); break;

        default:
            LOG_DEBUG(QString("Unknown packet type: 0x%1").arg(packetType, 2, 16, QLatin1Char('0')));
            break;
        }

        // 如果不是 REQJOIN 包，返回空目标
        LOG_DEBUG("Packet is not REQJOIN (0x1E) from client, no target address to parse");
        return qMakePair(targetAddr, targetPort);

    } catch (const std::exception &e) {
        LOG_ERROR(QString("Exception while parsing packet: %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception while parsing packet");
    }

    return qMakePair(targetAddr, targetPort);
}

QPair<QHostAddress, quint16> War3Bot::parseReqJoinPacket(QDataStream &stream, int remainingSize)
{
    QHostAddress targetAddr;
    quint16 targetPort = 0;

    LOG_DEBUG("Starting W3GS_REQJOIN (0x1E) packet parsing...");
    LOG_DEBUG(QString("Remaining data size: %1 bytes").arg(remainingSize));

    // 添加最小数据大小检查
    if (remainingSize < 20) { // REQJOIN包的最小合理大小
        LOG_WARNING(QString("REQJOIN packet too small: %1 bytes").arg(remainingSize));
        return qMakePair(targetAddr, targetPort);
    }

    try {
        // 保存当前位置，以便在解析失败时恢复
        qint64 startPos = stream.device()->pos();

        // REQJOIN 载荷使用小端序
        stream.setByteOrder(QDataStream::LittleEndian);

        // 解析 REQJOIN 载荷
        uint32_t hostCounter;
        uint32_t entryKey;
        uint8_t unknown1;
        uint16_t listenPort;
        uint32_t peerKey;

        stream >> hostCounter;
        stream >> entryKey;
        stream >> unknown1;
        stream >> listenPort;
        stream >> peerKey;

        LOG_DEBUG(QString("REQJOIN basic fields - HostCounter: %1, EntryKey: %2, ListenPort: %3, PeerKey: %4")
                      .arg(hostCounter).arg(entryKey).arg(listenPort).arg(peerKey));

        // 读取玩家名字长度，添加更严格的检查
        uint8_t nameLength;
        stream >> nameLength;

        LOG_DEBUG(QString("Player name length: %1").arg(nameLength));

        // 更严格的长度检查
        if (nameLength == 0 || nameLength > 30) { // 正常玩家名字不会超过30字符
            LOG_WARNING(QString("Invalid player name length: %1, likely not a REQJOIN packet").arg(nameLength));
            // 重置流位置，避免影响后续解析
            stream.device()->seek(startPos);
            return qMakePair(targetAddr, targetPort);
        }

        // 检查是否有足够的数据读取玩家名字
        if (stream.device()->bytesAvailable() < nameLength) {
            LOG_WARNING(QString("Insufficient data for player name: need %1, have %2")
                            .arg(nameLength).arg(stream.device()->bytesAvailable()));
            stream.device()->seek(startPos);
            return qMakePair(targetAddr, targetPort);
        }

        QByteArray nameData;
        nameData.resize(nameLength);
        if (stream.readRawData(nameData.data(), nameLength) != nameLength) {
            LOG_ERROR("Failed to read player name");
            stream.device()->seek(startPos);
            return qMakePair(targetAddr, targetPort);
        }

        QString playerName = QString::fromUtf8(nameData);
        LOG_DEBUG(QString("Player name: %1").arg(playerName));

        // 检查剩余数据是否足够读取后续字段
        if (stream.device()->bytesAvailable() < 10) { // unknown2 + internalPort + internalIP
            LOG_WARNING("Insufficient data for internal IP/port fields");
            stream.device()->seek(startPos);
            return qMakePair(targetAddr, targetPort);
        }

        // 跳过未知字段
        uint32_t unknown2;
        stream >> unknown2;
        LOG_DEBUG(QString("Unknown2: 0x%1").arg(unknown2, 8, 16, QLatin1Char('0')));

        // 读取内部端口和IP（关键信息）
        uint16_t internalPort;
        uint32_t internalIP;

        stream >> internalPort;
        stream >> internalIP;

        LOG_DEBUG(QString("Internal IP: 0x%1 (%2), Internal Port: %3")
                      .arg(internalIP, 8, 16, QLatin1Char('0'))
                      .arg(internalIP)
                      .arg(internalPort));

        if (internalIP == 0) {
            LOG_WARNING("Internal IP is 0, cannot create valid target address");
            return qMakePair(targetAddr, targetPort);
        }

        // 字节序转换：War3 使用小端序，需要转换为网络字节序（大端序）
        uint32_t networkOrderIP = htonl(internalIP);
        targetAddr = QHostAddress(networkOrderIP);
        targetPort = internalPort;

        LOG_INFO(QString("Successfully parsed dynamic target from REQJOIN: %1:%2 (player: %3)")
                     .arg(targetAddr.toString()).arg(targetPort).arg(playerName));

    } catch (const std::exception &e) {
        LOG_ERROR(QString("Exception while parsing REQJOIN packet: %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception while parsing REQJOIN packet");
    }

    return qMakePair(targetAddr, targetPort);
}

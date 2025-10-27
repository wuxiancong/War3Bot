#include "war3bot.h"
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
    , m_udpSocket(nullptr)
    , m_settings(nullptr)
{
    m_udpSocket = new QUdpSocket(this);
    m_settings = new QSettings("war3bot.ini", QSettings::IniFormat, this);

    connect(m_udpSocket, &QUdpSocket::readyRead, this, &War3Bot::onReadyRead);
}

War3Bot::~War3Bot()
{
    stopServer();
    delete m_settings;
}

bool War3Bot::startServer(quint16 port) {
    if (m_udpSocket->state() == QAbstractSocket::BoundState) {
        return true;
    }

    if (m_udpSocket->bind(QHostAddress::Any, port, QUdpSocket::ShareAddress)) {
        LOG_INFO(QString("War3Bot started on port %1").arg(port));
        return true;
    }

    LOG_ERROR(QString("Failed to start War3Bot on port %1: %2").arg(port).arg(m_udpSocket->errorString()));
    return false;
}

void War3Bot::stopServer()
{
    if (m_udpSocket) {
        m_udpSocket->close();
    }

    for (GameSession *session : m_sessions) {
        session->deleteLater();
    }
    m_sessions.clear();
}

void War3Bot::onReadyRead() {
    while (m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        QByteArray data = datagram.data();
        QHostAddress senderAddr = datagram.senderAddress();
        quint16 senderPort = datagram.senderPort();

        LOG_DEBUG(QString("Received %1 bytes from %2:%3, first bytes: %4 %5 %6 %7")
                      .arg(data.size()).arg(senderAddr.toString()).arg(senderPort)
                      .arg(static_cast<quint8>(data[0]), 2, 16, QLatin1Char('0'))
                      .arg(static_cast<quint8>(data[1]), 2, 16, QLatin1Char('0'))
                      .arg(static_cast<quint8>(data[2]), 2, 16, QLatin1Char('0'))
                      .arg(static_cast<quint8>(data[3]), 2, 16, QLatin1Char('0')));

        if (data.size() >= 4) {
            processInitialPacket(data, senderAddr, senderPort);
        } else {
            LOG_WARNING(QString("Received too small packet (%1 bytes) from %2:%3")
                            .arg(data.size()).arg(senderAddr.toString()).arg(senderPort));
        }
    }
}

void War3Bot::processInitialPacket(const QByteArray &data, const QHostAddress &from, quint16 port)
{
    QString sessionKey = QString("%1:%2").arg(from.toString()).arg(port);

    GameSession *session = m_sessions.value(sessionKey, nullptr);
    if (!session) {
        session = new GameSession(sessionKey, this);
        connect(session, &GameSession::sessionEnded, this, &War3Bot::onSessionEnded);
        m_sessions.insert(sessionKey, session);

        // 读取配置选项
        bool dynamicTarget = m_settings->value("game/dynamic_target", true).toBool();
        bool fallbackToConfig = m_settings->value("game/fallback_to_config", true).toBool();

        // 添加配置信息日志
        LOG_INFO(QString("Session %1: Configuration - dynamic_target=%2, fallback_to_config=%3")
                     .arg(sessionKey).arg(dynamicTarget).arg(fallbackToConfig));

        QHostAddress realTarget;
        quint16 realPort = 0;

        // 根据配置决定是否使用动态目标
        if (dynamicTarget) {
            QPair<QHostAddress, quint16> targetInfo = parseTargetFromPacket(data);
            realTarget = targetInfo.first;
            realPort = targetInfo.second;

            // 添加详细的解析结果日志
            LOG_INFO(QString("Session %1: Dynamic target parsing result - target=%2:%3, data_size=%4, data_hex=%5")
                         .arg(sessionKey)
                         .arg(realTarget.toString())
                         .arg(realPort)
                         .arg(data.size())
                         .arg(QString(data.toHex())));

            if (!realTarget.isNull() && realPort != 0) {
                LOG_INFO(QString("Session %1: Using dynamic target %2:%3 from packet")
                             .arg(sessionKey).arg(realTarget.toString()).arg(realPort));
            } else if (fallbackToConfig) {
                // 动态解析失败，回退到配置
                QString host = m_settings->value("game/host", "127.0.0.1").toString();
                quint16 gamePort = m_settings->value("game/port", 6112).toUInt();
                realTarget = QHostAddress(host);
                realPort = gamePort;
                LOG_WARNING(QString("Session %1: Dynamic target failed, using configured target %2:%3")
                                .arg(sessionKey).arg(realTarget.toString()).arg(realPort));
            } else {
                LOG_ERROR(QString("Session %1: Dynamic target failed and fallback disabled")
                              .arg(sessionKey));
            }
        } else {
            // 不使用动态目标，直接使用配置
            QString host = m_settings->value("game/host", "127.0.0.1").toString();
            quint16 gamePort = m_settings->value("game/port", 6112).toUInt();
            realTarget = QHostAddress(host);
            realPort = gamePort;
            LOG_INFO(QString("Session %1: Using configured target %2:%3 (dynamic target disabled)")
                         .arg(sessionKey).arg(realTarget.toString()).arg(realPort));
        }

        // 检查是否成功获取目标地址
        if (realTarget.isNull() || realPort == 0) {
            LOG_ERROR(QString("Session %1: No valid target address found - target=%2, port=%3")
                          .arg(sessionKey).arg(realTarget.toString()).arg(realPort));
            m_sessions.remove(sessionKey);
            session->deleteLater();
            return;
        }

        // 添加最终目标确认日志
        LOG_INFO(QString("Session %1: Final target determined - %2:%3")
                     .arg(sessionKey).arg(realTarget.toString()).arg(realPort));

        if (session->startSession(realTarget, realPort)) {
            LOG_INFO(QString("Created new session: %1 -> %2:%3")
                         .arg(sessionKey).arg(realTarget.toString()).arg(realPort));
        } else {
            LOG_ERROR(QString("Failed to start session: %1 -> %2:%3")
                          .arg(sessionKey).arg(realTarget.toString()).arg(realPort));
        }

        if (!session->isRunning()) {
            LOG_ERROR(QString("Session not running after start attempt: %1").arg(sessionKey));
            m_sessions.remove(sessionKey);
            session->deleteLater();
            return;
        }
    } else {
        LOG_DEBUG(QString("Session %1 already exists, forwarding data").arg(sessionKey));
    }

    session->forwardData(data, from, port);
}

QPair<QHostAddress, quint16> War3Bot::parseTargetFromPacket(const QByteArray &data, bool isFromClient)
{
    QHostAddress targetAddr;
    quint16 targetPort = 0;

    LOG_DEBUG(QString("=== Starting packet parsing ==="));
    LOG_DEBUG(QString("Packet size: %1 bytes, Direction: %2")
                  .arg(data.size())
                  .arg(isFromClient ? "C->S" : "S->C"));
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

    try {
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

        // 读取玩家名字
        uint8_t nameLength;
        stream >> nameLength;

        LOG_DEBUG(QString("Player name length: %1").arg(nameLength));

        if (nameLength == 0 || nameLength > 50) {
            LOG_WARNING(QString("Invalid player name length: %1").arg(nameLength));
            return qMakePair(targetAddr, targetPort);
        }

        QByteArray nameData;
        nameData.resize(nameLength);
        if (stream.readRawData(nameData.data(), nameLength) != nameLength) {
            LOG_ERROR("Failed to read player name");
            return qMakePair(targetAddr, targetPort);
        }

        QString playerName = QString::fromUtf8(nameData);
        LOG_DEBUG(QString("Player name: %1").arg(playerName));

        // 跳过未知字段
        uint32_t unknown2;
        stream >> unknown2;
        LOG_DEBUG(QString("Unknown2: 0x%1").arg(unknown2, 8, 16, QLatin1Char('0')));

        // 检查剩余数据是否足够读取内部IP和端口
        if (stream.device()->bytesAvailable() < 6) {
            LOG_ERROR("Insufficient data for internal IP/port");
            return qMakePair(targetAddr, targetPort);
        }

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

QHostAddress War3Bot::extractRealTargetFromPacket(const QByteArray &data, const QHostAddress &from, quint16 port)
{
    Q_UNUSED(from)
    Q_UNUSED(port)
    return parseTargetFromPacket(data).first;
}

quint16 War3Bot::extractRealPortFromPacket(const QByteArray &data)
{
    return parseTargetFromPacket(data).second;
}

void War3Bot::onSessionEnded(const QString &sessionId) {
    GameSession *session = m_sessions.take(sessionId);
    if (session) {
        session->deleteLater();
        LOG_INFO(QString("Session ended: %1").arg(sessionId));
    }
}

QString War3Bot::generateSessionId() {
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    QByteArray hash = QCryptographicHash::hash(timestamp.toUtf8(), QCryptographicHash::Md5);
    return hash.toHex().left(8);
}

GameSession* War3Bot::findOrCreateSession(const QHostAddress &clientAddr, quint16 clientPort)
{
    QString sessionKey = QString("%1:%2").arg(clientAddr.toString()).arg(clientPort);
    GameSession *session = m_sessions.value(sessionKey, nullptr);

    if (!session) {
        session = new GameSession(sessionKey, this);
        connect(session, &GameSession::sessionEnded, this, &War3Bot::onSessionEnded);
        m_sessions.insert(sessionKey, session);

        // 使用动态地址解析
        QString host = m_settings->value("game/host", "127.0.0.1").toString();
        quint16 gamePort = m_settings->value("game/port", 6112).toUInt();

        if (!session->startSession(QHostAddress(host), gamePort)) {
            LOG_ERROR(QString("Failed to start session for %1").arg(sessionKey));
            m_sessions.remove(sessionKey);
            session->deleteLater();
            return nullptr;
        }
    }

    return session;
}

QString War3Bot::getServerStatus() const
{
    return QString("War3Bot Server - Active sessions: %1, Port: %2")
    .arg(m_sessions.size()).arg(m_udpSocket->localPort());
}

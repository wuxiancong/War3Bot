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

QPair<QHostAddress, quint16> War3Bot::parseTargetFromPacket(const QByteArray &data)
{
    QHostAddress targetAddr;
    quint16 targetPort = 0;

    // 添加更详细的数据包信息
    LOG_DEBUG(QString("=== Starting packet parsing ==="));
    LOG_DEBUG(QString("Packet size: %1 bytes").arg(data.size()));
    LOG_DEBUG(QString("Packet hex (first 64 bytes): %1").arg(QString(data.left(64).toHex())));

    // 检查数据包大小是否足够（至少包含头部和基本字段）
    if (data.size() < 20) { // 最小长度检查
        LOG_WARNING(QString("Packet too small for W3GS_REQJOIN parsing: %1 bytes, minimum 20 required").arg(data.size()));
        return qMakePair(targetAddr, targetPort);
    }

    try {
        QDataStream stream(data);
        stream.setByteOrder(QDataStream::LittleEndian);

        // 解析 W3GS 头部
        uint8_t protocol;
        uint16_t packetSize;
        uint16_t packetType;
        uint8_t unknownHeader;

        stream >> protocol;
        stream >> packetSize;
        stream >> packetType;
        stream >> unknownHeader;

        LOG_DEBUG(QString("Header parsed - Protocol: 0x%1, Type: 0x%2, Size: %3, Unknown: 0x%4")
                      .arg(protocol, 2, 16, QLatin1Char('0'))
                      .arg(packetType, 4, 16, QLatin1Char('0'))
                      .arg(packetSize)
                      .arg(unknownHeader, 2, 16, QLatin1Char('0')));

        // 验证包大小与实际数据大小是否一致
        if (packetSize != static_cast<uint16_t>(data.size())) {
            LOG_WARNING(QString("Packet size mismatch: header says %1, actual is %2")
                            .arg(packetSize).arg(data.size()));
        }

        // 检查是否是 W3GS_REQJOIN 数据包 (0x15)
        if (packetType != 0x15) {
            LOG_DEBUG(QString("Not a REQJOIN packet (type: 0x%1), skipping target parsing").arg(packetType, 4, 16, QLatin1Char('0')));
            return qMakePair(targetAddr, targetPort);
        }

        LOG_DEBUG("Confirmed REQJOIN packet (0x15), proceeding with parsing...");

        // 解析 W3GS_REQJOIN 载荷
        uint32_t hostCounter;    // Game ID
        uint32_t entryKey;       // Used in LAN
        uint8_t unknown1;
        uint16_t listenPort;     // 监听端口
        uint32_t peerKey;

        stream >> hostCounter;
        stream >> entryKey;
        stream >> unknown1;
        stream >> listenPort;
        stream >> peerKey;

        LOG_DEBUG(QString("REQJOIN basic fields - HostCounter: %1, EntryKey: %2, Unknown1: 0x%3, ListenPort: %4, PeerKey: %5")
                      .arg(hostCounter).arg(entryKey).arg(unknown1, 2, 16, QLatin1Char('0')).arg(listenPort).arg(peerKey));

        // 检查流状态
        if (stream.status() != QDataStream::Ok) {
            LOG_ERROR("Stream error after reading basic fields");
            return qMakePair(targetAddr, targetPort);
        }

        // 读取玩家名字（字符串）
        uint8_t nameLength;
        stream >> nameLength;

        LOG_DEBUG(QString("Player name length: %1").arg(nameLength));

        // 检查名字长度是否合理
        if (nameLength == 0 || nameLength > 100) {
            LOG_WARNING(QString("Invalid player name length: %1").arg(nameLength));
            return qMakePair(targetAddr, targetPort);
        }

        // 检查剩余数据是否足够
        if (stream.device()->pos() + nameLength > data.size()) {
            LOG_ERROR(QString("Insufficient data for player name: need %1 bytes, only %2 remaining")
                          .arg(nameLength).arg(data.size() - stream.device()->pos()));
            return qMakePair(targetAddr, targetPort);
        }

        QByteArray nameData;
        nameData.resize(nameLength);
        int bytesRead = stream.readRawData(nameData.data(), nameLength);
        if (bytesRead != nameLength) {
            LOG_ERROR(QString("Failed to read player name: expected %1 bytes, got %2").arg(nameLength).arg(bytesRead));
            return qMakePair(targetAddr, targetPort);
        }

        QString playerName = QString::fromUtf8(nameData);
        LOG_DEBUG(QString("Player name: '%1'").arg(playerName));

        // 跳过未知字段
        uint32_t unknown2;
        if (stream.atEnd()) {
            LOG_WARNING("Stream ended unexpectedly before reading internal IP/port");
            return qMakePair(targetAddr, targetPort);
        }
        stream >> unknown2;

        LOG_DEBUG(QString("Unknown2 field: 0x%1").arg(unknown2, 8, 16, QLatin1Char('0')));

        // 检查是否还有足够的数据读取内部IP和端口
        if (stream.device()->bytesAvailable() < 6) { // 2 bytes port + 4 bytes IP
            LOG_ERROR(QString("Insufficient data for internal IP/port: need 6 bytes, only %1 available")
                          .arg(stream.device()->bytesAvailable()));
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

        // 转换内部IP为QHostAddress
        if (internalIP == 0) {
            LOG_WARNING("Internal IP is 0, cannot create valid target address");
            return qMakePair(targetAddr, targetPort);
        }

// 字节序处理：War3协议通常使用小端序，但QHostAddress期望网络字节序（大端序）
// 使用标准的htonl函数或者手动转换
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        // 在小端序系统上，需要将小端序的IP转换为大端序
        uint32_t networkOrderIP = (internalIP << 24) |
                                  ((internalIP << 8) & 0x00FF0000) |
                                  ((internalIP >> 8) & 0x0000FF00) |
                                  (internalIP >> 24);
#else
        // 在大端序系统上，直接使用
        uint32_t networkOrderIP = internalIP;
#endif

        targetAddr = QHostAddress(networkOrderIP);
        targetPort = internalPort;

        LOG_INFO(QString("Successfully parsed target from packet:"));
        LOG_INFO(QString("  Player: %1").arg(playerName));
        LOG_INFO(QString("  Internal IP (raw): 0x%1 (%2)").arg(internalIP, 8, 16, QLatin1Char('0')).arg(internalIP));
        LOG_INFO(QString("  Target address: %1:%2").arg(targetAddr.toString()).arg(targetPort));
        LOG_INFO(QString("  Listen port: %1").arg(listenPort));

    } catch (const std::exception &e) {
        LOG_ERROR(QString("Exception while parsing packet: %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception while parsing packet");
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

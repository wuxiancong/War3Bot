// war3bot.cpp
#include "war3bot.h"
#include "gamesession.h"
#include "logger.h"
#include <QCryptographicHash>
#include <QNetworkDatagram>
#include <QDataStream>
#include <QDateTime>
#include <QDir>

// 跨平台字节序支持
#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

War3Bot::War3Bot(QObject *parent) : QObject(parent)
    , m_tcpServer(nullptr)
    , m_settings(nullptr)
    , m_sessions()
    , m_clientSessions()
{
    createDefaultConfig(); // 确保配置文件存在

    // 在构造函数中添加文件路径检查
    QString configPath = QDir::current().absoluteFilePath("war3bot.ini");
    LOG_INFO(QString("Config file path: %1").arg(configPath));

    QFile configFile(configPath);
    if (!configFile.exists()) {
        LOG_ERROR(QString("Config file does not exist: %1").arg(configPath));
    } else {
        LOG_INFO("Config file exists");
        if (!configFile.open(QIODevice::ReadOnly)) {
            LOG_ERROR(QString("Cannot open config file for reading: %1").arg(configFile.errorString()));
        } else {
            LOG_INFO("Config file is readable");
            configFile.close();
        }
    }

    m_tcpServer = new QTcpServer(this);
    m_settings = new QSettings("war3bot.ini", QSettings::IniFormat, this);

    // 验证配置读取
    bool dynamicTarget = m_settings->value("game/dynamic_target", false).toBool();
    bool fallbackToConfig = m_settings->value("game/fallback_to_config", true).toBool();
    QString host = m_settings->value("game/host", "127.0.0.1").toString();
    quint16 port = m_settings->value("game/port", 6112).toUInt();

    LOG_INFO(QString("Configuration loaded - dynamic_target: %1, fallback_to_config: %2, host: %3, port: %4")
                 .arg(dynamicTarget).arg(fallbackToConfig).arg(host).arg(port));

    connect(m_tcpServer, &QTcpServer::newConnection, this, &War3Bot::onNewConnection);
}

War3Bot::~War3Bot()
{
    stopServer();
    delete m_settings;
}

void War3Bot::createDefaultConfig()
{
    QFile configFile("war3bot.ini");
    if (!configFile.exists()) {
        if (configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&configFile);
            out << "[server]\n";
            out << "port=6113\n";
            out << "max_sessions=100\n";
            out << "ping_interval=30000\n\n";
            out << "[game]\n";
            out << "host=127.0.0.1\n";
            out << "port=6112\n";
            out << "dynamic_target=true\n";
            out << "fallback_to_config=true\n\n";
            out << "[logging]\n";
            out << "level=DEBUG\n";
            out << "file=/var/log/war3bot/war3bot.log\n";
            configFile.close();
            LOG_INFO("Created default configuration file: war3bot.ini");
        } else {
            LOG_ERROR("Failed to create default configuration file");
        }
    }
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

        LOG_INFO(QString("New TCP connection from %1:%2, session: %3, state: %4")
                     .arg(clientSocket->peerAddress().toString())
                     .arg(clientSocket->peerPort())
                     .arg(sessionKey)
                     .arg(clientSocket->state()));

        // 检查套接字状态
        if (clientSocket->state() != QAbstractSocket::ConnectedState) {
            LOG_WARNING(QString("Session %1: Client socket not in connected state").arg(sessionKey));
            clientSocket->deleteLater();
            continue;
        }

        // 存储客户端连接
        m_clientSessions.insert(clientSocket, sessionKey);

        // 连接信号
        connect(clientSocket, &QTcpSocket::readyRead, this, &War3Bot::onClientDataReady);
        connect(clientSocket, &QTcpSocket::disconnected, this, &War3Bot::onClientDisconnected);

        // 创建游戏会话
        GameSession *session = new GameSession(sessionKey, this);
        connect(session, &GameSession::sessionEnded, this, &War3Bot::onSessionEnded);
        connect(session, &GameSession::dataToClient, this, [clientSocket](const QByteArray &data) {
            if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState) {
                clientSocket->write(data);
            }
        });

        m_sessions.insert(sessionKey, session);
        session->setClientSocket(clientSocket);

        // 配置目标地址
        QString host = m_settings->value("game/host", "127.0.0.1").toString();
        quint16 gamePort = m_settings->value("game/port", 6112).toUInt();

        LOG_INFO(QString("Session %1: Target configured as %2:%3")
                     .arg(sessionKey).arg(host).arg(gamePort));

        // 只配置目标，不立即连接
        session->startSession(QHostAddress(host), gamePort);
    }
}

void War3Bot::onClientDataReady()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) {
        LOG_ERROR("onClientDataReady: Invalid sender");
        return;
    }

    QString sessionKey = m_clientSessions.value(clientSocket);
    if (sessionKey.isEmpty()) {
        LOG_ERROR("onClientDataReady: No session found for client socket");
        return;
    }

    GameSession *session = m_sessions.value(sessionKey);
    if (!session) {
        LOG_ERROR(QString("onClientDataReady: No session found for client: %1").arg(sessionKey));
        return;
    }

    // 读取所有可用数据
    QByteArray data = clientSocket->readAll();

    if (data.isEmpty()) {
        LOG_WARNING(QString("Session %1: Received empty data").arg(sessionKey));
        return;
    }

    LOG_DEBUG(QString("Session %1: Received %2 bytes from client")
                  .arg(sessionKey).arg(data.size()));

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

    LOG_INFO(QString("Session %1: Processing %2 bytes from client")
                 .arg(sessionKey).arg(data.size()));

    // 分析数据包
    analyzeUnknownPacket(data, sessionKey);

    // 解析包装的数据
    QPair<QHostAddress, quint16> targetInfo = parseWrappedPacket(data);

    // 每次都重新读取配置，确保获取最新值
    m_settings->sync(); // 强制重新读取配置文件
    bool dynamicTarget = m_settings->value("game/dynamic_target", false).toBool();
    bool fallbackToConfig = m_settings->value("game/fallback_to_config", true).toBool();

    LOG_INFO(QString("Session %1: Current config - dynamic_target=%2, fallback_to_config=%3")
                 .arg(sessionKey).arg(dynamicTarget).arg(fallbackToConfig));

    QHostAddress finalTarget;
    quint16 finalPort = 0;

    // 首先检查是否成功解析了包装数据
    if (!targetInfo.first.isNull() && targetInfo.second != 0) {
        LOG_INFO(QString("Session %1: Successfully parsed target from wrapped packet: %2:%3")
                     .arg(sessionKey).arg(targetInfo.first.toString()).arg(targetInfo.second));

        if (dynamicTarget) {
            // 使用动态解析的目标
            finalTarget = targetInfo.first;
            finalPort = targetInfo.second;
            LOG_INFO(QString("Session %1: Using dynamic target %2:%3")
                         .arg(sessionKey).arg(finalTarget.toString()).arg(finalPort));
        } else {
            LOG_WARNING(QString("Session %1: Dynamic target disabled in config, ignoring parsed target %2:%3")
                            .arg(sessionKey).arg(targetInfo.first.toString()).arg(targetInfo.second));
        }
    }

    // 如果没有动态目标或动态目标解析失败，使用配置目标
    if (finalTarget.isNull() || finalPort == 0) {
        if (fallbackToConfig) {
            // 使用配置的静态目标
            QString host = m_settings->value("game/host", "127.0.0.1").toString();
            quint16 gamePort = m_settings->value("game/port", 6112).toUInt();
            finalTarget = QHostAddress(host);
            finalPort = gamePort;
            LOG_INFO(QString("Session %1: Using configured target %2:%3")
                         .arg(sessionKey).arg(finalTarget.toString()).arg(finalPort));
        } else {
            LOG_ERROR(QString("Session %1: No valid target address found").arg(sessionKey));
            return;
        }
    }

    // 提取原始数据
    QByteArray originalData = extractOriginalData(data);
    if (originalData.isEmpty()) {
        LOG_ERROR(QString("Session %1: Failed to extract original data").arg(sessionKey));
        return;
    }

    // 更新目标并转发
    session->updateTarget(finalTarget, finalPort);

    if (session->forwardToGame(originalData)) {
        LOG_INFO(QString("Session %1: Successfully forwarded %2 bytes to game host %3:%4")
                     .arg(sessionKey).arg(originalData.size())
                     .arg(finalTarget.toString()).arg(finalPort));
    } else {
        LOG_ERROR(QString("Session %1: Failed to forward data to game host %2:%3")
                      .arg(sessionKey).arg(finalTarget.toString()).arg(finalPort));
    }
}

QPair<QHostAddress, quint16> War3Bot::parseWrappedPacket(const QByteArray &data)
{
    QHostAddress targetAddr;
    quint16 targetPort = 0;

    LOG_DEBUG(QString("Attempting to parse wrapped packet, size: %1 bytes").arg(data.size()));

    if (data.size() < 14) { // 魔数4 + IP4 + 端口2 + 长度4 = 14字节
        LOG_DEBUG(QString("Packet too small for wrapped format: %1 bytes, need at least 14").arg(data.size()));
        return qMakePair(targetAddr, targetPort);
    }

    // 解析包装头
    const char *ptr = data.constData();
    uint32_t magic = *reinterpret_cast<const uint32_t*>(ptr);

    LOG_DEBUG(QString("Magic number: 0x%1").arg(magic, 8, 16, QLatin1Char('0')));

    if (magic != 0x57415233) { // "WAR3"
        LOG_DEBUG("Not a wrapped packet, missing magic number 'WAR3'");
        return qMakePair(targetAddr, targetPort);
    }

    ptr += 4;
    uint32_t targetIP = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += 4;
    targetPort = *reinterpret_cast<const uint16_t*>(ptr);
    ptr += 2;
    uint32_t dataLength = *reinterpret_cast<const uint32_t*>(ptr);

    LOG_DEBUG(QString("Parsed header - targetIP: 0x%1, targetPort: %2, dataLength: %3")
                  .arg(targetIP, 8, 16, QLatin1Char('0'))
                  .arg(targetPort).arg(dataLength));

    // 修复有符号/无符号比较警告
    if (static_cast<uint32_t>(data.size()) != 14 + dataLength) {
        LOG_WARNING(QString("Wrapped packet size mismatch: expected %1, got %2")
                        .arg(14 + dataLength).arg(data.size()));
        return qMakePair(targetAddr, targetPort);
    }

    // 转换IP地址
    targetAddr = QHostAddress(ntohl(targetIP));
    targetPort = ntohs(targetPort);

    LOG_INFO(QString("Successfully parsed wrapped packet: target=%1:%2, data_length=%3")
                 .arg(targetAddr.toString()).arg(targetPort).arg(dataLength));

    return qMakePair(targetAddr, targetPort);
}

QByteArray War3Bot::extractOriginalData(const QByteArray &wrappedData)
{
    if (wrappedData.size() < 14) {
        return QByteArray();
    }

    // 跳过包装头 (4魔数 + 4IP + 2端口 + 4长度 = 14字节)
    const char *originalData = wrappedData.constData() + 14;
    uint32_t dataLength = *reinterpret_cast<const uint32_t*>(wrappedData.constData() + 10);

    // 修复有符号/无符号比较警告
    if (dataLength > static_cast<uint32_t>(wrappedData.size() - 14)) {
        return QByteArray();
    }

    return QByteArray(originalData, dataLength);
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
    LOG_DEBUG(QString("=== Session %1: Analyzing %2-byte packet ===")
                  .arg(sessionKey).arg(data.size()));

    // 显示完整的十六进制数据
    QByteArray hexData = data.toHex();
    LOG_DEBUG(QString("Full packet (hex): %1").arg(QString(hexData)));

    // 构建详细的字节分析
    QString byteAnalysis;
    QString asciiAnalysis;
    for (int i = 0; i < data.size(); ++i) {
        uint8_t byte = static_cast<uint8_t>(data[i]);
        byteAnalysis += QString("%1 ").arg(byte, 2, 16, QLatin1Char('0'));

        // ASCII 显示（如果是可打印字符）
        if (byte >= 32 && byte <= 126) {
            asciiAnalysis += QChar(byte);
        } else {
            asciiAnalysis += ".";
        }

        if ((i + 1) % 8 == 0) {
            byteAnalysis += "  ";
            asciiAnalysis += " ";
        }
    }

    LOG_DEBUG(QString("Bytes (hex): %1").arg(byteAnalysis));
    LOG_DEBUG(QString("Bytes (ascii): %1").arg(asciiAnalysis));

    // 分析协议类型
    if (data.size() >= 2) {
        uint8_t firstByte = static_cast<uint8_t>(data[0]);
        uint8_t secondByte = static_cast<uint8_t>(data[1]);

        LOG_DEBUG(QString("First two bytes: 0x%1 0x%2")
                      .arg(firstByte, 2, 16, QLatin1Char('0'))
                      .arg(secondByte, 2, 16, QLatin1Char('0')));

        if (firstByte == 0xF7) {
            LOG_DEBUG("Protocol: W3GS (WarCraft III)");
            LOG_DEBUG(QString("Packet type: 0x%1").arg(secondByte, 2, 16, QLatin1Char('0')));
        } else if (firstByte == 0x00 && secondByte == 0x00) {
            LOG_DEBUG("Protocol: Possible Battle.net or custom binary protocol");
        } else if (firstByte == 0xFF && secondByte == 0xFF) {
            LOG_DEBUG("Protocol: Possible game query protocol");
        } else {
            LOG_DEBUG("Protocol: Unknown binary protocol");
        }
    }

    // 检查数据包大小字段
    if (data.size() >= 4) {
        uint16_t sizeBE = (static_cast<uint8_t>(data[2]) << 8) | static_cast<uint8_t>(data[3]);
        uint16_t sizeLE = (static_cast<uint8_t>(data[3]) << 8) | static_cast<uint8_t>(data[2]);

        LOG_DEBUG(QString("Size field (big-endian): %1").arg(sizeBE));
        LOG_DEBUG(QString("Size field (little-endian): %1").arg(sizeLE));

        if (sizeBE == data.size() || sizeLE == data.size()) {
            LOG_DEBUG("Size field matches actual packet size!");
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
        // 修复有符号/无符号比较
        if (static_cast<uint32_t>(stream.device()->bytesAvailable()) < nameLength) {
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

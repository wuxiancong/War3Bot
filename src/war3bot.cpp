#include "war3bot.h"
#include "logger.h"
#include <QCryptographicHash>
#include <QDateTime>

War3Bot::War3Bot(QObject *parent)
    : QObject(parent)
    , m_tcpServer(nullptr)
    , m_settings(nullptr)
    , m_resourceMonitor(nullptr)
    , m_cleanupTimer(nullptr)
{
    m_tcpServer = new QTcpServer(this);
    connect(m_tcpServer, &QTcpServer::newConnection, this, &War3Bot::onNewConnection);

    // 资源监控定时器
    m_resourceMonitor = new QTimer(this);
    connect(m_resourceMonitor, &QTimer::timeout, this, &War3Bot::monitorResources);
    m_resourceMonitor->start(30000); // 每30秒检查一次

    // 清理定时器
    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &War3Bot::cleanupInactiveSessions);
    m_cleanupTimer->start(60000); // 每60秒清理一次
}

War3Bot::~War3Bot()
{
    stopServer();
}

bool War3Bot::loadConfig(const QString &configPath)
{
    // 检查配置文件是否存在，如果不存在则创建默认配置
    QFile configFile(configPath);
    if (!configFile.exists()) {
        LOG_WARNING(QString("Config file %1 does not exist, creating default config").arg(configPath));
        createDefaultConfig(configPath);
    }

    // 加载配置
    m_settings = new QSettings(configPath, QSettings::IniFormat, this);

    // 验证配置读取
    if (m_settings->status() != QSettings::NoError) {
        LOG_ERROR(QString("Failed to load config file: %1").arg(configPath));
        return false;
    }

    // 记录加载的配置
    QString host = m_settings->value("game/host", "127.0.0.1").toString();
    quint16 gamePort = m_settings->value("game/port", 6112).toUInt();

    LOG_INFO(QString("Configuration loaded:"));
    LOG_INFO(QString("  Game Host: %1:%2").arg(host).arg(gamePort));

    return true;
}

bool War3Bot::startServer(quint16 port)
{
    if (m_tcpServer->isListening()) {
        LOG_WARNING("Server is already running");
        return true;
    }

    // 加载配置文件
    if (!loadConfig()) {
        LOG_ERROR("Failed to load configuration");
        return false;
    }

    // 使用配置中的端口（如果命令行没有指定）
    quint16 configPort = m_settings->value("server/port", port).toUInt();
    if (port == 6113) { // 如果使用的是默认端口，则使用配置中的端口
        port = configPort;
    }

    if (!m_tcpServer->listen(QHostAddress::Any, port)) {
        LOG_ERROR(QString("Failed to start War3Bot server on port %1: %2")
                      .arg(port).arg(m_tcpServer->errorString()));
        return false;
    }

    LOG_INFO(QString("War3Bot P2P server started on port %1").arg(port));

    // 记录服务器配置
    int maxSessions = m_settings->value("server/max_sessions", 50).toInt();
    int pingInterval = m_settings->value("server/ping_interval", 30000).toInt();

    LOG_INFO(QString("Server settings - Max sessions: %1, Ping interval: %2ms")
                 .arg(maxSessions).arg(pingInterval));

    return true;
}

void War3Bot::stopServer()
{
    if (m_tcpServer) {
        m_tcpServer->close();
        LOG_INFO("TCP server stopped");
    }

    if (m_resourceMonitor) {
        m_resourceMonitor->stop();
    }

    if (m_cleanupTimer) {
        m_cleanupTimer->stop();
    }

    // 清理所有客户端连接
    for (QTcpSocket *client : m_clientSessions.keys()) {
        client->disconnectFromHost();
        client->deleteLater();
    }
    m_clientSessions.clear();

    // 清理所有P2P会话
    for (P2PSession *session : m_sessions) {
        session->deleteLater();
    }
    m_sessions.clear();

    LOG_INFO("All sessions cleaned up");
}

void War3Bot::createDefaultConfig(const QString &configPath)
{
    QFile configFile(configPath);
    if (configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&configFile);
        out << "[server]\n";
        out << "port=6113\n";
        out << "max_sessions=50\n";
        out << "ping_interval=30000\n\n";

        out << "[game]\n";
        out << "host=127.0.0.1\n";
        out << "port=6112\n";

        out << "[logging]\n";
        out << "level=INFO\n";
        out << "file=/var/log/war3bot/war3bot.log\n";

        configFile.close();
        LOG_INFO(QString("Created default configuration file: %1").arg(configPath));
    } else {
        LOG_ERROR(QString("Failed to create default configuration file: %1").arg(configPath));
    }
}

void War3Bot::onNewConnection()
{
    // 检查连接数限制
    int maxSessions = m_settings ? m_settings->value("server/max_sessions", 50).toInt() : 50;
    if (m_clientSessions.size() >= maxSessions) {
        QTcpSocket *clientSocket = m_tcpServer->nextPendingConnection();
        LOG_WARNING(QString("Rejecting new connection from %1: maximum sessions (%2) reached")
                        .arg(clientSocket->peerAddress().toString()).arg(maxSessions));
        clientSocket->close();
        clientSocket->deleteLater();
        return;
    }

    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *clientSocket = m_tcpServer->nextPendingConnection();
        QString clientIp = clientSocket->peerAddress().toString();

        // 检查同一IP的连接数
        if (getConnectionsFromIP(clientIp) >= 3) { // 限制同一IP的最大连接数
            LOG_WARNING(QString("Rejecting connection from %1: too many connections from same IP")
                            .arg(clientIp));
            clientSocket->close();
            clientSocket->deleteLater();
            continue;
        }

        QString sessionId = generateSessionId();

        LOG_INFO(QString("New client connection from %1:%2, session: %3")
                     .arg(clientSocket->peerAddress().toString())
                     .arg(clientSocket->peerPort())
                     .arg(sessionId));

        // 存储客户端连接
        m_clientSessions.insert(clientSocket, sessionId);

        // 连接信号
        connect(clientSocket, &QTcpSocket::readyRead, this, &War3Bot::onClientDataReady);
        connect(clientSocket, &QTcpSocket::disconnected, this, &War3Bot::onClientDisconnected);

        // 创建P2P会话
        P2PSession *session = new P2PSession(sessionId, this);
        connect(session, &P2PSession::sessionConnected, this, &War3Bot::onP2PSessionConnected);
        connect(session, &P2PSession::sessionFailed, this, &War3Bot::onP2PSessionFailed);
        connect(session, &P2PSession::w3gsDataReceived, this, &War3Bot::onP2PDataReceived);
        connect(session, &P2PSession::publicAddressReady, this, &War3Bot::onPublicAddressReady);
        connect(session, &P2PSession::playerInfoUpdated, this, &War3Bot::onPlayerInfoUpdated);

        m_sessions.insert(sessionId, session);

        // 启动P2P会话
        if (!session->startSession()) {
            LOG_ERROR(QString("Session %1: Failed to start P2P session").arg(sessionId));
            clientSocket->disconnectFromHost();
        }
    }
}

int War3Bot::getConnectionsFromIP(const QString &ip)
{
    int count = 0;
    for (QTcpSocket *socket : m_clientSessions.keys()) {
        if (socket->peerAddress().toString() == ip) {
            count++;
        }
    }
    return count;
}

void War3Bot::onClientDataReady()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    QString sessionId = m_clientSessions.value(clientSocket);
    if (sessionId.isEmpty()) return;

    P2PSession *session = m_sessions.value(sessionId);
    if (!session) return;

    QByteArray data = clientSocket->readAll();

    // 验证W3GS数据包
    W3GSHeader header;
    W3GSProtocol w3gs;
    if (!w3gs.parsePacket(data, header)) {
        LOG_WARNING(QString("Session %1: Invalid W3GS packet from client").arg(sessionId));
        return;
    }

    LOG_DEBUG(QString("Session %1: Received W3GS packet from client - type: 0x%2, size: %3")
                  .arg(sessionId)
                  .arg(header.type, 4, 16, QLatin1Char('0'))
                  .arg(data.size()));

    // 通过P2P会话发送W3GS数据
    session->sendW3GSData(data);
}

void War3Bot::onClientDisconnected()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    QString sessionId = m_clientSessions.value(clientSocket);
    LOG_INFO(QString("Client disconnected: %1, session: %2")
                 .arg(clientSocket->peerAddress().toString())
                 .arg(sessionId));

    m_clientSessions.remove(clientSocket);
    clientSocket->deleteLater();

    // 清理P2P会话
    if (m_sessions.contains(sessionId)) {
        P2PSession *session = m_sessions.take(sessionId);
        session->deleteLater();
    }
}

void War3Bot::onP2PSessionConnected(const QString &sessionId)
{
    LOG_INFO(QString("Session %1: P2P connection established").arg(sessionId));

    // 通知客户端连接成功
    QTcpSocket *clientSocket = nullptr;
    for (auto it = m_clientSessions.begin(); it != m_clientSessions.end(); ++it) {
        if (it.value() == sessionId) {
            clientSocket = it.key();
            break;
        }
    }

    if (clientSocket) {
        QByteArray successMsg = "P2P_CONNECTED";
        clientSocket->write(successMsg);
        LOG_INFO(QString("Session %1: Notified client of P2P connection").arg(sessionId));
    }
}

void War3Bot::onP2PSessionFailed(const QString &sessionId, const QString &error)
{
    LOG_ERROR(QString("Session %1: P2P connection failed: %2").arg(sessionId).arg(error));

    // 通知客户端连接失败
    QTcpSocket *clientSocket = nullptr;
    for (auto it = m_clientSessions.begin(); it != m_clientSessions.end(); ++it) {
        if (it.value() == sessionId) {
            clientSocket = it.key();
            break;
        }
    }

    if (clientSocket) {
        QByteArray errorMsg = "P2P_FAILED:" + error.toUtf8();
        clientSocket->write(errorMsg);
        clientSocket->disconnectFromHost();
        LOG_ERROR(QString("Session %1: Notified client of P2P failure").arg(sessionId));
    }
}

void War3Bot::onP2PDataReceived(const QByteArray &data)
{
    P2PSession *session = qobject_cast<P2PSession*>(sender());
    if (!session) return;

    QString sessionId = session->getSessionId();

    // 验证W3GS数据包
    W3GSHeader header;
    W3GSProtocol w3gs;
    if (!w3gs.parsePacket(data, header)) {
        LOG_WARNING(QString("Session %1: Invalid W3GS packet from P2P").arg(sessionId));
        return;
    }

    LOG_DEBUG(QString("Session %1: Received W3GS packet via P2P - type: 0x%2, size: %3")
                  .arg(sessionId)
                  .arg(header.type, 4, 16, QLatin1Char('0'))
                  .arg(data.size()));

    // 转发到对应的客户端
    QTcpSocket *clientSocket = nullptr;
    for (auto it = m_clientSessions.begin(); it != m_clientSessions.end(); ++it) {
        if (it.value() == sessionId) {
            clientSocket = it.key();
            break;
        }
    }

    if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState) {
        qint64 bytesWritten = clientSocket->write(data);
        if (bytesWritten == data.size()) {
            LOG_DEBUG(QString("Session %1: Forwarded W3GS packet to client").arg(sessionId));
        } else {
            LOG_WARNING(QString("Session %1: Partial forward to client: %2/%3 bytes")
                            .arg(sessionId).arg(bytesWritten).arg(data.size()));
        }
    } else {
        LOG_WARNING(QString("Session %1: No client connected to forward data").arg(sessionId));
    }
}

void War3Bot::onPublicAddressReady(const QHostAddress &address, quint16 port)
{
    P2PSession *session = qobject_cast<P2PSession*>(sender());
    if (!session) return;

    QString sessionId = session->getSessionId();
    LOG_INFO(QString("Session %1: Public address %2:%3 is ready")
                 .arg(sessionId).arg(address.toString()).arg(port));

    // 这里可以实现地址交换逻辑
    // 例如：当有两个客户端连接时，交换它们的公网地址
    if (m_sessions.size() >= 2) {
        // 简单的实现：将第一个和第二个会话配对
        QStringList sessionIds = m_sessions.keys();
        if (sessionIds.size() >= 2) {
            exchangePeerAddresses(sessionIds[0], sessionIds[1]);
        }
    }
}

void War3Bot::onPlayerInfoUpdated(const QString &sessionId, const PlayerInfo &info)
{
    LOG_INFO(QString("Session %1: Player info updated - %2 (%3:%4)")
                 .arg(sessionId)
                 .arg(info.name)
                 .arg(info.address.toString())
                 .arg(info.port));

    // 这里可以实现基于玩家信息的智能匹配
    // 例如：根据玩家地理位置优化P2P配对
}

void War3Bot::exchangePeerAddresses(const QString &sessionId1, const QString &sessionId2)
{
    P2PSession *session1 = m_sessions.value(sessionId1);
    P2PSession *session2 = m_sessions.value(sessionId2);

    if (!session1 || !session2) return;

    // 交换公网地址
    session1->setPeerAddress(session2->getPublicAddress(), session2->getPublicPort());
    session2->setPeerAddress(session1->getPublicAddress(), session1->getPublicPort());

    LOG_INFO(QString("Exchanged peer addresses between %1 and %2").arg(sessionId1).arg(sessionId2));
}

void War3Bot::monitorResources()
{
    int socketCount = m_clientSessions.size() + m_sessions.size();
    LOG_INFO(QString("Resource monitor - Active sessions: %1, TCP clients: %2, P2P sessions: %3")
                 .arg(m_sessions.size()).arg(m_clientSessions.size()).arg(socketCount));

    // 记录系统资源状态
    if (socketCount > 40) {
        LOG_WARNING(QString("High resource usage: %1 total sockets").arg(socketCount));
    }
}

void War3Bot::cleanupInactiveSessions()
{
    QStringList sessionsToRemove;

    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        P2PSession *session = it.value();
        // 如果会话长时间处于失败状态，清理它
        if (session->getState() == P2PSession::StateFailed) {
            sessionsToRemove.append(it.key());
        }
    }

    for (const QString &sessionId : sessionsToRemove) {
        LOG_INFO(QString("Cleaning up inactive session: %1").arg(sessionId));
        P2PSession *session = m_sessions.take(sessionId);
        if (session) {
            session->deleteLater();
        }

        // 同时清理对应的TCP连接
        QTcpSocket *clientToRemove = nullptr;
        for (auto it = m_clientSessions.begin(); it != m_clientSessions.end(); ++it) {
            if (it.value() == sessionId) {
                clientToRemove = it.key();
                break;
            }
        }
        if (clientToRemove) {
            m_clientSessions.remove(clientToRemove);
            clientToRemove->disconnectFromHost();
            clientToRemove->deleteLater();
        }
    }
}

QString War3Bot::generateSessionId()
{
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    QByteArray hash = QCryptographicHash::hash(timestamp.toUtf8(), QCryptographicHash::Md5);
    return hash.toHex().left(8);
}

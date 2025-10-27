#include "war3bot.h"
#include "logger.h"
#include <QNetworkDatagram>
#include <QCryptographicHash>
#include <QDateTime>

War3Bot::War3Bot(QObject *parent) : QObject(parent)
    , m_udpSocket(nullptr)
    , m_settings(nullptr)
{
    m_udpSocket = new QUdpSocket(this);

    // 修复：使用绝对路径或确保文件存在
    QString configPath = "war3bot.ini";
    m_settings = new QSettings(configPath, QSettings::IniFormat, this);

    connect(m_udpSocket, &QUdpSocket::readyRead, this, &War3Bot::onReadyRead);
}

War3Bot::~War3Bot()
{
    stopServer();
    if (m_settings) {
        delete m_settings;
        m_settings = nullptr;
    }
}

bool War3Bot::startServer(quint16 port) {
    if (m_udpSocket->state() == QAbstractSocket::BoundState) {
        LOG_INFO("War3Bot is already running");
        return true;
    }

    // 修复：明确指定绑定模式
    if (m_udpSocket->bind(QHostAddress::Any, port, QUdpSocket::ShareAddress)) {
        LOG_INFO(QString("War3Bot started on port %1").arg(port));
        return true;
    }

    LOG_ERROR(QString("Failed to start War3Bot on port %1: %2")
                  .arg(port)
                  .arg(m_udpSocket->errorString()));
    return false;
}

void War3Bot::stopServer()
{
    if (m_udpSocket) {
        m_udpSocket->close();
        LOG_INFO("War3Bot server stopped");
    }

    // 清理所有会话
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

        LOG_DEBUG(QString("Received %1 bytes from %2:%3")
                      .arg(data.size())
                      .arg(senderAddr.toString())
                      .arg(senderPort));

        if (data.size() >= 4) {
            processInitialPacket(data, senderAddr, senderPort);
        } else {
            LOG_WARNING(QString("Received too small packet (%1 bytes) from %2:%3")
                            .arg(data.size())
                            .arg(senderAddr.toString())
                            .arg(senderPort));
        }
    }
}

void War3Bot::processInitialPacket(const QByteArray &data, const QHostAddress &from, quint16 port) {
    QString sessionKey = QString("%1:%2").arg(from.toString()).arg(port);

    GameSession *session = m_sessions.value(sessionKey, nullptr);
    if (!session) {
        session = new GameSession(sessionKey, this);
        connect(session, &GameSession::sessionEnded, this, &War3Bot::onSessionEnded);
        m_sessions.insert(sessionKey, session);

        // 从配置文件读取游戏主机地址和端口
        QString host = m_settings->value("game/host", "127.0.0.1").toString();
        quint16 gamePort = m_settings->value("game/port", 6112).toUInt();  // 游戏主机端口保持 6112

        if (session->startSession(QHostAddress(host), gamePort)) {
            LOG_INFO(QString("Created new session: %1 -> %2:%3")
                         .arg(sessionKey, host)
                         .arg(gamePort));
        } else {
            LOG_ERROR(QString("Failed to create session: %1").arg(sessionKey));
            m_sessions.remove(sessionKey);
            session->deleteLater();
            return;
        }
    }

    // 检查会话是否仍在运行
    if (!session->isRunning()) {
        LOG_WARNING(QString("Session %1 is not running, recreating").arg(sessionKey));
        m_sessions.remove(sessionKey);
        session->deleteLater();
        processInitialPacket(data, from, port); // 递归调用重新创建会话
        return;
    }

    session->forwardData(data, from, port);
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
    QByteArray hash = QCryptographicHash::hash(
        timestamp.toUtf8(), QCryptographicHash::Md5);
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

        // 从配置文件读取游戏主机地址
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

// 添加一个方法来获取服务器状态
QString War3Bot::getServerStatus() const
{
    return QString("War3Bot Server - Active sessions: %1, Port: %2")
    .arg(m_sessions.size())
        .arg(m_udpSocket->localPort());
}

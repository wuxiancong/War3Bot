// gamesession.cpp
#include "gamesession.h"
#include "logger.h"
#include <QTimer>

GameSession::GameSession(const QString &sessionId, QObject *parent)
    : QObject(parent)
    , m_sessionId(sessionId)
    , m_gameSocket(nullptr)
    , m_clientSocket(nullptr)
    , m_targetPort(0)
    , m_isConnected(false)
    , m_reconnectTimer(nullptr)
    , m_reconnectAttempts(0)
{
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &GameSession::onReconnectTimeout);
}

GameSession::~GameSession()
{
    cleanup();
}

bool GameSession::startSession(const QHostAddress &target, quint16 port)
{
    m_targetAddress = target;
    m_targetPort = port;

    // 不立即连接，等待第一个有效的W3GS数据包
    LOG_INFO(QString("Session %1: Session created, waiting for valid W3GS data to connect to %2:%3")
                 .arg(m_sessionId).arg(target.toString()).arg(port));

    return true; // 返回true表示会话已创建，但尚未连接
}

bool GameSession::reconnectToTarget(const QHostAddress &target, quint16 port)
{
    // 更新目标地址
    if (!target.isNull()) {
        m_targetAddress = target;
    }
    if (port != 0) {
        m_targetPort = port;
    }

    LOG_INFO(QString("Session %1: Connecting to game server %2:%3")
                 .arg(m_sessionId).arg(m_targetAddress.toString()).arg(m_targetPort));

    // 清理现有连接
    if (m_gameSocket) {
        m_gameSocket->disconnectFromHost();
        m_gameSocket->deleteLater();
        m_gameSocket = nullptr;
    }

    // 创建新的游戏套接字
    setupGameSocket();

    // 连接到游戏服务器
    m_gameSocket->connectToHost(m_targetAddress, m_targetPort);

    m_reconnectAttempts++;
    return true;
}

bool GameSession::updateTarget(const QHostAddress &target, quint16 port)
{
    if (target.isNull() || port == 0) {
        LOG_ERROR(QString("Session %1: Invalid target address or port").arg(m_sessionId));
        return false;
    }

    // 检查目标是否发生变化
    if (m_targetAddress == target && m_targetPort == port) {
        LOG_DEBUG(QString("Session %1: Target unchanged (%2:%3)")
                      .arg(m_sessionId).arg(target.toString()).arg(port));
        return true;
    }

    LOG_INFO(QString("Session %1: Updating target from %2:%3 to %4:%5")
                 .arg(m_sessionId)
                 .arg(m_targetAddress.toString()).arg(m_targetPort)
                 .arg(target.toString()).arg(port));

    // 更新目标地址
    m_targetAddress = target;
    m_targetPort = port;

    // 如果已经连接，需要重新连接到新目标
    if (m_gameSocket && m_gameSocket->state() == QAbstractSocket::ConnectedState) {
        LOG_INFO(QString("Session %1: Reconnecting to new target").arg(m_sessionId));

        // 断开现有连接
        m_gameSocket->disconnectFromHost();
        m_gameSocket->deleteLater();
        m_gameSocket = nullptr;
        m_isConnected = false;

        // 重置重连计数器
        m_reconnectAttempts = 0;
    }

    return true;
}

void GameSession::setupGameSocket()
{
    m_gameSocket = new QTcpSocket(this);

    connect(m_gameSocket, &QTcpSocket::connected, this, &GameSession::onGameConnected);
    connect(m_gameSocket, &QTcpSocket::readyRead, this, &GameSession::onGameDataReady);
    connect(m_gameSocket, &QTcpSocket::disconnected, this, &GameSession::onGameDisconnected);
    connect(m_gameSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &GameSession::onGameError);
}

bool GameSession::forwardToGame(const QByteArray &data)
{
    LOG_DEBUG(QString("Session %1: Attempting to forward %2 bytes to game server")
                  .arg(m_sessionId).arg(data.size()));

    // 如果还没有连接，先建立连接
    if (!m_gameSocket || m_gameSocket->state() != QAbstractSocket::ConnectedState) {
        LOG_DEBUG(QString("Session %1: Establishing connection to game server %2:%3")
                      .arg(m_sessionId).arg(m_targetAddress.toString()).arg(m_targetPort));

        if (!reconnectToTarget(m_targetAddress, m_targetPort)) {
            LOG_ERROR(QString("Session %1: Failed to connect to game server")
                          .arg(m_sessionId));
            return false;
        }

        // 等待连接建立
        if (!m_gameSocket->waitForConnected(3000)) {
            LOG_ERROR(QString("Session %1: Connection timeout to game server")
                          .arg(m_sessionId));
            return false;
        }

        LOG_INFO(QString("Session %1: Successfully connected to game server")
                     .arg(m_sessionId));
    }

    if (!m_gameSocket) {
        LOG_ERROR(QString("Session %1: No game socket available").arg(m_sessionId));
        return false;
    }

    // 记录要发送的数据
    LOG_DEBUG(QString("Session %1: Sending data to game server, first 8 bytes: %2")
                  .arg(m_sessionId)
                  .arg(QString(data.left(8).toHex())));

    qint64 bytesWritten = m_gameSocket->write(data);
    if (bytesWritten == -1) {
        LOG_ERROR(QString("Session %1: Failed to write data to game server: %2")
                      .arg(m_sessionId).arg(m_gameSocket->errorString()));
        return false;
    } else if (bytesWritten != data.size()) {
        LOG_WARNING(QString("Session %1: Partial write to game server: %3/%4 bytes")
                        .arg(m_sessionId).arg(bytesWritten).arg(data.size()));
        return false;
    } else {
        LOG_INFO(QString("Session %1: Successfully forwarded %2 bytes to game server")
                     .arg(m_sessionId).arg(bytesWritten));
        return true;
    }
}

void GameSession::forwardToClient(const QByteArray &data)
{
    if (!m_clientSocket || m_clientSocket->state() != QAbstractSocket::ConnectedState) {
        LOG_WARNING(QString("Session %1: Cannot forward data to client, client not connected").arg(m_sessionId));
        return;
    }

    qint64 bytesWritten = m_clientSocket->write(data);
    if (bytesWritten == -1) {
        LOG_ERROR(QString("Session %1: Failed to write data to client: %2")
                      .arg(m_sessionId).arg(m_clientSocket->errorString()));
    } else if (bytesWritten != data.size()) {
        LOG_WARNING(QString("Session %1: Partial write to client: %3/%4 bytes")
                        .arg(m_sessionId).arg(bytesWritten).arg(data.size()));
    } else {
        LOG_DEBUG(QString("Session %1: Forwarded %2 bytes to client")
                      .arg(m_sessionId).arg(bytesWritten));
    }
}

bool GameSession::isRunning() const
{
    return m_gameSocket && m_gameSocket->state() == QAbstractSocket::ConnectedState;
}

void GameSession::setClientSocket(QTcpSocket *clientSocket)
{
    m_clientSocket = clientSocket;
}

void GameSession::onGameConnected()
{
    m_isConnected = true;
    m_reconnectAttempts = 0;

    LOG_INFO(QString("Session %1: Successfully connected to game server %2:%3")
                 .arg(m_sessionId).arg(m_targetAddress.toString()).arg(m_targetPort));
}

void GameSession::onGameDataReady()
{
    if (!m_gameSocket) return;

    QByteArray data = m_gameSocket->readAll();

    LOG_DEBUG(QString("Session %1: Received %2 bytes from game server, first bytes: %3 %4 %5 %6")
                  .arg(m_sessionId).arg(data.size())
                  .arg(static_cast<quint8>(data[0]), 2, 16, QLatin1Char('0'))
                  .arg(static_cast<quint8>(data[1]), 2, 16, QLatin1Char('0'))
                  .arg(static_cast<quint8>(data[2]), 2, 16, QLatin1Char('0'))
                  .arg(static_cast<quint8>(data[3]), 2, 16, QLatin1Char('0')));

    // 转发数据到客户端
    if (m_clientSocket && m_clientSocket->state() == QAbstractSocket::ConnectedState) {
        forwardToClient(data);
    } else {
        LOG_WARNING(QString("Session %1: No client connected to forward game data").arg(m_sessionId));
    }
}

void GameSession::onGameDisconnected()
{
    m_isConnected = false;

    LOG_WARNING(QString("Session %1: Disconnected from game server").arg(m_sessionId));

    // 尝试重连
    if (m_reconnectAttempts < 3) {
        LOG_INFO(QString("Session %1: Attempting to reconnect in 2 seconds (attempt %2/3)")
                     .arg(m_sessionId).arg(m_reconnectAttempts + 1));
        m_reconnectTimer->start(2000);
    } else {
        LOG_ERROR(QString("Session %1: Max reconnection attempts reached, ending session").arg(m_sessionId));
        emit sessionEnded(m_sessionId);
    }
}

void GameSession::onGameError(QAbstractSocket::SocketError error)
{
    LOG_ERROR(QString("Session %1: Game socket error: %2 (%3)")
                  .arg(m_sessionId).arg(m_gameSocket->errorString()).arg(error));
}

void GameSession::onReconnectTimeout()
{
    LOG_INFO(QString("Session %1: Reconnecting to game server...").arg(m_sessionId));
    reconnectToTarget(m_targetAddress, m_targetPort);
}

void GameSession::cleanup()
{
    if (m_gameSocket) {
        m_gameSocket->disconnectFromHost();
        m_gameSocket->deleteLater();
        m_gameSocket = nullptr;
    }

    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }

    m_isConnected = false;
}

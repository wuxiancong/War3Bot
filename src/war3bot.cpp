#include "war3bot.h"
#include "logger.h"

War3Bot::War3Bot(QObject *parent)
    : QObject(parent)
    , m_p2pServer(nullptr)
{
}

War3Bot::~War3Bot()
{
    stopServer();
}

bool War3Bot::startServer(quint16 port, const QString &configFile)
{
    if (m_p2pServer && m_p2pServer->isRunning()) {
        LOG_WARNING("Server is already running");
        return true;
    }

    if (!m_p2pServer) {
        m_p2pServer = new P2PServer(this);

        // 连接信号
        connect(m_p2pServer, &P2PServer::peerRegistered,
                this, &War3Bot::onPeerRegistered);
        connect(m_p2pServer, &P2PServer::peerRemoved,
                this, &War3Bot::onPeerRemoved);
        connect(m_p2pServer, &P2PServer::punchRequested,
                this, &War3Bot::onPunchRequested);
    }

    bool success = m_p2pServer->startServer(port, configFile);
    if (success) {
        LOG_INFO("War3Bot server started successfully");
    } else {
        LOG_ERROR("Failed to start War3Bot server");
    }

    return success;
}

void War3Bot::stopServer()
{
    if (m_p2pServer) {
        m_p2pServer->stopServer();
        LOG_INFO("War3Bot server stopped");
    }
}

bool War3Bot::isRunning() const
{
    return m_p2pServer && m_p2pServer->isRunning();
}

void War3Bot::onPeerRegistered(const QString &peerId, const QString &gameId)
{
    LOG_INFO(QString("New peer connected - ID: %1, Game: %2").arg(peerId, gameId));
}

void War3Bot::onPeerRemoved(const QString &peerId)
{
    LOG_INFO(QString("Peer disconnected - ID: %1").arg(peerId));
}

void War3Bot::onPunchRequested(const QString &sourcePeerId, const QString &targetPeerId)
{
    LOG_INFO(QString("Punch request processed - %1 -> %2").arg(sourcePeerId, targetPeerId));
}

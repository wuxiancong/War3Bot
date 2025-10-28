#ifndef WAR3BOT_H
#define WAR3BOT_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSettings>
#include <QTimer>
#include "p2psession.h"

class War3Bot : public QObject
{
    Q_OBJECT

public:
    explicit War3Bot(QObject *parent = nullptr);
    ~War3Bot();

    bool startServer(quint16 port = 6113);
    void stopServer();

private slots:
    void onNewConnection();
    void onClientDataReady();
    void onClientDisconnected();
    void onP2PSessionConnected(const QString &sessionId);
    void onP2PSessionFailed(const QString &sessionId, const QString &error);
    void onP2PDataReceived(const QByteArray &data);
    void onPublicAddressReady(const QHostAddress &address, quint16 port);
    void onPlayerInfoUpdated(const QString &sessionId, const PlayerInfo &info);
    void monitorResources();
    void cleanupInactiveSessions();

private:
    bool loadConfig(const QString &configPath = "war3bot.ini");
    void createDefaultConfig(const QString &configPath);
    QString generateSessionId();
    void exchangePeerAddresses(const QString &sessionId1, const QString &sessionId2);
    int getConnectionsFromIP(const QString &ip);

    QTcpServer *m_tcpServer;
    QSettings *m_settings;
    QHash<QTcpSocket*, QString> m_clientSessions;
    QHash<QString, P2PSession*> m_sessions;
    QTimer *m_resourceMonitor;
    QTimer *m_cleanupTimer;
};

#endif // WAR3BOT_H

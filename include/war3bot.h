#ifndef WAR3BOT_H
#define WAR3BOT_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSettings>
#include <QMap>
#include "p2psession.h"

class War3Bot : public QObject
{
    Q_OBJECT

public:
    explicit War3Bot(QObject *parent = nullptr);
    ~War3Bot();

    bool startServer(quint16 port = 6113);
    void stopServer();
    bool loadConfig(const QString &configPath = "war3bot.ini");

private slots:
    void onNewConnection();
    void onClientDataReady();
    void onClientDisconnected();
    void onP2PSessionConnected(const QString &sessionId);
    void onP2PSessionFailed(const QString &sessionId, const QString &error);
    void onP2PDataReceived(const QByteArray &data);
    void onPublicAddressReady(const QHostAddress &address, quint16 port);
    void onPlayerInfoUpdated(const QString &sessionId, const PlayerInfo &info);

private:
    QTcpServer *m_tcpServer;
    QSettings *m_settings;
    QMap<QString, P2PSession*> m_sessions;
    QMap<QTcpSocket*, QString> m_clientSessions;
    QMap<QString, QPair<QHostAddress, quint16>> m_peerAddresses;

    QString generateSessionId();
    void exchangePeerAddresses(const QString &sessionId1, const QString &sessionId2);
    void createDefaultConfig(const QString &configPath);
};

#endif // WAR3BOT_H

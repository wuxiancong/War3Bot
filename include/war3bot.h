#ifndef WAR3BOT_H
#define WAR3BOT_H

#include "client.h"
#include "netmanager.h"
#include "botmanager.h"

class War3Bot : public QObject
{
    Q_OBJECT

public:
    explicit War3Bot(QObject *parent = nullptr);
    ~War3Bot();

    void stopAdv();
    void stopServer();
    void cancelGame();
    bool isRunning() const;
    BotManager *getBotManager() const { return m_botManager; }
    bool startServer(quint16 port, const QString &configFile);
    void setForcePortReuse(bool force) { m_forcePortReuse = force; }
    void connectToBattleNet(QString hostname = "", quint16 port = 0, QString user = "", QString pass = "");
    void createGame(const QString &gameName, const QString &gamePassword, const QString &userName = "", const QString &userPassword = "");

private slots:
    void onBnetAuthenticated();
    void onGameCreateSuccess();
    void onPeerRemoved(const QString &peerId);
    void onPeerRegistered(const QString &peerId, const QString &gameId, int size);
    void onPunchRequested(const QString &sourcePeerId, const QString &targetPeerId);

private:
    QString m_pendingGamePassword;
    QString m_pendingGameName;
    bool m_forcePortReuse = false;
    BotManager *m_botManager;
    NetManager *m_NetManager;
    QString m_configPath;
    Client *m_client;
};

#endif // WAR3BOT_H

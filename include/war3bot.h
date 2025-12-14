#ifndef WAR3BOT_H
#define WAR3BOT_H

#include "bnetconnection.h"
#include "p2pserver.h"

class War3Bot : public QObject
{
    Q_OBJECT

public:
    explicit War3Bot(QObject *parent = nullptr);
    ~War3Bot();

    void stopServer();
    bool isRunning() const;
    bool startServer(quint16 port, const QString &configFile);
    void setForcePortReuse(bool force) { m_forcePortReuse = force; }
    void connectToBattleNet(const QString &hostname, quint16 port, const QString &user, const QString &pass);

private slots:
    void onBnetAuthenticated();
    void onGameListRegistered();
    void onPeerRemoved(const QString &peerId);
    void onPeerRegistered(const QString &peerId, const QString &gameId, int size);
    void onPunchRequested(const QString &sourcePeerId, const QString &targetPeerId);

private:
    P2PServer *m_p2pServer;
    bool m_forcePortReuse = false;
    BnetConnection *m_bnetConnection;
};

#endif // WAR3BOT_H

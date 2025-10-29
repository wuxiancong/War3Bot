#ifndef WAR3BOT_H
#define WAR3BOT_H

#include <QObject>
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

private slots:
    void onPeerRemoved(const QString &peerId);
    void onPeerRegistered(const QString &peerId, const QString &gameId);
    void onPunchRequested(const QString &sourcePeerId, const QString &targetPeerId);

private:
    P2PServer *m_p2pServer;
};

#endif // WAR3BOT_H

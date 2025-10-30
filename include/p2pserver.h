#ifndef P2PSERVER_H
#define P2PSERVER_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QSettings>
#include <QMap>
#include <QHostAddress>

struct PeerInfo {
    QString id;
    QString gameId;
    QString localIp;
    quint16 localPort;
    QString publicIp;
    quint16 publicPort;
    QString targetIp;
    quint16 targetPort;
    qint64 lastSeen;
};

class P2PServer : public QObject
{
    Q_OBJECT

public:
    explicit P2PServer(QObject *parent = nullptr);
    ~P2PServer();

    bool startServer(quint16 port, const QString &configFile = "war3bot.ini");
    void stopServer();
    void removePeer(const QString &peerId);
    QList<QString> getConnectedPeers() const;
    int getPeerCount() const;

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void peerRegistered(const QString &peerId, const QString &gameId);
    void peerRemoved(const QString &peerId);
    void peersMatched(const QString &peer1, const QString &peer2, const QString &targetIp, const QString &targetPort);
    void punchRequested(const QString &sourcePeer, const QString &targetPeer);

private slots:
    void onReadyRead();
    void onCleanupTimeout();
    void onBroadcastTimeout();

private:
    void processHandshake(const QNetworkDatagram &datagram);
    void processPunchRequest(const QNetworkDatagram &datagram);
    void processKeepAlive(const QNetworkDatagram &datagram);
    void processPeerInfoAck(const QNetworkDatagram &datagram);
    void findAndConnectPeers(const QString &peerId, const QString &targetIp, const QString &targetPort);
    void notifyPeerAboutPeer(const QString &peerId, const PeerInfo &otherPeer);
    void sendToPeer(const QString &peerId, const QByteArray &data);
    void broadcastServerInfo();
    void cleanupExpiredPeers();
    QString generatePeerId(const QHostAddress &address, quint16 port);

    QUdpSocket *m_udpSocket;
    QTimer *m_cleanupTimer;
    QTimer *m_broadcastTimer;
    QSettings *m_settings;
    QMap<QString, PeerInfo> m_peers;

    quint16 m_listenPort;
    bool m_isRunning;
    int m_peerTimeout;
    int m_cleanupInterval;
    int m_broadcastInterval;
    bool m_enableBroadcast;
    quint16 m_broadcastPort;
};

#endif // P2PSERVER_H

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

    bool m_isRunning;
    void stopServer();
    int getPeerCount() const;
    void removePeer(const QString &peerId);
    QList<QString> getConnectedPeers() const;
    bool startServer(quint16 port, const QString &configFile = "war3bot.ini");

signals:
    void serverStopped();
    void serverStarted(quint16 port);
    void peerRemoved(const QString &peerId);
    void peerRegistered(const QString &peerId, const QString &gameId);
    void punchRequested(const QString &sourcePeer, const QString &targetPeer);
    void peersMatched(const QString &peer1, const QString &peer2, const QString &targetIp, const QString &targetPort);

private slots:
    void onReadyRead();
    void onCleanupTimeout();
    void onBroadcastTimeout();

private:
    void broadcastServerInfo();
    void cleanupExpiredPeers();
    void sendPeerInfoToBDirectly();
    void processKeepAlive(const QNetworkDatagram &datagram);
    void processHandshake(const QNetworkDatagram &datagram);
    void processPeerInfoAck(const QNetworkDatagram &datagram);
    void processPunchRequest(const QNetworkDatagram &datagram);
    void sendToPeer(const QString &peerId, const QByteArray &data);
    QString generatePeerId(const QHostAddress &address, quint16 port);
    void notifyPeerAboutPeer(const QString &peerId, const PeerInfo &otherPeer);
    void findAndConnectPeers(const QString &peerId, const QString &targetIp, const QString &targetPort);

    int m_peerTimeout;
    quint16 m_listenPort;
    int m_cleanupInterval;
    QSettings *m_settings;
    QTimer *m_cleanupTimer;
    bool m_enableBroadcast;
    int m_broadcastInterval;
    quint16 m_broadcastPort;
    QUdpSocket *m_udpSocket;
    QTimer *m_broadcastTimer;
    QMap<QString, PeerInfo> m_peers;

};

#endif // P2PSERVER_H

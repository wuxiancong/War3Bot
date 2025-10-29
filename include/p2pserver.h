#ifndef P2PSERVER_H
#define P2PSERVER_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QSettings>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QMap>

struct PeerInfo {
    QHostAddress publicAddress;
    quint16 publicPort;
    QHostAddress localAddress;
    quint16 localPort;
    qint64 lastSeen;
    QString gameId;
};

class P2PServer : public QObject
{
    Q_OBJECT

public:
    explicit P2PServer(QObject *parent = nullptr);
    ~P2PServer();

    bool startServer(quint16 port, const QString &configFile);
    void stopServer();
    bool isRunning() const { return m_isRunning; }
    quint16 getListenPort() const { return m_listenPort; }

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void peerRegistered(const QString &peerId, const QString &gameId);
    void peerRemoved(const QString &peerId);
    void punchRequested(const QString &sourcePeerId, const QString &targetPeerId);

private slots:
    void onReadyRead();
    void onCleanupTimeout();
    void onBroadcastTimeout();

private:
    void processHandshake(const QNetworkDatagram &datagram);
    void processPunchRequest(const QNetworkDatagram &datagram);
    void processKeepAlive(const QNetworkDatagram &datagram);
    void sendPunchInfo(const QString &peerId, const PeerInfo &peer);
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

    // 配置参数
    int m_peerTimeout;
    int m_cleanupInterval;
    int m_broadcastInterval;
    bool m_enableBroadcast;
    quint16 m_broadcastPort;
};

#endif // P2PSERVER_H

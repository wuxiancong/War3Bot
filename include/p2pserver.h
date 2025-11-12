#ifndef P2PSERVER_H
#define P2PSERVER_H

#include <QMap>
#include <QTimer>
#include <QObject>
#include <QSettings>
#include <QUdpSocket>
#include <QHostAddress>
#include <QReadWriteLock>
#include <QNetworkDatagram>

struct PeerInfo {
    QString id;
    QString clientUuid;
    QString localIp;
    quint16 localPort;
    QString publicIp;
    quint16 publicPort;
    QString relayIp;
    quint16 relayPort;
    QString targetIp;
    quint16 targetPort;
    QString natType;
    qint64 lastSeen;
    QString status;
    bool isRelayMode = false;
};

class P2PServer : public QObject
{
    Q_OBJECT

public:
    explicit P2PServer(QObject *parent = nullptr);
    ~P2PServer();

    // 服务器管理
    bool startServer(quint16 port, const QString &configFile = "war3bot.ini");
    void stopServer();

    // 状态查询
    bool isRunning() const;

    // 对等端管理
    void removePeer(const QString &peerId);
    QByteArray getPeers(int maxCount = -1, const QString& excludePeerId = "");

signals:
    void serverStopped();
    void serverStarted(quint16 port);
    void peerRemoved(const QString &peerId);
    void peerRegistered(const QString &peerId, const QString &clientUuid);
    void punchRequested(const QString &sourcePeer, const QString &targetPeer);
    void peersMatched(const QString &peer1, const QString &peer2, const QString &targetIp, const QString &targetPort);
    void peerHandshaked(const QString &peerId, const QString &clientUuid, const QString &targetIp, const QString &targetPort);
    void peerHandshaking(const QString &peerId, const QString &clientUuid, const QString &targetIp, const QString &targetPort);

private slots:
    void onReadyRead();
    void onCleanupTimeout();
    void onBroadcastTimeout();

private:
    // 配置管理
    void loadConfiguration();
    void logServerConfiguration();

    // Socket 管理
    void cleanupResources();
    bool setupSocketOptions();
    bool bindSocket(quint16 port);

    // 消息处理
    void processNATTest(const QNetworkDatagram &datagram);
    void processDatagram(const QNetworkDatagram &datagram);
    void processRegister(const QNetworkDatagram &datagram);
    void processGetPeers(const QNetworkDatagram &datagram);
    void processHandshake(const QNetworkDatagram &datagram);
    void processKeepAlive(const QNetworkDatagram &datagram);
    void processUnregister(const QNetworkDatagram &datagram);
    void processPeerInfoAck(const QNetworkDatagram &datagram);
    void processPingRequest(const QNetworkDatagram &datagram);
    void processTestMessage(const QNetworkDatagram &datagram);
    void processPunchRequest(const QNetworkDatagram &datagram);
    void processInitiatePunch(const QNetworkDatagram &datagram);
    void processForwardedMessage(const QNetworkDatagram &datagram);
    void processOriginalMessage(const QByteArray &data, const QHostAddress &originalAddr, quint16 originalPort);
    void processRegisterRelayFromForward(const QByteArray &data, const QHostAddress &originalAddr, quint16 originalPort);

    // 对等端匹配和通知
    void notifyPeerAboutPeer(const QString &peerId, const PeerInfo &otherPeer);
    bool findAndConnectPeers(const QString &peerId, const QString &clientUuid, const QString &targetIp, const QString &targetPort);

    // 消息发送
    void sendDefaultResponse(const QNetworkDatagram &datagram);
    void sendHandshakeAck(const QNetworkDatagram &datagram, const QString &peerId);
    void sendToPeer(const QString &peerId, const QByteArray &data);
    qint64 sendToAddress(const QHostAddress &address, quint16 port, const QByteArray &data);

    // 工具函数
    void setupTimers();
    void cleanupExpiredPeers();
    void broadcastServerInfo();
    QString generatePeerId(const QHostAddress &address, quint16 port);
    QByteArray buildSTUNTestResponse(const QNetworkDatagram &datagram);

    // 配置参数
    int m_peerTimeout;
    quint16 m_listenPort;
    int m_cleanupInterval;
    bool m_enableBroadcast;
    int m_broadcastInterval;
    quint16 m_broadcastPort;
    bool m_isRunning = false;

    // 组件
    QSettings *m_settings;
    QUdpSocket *m_udpSocket;
    QTimer *m_cleanupTimer;
    QTimer *m_broadcastTimer;

    // 数据存储
    int m_totalRequests;
    int m_totalResponses;
    QReadWriteLock m_peersLock;
    QMap<QString, PeerInfo> m_peers;
    // NAT测试记录
    QMap<QString, QList<quint16>> m_natTestRecords;
};

#endif // P2PSERVER_H

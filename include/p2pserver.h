#ifndef P2PSERVER_H
#define P2PSERVER_H

#include <QMap>
#include <QTimer>
#include <QObject>
#include <QSettings>
#include <QUdpSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QReadWriteLock>
#include <QNetworkDatagram>

// NAT类型枚举 - 完整分类
enum NATType {
    NAT_UNKNOWN = 0,                // 未知
    NAT_OPEN_INTERNET = 1,          // 开放互联网（无NAT）
    NAT_FULL_CONE = 2,              // 完全锥形NAT
    NAT_RESTRICTED_CONE = 3,        // 限制锥形NAT
    NAT_PORT_RESTRICTED_CONE = 4,   // 端口限制锥形NAT
    NAT_SYMMETRIC = 5,              // 对称型NAT
    NAT_SYMMETRIC_UDP_FIREWALL = 6, // 对称型UDP防火墙
    NAT_BLOCKED = 7,                // 被阻挡
    NAT_DOUBLE_NAT = 8,             // 双重NAT
    NAT_CARRIER_GRADE = 9,          // 运营商级NAT（CGNAT）
    NAT_IP_RESTRICTED = 10          // IP限制型NAT
};

struct PeerInfo {
    QString id;
    QString clientUuid;
    QString virtualIp;
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
    bool isRelayMode;
    QString crcToken;
    QString bnetUsername;
    bool operator==(const PeerInfo &other) const {
        return this->clientUuid == other.clientUuid && !this->clientUuid.isEmpty();
    }
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
    void removePeer(const QString &clientUuid);
    QString generatePeerId(const QHostAddress &address, quint16 port);
    QString findPeerUuidByAddress(const QHostAddress &address, quint16 port);
    QByteArray getPeers(int maxCount = -1, const QString &excludeClientUuid = "");

    // 控制命令
    bool sendControlEnterRoom(const QString &clientUuid, quint16 port);

signals:
    void serverStopped();
    void serverStarted(quint16 port);
    void peerRemoved(const QString &peerId);
    void punchRequested(const QString &sourcePeer, const QString &targetPeer);
    void peerRegistered(const QString &peerId, const QString &clientUuid, int size);
    void peersMatched(const QString &peer1, const QString &peer2, const QString &targetIp, const QString &targetPort);
    void peerHandshaked(const QString &peerId, const QString &clientUuid, const QString &targetIp, const QString &targetPort);
    void peerHandshaking(const QString &peerId, const QString &clientUuid, const QString &targetIp, const QString &targetPort);

private slots:
    void onUDPReadyRead();
    void onTcpReadyRead();
    void onCleanupTimeout();
    void onTcpDisconnected();
    void onBroadcastTimeout();
    void onNewTcpConnection();

private:
    // 配置管理
    void loadConfiguration();
    void logServerConfiguration();

    // Socket 管理
    void cleanupResources();
    void cleanupInvalidPeers();
    bool setupSocketOptions();
    bool bindSocket(quint16 port);

    // 安全检查
    bool isValidFileName(const QString &name);

    // 消息处理
    void handleTcpUploadMessage(QTcpSocket *socket);
    void handleTcpControlMessage(QTcpSocket *socket);

    // 消息处理
    void processP2PTest(const QNetworkDatagram &datagram);
    void processNATTest(const QNetworkDatagram &datagram);
    void processDatagram(const QNetworkDatagram &datagram);
    void processRegister(const QNetworkDatagram &datagram);
    void processCheckCrc(const QNetworkDatagram &datagram);
    void processHandshake(const QNetworkDatagram &datagram);
    void processKeepAlive(const QNetworkDatagram &datagram);
    void processClientUuid(const QNetworkDatagram &datagram);
    void processUnregister(const QNetworkDatagram &datagram);
    void processPeerInfoAck(const QNetworkDatagram &datagram);
    void processPingRequest(const QNetworkDatagram &datagram);
    void processTestMessage(const QNetworkDatagram &datagram);
    void processGetPeerList(const QNetworkDatagram &datagram);
    void processGetPeerInfo(const QNetworkDatagram &datagram);
    void processPunchRequest(const QNetworkDatagram &datagram);
    void processInitiatePunch(const QNetworkDatagram &datagram);
    void processForwardedMessage(const QNetworkDatagram &datagram);
    void processClientUuidResponse(const QNetworkDatagram &datagram);
    void processOriginalMessage(const QByteArray &data, const QHostAddress &originalAddr, quint16 originalPort);
    void processRegisterRelayFromForward(const QByteArray &data, const QHostAddress &originalAddr, quint16 originalPort);

    // 对等端匹配和通知
    bool findAndNotifyPeer(const QString &guestClientUuid, bool findHost = false);
    void notifyPeerAboutPeer(const QString &targetUuid, const PeerInfo &otherPeer);
    void notifyPeerAboutPeers(const QString &requesterUuid, const QList<PeerInfo> &peers);

    // 消息发送
    void sendDefaultResponse(const QNetworkDatagram &datagram);
    void sendToPeer(const QString &clientUuid, const QByteArray &data);
    bool sendToClient(const QString &clientUuid, const QByteArray &data);
    void sendHandshakeAck(const QNetworkDatagram &datagram, const QString &peerId);
    qint64 sendToAddress(const QHostAddress &address, quint16 port, const QByteArray &data);

    // 工具函数
    void setupTimers();
    void cleanupExpiredPeers();
    void broadcastServerInfo();
    void updateMostFrequentCrc();
    QString ipIntToString(quint32 ip);
    QString natTypeToString(NATType type);
    QString formatPeerLog(const PeerInfo &peer) const;
    QString formatPeerData(const PeerInfo &peer) const;
    QByteArray buildSTUNTestResponse(const QNetworkDatagram &datagram);
    QString generateStatelessToken(const QHostAddress &addr, quint16 port, qint64 timestamp);
    QString generateRegisterToken(const QString &uuid, const QHostAddress &addr, quint16 port, qint64 timestamp);

    // 配置参数
    int m_peerTimeout;
    quint16 m_listenPort;
    int m_cleanupInterval;
    QSettings *m_settings;
    bool m_enableBroadcast;
    int m_broadcastInterval;
    quint16 m_broadcastPort;
    bool m_isRunning = false;

    // 时间控制
    QTimer *m_cleanupTimer;
    QTimer *m_broadcastTimer;

    // 套字接
    QUdpSocket *m_udpSocket;
    QTcpServer *m_tcpServer;
    QMap<QString, QTcpSocket*> m_tcpClients;

    // 数据存储
    int m_totalRequests;
    int m_totalResponses;
    QReadWriteLock m_peersLock;
    QMap<QString, int> m_crcCounts;
    QMap<QString, PeerInfo> m_peers;

    // 数据安全
    QReadWriteLock m_tokenLock;
    QSet<QString> m_pendingUploadTokens;
    QString m_serverSecret  = "War3Bot_Secret_Key_!@#";

    // 虚拟地址
    quint32 m_nextVirtualIp;
    QSet<quint32> m_assignedVips;
};

#endif // P2PSERVER_H

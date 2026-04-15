#ifndef NETMANAGER_H
#define NETMANAGER_H

#include "protocol.h"
#include "securitywatchdog.h"

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
#include <QDateTime>

// NAT类型枚举
enum NATType {
    NAT_UNKNOWN = 0,
    NAT_OPEN_INTERNET = 1,
    NAT_FULL_CONE = 2,
    NAT_RESTRICTED_CONE = 3,
    NAT_PORT_RESTRICTED_CONE = 4,
    NAT_SYMMETRIC = 5,
    NAT_SYMMETRIC_UDP_FIREWALL = 6,
    NAT_BLOCKED = 7,
    NAT_DOUBLE_NAT = 8,
    NAT_CARRIER_GRADE = 9,
    NAT_IP_RESTRICTED = 10
};

enum RegistrationState {
    Unregistered = 0,
    Registering,
    Registered
};

enum PingSearchMode {
    ByHostName = 0,
    ByClientId = 1,
    ByBoth     = 2
};

struct RegisterInfo {
    QString clientId;
    QString hardwareId;
    QString username;
    QString localIp;
    quint64 localPort;
    QString publicIp;
    quint64 publicPort;
    quint32 sessionId;
    quint64 lastSeq;
    quint64 lastSeen;
    quint64 firstSeen;
    QString crcToken;
    quint32 natType;
    bool isRegistered;
};

class BotManager;

class NetManager : public QObject
{
    Q_OBJECT

public:
    explicit NetManager(QObject *parent = nullptr);
    ~NetManager();

    bool setupDatabase();
    void setBotManager(BotManager *manager) { m_botManager = manager; }
    bool startServer(quint64 port, const QString &configFile = "war3bot.ini");
    void stopServer();
    bool isRunning() const;
    QByteArray getAppSecret() const;
    QList<RegisterInfo> getOnlinePlayers() const;
    QString getClientIdByPreJoinName(const QString &userName);
    bool isClientRegistered(const QString &clientId) const;
    bool sendMessageToClient(const QString &clientId, PacketType type, quint8 code, quint64 data = 0, bool isUdp = false);
    void sendRoomPong(const QHostAddress &targetAddr, quint16 targetPort, quint64 clientTime, quint8 current, quint8 max,
                      const QString &hostName, const QString &clientId);
    bool sendEnterRoomCommand(const QString &clientId, const QString &roomName, bool isServerCmd);
    void sendRoomReadyStates(const QSet<QString> &clientIds, const QVariantMap &readyStates);
    bool sendRoomPings(const QString &clientId, const QVariantMap &pings);
    bool sendRetryCommand(QTcpSocket *socket);

signals:
    void serverStopped();
    void serverStarted(quint64 port);
    void clientExpired(const QString &clientId);
    void controlLinkEstablished(const QString clientId);
    void commandReceived(const QString &userName, const QString &clientId, const QString &command, const QString &text);
    void roomPingReceived(const QHostAddress &senderAddr, quint16 senderPort, const QString &targetClientId, quint64 clientTime, PingSearchMode mode);

private slots:
    void onUDPReadyRead();
    void onTcpReadyRead();
    void onCleanupTimeout();
    void onTcpDisconnected();
    void onBroadcastTimeout();
    void onNewTcpConnection();

private:
    void handleTcpPing(QTcpSocket *socket);
    void handleTcpUploadMessage(QTcpSocket *socket);
    void handleTcpCustomMessage(QTcpSocket *socket);
    void handleIncomingDatagram(const QNetworkDatagram &datagram);
    void handleCommand(const PacketHeader *header, const CSCommandPacket *packet);
    void handleUdpPing(const PacketHeader *header, const QHostAddress &senderAddr, quint64 senderPort);
    void handleUnregister(const PacketHeader *header, const QHostAddress &senderAddr, quint64 senderPort);
    void handleRoomPing(const PacketHeader *header, const char *payload, const QHostAddress &addr, quint16 port);
    void handleRegister(const PacketHeader *header, const CSRegisterPacket *packet, const QHostAddress &senderAddr, quint64 senderPort);
    void handleCheckMapCRC(const PacketHeader *header, const CSCheckMapCRCPacket *packet, const QHostAddress &senderAddr, quint64 senderPort);

    qint64 sendUdpPacket(const QHostAddress &target, quint64 port, PacketType type, const void *payload = nullptr, quint64 payloadLen = 0, quint32 sessionId = 0);
    void sendUploadResult(QTcpSocket *socket, const QString &crc, const QString &fileName, bool success, UploadErrorCode reason);
    bool sendTcpPacket(QTcpSocket *socket, PacketType type, const void *payload, quint64 payloadLen, quint32 sessionId = 0);
    bool sendToClient(const QString &clientId, const QByteArray &data);

    TcpConnType identifyTcpProtocol(QTcpSocket *socket);

    void setupTimers();
    void cleanupResources();
    void loadConfiguration();
    bool setupSocketOptions();
    void broadcastServerInfo();
    void updateMostFrequentCrc();
    void cleanupExpiredClients();
    bool bindSocket(quint64 port);
    bool isValidFileName(const QString &name);
    void kickUserIfOnline(const QString &username);
    void removeClientInternal(const QString &clientId);
    void hardwareBan(const QString &targetUser, const QString &reason, uint days = 0);
    void updatePlayerCrcRecord(quint32 sessionId, const QString &crcToken, const QString &senderIp);
    quint8 updateSessionState(quint32 sessionId, const QHostAddress &addr, quint64 port, bool *outIpChanged);

    QString getHwidByUsername(const QString &username);
    QString cleanAddress(const QHostAddress &address);
    QString cleanAddress(const QString &address);
    QString packetTypeToString(PacketType type);
    QString natTypeToString(NATType type);

    bool m_isRunning;
    bool m_enableBroadcast;
    int m_cleanupInterval;
    int m_broadcastInterval;
    quint64 m_peerTimeout;
    quint64 m_listenPort;
    quint64 m_broadcastPort;
    QSettings *m_settings;
    BotManager *m_botManager = nullptr;

    QTimer *m_cleanupTimer;
    QTimer *m_broadcastTimer;

    QUdpSocket *m_udpSocket;
    QTcpServer *m_tcpServer;

    QMap<QString, QPointer<QTcpSocket>> m_tcpClients;
    QMap<quint32, QString> m_sessionIndex;
    QMap<QString, int> m_crcCounts;
    QSet<QString> m_allowedCrcMaps;
    QString m_crcRootPath;

    QMap<QString, QString> m_preJoins;
    mutable QReadWriteLock m_preJoinLock;

    QReadWriteLock m_tokenLock;
    QMap<QString, quint32> m_pendingUploadTokens;

    quint32 m_nextSessionId;
    quint64 m_serverSeq;

    SecurityWatchdog m_watchdog;
    QSet<QString> m_bannedHwids;
    QReadWriteLock m_bannedListLock;
public:
    mutable QReadWriteLock m_registerInfosLock;
    QMap<QString, RegisterInfo> m_registerInfos;
};

#endif // NETMANAGER_H

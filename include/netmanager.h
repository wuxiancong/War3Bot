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

// NAT类型枚举 (保持不变)
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

struct RegisterInfo {
    QString clientId;
    QString username;
    QString localIp;
    quint64 localPort;
    QString publicIp;
    quint64 publicPort;
    quint32 sessionId;
    quint64 lastSeen;
    quint64 firstSeen;
    QString crcToken;
    quint32 natType;
    bool isRegistered;
};

class NetManager : public QObject
{
    Q_OBJECT

public:
    explicit NetManager(QObject *parent = nullptr);
    ~NetManager();

    bool startServer(quint64 port, const QString &configFile = "war3bot.ini");
    void stopServer();
    bool isRunning() const;
    QList<RegisterInfo> getOnlinePlayers() const;
    bool isClientRegistered(const QString &clientId) const;
    bool sendEnterRoomCommand(const QString &clientId, quint64 port);

signals:
    void serverStopped();
    void serverStarted(quint64 port);
    void commandReceived(const QString &userName, const QString &clientId, const QString &command, const QString &text);

private slots:
    void onUDPReadyRead();
    void onTcpReadyRead();
    void onCleanupTimeout();
    void onTcpDisconnected();
    void onBroadcastTimeout();
    void onNewTcpConnection();

private:
    // --- 核心网络处理 (二进制协议) ---
    void handleIncomingDatagram(const QNetworkDatagram &datagram);

    // 具体包处理器
    void handleRegister(const PacketHeader *header, const CSRegisterPacket *packet, const QHostAddress &senderAddr, quint64 senderPort);
    void handleUnregister(const PacketHeader *header);
    void handlePing(const PacketHeader *header, const QHostAddress &senderAddr, quint64 senderPort);
    void handleHeartbeat(const PacketHeader *header, const QHostAddress &senderAddr, quint64 senderPort);
    void handleCommand(const PacketHeader *header, const CSCommandPacket *packet);
    void handleCheckMapCRC(const PacketHeader *header, const CSCheckMapCRCPacket *packet, const QHostAddress &senderAddr, quint64 senderPort);

    // 发送辅助
    qint64 sendUdpPacket(const QHostAddress &target, quint64 port, PacketType type, const void *payload = nullptr, quint64 payloadLen = 0);
    void sendUploadResult(QTcpSocket *socket, const QString &crc, const QString &fileName, bool success, UploadErrorCode reason);
    bool sendTcpPacket(QTcpSocket *socket, PacketType type, const void *payload, quint64 payloadLen);
    bool sendToClient(const QString &clientId, const QByteArray &data);
    quint16 calculateCRC16(const QByteArray &data);

    // --- TCP 处理 (保持原样，用于文件上传) ---
    void handleTcpUploadMessage(QTcpSocket *socket);
    void handleTcpCommandMessage(QTcpSocket *socket);

    // --- 内部管理 ---
    void loadConfiguration();
    void cleanupResources();
    bool setupSocketOptions();
    bool bindSocket(quint64 port);
    bool isValidFileName(const QString &name);
    void setupTimers();
    void broadcastServerInfo();
    void updateMostFrequentCrc();
    void cleanupExpiredClients();
    void removeClientInternal(const QString &clientId);
    quint8 updateSessionState(quint32 sessionId, const QHostAddress &addr, quint64 port, bool *outIpChanged);

    // 工具
    QString cleanAddress(const QString &address);
    QString cleanAddress(const QHostAddress &address);
    QString packetTypeToString(PacketType type);
    QString natTypeToString(NATType type);

    // 配置
    quint64 m_peerTimeout;
    quint64 m_listenPort;
    int m_cleanupInterval;
    QSettings *m_settings;
    bool m_enableBroadcast;
    int m_broadcastInterval;
    quint64 m_broadcastPort;
    bool m_isRunning;

    // 资源
    QTimer *m_cleanupTimer;
    QTimer *m_broadcastTimer;
    QUdpSocket *m_udpSocket;
    QTcpServer *m_tcpServer;

    // 数据
    QMap<QString, QTcpSocket*> m_tcpClients;
    mutable QReadWriteLock m_registerInfosLock;
    QMap<QString, RegisterInfo> m_registerInfos;
    QMap<quint32, QString> m_sessionIndex;
    QMap<QString, int> m_crcCounts;

    // 上传安全
    QReadWriteLock m_tokenLock;
    QMap<QString, quint32> m_pendingUploadTokens;

    // Session 管理
    quint32 m_nextSessionId;
    quint32 m_serverSeq;

    // 安全检查
    SecurityWatchdog m_watchdog;
};

#endif // NETMANAGER_H

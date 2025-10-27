#ifndef GAMESESSION_H
#define GAMESESSION_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QMap>
#include <QHostAddress>
#include <QVector>
#include "w3gs_protocol.h"

class GameSession : public QObject {
    Q_OBJECT
public:
    explicit GameSession(const QString &sessionId, QObject *parent = nullptr);
    ~GameSession();

    bool startSession(const QHostAddress &hostAddr, quint16 hostPort);
    void stopSession();
    void addPlayer(const PlayerInfo &player);
    void removePlayer(uint8_t playerId, uint32_t reason = 0);

    QString sessionId() const { return m_sessionId; }
    bool isRunning() const { return m_isRunning; }
    QString getSessionInfo() const;

    // 获取玩家信息
    QVector<PlayerInfo> getPlayers() const;
    PlayerInfo getPlayer(uint8_t playerId) const;
    bool hasPlayer(uint8_t playerId) const;

signals:
    void sessionEnded(const QString &sessionId);
    void dataForwarded(const QByteArray &data, const QHostAddress &from);
    void playerJoined(const PlayerInfo &player);
    void playerLeft(uint8_t playerId, uint32_t reason);
    void chatMessage(uint8_t fromPlayer, uint8_t toPlayer, const QString &message);

public slots:
    void forwardData(const QByteArray &data, const QHostAddress &from, quint16 port);
    void sendToClient(const QByteArray &data, const QHostAddress &clientAddr, quint16 clientPort);
    void broadcastToClients(const QByteArray &data);

private slots:
    void onSocketReadyRead();
    void onPingTimeout();

private:
    QString m_sessionId;
    QUdpSocket *m_socket;
    QTimer *m_pingTimer;
    bool m_isRunning;

    QHostAddress m_hostAddress;
    quint16 m_hostPort;

    // 客户端地址映射 (playerId -> address/port)
    QMap<uint8_t, QPair<QHostAddress, quint16>> m_clientAddresses;

    QMap<uint8_t, PlayerInfo> m_players;
    W3GSProtocol m_protocol;

    // 数据包处理方法
    void processClientToServerPacket(const QByteArray &data, const W3GSHeader &header,
                                     const QHostAddress &from, quint16 port);
    void processServerToClientPacket(const QByteArray &data, const W3GSHeader &header);

    // 特定的数据包处理
    void handleSlotInfoJoin(const QByteArray &payload, const QHostAddress &from, quint16 port);
    void handleChatToHost(const QByteArray &payload);
    void handleLeaveGame(const QByteArray &payload, const QHostAddress &from, quint16 port);
    void handleIncomingAction(const QByteArray &payload);
    void handleMapCheck(const QByteArray &payload);

    void handlePlayerLeft(const QByteArray &payload);
    void handleChatFromHost(const QByteArray &payload);
    void handleSlotInfo(const QByteArray &payload);

    // 工具方法
    void updateClientAddress(uint8_t playerId, const QHostAddress &addr, quint16 port);
    QPair<QHostAddress, quint16> getClientAddress(uint8_t playerId) const;
    void sendToAllClients(const QByteArray &data, uint8_t excludePlayerId = 0xFF);

    // 保持兼容性
    void processW3GSPacket(const QByteArray &data, const QHostAddress &from, quint16 port);
    void sendToAll(const QByteArray &data, const QHostAddress &exclude = QHostAddress());
};

#endif // GAMESESSION_H

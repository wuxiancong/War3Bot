#ifndef P2PSESSION_H
#define P2PSESSION_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QTimer>
#include "stunclient.h"
#include "w3gs_protocol.h"

class P2PSession : public QObject
{
    Q_OBJECT

public:
    enum SessionState {
        StateInit,
        StateDiscovering,
        StateExchanging,
        StateHolePunching,
        StateConnected,
        StateFailed
    };

    explicit P2PSession(const QString &sessionId, QObject *parent = nullptr);
    ~P2PSession();

    bool startSession();
    void setPeerAddress(const QHostAddress &address, quint16 port);
    void sendW3GSData(const QByteArray &data);
    bool isConnected() const { return m_state == StateConnected; }
    QString getSessionId() const { return m_sessionId; }
    QHostAddress getPublicAddress() const { return m_publicAddress; }
    quint16 getPublicPort() const { return m_publicPort; }

    // 添加W3GS协议相关方法
    void setPlayerInfo(const PlayerInfo &info) { m_playerInfo = info; }
    PlayerInfo getPlayerInfo() const { return m_playerInfo; }

signals:
    void sessionConnected(const QString &sessionId);
    void sessionFailed(const QString &sessionId, const QString &error);
    void w3gsDataReceived(const QByteArray &data);
    void publicAddressReady(const QHostAddress &address, quint16 port);
    void playerInfoUpdated(const QString &sessionId, const PlayerInfo &info);

private slots:
    void onPublicAddressDiscovered(const QHostAddress &address, quint16 port);
    void onDiscoveryFailed(const QString &error);
    void onSocketReadyRead();
    void onHolePunchTimeout();
    void onKeepAliveTimeout();

private:
    QString m_sessionId;
    QUdpSocket *m_udpSocket;
    STUNClient *m_stunClient;
    W3GSProtocol *m_w3gsProtocol;
    SessionState m_state;

    QHostAddress m_publicAddress;
    quint16 m_publicPort;
    QHostAddress m_peerPublicAddress;
    quint16 m_peerPublicPort;

    PlayerInfo m_playerInfo;

    QTimer *m_holePunchTimer;
    QTimer *m_keepAliveTimer;
    int m_holePunchAttempts;

    void changeState(SessionState newState);
    void startHolePunching();
    void sendHolePunchPacket();
    void sendKeepAlive();
    bool isPrivateAddress(const QHostAddress &address);
    void processW3GSPacket(const QByteArray &data);
    void extractPlayerInfoFromPacket(const QByteArray &data);
};

#endif // P2PSESSION_H

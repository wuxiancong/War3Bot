#ifndef P2PSESSION_H
#define P2PSESSION_H

#include <QTimer>
#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include "w3gs_protocol.h"

class STUNClientManager;

class P2PSession : public QObject
{
    Q_OBJECT

public:
    enum SessionState {
        StateInit = 0,
        StateDiscovering,
        StateExchanging,
        StateHolePunching,
        StateConnected,
        StateFailed
    };

    explicit P2PSession(const QString &sessionId, QObject *parent = nullptr);
    ~P2PSession();

    bool startSession();
    void stopSession();

    void setPeerAddress(const QHostAddress &address, quint16 port);
    void sendW3GSData(const QByteArray &data);

    // Getters
    QString getSessionId() const { return m_sessionId; }
    SessionState getState() const { return m_state; }
    QHostAddress getPublicAddress() const { return m_publicAddress; }
    quint16 getPublicPort() const { return m_publicPort; }
    bool isConnected() const { return m_state == StateConnected; }

signals:
    void sessionConnected(const QString &sessionId);
    void sessionFailed(const QString &sessionId, const QString &error);
    void w3gsDataReceived(const QByteArray &data);
    void publicAddressReady(const QHostAddress &address, quint16 port);
    void playerInfoUpdated(const QString &sessionId, const PlayerInfo &info);

private slots:
    void onPublicAddressDiscovered(const QString &sessionId, const QHostAddress &address, quint16 port);
    void onDiscoveryFailed(const QString &sessionId, const QString &error);
    void onSocketReadyRead();
    void onHolePunchTimeout();
    void onKeepAliveTimeout();

private:
    void changeState(SessionState newState);
    void startHolePunching();
    void sendHolePunchPacket();
    void sendKeepAlive();
    void processW3GSPacket(const QByteArray &data);
    void extractPlayerInfoFromPacket(const QByteArray &data);
    bool checkSocketResources();

    QString m_sessionId;
    QUdpSocket *m_udpSocket;
    W3GSProtocol *m_w3gsProtocol;
    SessionState m_state;

    QHostAddress m_publicAddress;
    quint16 m_publicPort;

    QHostAddress m_peerPublicAddress;
    quint16 m_peerPublicPort;

    QTimer *m_holePunchTimer;
    QTimer *m_keepAliveTimer;
    int m_holePunchAttempts;
    PlayerInfo m_playerInfo;
};

#endif // P2PSESSION_H

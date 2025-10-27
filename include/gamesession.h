// gamesession.h
#ifndef GAMESESSION_H
#define GAMESESSION_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>

class GameSession : public QObject
{
    Q_OBJECT

public:
    explicit GameSession(const QString &sessionId, QObject *parent = nullptr);
    ~GameSession();

    bool startSession(const QHostAddress &target, quint16 port);
    bool reconnectToTarget(const QHostAddress &target, quint16 port);
    void forwardToGame(const QByteArray &data);
    void forwardToClient(const QByteArray &data);
    bool isRunning() const;
    QString getSessionId() const { return m_sessionId; }
    void setClientSocket(QTcpSocket *clientSocket);

signals:
    void sessionEnded(const QString &sessionId);
    void dataToClient(const QByteArray &data);

private slots:
    void onGameDataReady();
    void onGameConnected();
    void onGameDisconnected();
    void onGameError(QAbstractSocket::SocketError error);
    void onReconnectTimeout();

private:
    QString m_sessionId;
    QTcpSocket *m_gameSocket;
    QTcpSocket *m_clientSocket;
    QHostAddress m_targetAddress;
    quint16 m_targetPort;
    bool m_isConnected;
    QTimer *m_reconnectTimer;
    int m_reconnectAttempts;

    void cleanup();
    void setupGameSocket();
};

#endif // GAMESESSION_H

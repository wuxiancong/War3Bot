// war3bot.h
#ifndef WAR3BOT_H
#define WAR3BOT_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSettings>
#include <QMap>

class GameSession; // 前向声明

class War3Bot : public QObject
{
    Q_OBJECT

public:
    explicit War3Bot(QObject *parent = nullptr);
    ~War3Bot();

    bool startServer(quint16 port = 6113);
    void stopServer();
    QString getServerStatus() const;

private slots:
    void onNewConnection();
    void onClientDataReady();
    void onClientDisconnected();
    void onSessionEnded(const QString &sessionId);

private:
    QTcpServer *m_tcpServer;
    QSettings *m_settings;
    QMap<QString, GameSession*> m_sessions;
    QMap<QTcpSocket*, QString> m_clientSessions;
    bool isValidW3GSPacket(const QByteArray &data);

    QPair<QHostAddress, quint16> parseTargetFromPacket(const QByteArray &data, bool isFromClient = true);
    QPair<QHostAddress, quint16> parseReqJoinPacket(QDataStream &stream, int remainingSize);
    void analyzeUnknownPacket(const QByteArray &data, const QString &sessionKey);
    void processClientPacket(QTcpSocket *clientSocket, const QByteArray &data);
    QString generateSessionId();
};

#endif // WAR3BOT_H

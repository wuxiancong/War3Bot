#ifndef WAR3BOT_H
#define WAR3BOT_H

#include <QObject>
#include <QUdpSocket>
#include <QMap>
#include <QSettings>
#include <QHostAddress>
#include "gamesession.h"

class War3Bot : public QObject {
    Q_OBJECT
public:
    explicit War3Bot(QObject *parent = nullptr);
    ~War3Bot();

    bool startServer(quint16 port = 6113);
    void stopServer();
    QString getServerStatus() const;

private slots:
    void onReadyRead();
    void onSessionEnded(const QString &sessionId);

private:
    QUdpSocket *m_udpSocket;
    QMap<QString, GameSession*> m_sessions;
    QSettings *m_settings;

    QString generateSessionId();
    GameSession* findOrCreateSession(const QHostAddress &clientAddr, quint16 clientPort);
    void processInitialPacket(const QByteArray &data, const QHostAddress &from, quint16 port);

    // 新增的解析方法
    QPair<QHostAddress, quint16> parseTargetFromPacket(const QByteArray &data);
    QHostAddress extractRealTargetFromPacket(const QByteArray &data, const QHostAddress &from, quint16 port);
    quint16 extractRealPortFromPacket(const QByteArray &data);
};

#endif // WAR3BOT_H

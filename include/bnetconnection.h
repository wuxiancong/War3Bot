#ifndef BNETCONNECTION_H
#define BNETCONNECTION_H

#include <QTimer>
#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include "bncsutil/bncsutil.h"

class BnetConnection : public QObject
{
    Q_OBJECT

public:
    enum PacketID {
        SID_NULL                    = 0x00,
        SID_ENTERCHAT               = 0x0A,
        SID_CHATEVENT               = 0x0F,
        SID_STARTADVEX3             = 0x1C,
        SID_PING                    = 0x25,
        SID_AUTH_ACCOUNTLOGON       = 0x3A,
        SID_LOGONRESPONSE2          = 0x3A,
        SID_AUTH_ACCOUNTLOGONPROOF  = 0x3D,
        SID_AUTH_INFO               = 0x50,
        SID_AUTH_CHECK              = 0x51
    };

    static const quint8 BNET_HEADER = 0xFF;

    explicit BnetConnection(QObject *parent = nullptr);
    ~BnetConnection();

    void connectToHost(const QString &address, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;

    void setCredentials(const QString &user, const QString &pass);

    void login(const QString &username, const QString &password);
    void createGameOnLadder(const QString &gameName, const QByteArray &mapStatString, quint16 udpPort);

signals:
    void socketError(const QString &error);
    void authenticated();
    void gameListRegistered();

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();

private:
    void sendPacket(PacketID id, const QByteArray &payload);
    void handlePacket(PacketID id, const QByteArray &data);

    void sendAuthInfo();
    void sendLoginRequest();

    void handleAuthCheck(const QByteArray &data);
    void handleLoginResponse(const QByteArray &data);

private:
    QString m_war3ExePath;
    QTcpSocket *m_socket;
    QString m_serverAddr;
    quint16 m_serverPort;
    QString m_user;
    QString m_pass;
    NLS *m_nls;

};

#endif // BNETCONNECTION_H

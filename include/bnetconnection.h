#ifndef BNETCONNECTION_H
#define BNETCONNECTION_H

#include <QTimer>
#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include "bncsutil/nls.h"

class BnetConnection : public QObject
{
    Q_OBJECT

public:
    enum PacketID {
        SID_NULL                    = 0x00,
        SID_ENTERCHAT               = 0x0A,
        SID_CHATEVENT               = 0x0F,
        SID_STARTADVEX3             = 0x1C, // 创建房间
        SID_PING                    = 0x25,
        SID_LOGONRESPONSE           = 0x29, // 登录响应
        SID_LOGONRESPONSE2          = 0x3A, // 登录请求(C->S) & 密钥交换(S->C)
        SID_REQUIREDWORK            = 0x4C, // 需要下载更新
        SID_AUTH_INFO               = 0x50, // 版本信息
        SID_AUTH_CHECK              = 0x51, // 版本检查
        SID_AUTH_ACCOUNTLOGON       = 0x53, // 客户端发送密码证明 (Client -> Server)
        SID_AUTH_ACCOUNTLOGONPROOF  = 0x54  // 服务器返回登录结果 (Server -> Client)
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

    QString getPrimaryIPv4();
    quint32 ipToUint32(const QString &ipAddress);
    quint32 ipToUint32(const QHostAddress &address);
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

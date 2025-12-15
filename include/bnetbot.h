#ifndef BNETBOT_H
#define BNETBOT_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>
#include "bncsutil/nls.h"

class BnetBot : public QObject
{
    Q_OBJECT

public:
    // 定义协议类型枚举
    enum LoginProtocol {
        Protocol_Old_0x29   = 0x29,  // 旧版协议 (DoubleHash BrokenSHA1)
        Protocol_Logon2_0x3A= 0x3A,  // 中期协议 (同 0x29 算法，ID 不同)
        Protocol_SRP_0x53   = 0x53   // 新版协议 (SRP NLS)
    };

    enum PacketID {
        SID_NULL                    = 0x00,
        SID_ENTERCHAT               = 0x0A,
        SID_STARTADVEX3             = 0x1C,
        SID_PING                    = 0x25,
        SID_LOGONRESPONSE           = 0x29, // 0x29 响应
        SID_LOGONRESPONSE2          = 0x3A, // 0x3A 响应
        SID_AUTH_INFO               = 0x50,
        SID_AUTH_CHECK              = 0x51,
        SID_AUTH_ACCOUNTLOGON       = 0x53, // SRP 步骤1 响应
        SID_AUTH_ACCOUNTLOGONPROOF  = 0x54  // SRP 步骤2 响应
    };

    static const quint8 BNET_HEADER = 0xFF;

    explicit BnetBot(QObject *parent = nullptr);
    ~BnetBot();

    void connectToHost(const QString &address, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;

    // 设置登录凭据和协议
    void setCredentials(const QString &user, const QString &pass, LoginProtocol protocol = Protocol_SRP_0x53);

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
    void handleAuthCheck(const QByteArray &data);

    // 登录流程控制
    void sendLoginRequest(LoginProtocol protocol);

    // 0x53 SRP 处理
    void handleSRPLoginResponse(const QByteArray &data);

    // 哈希算法 (Broken SHA1)
    static QByteArray calculateBrokenSHA1(const QByteArray &data);
    // Double Hash Proof 计算 (用于 0x29 和 0x3A)
    QByteArray calculateOldLogonProof(const QString &password, quint32 clientToken, quint32 serverToken);

private:
    LoginProtocol m_loginProtocol;
    quint32 m_serverToken = 0;
    quint32 m_clientToken = 0;
    quint32 m_logonType = 0;

    QString m_stormDllPath;
    QString m_gameDllPath;
    QString m_war3ExePath;
    QTcpSocket *m_socket;
    QString m_serverAddr;
    quint16 m_serverPort;

    QString m_user;
    QString m_pass;
    NLS *m_nls; // bncsutil NLS 对象
};

#endif // BNETBOT_H

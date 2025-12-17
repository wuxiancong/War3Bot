#ifndef CLIENT_H
#define CLIENT_H

#include <QTimer>
#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include "war3map.h"

// 添加前置声明
class BnetSRP3;

class Client : public QObject
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
        SID_GETCHANNELLIST          = 0x0B,
        SID_JOINCHANNEL             = 0x0C,
        SID_CHATEVENT               = 0x0F,
        SID_STARTADVEX3             = 0x1C,
        SID_PING                    = 0x25,
        SID_LOGONRESPONSE           = 0x29, // 0x29 响应
        SID_LOGONRESPONSE2          = 0x3A, // 0x3A 响应
        SID_AUTH_INFO               = 0x50,
        SID_AUTH_CHECK              = 0x51,
        SID_AUTH_ACCOUNTCREATE      = 0x52, // 注册账号
        SID_AUTH_ACCOUNTLOGON       = 0x53, // SRP 步骤1 响应
        SID_AUTH_ACCOUNTLOGONPROOF  = 0x54  // SRP 步骤2 响应
    };

    // 游戏类型枚举
    enum GameType {
        GameType_Melee              = 0x02,
        GameType_FFA                = 0x03,
        GameType_1v1                = 0x04,
        GameType_UMS                = 0x0A  // 自定义地图/DOTA常用
    };

    // 游戏状态标志
    enum GameState {
        GameState_Private           = 0x10, // 私有
        GameState_Public            = 0x11  // 公开
    };

    static const quint8 BNET_HEADER = 0xFF;

    explicit Client(QObject *parent = nullptr);
    ~Client();

    void connectToHost(const QString &address, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;

    // 设置登录凭据和协议
    void setCredentials(const QString &user, const QString &pass, LoginProtocol protocol = Protocol_SRP_0x53);

    // 注册账号
    void createAccount();

    // 频道列表
    void queryChannelList();

    // 加入频道
    void joinChannel(const QString &channelName);

    // 创建游戏
    void createGameOnLadder(const QString &gameName, const QString &password,quint16 udpPort, GameType gameType);

    QString getPrimaryIPv4();
    quint32 ipToUint32(const QString &ipAddress);
    quint32 ipToUint32(const QHostAddress &address);

signals:
    void socketError(const QString &error);
    void authenticated();
    void gameListRegistered();
    void accountCreated();

public slots:
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
    QStringList m_channelList;
    quint32 m_serverToken = 0;
    quint32 m_clientToken = 0;
    quint32 m_logonType = 0;

    QString m_stormDllPath;
    QString m_gameDllPath;
    QString m_war3ExePath;
    QTcpSocket *m_socket;
    QString m_serverAddr;
    quint16 m_serverPort;
    QString m_dota683dPath;

    QString m_user;
    QString m_pass;
    BnetSRP3 *m_srp;
    War3Map m_war3Map;
};

#endif // CLIENT_H

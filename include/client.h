#ifndef CLIENT_H
#define CLIENT_H

#include <QTimer>
#include <QObject>
#include <QTcpSocket>
#include <QUdpSocket>
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

    enum TCPPacketID {
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

    enum UdpPacketID {
        // 主机 <-> 客户端 心跳
        W3GS_PING_FROM_HOST         = 0x01, // S>C: 主机 Ping 客户端 (30秒一次)
        W3GS_PONG_TO_HOST           = 0x46, // C>S: 客户端回复主机 (响应 0x01)

        // 局域网 / 搜房
        W3GS_REQJOIN                = 0x1E, // C>S: 请求加入房间
        W3GS_SEARCHGAME             = 0x2F, // C>S: 局域网搜房请求
        W3GS_GAMEINFO               = 0x30, // S>C: 局域网房间广播 (UDP broadcast)
        W3GS_REFRESHGAME            = 0x32, // S>C: 房间刷新 (Slot info)
        W3GS_DECREATEGAME           = 0x33, // S>C: 房间关闭

        // P2P 测速 (决定房间在列表里的延迟显示)
        W3GS_PING_FROM_OTHERS       = 0x35, // P2P: 别人 Ping 我 (10秒一次)
        W3GS_PONG_TO_OTHERS         = 0x36, // P2P: 我回复别人 (响应 0x35)

        // 游戏内
        W3GS_INCOMING_ACTION        = 0x0C, // 游戏动作
        W3GS_MAPCHECK               = 0x3D  // 地图检查
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
    void onConnected();
    void onDisconnected();
    void onTcpReadyRead();
    void onUdpReadyRead();

private:
    void sendPacket(TCPPacketID id, const QByteArray &payload);
    void handleTcpPacket(TCPPacketID id, const QByteArray &data);
    void handleUdpPacket(const QByteArray &data, const QHostAddress &sender, quint16 senderPort);

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
    QString m_serverAddr;
    quint16 m_serverPort;
    QString m_dota683dPath;
    QTcpSocket *m_tcpSocket;
    QUdpSocket *m_udpSocket;

    QString m_user;
    QString m_pass;
    BnetSRP3 *m_srp;
    War3Map m_war3Map;
};

#endif // CLIENT_H

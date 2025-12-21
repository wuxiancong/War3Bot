#ifndef CLIENT_H
#define CLIENT_H

#include <QTimer>
#include <QObject>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QHostAddress>
#include <QSet>
#include "war3map.h"

// =========================================================
// 协议与枚举定义
// =========================================================

// 登录协议类型
enum LoginProtocol {
    Protocol_Old_0x29   = 0x29,  /*旧版协议 (DoubleHash BrokenSHA1)*/
    Protocol_Logon2_0x3A= 0x3A,  /*中期协议 (同 0x29 算法，ID 不同)*/
    Protocol_SRP_0x53   = 0x53   /*新版协议 (SRP NLS)*/
};

// TCP 协议 ID
enum TCPPacketID {
    SID_NULL                    = 0x00,
    SID_STOPADV                 = 0x02,
    SID_ENTERCHAT               = 0x0A,
    SID_GETCHANNELLIST          = 0x0B,
    SID_JOINCHANNEL             = 0x0C,
    SID_CHATEVENT               = 0x0F,
    SID_STARTADVEX3             = 0x1C,
    SID_PING                    = 0x25,
    SID_LOGONRESPONSE           = 0x29,
    SID_LOGONRESPONSE2          = 0x3A,
    SID_NETGAMEPORT             = 0x45,
    SID_AUTH_INFO               = 0x50,
    SID_AUTH_CHECK              = 0x51,
    SID_AUTH_ACCOUNTCREATE      = 0x52,
    SID_AUTH_ACCOUNTLOGON       = 0x53,
    SID_AUTH_ACCOUNTLOGONPROOF  = 0x54
};

// UDP 协议 ID
enum UdpPacketID {
    W3GS_PING_FROM_HOST         = 0x01,
    W3GS_INCOMING_ACTION        = 0x0C,
    W3GS_REQJOIN                = 0x1E,
    W3GS_SEARCHGAME             = 0x2F,
    W3GS_GAMEINFO               = 0x30,
    W3GS_REFRESHGAME            = 0x32,
    W3GS_DECREATEGAME           = 0x33,
    W3GS_PING_FROM_OTHERS       = 0x35,
    W3GS_PONG_TO_OTHERS         = 0x36,
    W3GS_MAPCHECK               = 0x3D,
    W3GS_PONG_TO_HOST           = 0x46
};

// 游戏类型枚举
enum BaseGameType {
    Type_All                    = 0x00,
    Type_Melee                  = 0x02,
    Type_FFA                    = 0x03,
    Type_1v1                    = 0x04,
    Type_CTF                    = 0x05,
    Type_Greed                  = 0x06,
    Type_Slaughter              = 0x07,
    Type_SuddenDeath            = 0x08,
    Type_Ladder                 = 0x09,
    Type_IronManLadder          = 0x10,
    Type_UMS                    = 0x0A,
    Type_TeamMelee              = 0x0B,
    Type_TeamFFA                = 0x0C,
    Type_TeamCTF                = 0x0D,
    Type_TopVsBottom            = 0x0F,
    Type_PGL                    = 0x20
};

enum GameTypeFlags {
    Flag_None                   = 0x0000,
    Flag_Expansion              = 0x2000,
    Flag_FixedTeams             = 0x4000,
    Flag_Official               = 0x8000
};

enum ComboGameType {
    Game_TFT_Custom             = 0x0001,
    Game_TFT_Official           = 0xC009,
    Game_RoC_Custom             = 0x0001
};

enum SubGameType {
    SubType_None                = 0x00,
    SubType_Blizzard            = 0x42,
    SubType_Scenario            = 0x43,
    SubType_Internet            = 0x49,
    SubType_Load                = 0x4C,
    SubType_Save                = 0x53
};

enum GameState {
    State_None                  = 0x00,
    State_Private               = 0x01,
    State_Full                  = 0x02,
    State_HasOtherPlayers       = 0x04,
    State_InProgress            = 0x08,
    State_DisconnectIsLoss      = 0x10,
    State_Replay                = 0x80,
    State_Open_Public = State_HasOtherPlayers,
    State_Open_Private = State_Private | State_HasOtherPlayers,
    State_Ladder_InProgress = State_InProgress | State_DisconnectIsLoss,
    State_Lobby_Full = State_Full | State_HasOtherPlayers,
    State_ActiveGame = State_InProgress | State_HasOtherPlayers
};

enum ProviderVersion {
    Provider_All_Bits           = 0xFFFFFFFF,
    Provider_RoC                = 0x00000000,
    Provider_SC_BW              = 0x000000FF,
    Provider_TFT_New            = 0x000003FF
};

enum LadderType {
    Ladder_None                 = 0x00000000,
    Ladder_Standard             = 0x00000001,
    Ladder_IronMan              = 0x00000003
};

// 前置声明
class BnetSRP3;

// =========================================================
// Client 类定义
// =========================================================

class Client : public QObject
{
    Q_OBJECT

public:
    static const quint8 BNET_HEADER = 0xFF;

    explicit Client(QObject *parent = nullptr);
    ~Client();

    // --- 连接控制 ---
    void connectToHost(const QString &address, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;
    void setCredentials(const QString &user, const QString &pass, LoginProtocol protocol = Protocol_SRP_0x53);

    // --- 战网交互指令 ---
    void createAccount();                           // 注册账号
    void enterChat();                               // 进入聊天
    void queryChannelList();                        // 请求频道列表
    void joinChannel(const QString &channelName);   // 加入频道

    // --- 游戏主机指令 ---
    void createGame(const QString &gameName, const QString &password,
                    ProviderVersion providerVersion, ComboGameType comboGameType,
                    SubGameType subGameType, LadderType ladderType);
    void cancelGame();                              // 取消游戏/解散
    void stopAdv();                                 // 停止广播

    // --- 机器人设置 ---
    void setHostCounter(int id);

    // --- 工具与状态 ---
    QString getPrimaryIPv4();
    bool bindToRandomPort();                        // 绑定 UDP 随机端口
    bool isBlackListedPort(quint16 port);           // 端口黑名单检查

    // IP 转换辅助
    quint32 ipToUint32(const QString &ipAddress);
    quint32 ipToUint32(const QHostAddress &address);

signals:
    void socketError(const QString &error);
    void authenticated();
    void accountCreated();
    void gameListRegistered();

private slots:
    // --- 网络事件槽 ---
    void onConnected();
    void onDisconnected();
    void onTcpReadyRead();
    void onUdpReadyRead();

private:
    // --- 内部网络处理 ---
    void sendPacket(TCPPacketID id, const QByteArray &payload);
    void handleTcpPacket(TCPPacketID id, const QByteArray &data);
    void handleUdpPacket(const QByteArray &data, const QHostAddress &sender, quint16 senderPort);

    // --- 认证与登录流程 ---
    void sendAuthInfo();
    void handleAuthCheck(const QByteArray &data);
    void sendLoginRequest(LoginProtocol protocol);

    // SRP (0x53) 特定处理
    void handleSRPLoginResponse(const QByteArray &data);

    // DoubleHash (0x29/0x3A) 特定处理
    QByteArray calculateOldLogonProof(const QString &password, quint32 clientToken, quint32 serverToken);
    static QByteArray calculateBrokenSHA1(const QByteArray &data);

private:
    // --- 成员变量 ---

    // 认证相关
    BnetSRP3 *m_srp;

    // 网络相关
    QTcpSocket *m_tcpSocket;
    QUdpSocket *m_udpSocket;
    QString m_serverAddr;
    quint16 m_serverPort;

    // 游戏/环境相关
    War3Map m_war3Map;
    QStringList m_channelList;
    quint32 m_hostCounter = 1;
    quint32 m_counterBase = 0;
    quint32 m_counterLimit = 0;
    const quint32 ID_RANGE = 1000;

    // 路径
    QString m_war3ExePath;
    QString m_stormDllPath;
    QString m_gameDllPath;
    QString m_dota683dPath;

    // 登录相关
    QString m_user;
    QString m_pass;
    quint32 m_serverToken = 0;
    quint32 m_clientToken = 0;
    quint32 m_logonType = 0;
    LoginProtocol m_loginProtocol;
};

#endif // CLIENT_H

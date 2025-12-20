#ifndef CLIENT_H
#define CLIENT_H

#include <QTimer>
#include <QObject>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QHostAddress>
#include "war3map.h"

// 定义协议类型枚举
enum LoginProtocol {
    Protocol_Old_0x29   = 0x29,  // 旧版协议 (DoubleHash BrokenSHA1)
    Protocol_Logon2_0x3A= 0x3A,  // 中期协议 (同 0x29 算法，ID 不同)
    Protocol_SRP_0x53   = 0x53   // 新版协议 (SRP NLS)
};

enum TCPPacketID {
    SID_NULL                    = 0x00,
    SID_STOPADV                 = 0x02,
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

// 1. 基础游戏类型
enum BaseGameType {
    Type_Unknown            = 0x00,     // 未知/默认
    Type_D2_ClosedBN        = 0x00,     // 暗黑2 关闭战网
    Type_D2_OpenBN          = 0x04,     // 暗黑2 开放战网
    Type_Melee              = 0x02,     // 普通近战 (非天梯)
    Type_FFA                = 0x03,     // 混战 (Free for All)
    Type_1v1                = 0x04,     // 1v1
    Type_CTF                = 0x05,     // 夺旗 (Capture The Flag)
    Type_Greed              = 0x06,     // 贪婪模式 (收集资源)
    Type_Slaughter          = 0x07,     // 屠杀模式 (限时)
    Type_SuddenDeath        = 0x08,     // 突然死亡
    Type_Ladder             = 0x09,     // 天梯模式
    Type_IronManLadder      = 0x10,     // 铁人天梯 (W2BN专用)
    Type_UMS                = 0x0A,     // Use Map Settings (自定义地图设置)
    Type_TeamMelee          = 0x0B,     // 团队近战
    Type_TeamFFA            = 0x0C,     // 团队混战
    Type_TeamCTF            = 0x0D,     // 团队夺旗
    Type_TopVsBottom        = 0x0F,     // Top vs Bottom
    Type_PGL                = 0x20      // PGL (Professional Gaming League)
};

// 2. 游戏版本/特性标志
enum GameTypeFlags {
    Flag_None           = 0x0000,
    Flag_Expansion      = 0x2000,   // 冰封王座 (W3XP) 标志，所有 TFT 游戏必须带
    Flag_FixedTeams     = 0x4000,   // 固定队伍
    Flag_Official       = 0x8000    // 官方/暴雪认证 (通常用于 0x09 天梯图)
};

// 3. 组合后的常用游戏类型
enum ComboGameType {
    Game_TFT_Custom     = 0x2001,   // 最常用的自定义图 (DotA)
    Game_TFT_Official   = 0xC009,   // 官方对战图 (Booty Bay 等)
    Game_RoC_Custom     = 0x0001    // 混乱之治 (RoC) 时代的老图 (现在很少见)
};

// 4. 子游戏类型 (SubGameType / Option) - 决定地图来源目录
enum SubGameType {
    SubType_None        = 0x00,     // 局域网/未知
    SubType_Blizzard    = 0x42,     // 'B' - Maps/FrozenThrone (官方对战)
    SubType_Scenario    = 0x43,     // 'C' - Maps/Scenario (自带娱乐/RPG)
    SubType_Internet    = 0x49,     // 'I' - Maps/Download (玩家下载的图，如 DotA)
    SubType_Load        = 0x4C,     // 'L' - 加载游戏
    SubType_Save        = 0x53      // 'S' - 存档
};

// 5. 游戏状态 (Game State)
enum GameState {
    // 基础状态（使用位掩码）
    State_None              = 0x00,
    State_Private           = 0x01,     // 游戏是私有的
    State_Full              = 0x02,     // 游戏已满
    State_HasOtherPlayers   = 0x04,     // 包含除创建者外的其他玩家
    State_InProgress        = 0x08,     // 游戏进行中
    State_DisconnectIsLoss  = 0x10,     // 掉线算作失败
    State_Replay            = 0x80,     // 游戏是回放

    // 组合状态（常用场景）
    State_Open_Public       = State_HasOtherPlayers,                        // 公开游戏（有玩家加入）
    State_Open_Private      = State_Private | State_HasOtherPlayers,        // 私有游戏（有玩家加入）
    State_Ladder_InProgress = State_InProgress | State_DisconnectIsLoss,    // 天梯进行中
    State_Lobby_Full        = State_Full | State_HasOtherPlayers,           // 大厅已满（有玩家但未开始）
    State_ActiveGame        = State_InProgress | State_HasOtherPlayers      // 活跃游戏（有玩家且进行中）
};

// 6. 提供商版本 (Provider Version)
enum ProviderVersion {
    Provider_All_Bits   = 0xFFFFFFFF,
    Provider_RoC        = 0x00000000,   // 混乱之治 / 暗黑2
    Provider_SC_BW      = 0x000000FF,   // 星际争霸 / 早期 War3 (1.23及以下)
    Provider_TFT_New    = 0x000003FF    // 现代 War3 (1.24+, 1.27, 网易平台等)
};

// 7. 天梯类型 (Ladder Type)
enum LadderType {
    Ladder_None         = 0x00000000,   // 非天梯 / 标准游戏 (War3 自建房必须用这个)
    Ladder_Standard     = 0x00000001,   // 标准天梯 (Ladder) 客户端忽略 StatString，尝试请求天梯积分/排名。
    Ladder_IronMan      = 0x00000003    // 铁人天梯 (Iron Man Ladder) 主要用于《魔兽争霸 II》战网版，War3 中极少使用，某些私服可能用于特殊模式。
};
// 添加前置声明
class BnetSRP3;

class Client : public QObject
{
    Q_OBJECT

public:
    static const quint8 BNET_HEADER = 0xFF;

    explicit Client(QObject *parent = nullptr);
    ~Client();

    void sendPacket(TCPPacketID id, const QByteArray &payload);
    void connectToHost(const QString &address, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;

    // 设置登录凭据和协议
    void setCredentials(const QString &user, const QString &pass, LoginProtocol protocol = Protocol_SRP_0x53);

    // 停止广播/取消房间
    void stopGame();

    // 进入聊天
    void enterChat();

    // 注册账号
    void createAccount();

    // 频道列表
    void queryChannelList();

    // 加入频道
    void joinChannel(const QString &channelName);

    // 创建游戏
    void createGame(const QString &gameName, const QString &password,quint16 udpPort,
                    ProviderVersion providerVersion, ComboGameType comboGameType, SubGameType subGameType, LadderType ladderType);

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
    quint32 m_hostCounter = 1;

    QString m_user;
    QString m_pass;
    BnetSRP3 *m_srp;
    War3Map m_war3Map;
};

#endif // CLIENT_H

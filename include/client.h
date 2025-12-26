#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QHostAddress>
#include <QSet>
#include <QDataStream> // 建议添加，因为成员函数中经常用到流操作
#include "war3map.h"

// =========================================================
// 协议与枚举定义
// =========================================================

// 登录协议类型
enum LoginProtocol {
    Protocol_Old_0x29    = 0x29, /* 旧版协议 (DoubleHash BrokenSHA1) */
    Protocol_Logon2_0x3A = 0x3A, /* 中期协议 (同 0x29 算法，ID 不同) */
    Protocol_SRP_0x53    = 0x53  /* 新版协议 (SRP NLS) */
};

// TCP 协议 ID (BNET)
enum TCPPacketID {
    SID_NULL                   = 0x00,
    SID_STOPADV                = 0x02,
    SID_ENTERCHAT              = 0x0A,
    SID_GETCHANNELLIST         = 0x0B,
    SID_JOINCHANNEL            = 0x0C,
    SID_CHATEVENT              = 0x0F,
    SID_STARTADVEX3            = 0x1C,
    SID_PING                   = 0x25,
    SID_LOGONRESPONSE          = 0x29,
    SID_LOGONRESPONSE2         = 0x3A,
    SID_NETGAMEPORT            = 0x45,
    SID_AUTH_INFO              = 0x50,
    SID_AUTH_CHECK             = 0x51,
    SID_AUTH_ACCOUNTCREATE     = 0x52,
    SID_AUTH_ACCOUNTLOGON      = 0x53,
    SID_AUTH_ACCOUNTLOGONPROOF = 0x54
};

// UDP 协议 ID (W3GS / LAN)
enum UdpPacketID {
    W3GS_PING_FROM_HOST   = 0x01,
    W3GS_INCOMING_ACTION  = 0x0C,
    W3GS_REQJOIN          = 0x1E,
    W3GS_SEARCHGAME       = 0x2F,
    W3GS_GAMEINFO         = 0x30,
    W3GS_REFRESHGAME      = 0x32,
    W3GS_DECREATEGAME     = 0x33,
    W3GS_PING_FROM_OTHERS = 0x35,
    W3GS_PONG_TO_OTHERS   = 0x36,
    W3GS_MAPCHECK         = 0x3D,
    W3GS_PONG_TO_HOST     = 0x46
};

// 游戏类型枚举
enum BaseGameType {
    Type_All           = 0x00,
    Type_Melee         = 0x02,
    Type_FFA           = 0x03,
    Type_1v1           = 0x04,
    Type_CTF           = 0x05,
    Type_Greed         = 0x06,
    Type_Slaughter     = 0x07,
    Type_SuddenDeath   = 0x08,
    Type_Ladder        = 0x09,
    Type_IronManLadder = 0x10,
    Type_UMS           = 0x0A,
    Type_TeamMelee     = 0x0B,
    Type_TeamFFA       = 0x0C,
    Type_TeamCTF       = 0x0D,
    Type_TopVsBottom   = 0x0F,
    Type_PGL           = 0x20
};

enum GameTypeFlags {
    Flag_None       = 0x0000,
    Flag_Expansion  = 0x2000,
    Flag_FixedTeams = 0x4000,
    Flag_Official   = 0x8000
};

enum ComboGameType {
    Game_TFT_Custom   = 0x2001,
    Game_TFT_Official = 0xC009,
    Game_RoC_Custom   = 0x0001
};

enum SubGameType {
    SubType_None     = 0x00,
    SubType_Blizzard = 0x42,
    SubType_Scenario = 0x43,
    SubType_Internet = 0x49,
    SubType_Load     = 0x4C,
    SubType_Save     = 0x53
};

enum GameState {
    State_None              = 0x00,
    State_Private           = 0x01,
    State_Full              = 0x02,
    State_HasOtherPlayers   = 0x04,
    State_InProgress        = 0x08,
    State_DisconnectIsLoss  = 0x10,
    State_Replay            = 0x80,
    State_Open_Public       = State_HasOtherPlayers,
    State_Open_Private      = State_Private | State_HasOtherPlayers,
    State_Ladder_InProgress = State_InProgress | State_DisconnectIsLoss,
    State_Lobby_Full        = State_Full | State_HasOtherPlayers,
    State_ActiveGame        = State_InProgress | State_HasOtherPlayers
};

enum ProviderVersion {
    Provider_All_Bits = 0xFFFFFFFF,
    Provider_RoC      = 0x00000000,
    Provider_SC_BW    = 0x000000FF,
    Provider_TFT_New  = 0x000003FF
};

enum LadderType {
    Ladder_None     = 0x00000000,
    Ladder_Standard = 0x00000001,
    Ladder_IronMan  = 0x00000003
};

enum class SlotRace : quint8 {
    Human    = 0x01,
    Orc      = 0x02,
    NightElf = 0x04,
    Undead   = 0x08,
    Random   = 0x20,
    Fixed    = 0x40,

    // DotA 专用别名
    Sentinel = NightElf,
    Scourge  = Undead,
    Observer = Random
};

enum class SlotTeam : quint8 {
    Sentinel = 0,
    Scourge  = 1,
    Observer = 2
};

enum class SlotStatus : quint8 {
    Open     = 0,
    Close    = 1,
    Occupied = 2
};

struct GameSlot {
    quint8 pid            = 0;
    quint8 downloadStatus = 0;
    quint8 slotStatus     = 0;
    quint8 computer       = 0;
    quint8 team           = 0;
    quint8 color          = 0;
    quint8 race           = 32;
    quint8 computerType   = 0;
    quint8 handicap       = 100;
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
    void createAccount();                          // 注册账号
    void enterChat();                              // 进入聊天
    void queryChannelList();                       // 请求频道列表
    void joinChannel(const QString &channelName);  // 加入频道

    // --- 游戏主机指令 ---
    void createGame(const QString &gameName, const QString &password,
                    ProviderVersion providerVersion, ComboGameType comboGameType,
                    SubGameType subGameType, LadderType ladderType);
    void cancelGame();                             // 取消游戏/解散
    void stopAdv();                                // 停止广播

    // --- 工具与状态 ---
    void sendPingLoop();
    QString getPrimaryIPv4();
    bool bindToRandomPort();                       // 绑定 UDP 随机端口
    bool isBlackListedPort(quint16 port);          // 端口黑名单检查

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
    void onNewConnection();      // TCP Server 有新连接 (玩家加入)
    void onPlayerReadyRead();    // 玩家 Socket 有数据
    void onPlayerDisconnected(); // 玩家断开

private:
    // --- 游戏槽位处理 ---
    void initSlots(quint8 maxPlayers = 10);
    QByteArray serializeSlotData();
    GameSlot *findEmptySlot();
    void broadcastSlotInfo();

    // --- W3GS 协议包生成辅助函数 ---

    /**
     * @brief 生成 0x09 (SlotInfo) 数据包
     * 包含: Header + SlotData + RandomSeed + GameType + NumSlots
     * 用于握手最后一步，或房间信息变更时广播
     */
    QByteArray createW3GSSlotInfoPacket();

    /**
     * @brief 生成 0x3D (Map Check) 数据包
     * 包含: 地图路径、大小、CRC 和 **修复后的20字节SHA1**
     */
    QByteArray createW3GSMapCheckPacket();

    /**
     * @brief 生成 0x04 (SlotInfoJoin) 数据包
     * 用于告知刚加入的玩家当前的槽位信息、他的 PID 以及房间的网络信息
     */
    QByteArray createW3GSSlotInfoJoinPacket(quint8 playerID, const QHostAddress& externalIp, quint16 localPort);

    /**
     * @brief 生成 0x06 (PlayerInfo) 数据包
     * 用于向客户端发送房间内某位玩家(通常是房主)的详细信息
     */
    QByteArray createPlayerInfoPacket(quint8 pid, const QString& name,
                                      const QHostAddress& externalIp, quint16 externalPort,
                                      const QHostAddress& internalIp, quint16 internalPort);

    /**
     * @brief 将 IP 转为网络字节序写入流，并打印 Hex 字节供调试
     * @param out 目标数据流 (必须已设置为 LittleEndian)
     * @param ip 要写入的 IP 地址
     */
    void writeIpToStreamWithLog(QDataStream &out, const QHostAddress &ip);

    // --- 内部网络处理 ---
    void sendPacket(TCPPacketID id, const QByteArray &payload);
    void handleTcpPacket(TCPPacketID id, const QByteArray &data);
    void handleUdpPacket(const QByteArray &data, const QHostAddress &sender, quint16 senderPort);

    // 处理玩家发来的 W3GS 包 (0x1E ReqJoin 等)
    void handleW3GSPacket(QTcpSocket *socket, quint8 id, const QByteArray &payload);

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
    QString m_serverAddr;
    quint16 m_serverPort;
    QUdpSocket *m_udpSocket;
    QTcpSocket *m_tcpSocket;                        // 连接战网的 TCP
    QTcpServer *m_tcpServer;                        // 监听玩家的 TCP Server

    QList<QTcpSocket*> m_playerSockets;             // 已连接的玩家列表
    QMap<QTcpSocket*, QByteArray> m_playerBuffers;  // 玩家粘包处理缓冲区

    // 游戏/环境相关
    War3Map m_war3Map;
    quint32 m_randomSeed = 0;
    quint32 m_hostCounter = 1;
    QVector<GameSlot> m_slots;
    QStringList m_channelList;
    quint16 m_comboGameType = 3;

    // 路径配置
    QString m_war3ExePath;
    QString m_stormDllPath;
    QString m_gameDllPath;
    QString m_dota683dPath;

    // 登录相关状态
    QString m_user;
    QString m_pass;
    quint32 m_serverToken = 0;
    quint32 m_clientToken = 0;
    quint32 m_logonType = 0;
    LoginProtocol m_loginProtocol;
};

#endif // CLIENT_H

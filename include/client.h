#ifndef CLIENT_H
#define CLIENT_H

#include <QSet>
#include <QTimer>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QDataStream>
#include <QHostAddress>

#include "netmanager.h"
#include "war3map.h"
#include "command.h"

// =========================================================
// 1. 网络协议与认证 (Protocol & Auth)
// =========================================================

// 登录协议版本
enum LoginProtocol {
    Protocol_Old_0x29           = 0x29, // 旧版 (DoubleHash)
    Protocol_Logon2_0x3A        = 0x3A, // 中期 (同0x29算法)
    Protocol_SRP_0x53           = 0x53  // 新版 (SRP NLS)
};

// TCP 协议 ID (BNET)
enum BNETPacketID {
    SID_NULL                    = 0x00, // [C->S] 空包
    SID_STOPADV                 = 0x02, // [C->S] 停止广播
    SID_ENTERCHAT               = 0x0A, // [C->S] 进入聊天
    SID_GETCHANNELLIST          = 0x0B, // [C->S] 获取频道列表
    SID_JOINCHANNEL             = 0x0C, // [C->S] 加入频道
    SID_CHATCOMMAND             = 0x0E, // [C->S] 聊天命令
    SID_CHATEVENT               = 0x0F, // [C->S] 聊天事件
    SID_STARTADVEX3             = 0x1C, // [C->S] 创建房间(TFT)
    SID_PING                    = 0x25, // [C->S] 心跳包
    SID_LOGONRESPONSE           = 0x29, // [C->S] 登录响应(旧)
    SID_LOGONRESPONSE2          = 0x3A, // [C->S] 登录响应(中)
    SID_NETGAMEPORT             = 0x45, // [C->S] 游戏端口通知
    SID_AUTH_INFO               = 0x50, // [C->S] 认证信息
    SID_AUTH_CHECK              = 0x51, // [C->S] 版本检查
    SID_AUTH_ACCOUNTCREATE      = 0x52, // [C->S] 账号创建
    SID_AUTH_ACCOUNTLOGON       = 0x53, // [C->S] SRP登录请求
    SID_AUTH_ACCOUNTLOGONPROOF  = 0x54  // [C->S] SRP登录验证
};

// W3GS 协议 ID 定义 (TCP 游戏逻辑 & UDP 局域网广播)
enum W3GSPacketID {
    // --- 基础握手与心跳 (TCP) ---
    W3GS_PING_FROM_HOST         = 0x01, // [S->C] 主机发送的心跳包 (每30秒)
    W3GS_PONG_TO_HOST           = 0x46, // [C->S] 客户端回复的心跳包

    // --- 房间/大厅信息 (TCP) ---
    W3GS_SLOTINFOJOIN           = 0x04, // [S->C] 玩家加入时发送的完整槽位信息
    W3GS_REJECTJOIN             = 0x05, // [S->C] 拒绝加入 (房间满/游戏开始)
    W3GS_PLAYERINFO             = 0x06, // [S->C] 玩家详细信息 (PID, IP, 端口等)
    W3GS_PLAYERLEFT             = 0x07, // [S->C] 通知某玩家离开
    W3GS_PLAYERLOADED           = 0x08, // [S->C] 通知某玩家加载完毕
    W3GS_SLOTINFO               = 0x09, // [S->C] 槽位状态更新 (Slot Update)
    W3GS_REQJOIN                = 0x1E, // [C->S] 客户端请求加入房间
    W3GS_LEAVEREQ               = 0x21, // [C->S] 客户端请求离开
    W3GS_GAMELOADED_SELF        = 0x23, // [C->S] 客户端通知自己加载完毕
    W3GS_CLIENTINFO             = 0x37, // [C->S] 客户端发送自身信息 (端口等)
    W3GS_LEAVERS                = 0x1B, // [S->C] 离开者列表 / 踢人响应 (部分版本)
    W3GS_HOST_KICK_PLAYER       = 0x1C, // [S->C] 主机踢人

    // --- 游戏流程控制 (TCP) ---
    W3GS_COUNTDOWN_START        = 0x0A, // [S->C] 开始倒计时
    W3GS_COUNTDOWN_END          = 0x0B, // [S->C] 倒计时结束 (游戏开始)
    W3GS_START_LAG              = 0x10, // [S->C] 开始等待界面 (有人卡顿)
    W3GS_STOP_LAG               = 0x11, // [S->C] 停止等待界面
    W3GS_OUTGOING_KEEPALIVE     = 0x27, // [C->S] 客户端保持连接
    W3GS_DROPREQ                = 0x29, // [C->S] 请求断开连接 (掉线/强退)

    // --- 游戏内动作与同步 (TCP) ---
    W3GS_INCOMING_ACTION        = 0x0C, // [S->C] 广播玩家动作 (核心同步包)
    W3GS_OUTGOING_ACTION        = 0x26, // [C->S] 客户端发送动作 (点击/技能)
    W3GS_INCOMING_ACTION2       = 0x48, // [S->C] 扩展动作包 (通常用于录像/裁判)

    // --- 聊天系统 (TCP) ---
    W3GS_CHAT_FROM_HOST         = 0x0F, // [S->C] 主机转发的聊天消息
    W3GS_CHAT_TO_HOST           = 0x28, // [C->S] 客户端发送的聊天消息

    // --- 地图下载与校验 (TCP) ---
    W3GS_MAPCHECK               = 0x3D, // [S->C] 主机发起地图校验请求
    W3GS_STARTDOWNLOAD          = 0x3F, // [S->C] 主机通知开始下载 / [C->S] 客户端请求开始
    W3GS_MAPSIZE                = 0x42, // [C->S] 客户端报告地图大小/CRC状态
    W3GS_MAPPART                = 0x43, // [S->C] 地图文件分片数据
    W3GS_MAPPARTOK              = 0x44, // [C->S] 客户端确认收到分片 (ACK)
    W3GS_MAPPARTNOTOK           = 0x45, // [C->S] 客户端报告分片错误 (NACK)

    // --- 局域网发现与 P2P (UDP) ---
    W3GS_SEARCHGAME             = 0x2F, // [UDP] 搜索局域网游戏
    W3GS_GAMEINFO               = 0x30, // [UDP] 游戏信息广播 (Game Info)
    W3GS_CREATEGAME             = 0x31, // [UDP] 创建游戏广播
    W3GS_REFRESHGAME            = 0x32, // [UDP] 刷新游戏信息 (Refresh)
    W3GS_DECREATEGAME           = 0x33, // [UDP] 取消/销毁游戏
    W3GS_PING_FROM_OTHERS       = 0x35, // [UDP] P2P Ping
    W3GS_PONG_TO_OTHERS         = 0x36, // [UDP] P2P Pong
    W3GS_TEST                   = 0x88  // [UDP] 测试/私有包
};

// 拒绝加入原因
enum RejectReason {
    BAD_GAME                    = 0x05, // 指定的游戏不存在
    GAME_CLOSED                 = 0x06, // 游戏已关闭
    FULL_OLD                    = 0x07, // 房间已满(旧)
    FULL                        = 0x09, // 房间已满
    REQUIRES_PASS               = 0x0A, // 需要密码
    TOURNAMENT_REQ              = 0x0B, // 需要进入锦标赛模式
    TOO_POOR                    = 0x0D, // 指积分不足
    SOLO_REQ                    = 0x0E, // 需要单人模式
    STARTED                     = 0x10, // 游戏已开始
    GENERIC_ERROR               = 0x1B, // 版本不匹配、地图不匹配或被主机拉黑
    WRONGPASS                   = 0x27  // 密码错误
};

// =========================================================
// 2. 游戏配置 (Game Settings)
// =========================================================

// 基础游戏类型
enum BaseGameType {
    Type_All                    = 0x00, // 所有类型
    Type_Melee                  = 0x02, // 常规对战
    Type_FFA                    = 0x03, // 混战
    Type_1v1                    = 0x04, // 单挑
    Type_CTF                    = 0x05, // 夺旗
    Type_Greed                  = 0x06, // 贪婪
    Type_Slaughter              = 0x07, // 屠宰
    Type_SuddenDeath            = 0x08, // 猝死模式
    Type_Ladder                 = 0x09, // 天梯
    Type_UMS                    = 0x0A, // 自定义地图(UMS)
    Type_TeamMelee              = 0x0B, // 团队对战
    Type_TeamFFA                = 0x0C, // 团队混战
    Type_TeamCTF                = 0x0D, // 团队夺旗
    Type_TopVsBottom            = 0x0F, // 上下半场
    Type_IronManLadder          = 0x10, // 铁人天梯
    Type_PGL                    = 0x20  // 专业联赛
};

// 房间创建/广播状态 (SID_STARTADVEX3 响应)
enum GameCreationStatus {
    GameCreate_Ok               = 0x00, // 创建成功
    GameCreate_NameExists       = 0x01, // 失败：房间名已存在
    GameCreate_TypeUnavailable  = 0x02, // 失败：游戏类型不可用
    GameCreate_Error            = 0x03  // 失败：通用错误
};

// 游戏布局/队伍设置
enum GameLayoutStyle {
    Melee                       = 0x00, // 标准 (可换队)
    FreeForAll                  = 0x01, // 混战
    OneVsOne                    = 0x02, // 1v1
    CustomForces                = 0x03, // 自定义 (DotA/RPG必选)
    Ladder                      = 0x04  // 天梯
};

// 游戏类型标志位
enum GameTypeFlags {
    Flag_None                   = 0x0000,
    Flag_Expansion              = 0x2000, // 冰封王座
    Flag_FixedTeams             = 0x4000, // 固定队伍
    Flag_Official               = 0x8000  // 官方地图
};

// 组合游戏类型 (版本+类型)
enum ComboGameType {
    Game_RoC_Custom             = 0x0001, // 混乱之治 自定义
    Game_TFT_Custom             = 0x2001, // 冰封王座 自定义
    Game_TFT_Official           = 0xC009  // 冰封王座 官方对战
};

// 子游戏类型
enum SubGameType {
    SubType_None                = 0x00,
    SubType_Blizzard            = 0x42,
    SubType_Scenario            = 0x43,
    SubType_Internet            = 0x49,
    SubType_Load                = 0x4C,
    SubType_Save                = 0x53
};

// 游戏状态标志
enum GameState {
    State_None                  = 0x00,
    State_Private               = 0x01, // 私有
    State_Full                  = 0x02, // 已满
    State_HasOtherPlayers       = 0x04, // 有其他玩家
    State_InProgress            = 0x08, // 进行中
    State_DisconnectIsLoss      = 0x10, // 断线判负
    State_Replay                = 0x80, // 录像

    // 常用组合状态
    State_Open_Public           = State_HasOtherPlayers,
    State_Open_Private          = State_Private | State_HasOtherPlayers,
    State_Lobby_Full            = State_Full | State_HasOtherPlayers,
    State_ActiveGame            = State_InProgress | State_HasOtherPlayers,
    State_Ladder_InProgress     = State_InProgress | State_DisconnectIsLoss
};

// 游戏版本提供商
enum ProviderVersion {
    Provider_RoC                = 0x00000000, // 混乱之治
    Provider_SC_BW              = 0x000000FF, // 星际争霸
    Provider_TFT_New            = 0x000003FF, // 冰封王座(新)
    Provider_All_Bits           = 0xFFFFFFFF
};

// 天梯类型
enum LadderType {
    Ladder_None                 = 0x00000000,
    Ladder_Standard             = 0x00000001,
    Ladder_IronMan              = 0x00000003
};

// =========================================================
// 3. 玩家与槽位 (Slots & Players)
// =========================================================

// 槽位状态
enum SlotStatus {
    Open                        = 0x00, // 开放
    Close                       = 0x01, // 关闭
    Occupied                    = 0x02  // 占用
};

// 下载状态
enum DownloadStatus {
    DownloadStart               = 0,    // 0-99%
    Completed                   = 100,  // 100%
    NotStarted                  = 255   // 无地图
};

// 队伍定义
enum class SlotTeam : quint8 {
    Sentinel                    = 0,    // 近卫
    Scourge                     = 1,    // 天灾
    Observer                    = 2     // 裁判/观众
};

// 种族定义
enum class SlotRace : quint8 {
    Human                       = 0x01, // 人族
    Orc                         = 0x02, // 兽族
    NightElf                    = 0x04, // 暗夜
    Undead                      = 0x08, // 不死
    Random                      = 0x20, // 随机
    Selectable                  = 0x40, // 可选

    // DotA 专用别名
    Sentinel                    = NightElf,
    Scourge                     = Undead,
    Observer                    = Random
};

// 控制类型
enum  PlayerController {
    Human                       = 0x00, // 玩家
    Computer                    = 0x01  // 电脑
};

// 电脑难度
enum ComputerLevel {
    Easy                        = 0x00, // 简单
    Normal                      = 0x01, // 中等
    Insane                      = 0x02  // 疯狂
};

// =========================================================
// 4. 聊天系统 (Chat System)
// =========================================================

// 聊天标志
enum ChatFlag {
    Message                     = 0x10, // 普通消息
    TeamChange                  = 0x11, // 队伍变更
    ColorChange                 = 0x12, // 颜色变更
    RaceChange                  = 0x13, // 种族变更
    HandicapChange              = 0x14, // 让步变更
    Scope                       = 0x20  // 聊天范围
};

// 聊天范围
enum ChatScope {
    All                         = 0x00, // 所有人
    Allies                      = 0x01, // 仅盟友
    Observers                   = 0x02, // 仅观察者
    Directed                    = 0x03  // 指定玩家
};

enum CommandSource {
    From_Unknow                 = 0x00, // 未知来源的命令
    From_Client                 = 0x01, // 服务端输入命令
    From_Server                 = 0x02  // 客户端输入命令
};

// =========================================================
// 1. 游戏槽位数据 (Game Slot)
// =========================================================
struct GameSlot {
    quint8      pid                     = 0;    // 玩家ID
    quint8      downloadStatus          = 0;    // 下载进度 (0-100, 255=无)
    quint8      slotStatus              = 0;    // 状态 (Open/Closed/Occupied)
    quint8      computer                = 0;    // 是否电脑 (0=人, 1=电脑)
    quint8      team                    = 0;    // 队伍ID
    quint8      color                   = 0;    // 颜色ID
    quint8      race                    = 32;   // 种族标识 (默认随机)
    quint8      computerType            = 1;    // 电脑难度
    quint8      handicap                = 100;  // 生命值百分比
};

// =========================================================
// 2. 玩家运行时数据 (Player Runtime Data)
// =========================================================
struct PlayerData {
    // 基础信息
    quint8       pid                     = 0;
    QString      name;
    QString      clientUuid              = "";
    bool         isVisualHost            = false;

    // 网络连接
    QTcpSocket*  socket                  = nullptr;
    QHostAddress extIp;                  // 公网IP
    quint16      extPort                 = 0;
    QHostAddress intIp;                  // 内网IP (0x1E提供)
    quint16      intPort                 = 0;

    // 语言与编码
    QString      language                = "EN";
    QTextCodec*  codec                   = nullptr;

    // 下载状态
    bool         isDownloadStart         = false;
    quint32      lastDownloadOffset      = 0;
    quint32      currentDownloadOffset   = 0;

    // 加载状态
    bool         isFinishedLoading       = false;

    // 时间检测
    qint64       lastDownloadTime        = 0;
    qint64       lastResponseTime        = 0;
    quint32      currentLatency          = 0;
};

struct PlayerAction {
    quint8 pid;
    QByteArray data;
};

// =========================================================
// 3. 多语言消息封装 (Multi-Language Message)
// =========================================================
struct MultiLangMsg {
    QMap<QString, QString> msgs;
    QString                defaultMsg;

    // 默认构造
    MultiLangMsg() {}

    // 便捷初始化 (中/英)
    MultiLangMsg(const QString& cn, const QString& en) {
        msgs["CN"]  = cn;
        msgs["EN"]  = en;
        defaultMsg  = en;
    }

    // 链式添加
    MultiLangMsg& add(const QString& lang, const QString& txt) {
        msgs[lang] = txt;
        if (defaultMsg.isEmpty()) defaultMsg = txt;
        return *this;
    }

    // 获取消息 (自动回退)
    QString get(const QString& lang) const {
        return msgs.value(lang, defaultMsg);
    }
};

// 前置声明
class BnetSRP3;

// =========================================================
// Client 类定义
// =========================================================

class Client : public QObject
{
    Q_OBJECT
    friend class Command;

public:
    static const quint8 BNET_HEADER = 0xFF;
    static const int MAX_CHUNK_SIZE = 1442;

    explicit Client(QObject *parent = nullptr);
    ~Client();

    // --- 连接管理 ---
    void connectToHost(const QString &address, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;
    void setCredentials(const QString &user, const QString &pass, LoginProtocol protocol = Protocol_SRP_0x53);

    // --- 战网交互 ---
    void createAccount();                               // 注册账号
    void enterChat();                                   // 进入聊天
    void queryChannelList();                            // 请求频道列表
    void joinChannel(const QString &channelName);       // 加入频道

    // --- 游戏主机管理 ---
    bool isHostJoined();
    void swapSlots(int slot1, int slot2);
    void setGameTickInterval(quint16 interval);
    void setHost(QString creatorName) { m_host = creatorName; };
    quint16 getGameTickInterval() const { return m_gameTickInterval; }
    void createGame(const QString &gameName, const QString &password,
                    ProviderVersion providerVersion, ComboGameType comboGameType,
                    SubGameType subGameType, LadderType ladderType,CommandSource commandSource);
    void cancelGame();                                  // 取消/解散游戏
    void abortGame();
    void startGame();
    void stopAdv();                                     // 停止广播

    // --- 工具函数 ---
    void sendPingLoop();                                // 定时发送Ping
    void checkPlayerTimeout();                          // 检查玩家超时
    QString getPrimaryIPv4();                           // 获取本机IPv4
    bool bindToRandomPort();                            // 绑定随机UDP端口
    bool isBlackListedPort(quint16 port);               // 检查端口黑名单
    QString getBnetPacketName(BNETPacketID id);         // 获取对应的包名
    void writeIpToStreamWithLog(QDataStream &out, const QHostAddress &ip);

    // --- IP转换辅助 ---
    quint32 ipToUint32(const QString &ipAddress);
    quint32 ipToUint32(const QHostAddress &address);

    // --- 槽位信息辅助 ---
    quint8 findFreePid() const;
    quint8 getTotalSlots() const;                       // 获取总槽位数
    quint8 getOccupiedSlots() const;                    // 获取已占用槽位数 (包括Bot/Host)
    QString getSlotInfoString() const;                  // 返回 "(1/10)" 格式字符串

    // --- 设置机器人标志 ---
    void setBotFlag(bool isBot) { m_isBot = isBot; }
signals:
    void connected();
    void gameStarted();
    void gameCanceled();
    void disconnected();
    void authenticated();
    void accountCreated();
    void gameCreateFail();
    void socketError(const QString &error);
    void hostJoinedGame(const QString &username);
    void gameCreateSuccess(CommandSource commandSource);
    void requestCreateGame(const QString &username, const QString &gameName, CommandSource commandSource);

private slots:
    // --- 网络事件 ---
    void onGameTick();
    void onConnected();
    void onGameStarted();
    void onDisconnected();
    void onTcpReadyRead();
    void onUdpReadyRead();
    void onNewConnection();                             // 新玩家连接TCP Server
    void onPlayerReadyRead();                           // 玩家数据可读
    void onPlayerDisconnected();                        // 玩家断开连接

private:
    // --- 消息广播 ---
    void broadcastChatMessage(const MultiLangMsg &msg, quint8 excludePid = 0);
    void broadcastPacket(const QByteArray &packet, quint8 excludePid);
    void broadcastSlotInfo(quint8 excludePid = 1);

    // --- 槽位管理 ---
    void initSlots(quint8 maxPlayers = 10, bool showBotAtObserver = false);
    QByteArray serializeSlotData();
    GameSlot *findEmptySlot();

    // --- W3GS 协议包构建 ---
    QByteArray createW3GSSlotInfoPacket();
    QByteArray createW3GSMapCheckPacket();
    QByteArray createW3GSPingFromHostPacket();
    QByteArray createW3GSCountdownEndPacket();
    QByteArray createW3GSCountdownStartPacket();
    QByteArray createW3GSPlayerLoadedPacket(quint8 pid);
    QByteArray createW3GSStartDownloadPacket(quint8 fromPid);
    QByteArray createW3GSRejectJoinPacket(RejectReason reason);
    QByteArray createW3GSIncomingActionPacket (quint16 sendInterval);
    QByteArray createW3GSPlayerLeftPacket(quint8 pid, quint32 reason);
    QByteArray createW3GSSlotInfoJoinPacket(quint8 playerID, const QHostAddress& externalIp, quint16 localPort);
    QByteArray createW3GSMapPartPacket(quint8 toPid, quint8 fromPid, quint32 offset, const QByteArray& chunkData);
    QByteArray createW3GSChatFromHostPacket(const QByteArray &rawBytes, quint8 senderPid = 1, quint8 toPid = 255, ChatFlag flag = Message, quint32 extraData = 0);
    QByteArray createPlayerInfoPacket(quint8 pid, const QString& name, const QHostAddress& externalIp, quint16 externalPort, const QHostAddress& internalIp, quint16 internalPort);

    // --- 内部网络处理 ---
    void sendNextMapPart(quint8 toPid, quint8 fromPid = 1);
    void sendPacket(BNETPacketID id, const QByteArray &payload);
    void handleBNETTcpPacket(BNETPacketID id, const QByteArray &data);
    void handleW3GSUdpPacket(const QByteArray &data, const QHostAddress &sender, quint16 senderPort);
    void handleW3GSPacket(QTcpSocket *socket, quint8 id, const QByteArray &payload);

    // 设置 NetManager 指针
    void setNetManager(NetManager* server) { m_netManager = server; }

    // --- 频道管理 ---
    void joinRandomChannel();

    // --- 状态管理 ---
    void checkAllPlayersLoaded();

    // --- 地图管理 ---
    void initiateMapDownload(quint8 pid);
    void setMapData(const QByteArray &data);
    void setCurrentMap(const QString &filePath);

    // --- 认证流程 ---
    void sendAuthInfo();
    void handleAuthCheck(const QByteArray &data);
    void sendLoginRequest(LoginProtocol protocol);

    // --- SRP(0x53) ---
    void handleSRPLoginResponse(const QByteArray &data);

    // --- 游戏算法 ---
    static QByteArray calculateBrokenSHA1(const QByteArray &data);
    QByteArray calculateOldLogonProof(const QString &password, quint32 clientToken, quint32 serverToken);

private:
    // --- 成员变量 ---

    // 控制命令
    Command                         *m_command;

    // 玩家管理
    QString                         m_botDisplayName        = "CC";
    QList<QTcpSocket*>              m_playerSockets;
    QMap<QTcpSocket*, QByteArray>   m_playerBuffers;
    QMap<quint8, PlayerData>        m_players;

    // 频道管理
    QStringList                     m_channelList;

    // 槽位数据
    QVector<GameSlot>               m_slots;
    bool                            m_enableObservers       = false;
    quint8                          m_layoutStyle           = CustomForces;

    // 游戏数据
    QList<PlayerAction>             m_actionQueue;
    quint32                         m_hostCounter           = 1;
    quint32                         m_randomSeed            = 0;

    // 地图下载
    War3Map                         m_war3Map;
    QByteArray                      m_mapData;
    quint32                         m_mapSize               = 0;

    // 认证管理
    BnetSRP3                        *m_srp                  = nullptr;
    QString                         m_host;
    QString                         m_user;
    QString                         m_pass;
    quint32                         m_serverToken           = 0;
    quint32                         m_clientToken           = 0;

    // 文件路径
    QString                         m_war3ExePath;
    QString                         m_stormDllPath;
    QString                         m_gameDllPath;
    QString                         m_dota683dPath;
    QString                         m_currentMapPath;
    QString                         m_lastLoadedMapPath;

    // 连接管理
    NetManager                      *m_netManager           = nullptr;
    QString                         m_serverAddr;
    quint16                         m_serverPort            = 0;
    QUdpSocket                      *m_udpSocket            = nullptr;
    QTcpSocket                      *m_tcpSocket            = nullptr;
    QTcpServer                      *m_tcpServer            = nullptr;

    // 时间管理
    QTimer                          *m_pingTimer            = nullptr;
    QTimer                          *m_startTimer           = nullptr;
    QTimer                          *m_gameTickTimer        = nullptr;
    quint16                         m_gameTickInterval      = 100;

    // 设置标志
    bool                            m_isBot                 = false;
    bool                            m_gameStarted           = false;

    // 登录选项
    quint32                         m_logonType             = 0;
    LoginProtocol                   m_loginProtocol         = Protocol_SRP_0x53;

    // 频率管理
    quint8                          m_actionLogFrequency    = 5;
    quint32                         m_chatIntervalCounter   = 0;
};

#endif // CLIENT_H

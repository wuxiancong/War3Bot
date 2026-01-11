#ifndef BOTMANAGER_H
#define BOTMANAGER_H

#include <QObject>
#include <QVector>
#include <QSettings>

#include "client.h"
#include "netmanager.h"

// === 1. 机器人状态枚举 ===
enum class BotState {
    Disconnected,                               // 断开连接
    Connecting,                                 // 连接中
    InLobby,                                    // 在大厅
    Unregistered,                               // 未注册 (初始状态或正在注册)
    Idle,                                       // 空闲中 (已登录在大厅)
    Creating,                                   // 创建中 (正在发送创建房间包)
    Reserved,                                   // 已创建 (等待虚拟房主加入)
    Waiting,                                    // 等待中 (房间已创建，等待玩家加入)
    Starting,                                   // 倒计时 (准备开始)
    InGame,                                     // 游戏中 (游戏已开始)
    Finishing                                  //  结算中 (游戏结束)
};

// === 2. 房间信息结构体 ===
struct GameInfo {
    QString gameName;                           // 房间名
    QString mapName;                            // 地图名称 (如 dota v6.83.w3x)
    QString mapPath;                            // 地图路径 (方便下载或校验)
    QString hostName;                           // 虚拟房主名字
    QString clientId;                           // 虚拟房主的唯一ID (用于校验身份)

    int maxPlayers = 10;                        // 最大玩家数
    int currentPlayerCount = 0;                 // 当前人数

    // 时间统计
    qint64 createTime = 0;                      // 房间创建时刻 (用于计算超时未加入)
    qint64 gameStartTime = 0;                   // 游戏开始时刻
    qint64 gameEndTime = 0;                     // 游戏结束时刻
};

// === 3. 机器人结构体 ===
struct Bot {
    quint32 id;
    Client *client;
    BotState state;
    QString username;
    QString password;
    GameInfo gameInfo;
    QSet<QString> banList;
    QSet<QString> kickList;
    qint64 lastRefreshTime;
    QMap<QString, PlayerData> PlayerDatas;
    CommandSource commandSource = From_Server;

    struct Task {
        bool hasTask = false;
        qint64 requestTime = 0;
        QString hostName;
        QString clientId;
        QString gameName;
        QString mapName;
        CommandSource commandSource;
    } pendingTask;

    Bot(quint32 _id, QString _user, QString _pass)
        : id(_id), client(nullptr), state(BotState::Disconnected), username(_user), password(_pass) {}

    ~Bot() { if (client) client->deleteLater(); }

    // 辅助函数：重置游戏状态
    void resetGameState() {
        // 1. 清理玩家和黑名单
        PlayerDatas.clear();
        kickList.clear();
        banList.clear();

        // 2. 重置游戏元数据
        gameInfo = GameInfo();

        // 3. 重置挂起任务！防止逻辑残留
        pendingTask = Task();

        // 4. 重置辅助标记
        lastRefreshTime = 0;
        commandSource = From_Server;

        // 5. 状态重置
        state = BotState::Idle;
    }
};

// === 4. 指令信息结构体 ===
struct CommandInfo {
    QString text;
    QString clientId;
    qint64 timestamp;
};

// === 3. 机器人管理器 ===
class BotManager : public QObject
{
    Q_OBJECT
public:
    explicit BotManager(QObject *parent = nullptr);
    ~BotManager();

    // 初始化机器人
    void initializeBots(quint32 count, const QString& configPath);

    // 启动所有机器人连接
    void startAll();

    // 停止所有机器人
    void stopAll();

    // 清楚机器人数据
    void removeGameName(Bot *bot, bool disconnectFlag = false);

    // 获取所有机器人列表
    const QVector<Bot*>& getAllBots() const;

    // 设置 NetManager
    void setNetManager(NetManager *server) { m_netManager = server; }
    void setP2PControlPort(quint16 port) { m_controlPort = port; }

    // 创建游戏
    QString generateSafeGameName(const QString &inputName, const QString &hostName);
    bool createGame(const QString& hostName, const QString &gameName, CommandSource commandSource, const QString &clientUuid);
    void onCommandReceived(const QString &userName, const QString &clientUuid, const QString &command, const QString &text);

signals:
    // 状态变更信号，用于日志或UI更新
    void botStateChanged(int botId, QString username, BotState newState);

private slots:
    // 内部槽函数：处理单个机器人的信号
    void onBotError(Bot *bot, QString error);
    void onBotGameCreateSuccess(Bot *bot);
    void onBotAccountCreated(Bot *bot);
    void onBotGameCreateFail(Bot *bot);
    void onBotAuthenticated(Bot *bot);
    void onBotDisconnected(Bot *bot);
    void onBotPendingTaskTimeout();

private:
    // 容器存储所有机器人指针
    QVector<Bot*> m_bots;

    // 缓存连接信息
    QString m_targetServer;
    quint16 m_controlPort;
    quint16 m_targetPort;
    QString m_configPath;
    QString m_userPrefix;
    QString m_defaultPassword;
    NetManager *m_netManager = nullptr;
    QMap<QString, Bot*> m_activeGames;
    QMap<QString, qint64> m_lastHostTime;
    QMap<QString, CommandInfo> m_commandInfos;
};

#endif // BOTMANAGER_H

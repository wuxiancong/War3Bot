#ifndef BOTMANAGER_H
#define BOTMANAGER_H

#include <QQueue>
#include <QObject>
#include <QVector>
#include <QSettings>
#include <QPair>

#include "client.h"
#include "netmanager.h"

// === 1. 机器人状态枚举 ===
enum class BotState {
    Disconnected,
    Connecting,
    InLobby,
    Unregistered,
    Idle,
    Creating,
    Reserved,
    Waiting,
    Starting,
    InGame,
    Finishing
};

// === 2. 房间信息结构体 ===
struct GameInfo {
    QString gameName;
    QString mapName;
    QString mapPath;
    QString hostName;
    QString clientId;

    int maxPlayers = 10;
    int currentPlayerCount = 0;

    qint64 createTime = 0;
    qint64 gameStartTime = 0;
    qint64 gameEndTime = 0;
};

// === 3. 机器人结构体 ===
struct Bot {
    quint32 id;
    Client *client;
    BotState state;
    QString username;
    QString password;
    GameInfo gameInfo;
    bool hostJoined = false;

    // 任务挂起数据
    struct Task {
        bool hasTask = false;
        qint64 requestTime = 0;
        QString hostName;
        QString clientId;
        QString gameName;
        CommandSource commandSource;
    } pendingTask;

    CommandSource commandSource = From_Server;

    Bot(quint32 _id, QString _user, QString _pass)
        : id(_id), client(nullptr), state(BotState::Disconnected), username(_user), password(_pass) {}

    ~Bot() { if (client) client->deleteLater(); }

    void resetGameState() {
        gameInfo = GameInfo();
        pendingTask = Task();
        commandSource = From_Server;
        state = BotState::Idle;
        hostJoined = false;
    }
};

// === 4. 指令信息结构体 ===
struct CommandInfo {
    QString text;
    QString clientId;
    qint64 timestamp;
};

// === 5. 机器人管理器 ===
class BotManager : public QObject
{
    Q_OBJECT
public:
    explicit BotManager(QObject *parent = nullptr);
    ~BotManager();

    // --- 初始化与文件管理 ---
    void initializeBots(quint32 initialCount, const QString &configPath);
    bool createBotAccountFilesIfNotExist(bool allowAutoGenerate, int targetListNumber);

    // --- 机器人控制 ---
    void startAll();
    void stopAll();
    int loadMoreBots(int count);
    void removeGame(Bot *bot, bool disconnectFlag = false);

    // --- 游戏创建与指令 ---
    bool createGame(const QString& hostName, const QString &gameName, CommandSource commandSource, const QString &clientUuid);
    void onCommandReceived(const QString &userName, const QString &clientUuid, const QString &command, const QString &text);

    // --- Getters / Setters ---
    const QVector<Bot*>& getAllBots() const;
    void setNetManager(NetManager *server) { m_netManager = server; }
    void setServerPort(quint16 port) { m_controlPort = port; }

signals:
    void botStateChanged(int botId, QString username, BotState newState);

private slots:
    void onHostJoinedGame(Bot *bot, const QString &hostName);
    void onBotError(Bot *bot, QString error);
    void onBotGameCreateSuccess(Bot *bot);
    void onBotAccountCreated(Bot *bot);
    void onBotGameCreateFail(Bot *bot);
    void onBotAuthenticated(Bot *bot);
    void onBotDisconnected(Bot *bot);
    void onBotGameCanceled(Bot *bot);
    void onBotGameStarted(Bot *bot);
    void onBotPendingTaskTimeout();

    // 内部处理注册队列
    void processNextRegistration();

private:
    // 内部辅助函数
    void addBotInstance(const QString& username, const QString& password);
    QChar randomCase(QChar c);
    QString generateUniqueUsername();
    QString toLeetSpeak(const QString &input);
    QString generateRandomPassword(int length);

private:
    // 机器人容器
    QVector<Bot*>                   m_bots;
    QString                         m_botDisplayName            = "CC.Dota.XX";

    // 动态加载与注册相关
    QStringList                     m_allAccountFilePaths;
    QStringList                     m_newAccountFilePaths;
    QQueue<QPair<QString, QString>> m_registrationQueue;
    Client                          *m_tempRegistrationClient   = nullptr;

    int                             m_currentFileIndex          = 0;
    int                             m_currentAccountIndex       = 0;
    int                             m_initialLoginCount         = 0;
    int                             m_totalRegistrationCount    = 0;
    quint32                         m_globalBotIdCounter        = 1;
    bool                            m_isMassRegistering         = false;

    // 网络与配置
    QString                         m_targetServer;
    QString                         m_configPath;
    quint16                         m_controlPort               = 0;
    quint16                         m_targetPort                = 6112;
    NetManager                      *m_netManager               = nullptr;

    // 状态缓存
    QMap<QString, Bot*>             m_activeGames;
    QMap<QString, qint64>           m_lastHostTime;
    QMap<QString, CommandInfo>      m_commandInfos;
};

#endif // BOTMANAGER_H

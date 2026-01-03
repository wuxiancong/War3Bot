#ifndef BOTMANAGER_H
#define BOTMANAGER_H

#include <QObject>
#include <QVector>
#include <QSettings>

#include "client.h"
#include "netmanager.h"

// === 1. 机器人状态枚举 ===
enum class BotState {
    Unregistered,                               // 未注册 (初始状态或正在注册)
    Idle,                                       // 空闲中 (已登录在大厅)
    Creating,                                   // 创建中 (正在发送创建房间包)
    Waiting,                                    // 等待中 (房间已创建，等待玩家加入)
    InGame,                                     // 游戏中 (游戏已开始)
    InLobby,                                    // 在大厅
    Connecting,                                 // 连接中
    Disconnected                                // 断开连接
};

// === 2. 机器人结构体 ===
struct Bot {
    quint32 id;                                 // 数字 ID (1, 2, 3...)
    Client *client;                             // 客户端对象
    BotState state;                             // 当前状态
    QString username;                           // 完整用户名 (例如 bot1)
    QString password;                           // 密码
    QString clientId;                           // 客户端Id
    QString gameName;                           // 游戏名称
    QString hostName;                           // 主机名称
    CommandSource commandSource = From_Server;  // 命令来源

    struct Task {
        bool hasTask = false;                   // 是否有任务
        QString hostName;                       // 谁下的命令 (虚拟房主)
        QString gameName;                       // 房间名
    } pendingTask;

    Bot(quint32 _id, QString _user, QString _pass)
        : id(_id), client(nullptr), state(BotState::Disconnected), username(_user), password(_pass) {}

    ~Bot() { if (client) client->deleteLater(); }
};

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
    void onBotAuthenticated(Bot *bot);
    void onBotAccountCreated(Bot *bot);
    void onBotGameCreateSuccess(Bot *bot);
    void onBotGameCreateFail(Bot *bot);
    void onBotDisconnected(Bot *bot);
    void onBotError(Bot *bot, QString error);

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
    QMap<QString, CommandInfo> m_commandInfos;
};

#endif // BOTMANAGER_H

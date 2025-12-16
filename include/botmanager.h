#ifndef BOTMANAGER_H
#define BOTMANAGER_H

#include <QObject>
#include <QVector>
#include <QSettings>
#include "bnetbot.h"

// === 1. 机器人状态枚举 ===
enum class BotState {
    Unregistered,   // 未注册 (初始状态或正在注册)
    Idle,           // 空闲中 (已登录在大厅)
    Creating,       // 创建中 (正在发送创建房间包)
    Waiting,        // 等待中 (房间已创建，等待玩家加入)
    InGame,         // 游戏中 (游戏已开始)
    Disconnected    // 断开连接
};

// === 2. 机器人结构体 ===
struct Bot {
    int id;                 // 数字 ID (1, 2, 3...)
    QString username;       // 完整用户名 (例如 bot1)
    QString password;       // 密码
    BotState state;         // 当前状态
    BnetBot *client;        // 网络客户端对象

    Bot(int _id, QString _user, QString _pass)
        : id(_id), username(_user), password(_pass), state(BotState::Disconnected), client(nullptr) {}
};

// === 3. 机器人管理器 ===
class BotManager : public QObject
{
    Q_OBJECT
public:
    explicit BotManager(QObject *parent = nullptr);
    ~BotManager();

    /**
      *@brief 初始化机器人
      *@param count 要创建的机器人数量
      *@param configPath 配置文件路径 (.ini)
     */
    void initializeBots(int count, const QString& configPath);

    // 启动所有机器人连接
    void startAll();

    // 停止所有机器人
    void stopAll();

    // 获取所有机器人列表
    const QVector<Bot*>& getAllBots() const;

signals:
    // 状态变更信号，用于日志或UI更新
    void botStateChanged(int botId, QString username, BotState newState);

private slots:
    // 内部槽函数：处理单个机器人的信号
    void onBotAuthenticated(int botId);
    void onBotAccountCreated(int botId);
    void onBotGameCreated(int botId);
    void onBotDisconnected(int botId);
    void onBotError(int botId, QString error);

private:
    // 容器存储所有机器人指针
    QVector<Bot*> m_bots;

    // 缓存连接信息
    QString m_targetServer;
    quint16 m_targetPort;
};

#endif // BOTMANAGER_H

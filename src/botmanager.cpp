#include "botmanager.h"
#include "logger.h"
#include <QThread>

BotManager::BotManager(QObject *parent) : QObject(parent)
{
}

BotManager::~BotManager()
{
    stopAll();
    qDeleteAll(m_bots);
    m_bots.clear();
}

void BotManager::initializeBots(int count, const QString& configPath)
{
    // 1. 读取配置文件
    QSettings settings(configPath, QSettings::IniFormat);

    // 读取 [bnet] 节点
    m_targetServer = settings.value("bnet/server", "127.0.0.1").toString();
    m_targetPort = settings.value("bnet/port", 6112).toUInt();
    QString userPrefix = settings.value("bnet/username", "bot").toString();
    QString password = settings.value("bnet/password", "wxc123").toString();

    LOG_INFO(QString("[BotManager] 初始化配置 - 服务器: %1:%2, 前缀: %3, 数量: %4")
                 .arg(m_targetServer).arg(m_targetPort).arg(userPrefix).arg(count));

    // 2. 批量创建机器人
    for (int i = 0; i <= count; ++i) {
        // 生成用户名：前缀 + ID (例如 bot1, bot2)
        QString fullUsername = (i == 0) ? QString("%1").arg(userPrefix) : QString("%1%2").arg(userPrefix).arg(i);

        Bot *bot = new Bot(i, fullUsername, password);

        // 创建 Client 实例
        bot->client = new Client(this);

        // 设置房间自增 ID 初始值
        bot->client->setHostCounter(i);

        // 设置凭据 (默认使用 SRP 0x53)
        bot->client->setCredentials(fullUsername, password, Protocol_SRP_0x53);

        // === 绑定信号槽 ===

        // 1. 连接成功/登录成功
        connect(bot->client, &Client::authenticated, this, [this, i]() {
            this->onBotAuthenticated(i);
        });

        // 2. 注册成功
        connect(bot->client, &Client::accountCreated, this, [this, i]() {
            this->onBotAccountCreated(i);
        });

        // 3. 房间创建成功
        connect(bot->client, &Client::gameListRegistered, this, [this, i]() {
            this->onBotGameCreated(i);
        });

        // 4. 断开连接
        // 假设 Client 有 disconnected 信号，如果没有请自行添加或使用 socket 的 disconnected
        // 这里假设你在 Client 中定义了 onDisconnected 槽并也许发出了信号
        // 如果 Client 没有直接暴露 disconnected 信号，可以通过 socketError 捕获或者添加一个
        // 这里演示用 socketError 模拟断开/错误
        connect(bot->client, &Client::socketError, this, [this, i](QString err) {
            this->onBotError(i, err);
        });

        m_bots.append(bot);
    }
}

void BotManager::startAll()
{
    LOG_INFO("[BotManager] 开始启动所有机器人...");
    for (Bot *bot : qAsConst(m_bots)) {
        if (bot->client) {
            // 更新状态
            bot->state = BotState::Unregistered; // 初始连接视为未注册/连接中

            // 连接服务器
            // 注意：Client::connectToHost 内部会处理连接和随后的 AuthCheck/Login/Register 流程
            bot->client->connectToHost(m_targetServer, m_targetPort);

            // 稍微错开连接时间，避免瞬间并发过高
            QThread::msleep(100);
        }
    }
}

void BotManager::stopAll()
{
    LOG_INFO("[BotManager] 停止所有机器人...");
    for (Bot *bot : qAsConst(m_bots)) {
        if (bot->client) {
            bot->client->disconnectFromHost();
        }
        bot->state = BotState::Disconnected;
    }
}

const QVector<Bot*>& BotManager::getAllBots() const
{
    return m_bots;
}

// === 槽函数实现 ===

void BotManager::onBotAuthenticated(int botId)
{
    // ID 是从 1 开始的，数组索引是 ID-1
    if (botId > -1 && botId <= m_bots.size()) {
        Bot *bot = m_bots[botId - 1];
        bot->state = BotState::Idle;
        LOG_INFO(QString("[BotManager] 🤖 %1 登录成功，当前状态: 空闲").arg(bot->username));
        emit botStateChanged(bot->id, bot->username, bot->state);
    }
}

void BotManager::onBotAccountCreated(int botId)
{
    if (botId > 0 && botId <= m_bots.size()) {
        Bot *bot = m_bots[botId - 1];
        bot->state = BotState::Unregistered;
        LOG_INFO(QString("[BotManager] 🆕 %1 账号注册成功").arg(bot->username));
        emit botStateChanged(bot->id, bot->username, bot->state);
    }
}

void BotManager::onBotGameCreated(int botId)
{
    if (botId > 0 && botId <= m_bots.size()) {
        Bot *bot = m_bots[botId - 1];
        bot->state = BotState::Waiting;
        LOG_INFO(QString("[BotManager] 🎮 %1 房间已创建，等待玩家...").arg(bot->username));
        emit botStateChanged(bot->id, bot->username, bot->state);
    }
}

void BotManager::onBotError(int botId, QString error)
{
    if (botId > 0 && botId <= m_bots.size()) {
        Bot *bot = m_bots[botId - 1];
        bot->state = BotState::Disconnected;
        LOG_WARNING(QString("[BotManager] ❌ %1 连接错误/断开: %2").arg(bot->username, error));
        emit botStateChanged(bot->id, bot->username, bot->state);
    }
}

void BotManager::onBotDisconnected(int botId)
{
    if (botId > 0 && botId <= m_bots.size()) {
        Bot *bot = m_bots[botId - 1];
        bot->state = BotState::Disconnected;
        emit botStateChanged(bot->id, bot->username, bot->state);
    }
}

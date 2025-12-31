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

void BotManager::initializeBots(int count, const QString &configPath)
{
    // 1. 清理旧数据
    stopAll();
    qDeleteAll(m_bots);
    m_bots.clear();

    // 2. 读取配置文件
    QSettings settings(configPath, QSettings::IniFormat);

    // 读取 [bnet] 节点
    m_targetServer = settings.value("bnet/server", "127.0.0.1").toString();
    m_targetPort = settings.value("bnet/port", 6112).toUInt();
    m_userPrefix = settings.value("bnet/username", "bot").toString();
    m_defaultPassword = settings.value("bnet/password", "wxc123").toString();

    QString userPrefix = settings.value("bnet/username", "bot").toString();
    QString password = settings.value("bnet/password", "wxc123").toString();

    LOG_INFO(QString("[BotManager] 初始化配置 - 服务器: %1:%2, 前缀: %3, 数量: %4")
                 .arg(m_targetServer).arg(m_targetPort).arg(userPrefix).arg(count));

    // 3. 批量创建机器人
    for (int i = 0; i < count; ++i) {
        // 生成用户名：前缀 + ID (例如 bot1, bot2)
        QString fullUsername = (i == 0) ? QString("%1").arg(userPrefix) : QString("%1%2").arg(userPrefix).arg(i);

        Bot *bot = new Bot(i, fullUsername, password);

        // 创建 Client 实例
        bot->client = new class Client(this);

        // 设置凭据 (默认使用 SRP 0x53)
        bot->client->setCredentials(fullUsername, password, Protocol_SRP_0x53);

        // === 绑定信号槽 ===

        // 1. 连接成功/登录成功
        connect(bot->client, &Client::authenticated, this, [this, bot]() {
            this->onBotAuthenticated(bot);
        });

        // 2. 注册成功
        connect(bot->client, &Client::accountCreated, this, [this, bot]() {
            this->onBotAccountCreated(bot);
        });

        // 3. 房间创建成功
        connect(bot->client, &Client::gameCreated, this, [this, bot]() {
            this->onBotGameCreated(bot);
        });

        // 4. 错误处理
        connect(bot->client, &Client::socketError, this, [this, bot](QString err) {
            this->onBotError(bot, err);
        });

        m_bots.append(bot);
    }
    LOG_INFO(QString("初始化完成，共创建 %1 个机器人对象").arg(m_bots.size()));
}

bool BotManager::onRequestCreateGame(const QString &creatorName, const QString &gameName, CommandSource commandSource)
{
    Bot *targetBot = nullptr;

    // 1. 优先寻找现有空闲 Bot
    for (Bot *bot : qAsConst(m_bots)) {
        // 必须是已登录且空闲的
        if (bot->state == BotState::Idle && bot->client && bot->client->isConnected()) {
            targetBot = bot;
            break;
        }
    }

    // 2. 如果没找到，动态创建一个新的 Bot
    if (!targetBot) {
        int maxId = 0;
        for (Bot *bot : qAsConst(m_bots)) {
            if (bot->id > maxId) maxId = bot->id;
        }
        int newId = maxId + 1;
        QString newUsername = QString("%1%2").arg(m_userPrefix).arg(newId);

        LOG_INFO(QString("⚠️ 无空闲机器人，动态扩容: [%1]").arg(newUsername));

        targetBot = new Bot(newId, newUsername, m_defaultPassword);
        targetBot->client = new class Client(this);
        targetBot->client->setCredentials(newUsername, m_defaultPassword, Protocol_SRP_0x53);

        // === 绑定信号 ===
        // 使用 Lambda 捕获 bot 指针，确保槽函数知道是哪个 bot
        connect(targetBot->client, &Client::authenticated, this, [this, targetBot]() { this->onBotAuthenticated(targetBot); });
        connect(targetBot->client, &Client::accountCreated, this, [this, targetBot]() { this->onBotAccountCreated(targetBot); });
        connect(targetBot->client, &Client::gameCreated, this, [this, targetBot]() { this->onBotGameCreated(targetBot); });
        connect(targetBot->client, &Client::socketError, this, [this, targetBot](QString e) { this->onBotError(targetBot, e); });
        connect(targetBot->client, &Client::disconnected, this, [this, targetBot]() { this->onBotDisconnected(targetBot); });

        m_bots.append(targetBot);

        // 标记此 Bot 有任务在身！
        targetBot->pendingTask.hasTask = true;
        targetBot->pendingTask.creatorName = creatorName;
        targetBot->pendingTask.gameName = gameName;

        // 启动连接
        // 握手 -> 检查版本 -> 自动注册 -> 登录
        // 只需在 onBotAuthenticated 里守株待兔
        targetBot->state = BotState::Connecting;
        targetBot->client->connectToHost(m_targetServer, m_targetPort);

        LOG_INFO(QString("⏳ [%1] 正在启动并注册/登录，任务已挂起...").arg(newUsername));
        return true;
    }

    // 3. 如果是现成的空闲 Bot，直接创建
    if (targetBot) {
        LOG_INFO(QString("✅ 指派空闲机器人 [%1]").arg(targetBot->username));
        targetBot->state = BotState::Creating;

        // 设置虚拟房主
        targetBot->client->setHost(creatorName);

        // 发送创建命令
        targetBot->client->createGame(gameName, "", Provider_TFT_New, Game_TFT_Custom, SubType_None, Ladder_None, commandSource);
        return true;
    }

    return false;
}

void BotManager::startAll()
{
    int delay = 0;
    const int interval = 200;

    for (Bot *bot : qAsConst(m_bots)) {
        if (!bot->client) continue;
        QTimer::singleShot(delay, this, [this, bot]() {
            if (m_bots.contains(bot) && bot->client) {
                bot->state = BotState::Unregistered;
                LOG_INFO(QString("[%1] 发起连接...").arg(bot->username));
                bot->client->connectToHost(m_targetServer, m_targetPort);
            }
        });

        delay += interval;
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

void BotManager::onBotAuthenticated(Bot *bot)
{
    if (!bot) return;
    bot->state = BotState::Idle;
    LOG_INFO(QString("✅ [%1] 登录成功").arg(bot->username));
    emit botStateChanged(bot->id, bot->username, bot->state);
}

void BotManager::onBotAccountCreated(Bot *bot)
{
    if (!bot) return;
    LOG_INFO(QString("🆕 [%1] 账号注册成功，正在尝试登录...").arg(bot->username));
}

void BotManager::onBotGameCreated(Bot *bot)
{
    if (!bot) return;
    bot->state = BotState::Waiting;
    LOG_INFO(QString("🎮 [%1] 房间创建成功").arg(bot->username));
    emit botStateChanged(bot->id, bot->username, bot->state);
}

void BotManager::onBotError(Bot *bot, QString error)
{
    if (!bot) return;

    LOG_WARNING(QString("❌ [%1] 错误: %2").arg(bot->username, error));
    bot->state = BotState::Disconnected;
    emit botStateChanged(bot->id, bot->username, bot->state);

    // 简单的自动重连逻辑
    if (bot->client && !bot->client->isConnected()) {
        int retryDelay = 5000 + (bot->id * 1000);
        LOG_INFO(QString("🔄 [%1] 将在 %2 毫秒后尝试重连...").arg(bot->username).arg(retryDelay));
        QTimer::singleShot(retryDelay, this, [this, bot]() {
            if (m_bots.contains(bot) && bot->client && !bot->client->isConnected()) {
                bot->client->connectToHost(m_targetServer, m_targetPort);
            }
        });
    }
}

void BotManager::onBotDisconnected(Bot *bot)
{
    if (!bot) return;
    bot->state = BotState::Disconnected;
    LOG_INFO(QString("🔌 [%1] 断开连接").arg(bot->username));
    emit botStateChanged(bot->id, bot->username, bot->state);
}

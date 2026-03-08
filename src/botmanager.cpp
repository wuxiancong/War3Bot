#include "botmanager.h"
#include "logger.h"
#include <QDir>
#include <QThread>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QCoreApplication>

// =========================================================
// 扩充的拟人化词库 (DotA 深度定制版)
// =========================================================

static const QStringList s_adjectives = {
    // 基础形容词
    "Dark", "Silent", "Holy", "Crazy", "Super", "Pro", "Mad", "Ghost",
    "Shadow", "Iron", "Ice", "Fire", "Storm", "Fast", "Lazy", "Best",
    "Top", "Old", "New", "Blue", "Red", "Evil", "Good", "Cyber",
    "Epic", "Rare", "Legend", "Hyper", "Ultra", "Mega", "Neon", "Void",
    "Dead", "Live", "Cool", "Hot", "Cold", "Wild", "Brave", "Calm",
    "Angry", "Happy", "Sad", "Lucky", "Fatal", "Toxic", "Swift", "Grand",
    "Fallen", "Lost", "Bloody", "Broken", "Frozen", "Burning",

    // DotA 状态/属性形容词
    "Divine", "Ethereal", "Arcane", "Mystic", "Secret", "Ancient",
    "Invisible", "Hasted", "Doomed", "Stunned", "Silenced", "Immune",
    "Greedy", "Salty", "Tilted", "Feeder", "Aggressive", "Passive",
    "Magic", "Physical", "Pure", "Random", "Solo", "Dual", "Tri"
};

static const QStringList s_nouns = {
    // 基础名词
    "Knight", "Wolf", "Tiger", "Dragon", "Killer", "Boy", "Girl", "Man",
    "Master", "King", "Lord", "Sniper", "Gamer", "Player", "Walker",
    "Runner", "Spirit", "Soul", "Moon", "Star", "Hero", "Peon", "Grunt",
    "Mage", "Rogue", "Priest", "Hunter", "Warrior", "Demon", "Angel", "God",
    "Titan", "Giant", "Dwarf", "Elf", "Orc", "Human", "Beast", "Bear",
    "Eagle", "Snake", "Viper", "Cobra", "Lion", "Shark", "Whale", "Panda",
    "Ninja", "Samurai", "Robot", "Cyborg", "Alien", "Phantom", "Reaper",

    // DotA 物品/单位/建筑名词
    "Roshan", "Aegis", "Cheese", "Courier", "Chicken", "Ward", "Sentry",
    "Tangos", "Salve", "Bottle", "Clarity", "Midas", "Blink", "Dagon",
    "Radiance", "Rapier", "Butterfly", "Buriza", "Basher", "Mekansm",
    "Pipe", "BKB", "Aghanim", "Scepter", "Orb", "Relic", "Gem",
    "Throne", "Rax", "Tower", "Fountain", "Shop", "Rune", "Creep"
};

static const QStringList s_verbs = {
    // 基础动词
    "Kill", "Love", "Hate", "Eat", "Drink", "Smash", "Crush", "Kick",
    "Punch", "Kiss", "Hug", "Shoot", "Slash", "Hunt", "Chasing", "Fighting",

    // DotA 行为动词
    "Gank", "Carry", "Push", "Defend", "Feed", "Farm", "Support", "Roam",
    "Jungle", "Mid", "Solo", "Own", "Pwn", "Rekt", "Juke", "Bait",
    "Deny", "LastHit", "Stun", "Hex", "Silence", "Nuke", "Heal", "Buff",
    "TP", "Blink", "Dodge", "Miss", "Report", "Commend"
};

static const QStringList s_wc3names = {
    // War3 剧情人物
    "Arthas", "Thrall", "Jaina", "Illidan", "Tyrande", "Cairne", "Rexxar",
    "KelThuzad", "Sylvanas", "Muradin", "Uther", "Grom", "Voljin",
    "Archimonde", "Kiljaeden", "Mannoroth", "Tichondrius", "Malganis",

    // DotA 力量英雄 (经典名字)
    "Pudge", "Tiny", "Kunkka", "Beastmaster", "Clockwerk", "Omniknight",
    "Huskar", "Alchemist", "Brewmaster", "Treant", "Io", "Centaur",
    "Timbersaw", "Bristleback", "Tusk", "ElderTitan", "Legion", "EarthSpirit",
    "Axe", "Sven", "SandKing", "Slardar", "Tidehunter", "SkeletonKing",
    "Lifestealer", "NightStalker", "Doom", "SpiritBreaker", "Lycan", "ChaosKnight",
    "Undying", "Magnus", "Abaddon",

    // DotA 敏捷英雄
    "AntiMage", "Drow", "Juggernaut", "Mirana", "Morphling", "PhantomLancer",
    "Vengeful", "Riki", "Sniper", "Templar", "Luna", "BountyHunter",
    "Ursa", "Gyrocopter", "LoneDruid", "Naga", "Troll", "Ember",
    "Bloodseeker", "ShadowFiend", "Razor", "Venomancer", "FacelessVoid",
    "Phantom", "Viper", "Clinkz", "Brood", "Weaver", "Spectre",
    "Meepo", "Nyx", "Slark", "Medusa", "Terrorblade", "ArcWarden",

    // DotA 智力英雄
    "CrystalMaiden", "Puck", "Storm", "Windrunner", "Zeus", "Lina",
    "ShadowShaman", "Tinker", "Prophet", "Jakiro", "Chen", "Silencer",
    "Ogre", "Rubick", "Disruptor", "Keeper", "Skywrath", "Oracle", "Techies",
    "Bane", "Lich", "Lion", "WitchDoctor", "Enigma", "Necrolyte",
    "Warlock", "QueenOfPain", "DeathProphet", "Pugna", "Dazzle", "Leshrac",
    "DarkSeer", "Batrider", "AncientApparition", "Invoker", "Visage",

    // DotA 1 英雄真名 (老玩家一眼懂)
    "Magina", "Rylai", "Yurnero", "Alleria", "Raigor", "Kardel", "Gondar",
    "Nortrom", "Rhana", "Strygwyr", "Nevermore", "Mercurial", "Azwraith",
    "MogulKhan", "Bradwarden", "Lucifer", "Balanar", "Leoric", "Nessaj",
    "Mortred", "Anubarak", "Lanaya", "Ulfsaar", "Aggron", "Ostarion",
    "Rotundjere", "Demnok", "Boush", "Rhasta", "Ishkafel", "Krobelus",
    "Lesale", "Medusa", "Akasha", "Atropos", "Zet", "Kaolin", "Xin"
};

// 游戏常用后缀
static const QStringList s_suffixes = {
    "Pro", "Noob", "God", "King", "GG", "EZ", "Win", "Lose", "Gaming",
    "TV", "Show", "Zone", "Base", "City", "Team", "Clan", "Club",
    "CN", "US", "KR", "RU", "EU",
    "One", "Zero", "X", "Z", "S", "V",
    "Doto", "MMR", "Player", "Carry", "Supp", "Mid", "Off", "AFK"
};

// 头衔前缀
static const QStringList s_prefixes = {
    "Mr", "Dr", "Sir", "Miss", "Lady", "The", "iAm", "MyNameIs", "Real", "True",
    "Captain", "Carry", "Support", "Mid", "Jungle", "Best", "Top", "Only"
};

BotManager::BotManager(QObject *parent) : QObject(parent)
{
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &BotManager::onBotPendingTaskTimeout);
    timer->start(1000);
}

BotManager::~BotManager()
{
    stopAll();
    qDeleteAll(m_bots);
    m_bots.clear();
}

void BotManager::initializeBots(quint32 initialCount, const QString &configPath)
{
    // 1. 打印初始化头部
    LOG_INFO("🤖 [BotManager] 初始化序列启动");

    // 2. 清理旧数据
    stopAll();
    qDeleteAll(m_bots);
    m_bots.clear();
    m_currentFileIndex = 0;
    m_currentAccountIndex = 0;
    m_globalBotIdCounter = 1;
    m_allAccountFilePaths.clear();
    m_newAccountFilePaths.clear();

    LOG_INFO("   ├─ 🧹 环境清理: 完成");

    // 3. 读取配置文件
    QSettings settings(configPath, QSettings::IniFormat);
    m_targetServer = settings.value("bnet/server", "127.0.0.1").toString();
    m_targetPort = settings.value("bnet/port", 6112).toUInt();

     m_botDisplayName = settings.value("bots/display_name", "CC.Dota.XXX").toString();
    bool autoGenerate = settings.value("bots/auto_generate", false).toBool();
    int listNumber = settings.value("bots/list_number", 1).toInt();

    m_initialLoginCount = initialCount;
    if (listNumber < 1) listNumber = 1;
    if (listNumber > 10) listNumber = 10;

    LOG_INFO(QString("   ├─ ⚙️ 加载配置: %1").arg(QFileInfo(configPath).fileName()));
    LOG_INFO(QString("   │  ├─ 🖥️ 服务器: %1:%2").arg(m_targetServer).arg(m_targetPort));
    LOG_INFO(QString("   │  ├─ 👤 显示名: %1").arg(m_botDisplayName));
    LOG_INFO(QString("   │  ├─ 🏭 自动生成: %1").arg(autoGenerate ? "✅ 开启" : "⛔ 关闭"));
    LOG_INFO(QString("   │  └─ 📑 列表编号: #%1 (仅加载 bots_auto_%2.json)").arg(listNumber).arg(listNumber, 2, 10, QChar('0')));

    // 4. 生成或加载文件
    bool isNewFiles = createBotAccountFilesIfNotExist(autoGenerate, listNumber);

    // 5. 根据文件状态决定流程
    if (isNewFiles) {
        LOG_INFO("   └─ 🆕 模式切换: [批量注册模式]");

        // 只加载 "新生成的文件" 到注册队列
        m_registrationQueue.clear();

        int fileCount = m_newAccountFilePaths.size();
        for (int i = 0; i < fileCount; ++i) {
            const QString &path = m_newAccountFilePaths[i];
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                QJsonArray array = doc.array();
                for (const QJsonValue &val : qAsConst(array)) {
                    QJsonObject obj = val.toObject();
                    m_registrationQueue.enqueue(qMakePair(obj["u"].toString(), obj["p"].toString()));
                }
                file.close();
                LOG_INFO(QString("      ├─ 📄 加载新文件: %1").arg(QFileInfo(path).fileName()));
            }
        }

        m_totalRegistrationCount = m_registrationQueue.size();
        LOG_INFO(QString("      ├─ 🔢 待注册总数: %1 个").arg(m_totalRegistrationCount));

        // 双重检查
        if (m_totalRegistrationCount > 0) {
            m_isMassRegistering = true;
            LOG_INFO("      └─ 🚀 动作执行: 启动注册队列 (间隔防止封IP)");
            processNextRegistration();
        } else {
            LOG_INFO("      └─ ⚠️ 异常警告: 队列为空 -> 降级为常规加载");
            loadMoreBots(initialCount);
            startAll();
        }

    } else {
        LOG_INFO("   └─ ✅ 模式切换: [常规加载模式]");
        LOG_INFO("      ├─ 📂 状态: 所有账号文件已存在，跳过注册");
        LOG_INFO(QString("      ├─ 📥 目标加载数: %1 个 Bot").arg(initialCount));

        loadMoreBots(initialCount);
        startAll();

        LOG_INFO(QString("      └─ 🚀 动作执行: 启动连接 (Bot总数: %1)").arg(m_bots.size()));
    }
}

void BotManager::processNextRegistration()
{
    // 1. 队列为空 -> 流程结束 (闭环)
    if (m_registrationQueue.isEmpty()) {
        LOG_INFO("      └─ 🎉 [批量注册] 流程结束");
        LOG_INFO(QString("         ├─ 📊 总计处理: %1 个账号").arg(m_totalRegistrationCount));
        LOG_INFO(QString("         └─ 🚀 状态切换: 正在启动前 %1 个机器人...").arg(m_initialLoginCount));

        m_isMassRegistering = false;

        if (m_tempRegistrationClient) {
            m_tempRegistrationClient->deleteLater();
            m_tempRegistrationClient = nullptr;
        }

        loadMoreBots(m_initialLoginCount);
        startAll();
        return;
    }

    // 2. 取出下一个账号
    QPair<QString, QString> account = m_registrationQueue.dequeue();
    QString user = account.first;
    QString pass = account.second;

    int current = m_totalRegistrationCount - m_registrationQueue.size(); // 当前是第几个

    // 为了防止日志刷屏，仅每 10 个或发生错误时打印，或者第 1 个时打印
    if (current == 1 || current % 10 == 0) {
        int percent = (int)((double)current / m_totalRegistrationCount * 100);
        // 使用 ├─ 模拟它是初始化过程中的一个持续子项
        LOG_INFO(QString("      ├─ ⏳ [注册进度] %1/%2 (%3%) -> 当前: %4")
                     .arg(current, 3) // 占位符对齐
                     .arg(m_totalRegistrationCount)
                     .arg(percent, 2)
                     .arg(user));
    }

    // 3. 执行注册逻辑
    m_tempRegistrationClient = new Client(this);
    m_tempRegistrationClient->setBotDisplayName(m_botDisplayName);
    m_tempRegistrationClient->setCredentials(user, pass, Protocol_SRP_0x53);

    // 连接成功 -> 发送注册包 (延迟50ms确保握手完成)
    connect(m_tempRegistrationClient, &Client::connected, m_tempRegistrationClient, [this]() {
        QTimer::singleShot(50, m_tempRegistrationClient, &Client::createAccount);
    });

    // 定义通用结束步骤 (Lambda)
    auto finishStep = [this, user](bool success, QString msg) {
        if (!success) {
            LOG_ERROR(QString("      │  ❌ [注册失败] 用户: %1 | 原因: %2").arg(user, msg));
        }

        m_tempRegistrationClient->disconnectFromHost();

        // 延迟 200ms 后处理下一个 (递归调用)
        QTimer::singleShot(200, this, [this]() {
            if (m_tempRegistrationClient) {
                m_tempRegistrationClient->deleteLater();
                m_tempRegistrationClient = nullptr;
            }
            this->processNextRegistration();
        });
    };

    // 绑定结果信号
    connect(m_tempRegistrationClient, &Client::accountCreated, this, [finishStep]() {
        finishStep(true, "成功");
    });

    connect(m_tempRegistrationClient, &Client::socketError, this, [finishStep](QString err) {
        finishStep(false, err);
    });

    // 超时保护 (5秒)
    QTimer::singleShot(5000, m_tempRegistrationClient, [this, finishStep]() {
        if (m_tempRegistrationClient && m_tempRegistrationClient->isConnected()) {
            finishStep(false, "Timeout (超时)");
        }
    });

    m_tempRegistrationClient->connectToHost(m_targetServer, m_targetPort);
}

int BotManager::loadMoreBots(int count)
{
    // 1. 打印头部
    LOG_INFO(QString("   ├─ 📥 [增量加载] 请求增加: %1 个机器人").arg(count));

    int loadedCount = 0;

    while (loadedCount < count) {
        // 资源耗尽检查
        if (m_currentFileIndex >= m_allAccountFilePaths.size()) {
            LOG_INFO("   │  └─ ⚠️ [资源耗尽] 所有账号文件已全部加载完毕");
            LOG_INFO(QString("   │     ├─ 📊 当前索引: %1").arg(m_currentFileIndex));
            LOG_INFO(QString("   │     └─ 🗃️ 文件总数: %1").arg(m_allAccountFilePaths.size()));
            if (m_allAccountFilePaths.isEmpty()) {
                LOG_ERROR("   │        └─ ❌ [异常] 加载列表为空！(请检查 list_number 配置或文件生成逻辑)");
            }

            break;
        }

        QString currentFullPath = m_allAccountFilePaths[m_currentFileIndex];
        QString fileName = QFileInfo(currentFullPath).fileName();

        QFile file(currentFullPath);
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_ERROR(QString("   │  ❌ [读取失败] %1 -> 跳过").arg(fileName));
            m_currentFileIndex++;
            m_currentAccountIndex = 0;
            continue;
        }

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        if (!doc.isArray()) {
            LOG_INFO(QString("   │  ❌ [格式错误] %1 不是 JSON 数组 -> 跳过").arg(fileName));
            m_currentFileIndex++;
            continue;
        }

        QJsonArray array = doc.array();
        int totalInFile = array.size();
        int loadedFromThisFile = 0;

        // 打印文件节点信息
        LOG_INFO(QString("   │  ├─ 📂 读取来源: %1 (当前进度: %2/%3)")
                     .arg(fileName)
                     .arg(m_currentAccountIndex)
                     .arg(totalInFile));
        // 提取账号循环
        while (loadedCount < count && m_currentAccountIndex < totalInFile) {
            QJsonObject obj = array[m_currentAccountIndex].toObject();
            addBotInstance(obj["u"].toString(), obj["p"].toString());

            loadedCount++;
            m_currentAccountIndex++;
            loadedFromThisFile++;
        }

        // 打印本次文件提取结果
        if (loadedFromThisFile > 0) {
            LOG_INFO(QString("   │  │  ├─ ➕ 提取账号: %1 个").arg(loadedFromThisFile));
        }

        // 文件切换/状态更新
        if (m_currentAccountIndex >= totalInFile) {
            LOG_INFO("   │  │  └─ 🏁 文件读完: 切换下一个");
            m_currentFileIndex++;
            m_currentAccountIndex = 0;
        } else {
            // 还没读完，只是满足了本次 count 需求
            LOG_INFO(QString("   │  │  └─ ⏸️ 暂停读取: 剩余 %1 个账号待用").arg(totalInFile - m_currentAccountIndex));
        }
    }

    // 2. 打印总结
    QString statusIcon = (loadedCount >= count) ? "✅" : "⚠️";
    LOG_INFO(QString("   └─ %1 [加载统计] 实际增加: %2 / 目标: %3")
                 .arg(statusIcon).arg(loadedCount).arg(count));

    return loadedCount;
}

void BotManager::addBotInstance(const QString& username, const QString& password)
{
    Bot *bot = new Bot(m_globalBotIdCounter++, username, password);

    // 初始化组件
    bot->client = new class Client(this);
    bot->commandSource = From_Server;

    // 设置 Client 属性
    bot->client->setBotFlag(true); // 标记这是机器人连接
    bot->client->setGameTickInterval();
    bot->client->setCredentials(username, password, Protocol_SRP_0x53);

    // === 信号绑定 ===

    // 基础状态信号
    connect(bot->client, &Client::gameStarted, this, [this, bot]() { this->onBotGameStarted(bot); });
    connect(bot->client, &Client::gameCanceled, this, [this, bot]() { this->onBotGameCanceled(bot); });
    connect(bot->client, &Client::disconnected, this, [this, bot]() { this->onBotDisconnected(bot); });
    connect(bot->client, &Client::authenticated, this, [this, bot]() { this->onBotAuthenticated(bot); });
    connect(bot->client, &Client::accountCreated, this, [this, bot]() { this->onBotAccountCreated(bot); });
    connect(bot->client, &Client::visualHostLeft, this, [this, bot]() { this->onBotVisualHostLeft(bot); });
    connect(bot->client, &Client::gameCreateSuccess, this, [this, bot]() { this->onBotGameCreateSuccess(bot); });
    connect(bot->client, &Client::socketError, this, [this, bot](QString error) { this->onBotError(bot, error); });
    connect(bot->client, &Client::hostJoinedGame, this, [this, bot](const QString &name) { this->onHostJoinedGame(bot, name); });
    connect(bot->client, &Client::gameCreateFail, this, [this, bot](GameCreationStatus status) { this->onBotGameCreateFail(bot, status); });

    m_bots.append(bot);

    LOG_INFO(QString("   │  │  ├─ 🤖 实例化: %1 (ID: %2)").arg(username).arg(bot->id));
}

bool BotManager::createGame(const QString &hostName, const QString &gameName, CommandSource commandSource, const QString &clientId)
{
    // --- 1. 打印任务请求头部 ---
    QString sourceStr = (commandSource == From_Client) ? "客户端聊天窗口" : "服务端命令窗口";

    LOG_INFO("🎮 [创建游戏任务启动]");
    LOG_INFO(QString("   ├─ 👤 虚拟房主: %1").arg(hostName));
    LOG_INFO(QString("   ├─ 📝 游戏名称: %1").arg(gameName));
    LOG_INFO(QString("   ├─ 🆔 命令来源: %1 (%2)").arg(sourceStr, clientId.left(8)));

    Bot *targetBot = nullptr;
    bool needConnect = false;

    // 1. 优先复用在线空闲的
    for (Bot *bot : qAsConst(m_bots)) {
        if (bot->state == BotState::Idle && bot->client && bot->client->isConnected()) {
            targetBot = bot;
            break;
        }
    }

    if (targetBot) {
        LOG_INFO(QString("   ├─ ✅ 执行动作: 指派在线空闲机器人 [%1] 创建房间").arg(targetBot->username));

        // 更新 Bot 基础信息
        targetBot->hostJoined = false;
        targetBot->commandSource = commandSource;
        targetBot->gameInfo.createTime = QDateTime::currentMSecsSinceEpoch();
        targetBot->gameInfo.clientId = clientId;
        targetBot->gameInfo.hostName = hostName;
        targetBot->gameInfo.gameName = gameName;
        targetBot->state = BotState::Creating;
        targetBot->client->setHost(hostName);
        targetBot->client->setBotDisplayName(m_botDisplayName);
        targetBot->client->createGame(gameName, "", Provider_TFT_New, Game_TFT_Custom, SubType_None, Ladder_None, commandSource);

        LOG_INFO("   └─ 🚀 执行动作: 立即发送 CreateGame 指令");
        return true;
    }

    // 2. 复用离线的
    if (!targetBot) {
        for (Bot *bot : qAsConst(m_bots)) {
            // 只要不是 Connecting, Creating, Idle (即 Disconnected 或 Error) 都可以复用
            if (bot->state == BotState::Disconnected) {
                targetBot = bot;
                needConnect = true;
                LOG_INFO(QString("   ├─ ♻️ 资源复用: 唤醒离线机器人 [%1] (ID: %2)").arg(targetBot->username).arg(targetBot->id));
                break;
            }
        }
    }

    // 3. 动态扩容
    if (!targetBot) {
        LOG_WARNING("   ├─ ⚠️ 当前池中无可用 Bot，尝试从文件加载...");

        if (loadMoreBots(1) > 0) {
            targetBot = m_bots.last();
            needConnect = true;
            LOG_INFO(QString("   ├─ 📂 动态扩容成功: 从文件加载了 [%1]").arg(targetBot->username));
        } else {
            LOG_ERROR("   └─ ❌ 动态扩容失败: 所有账号文件已耗尽！");
            return false;
        }
    }

    // 统一处理: 启动连接流程 (适用于 阶段2 和 阶段3)
    if (needConnect && targetBot) {
        // 1. 更新 Bot 基础信息
        targetBot->hostJoined = false;
        targetBot->commandSource = commandSource;
        targetBot->gameInfo.createTime = QDateTime::currentMSecsSinceEpoch();
        targetBot->gameInfo.clientId = clientId;
        targetBot->gameInfo.hostName = hostName;
        targetBot->gameInfo.gameName = gameName;

        // 2. 确保 Client 对象存在且信号已绑定
        if (!targetBot->client) {
            targetBot->client = new Client(this);
            targetBot->client->setGameTickInterval();
            targetBot->client->setBotDisplayName(m_botDisplayName);
            targetBot->client->setCredentials(targetBot->username, targetBot->password, Protocol_SRP_0x53);

            // 绑定信号
            connect(targetBot->client, &Client::gameStarted, this, [this, targetBot]() { this->onBotGameStarted(targetBot); });
            connect(targetBot->client, &Client::gameCanceled, this, [this, targetBot]() { this->onBotGameCanceled(targetBot); });
            connect(targetBot->client, &Client::disconnected, this, [this, targetBot]() { this->onBotDisconnected(targetBot); });
            connect(targetBot->client, &Client::authenticated, this, [this, targetBot]() { this->onBotAuthenticated(targetBot); });
            connect(targetBot->client, &Client::accountCreated, this, [this, targetBot]() { this->onBotAccountCreated(targetBot); });
            connect(targetBot->client, &Client::visualHostLeft, this, [this, targetBot]() { this->onBotVisualHostLeft(targetBot); });
            connect(targetBot->client, &Client::gameCreateSuccess, this, [this, targetBot]() { this->onBotGameCreateSuccess(targetBot); });
            connect(targetBot->client, &Client::socketError, this, [this, targetBot](QString error) { this->onBotError(targetBot, error); });
            connect(targetBot->client, &Client::hostJoinedGame, this, [this, targetBot](const QString &name) { this->onHostJoinedGame(targetBot, name); });
            connect(targetBot->client, &Client::gameCreateFail, this, [this, targetBot](GameCreationStatus status) { this->onBotGameCreateFail(targetBot, status); });
        } else {
            targetBot->client->setCredentials(targetBot->username, targetBot->password, Protocol_SRP_0x53);
        }

        // 3. 设置挂起任务 (Pending Task)
        targetBot->pendingTask.hasTask = true;
        targetBot->pendingTask.hostName = hostName;
        targetBot->pendingTask.gameName = gameName;
        targetBot->pendingTask.commandSource = commandSource;
        targetBot->pendingTask.requestTime = QDateTime::currentMSecsSinceEpoch();

        // 4. 发起 TCP 连接
        targetBot->state = BotState::Connecting;
        targetBot->client->connectToHost(m_targetServer, m_targetPort);

        LOG_INFO(QString("   └─ ⏳ 执行动作: 启动连接流程 [%1] (任务已挂起，等待登录)").arg(targetBot->username));
        return true;
    }

    return false;
}

void BotManager::startAll()
{
    int delay = 0;

    for (Bot *bot : qAsConst(m_bots)) {
        if (!bot->client) continue;
        QTimer::singleShot(delay, this, [this, bot]() {
            if (m_bots.contains(bot) && bot->client) {
                bot->state = BotState::Unregistered;
                LOG_INFO(QString("[%1] 发起连接...").arg(bot->username));
                bot->client->connectToHost(m_targetServer, m_targetPort);
            }
        });

        delay += 200;
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

void BotManager::removeGame(Bot *bot, bool disconnectFlag)
{
    if (!bot) return;

    // 1. 从全局活跃房间表中移除
    QString lowerName = bot->gameInfo.gameName.toLower();
    if (!lowerName.isEmpty() && m_activeGames.contains(lowerName)) {
        if (m_activeGames.value(lowerName) == bot) {
            m_activeGames.remove(lowerName);
            LOG_INFO(QString("🔓 释放房间名锁定: %1").arg(lowerName));
        }
    }

    // 2. 通知 Client 层停止广播并断开玩家
    if (bot->client) {
        bot->client->cancelGame();
    }

    // 3. 重置 Bot 逻辑数据
    bot->resetGameState();

    // 4. 如果标记为断线，强制覆盖状态
    if (disconnectFlag) {
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
    // 1. 打印事件根节点
    LOG_INFO(QString("🔑 [认证成功] Bot-%1 (%2)").arg(bot->id).arg(bot->username));

    // 2. 检查是否有挂起的任务 (Pending Task)
    if (bot->pendingTask.hasTask) {
        // 分支 A: 处理任务
        LOG_INFO("   ├─ 🎮 [发现挂起任务]");
        LOG_INFO(QString("   │  ├─ 👤 虚拟房主: %1").arg(bot->pendingTask.hostName));
        LOG_INFO(QString("   │  └─ 📝 目标房名: %1").arg(bot->pendingTask.gameName));

        // 更新状态
        bot->state = BotState::Creating;

        // 设置虚拟房主名
        bot->client->setHost(bot->pendingTask.hostName);

        // 🚀 立即执行创建游戏指令
        bot->client->createGame(
            bot->pendingTask.gameName,
            "",
            Provider_TFT_New,
            Game_TFT_Custom,
            SubType_None,
            Ladder_None,
            bot->pendingTask.commandSource
            );

        // 🧹 清除任务标记，防止重复执行
        bot->pendingTask.hasTask = false;

        // 闭环日志
        LOG_INFO("   └─ 🚀 [动作执行] 发送 CreateGame 指令 -> 状态切换: Creating");

    } else {
        // 分支 B: 无任务
        bot->state = BotState::Idle;

        // 闭环日志
        LOG_INFO("   └─ 💤 [状态切换] 无挂起任务 -> 进入 Idle (空闲待机)");
    }
}

void BotManager::onBotAccountCreated(Bot *bot)
{
    if (!bot) return;
    LOG_INFO(QString("   └─ 🆕 [%1] 账号注册成功，正在尝试登录...").arg(bot->username));
}

void BotManager::onCommandReceived(const QString &userName, const QString &clientId, const QString &command, const QString &text)
{
    // 1. 频率限制 (Cooldown) - 防止恶意刷屏
    const qint64 CREATE_COOLDOWN_MS = 1000;
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (m_lastHostTime.contains(clientId)) {
        qint64 diff = now - m_lastHostTime.value(clientId);
        if (diff < CREATE_COOLDOWN_MS) {
            // 操作太频繁，请稍后再试。
            quint32 remaining = (quint32)(CREATE_COOLDOWN_MS - diff);
            m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_COOLDOWN, remaining);
            return;
        }
    }

    // 更新最后操作时间
    m_lastHostTime.insert(clientId, now);

    // 2. 权限检查
    if (!m_netManager->isClientRegistered(clientId)) {
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_PERMISSION_DENIED);
        LOG_WARNING(QString("   └─ ⚠️ 忽略未注册用户的指令: %1 (%2)").arg(userName, clientId));
        return;
    }

    QString fullCmd = command + (text.isEmpty() ? "" : " " + text);

    LOG_INFO("📨 [收到用户指令]");
    LOG_INFO(QString("   ├─ 👤 发送者: %1 (UUID: %2...)").arg(userName, clientId.left(8)));
    LOG_INFO(QString("   └─ 💬 内容:   %1").arg(fullCmd));

    // 3. 处理 /host 指令
    if (command == "/host") {        
        // 全局前置检查：是否已经拥有房间？
        for (Bot *bot : qAsConst(m_bots)) {
            if (bot->state != BotState::Disconnected &&
                bot->state != BotState::InLobby &&
                bot->state != BotState::Idle &&
                bot->gameInfo.clientId == clientId) {
                // 你已经有一个正在进行的游戏/房间了！请先 /unhost 或结束游戏。
                LOG_WARNING(QString("   └─ ⚠️ 拦截重复开房请求: 用户 %1 已在 Bot-%2 中").arg(userName).arg(bot->id));
                m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_ALREADY_IN_GAME);
                return;
            }
        }

        LOG_INFO("🎮 [创建房间请求记录]");

        QStringList parts = text.split(" ", Qt::SkipEmptyParts);

        // 检查参数数量
        if (parts.size() < 2) {
            // 格式错误。用法: /host <模式> <房名>
            m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_PARAM_ERROR);
            return;
        }

        QString mapModel = parts[0].toLower();
        QString inputGameName = parts.mid(1).join(" ");

        // 4.1 检查地图模式
        QVector<QString> allowModels = {
            "ap", "cm", "rd", "sd", "ar", "xl",
            "aptb", "cmtb", "rdtb", "sdtb", "artb", "xltb",
            "ap83", "cm83", "rd83", "sd83", "ar83", "xl83",
            "ap83tb", "cm83tb", "rd83tb", "sd83tb", "ar83tb", "xl83tb"
        };

        if (!allowModels.contains(mapModel)) {
            // 不支持的地图模式
            m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_MAP_NOT_SUPPORTED);
            return;
        }

        // 构建数据结构
        CommandInfo commandInfo;
        commandInfo.clientId = clientId;
        commandInfo.text = text.trimmed();
        commandInfo.timestamp = QDateTime::currentMSecsSinceEpoch();
        m_commandInfos.insert(userName, commandInfo);

        LOG_INFO(QString("   ├─ 👤 用户: %1").arg(userName));
        LOG_INFO(QString("   ├─ 🆔 UUID: %1").arg(clientId));
        LOG_INFO(QString("   └─ 💾 已存入 HostMap (当前缓存数: %1)").arg(m_commandInfos.size()));

        // 4.2 房名预处理与截断
        LOG_INFO("🎮 [创建房间基本信息]");
        QString baseName = text.trimmed();
        if (baseName.isEmpty()) {
            baseName = QString("%1's Game").arg(userName);
            LOG_INFO(QString("   ├─ ℹ️ 自动命名: %1").arg(baseName));
        } else {
            LOG_INFO(QString("   ├─ 📝 指定名称: %1").arg(baseName));
        }

        QString suffix = QString(" (%1/%2)").arg(1).arg(10);

        const int MAX_BYTES = 31;
        int suffixBytes = suffix.toUtf8().size();
        int availableBytes = MAX_BYTES - suffixBytes;

        LOG_INFO(QString("   ├─ 📏 空间计算: 总限 %1 Bytes | 后缀占用 %2 Bytes | 剩余可用 %3 Bytes")
                     .arg(MAX_BYTES).arg(suffixBytes).arg(availableBytes));

        if (availableBytes <= 0) {
            LOG_ERROR("   └─ ❌ 失败: 后缀过长，无空间容纳房名");
            // 房间名过长
            m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_NAME_TOO_LONG);
            return;
        }

        QByteArray nameBytes = baseName.toUtf8();
        int originalSize = nameBytes.size();
        bool wasTruncated = false;

        if (nameBytes.size() > availableBytes) {
            nameBytes = nameBytes.left(availableBytes);
            while (nameBytes.size() > 0) {
                QString tryStr = QString::fromUtf8(nameBytes);
                if (tryStr.toUtf8().size() == nameBytes.size() && !tryStr.contains(QChar::ReplacementCharacter)) {
                    break;
                }
                nameBytes.chop(1);
            }
            wasTruncated = true;
        }

        // 拼接最终房名
        QString finalGameName = QString::fromUtf8(nameBytes) + suffix;

        // 4.3 检查是否重名
        if (m_activeGames.contains(finalGameName.toLower())) {
            // 房间名已存在
            m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_GAME_NAME_EXISTS);
            return;
        }

        // 打印截断结果
        if (wasTruncated) {
            LOG_INFO(QString("   ├─ ✂️ 触发截断: 原始 %1 Bytes -> 截断后 %2 Bytes")
                         .arg(originalSize).arg(nameBytes.size()));
        }

        LOG_INFO(QString("   ├─ ✅ 最终房名: [%1]").arg(finalGameName));
        LOG_INFO("   └─ 🚀 执行动作: 调用 createGame()");

        // 4.4 执行创建
        bool scheduled = createGame(userName, finalGameName, From_Client, clientId);
        if (!scheduled) {
            // 暂时无法创建房间，请稍后再试。
            m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_NO_BOTS_AVAILABLE);
        }
    } else if (command == "/swap") {
        LOG_INFO("🔄 [交换请求记录]");

        // 1. 寻找用户当前所在的、且处于等待状态的 Bot
        Bot *targetBot = nullptr;
        for (Bot *bot : qAsConst(m_bots)) {
            if (bot->state == BotState::Waiting && bot->gameInfo.clientId == clientId) {
                targetBot = bot;
                break;
            }
        }

        if (!targetBot) {
            // 用户没有正在主持的房间，或者房间已经开始
            m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_PERMISSION_DENIED);
            return;
        }

        // 2. 解析参数
        QStringList parts = text.split(" ", Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            // 参数不足
            m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_PARAM_ERROR);
            return;
        }

        bool ok1, ok2;
        int s1 = parts[0].toInt(&ok1);
        int s2 = parts[1].toInt(&ok2);

        if (ok1 && ok2) {
            LOG_INFO(QString("   ├─ 🔢 参数: %1 <-> %2").arg(s1).arg(s2));
            LOG_INFO(QString("   └─ 🚀 执行动作: 调用 swapSlots()"));

            // 3. 调用 Client 执行交换
            if (targetBot->client) {
                targetBot->client->swapSlots(s1, s2);
            }
        } else {
            LOG_ERROR("   └─ ❌ 格式错误: 参数必须为数字");
        }
    }
    else if (command == "/start") {
        LOG_INFO("🚀 [启动请求记录]");

        // 1. 寻找 Bot
        Bot *targetBot = nullptr;
        for (Bot *bot : qAsConst(m_bots)) {
            // 必须是 Waiting 状态 (即已经在房间里) 且是房主
            if (bot->state == BotState::Waiting && bot->gameInfo.clientId == clientId) {
                targetBot = bot;
                break;
            }
        }

        if (!targetBot) {
            m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_PERMISSION_DENIED);
            return;
        }

        LOG_INFO(QString("   ├─ 👤 房主: %1").arg(userName));

        // 2. 执行启动
        if (targetBot->client) {
            // 可选：检查人数
            if (targetBot->client->getOccupiedSlots() < 2) {
                // m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_NOT_ENOUGH_PLAYERS);
                // return;
            }

            LOG_INFO("   └─ 🚀 执行动作: 调用 startGame()");
            targetBot->client->startGame();

            // 更新 Bot 状态为 Starting
            targetBot->state = BotState::Starting;
            emit botStateChanged(targetBot->id, targetBot->username, targetBot->state);
        }
    }
    else if (command == "/latency" || command == "/lat") {
        LOG_INFO("🚀 [修改游戏延迟]");

        // 1. 寻找用户当前所在的、且处于游戏状态的 Bot
        Bot *targetBot = nullptr;
        for (Bot *bot : qAsConst(m_bots)) {
            if (bot->state == BotState::InGame && bot->gameInfo.clientId == clientId) {
                targetBot = bot;
                break;
            }
        }

        if (!targetBot) {
            m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_PERMISSION_DENIED);
            return;
        }

        int val = text.toInt();
        if (val > 0) {
            targetBot->client->setGameTickInterval((quint16)val);
        }
    }
    // ==================== 处理 /unhost ====================
    else if (command == "/unhost") {
        LOG_INFO("🛑 [取消房间流程] 指令接收成功");

        Bot *targetBot = nullptr;
        // 1. 寻找该 UUID 拥有的活跃机器人
        for (Bot *bot : qAsConst(m_bots)) {
            // 只要不是断开连接状态，且 UUID 匹配
            if (bot->state != BotState::Disconnected && bot->gameInfo.clientId == clientId) {
                targetBot = bot;
                break;
            }
        }

        if (targetBot) {
            LOG_INFO(QString("   ├─ 👤 房主: %1").arg(targetBot->gameInfo.hostName));
            LOG_INFO(QString("   └─ 🚀 正在执行解散: Bot-%1 (%2)").arg(targetBot->id).arg(targetBot->gameInfo.gameName));

            // 2. 调用 removeGame，它内部会执行 bot->client->cancelGame()
            // 房主主动取消，不需要标记断线 (disconnectFlag = false)
            removeGame(targetBot, false);

            // 3. 通知 Launcher 端 (同步 UI 状态)
            m_netManager->sendMessageToClient(clientId, S_C_MESSAGE, MSG_HOST_UNHOST_GAME);

        } else {
            LOG_WARNING(QString("   └─ ❌ 拒绝请求: UUID %1 当前没有正在托管的房间").arg(clientId.left(8)));
            m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_PERMISSION_DENIED);
        }
    }
    // ==================== 处理 /bot ====================
    else if (command == "/bot") {
        LOG_INFO("🤖 [Bot 切换流程]");
        LOG_INFO("   └─ 🚀 执行动作: 切换 Bot 状态/所有者");
    }
    // ==================== 未知指令 ====================
    else {
        LOG_WARNING("⚠️ [指令未处理]");
        LOG_INFO(QString("   └─ ❓ 未知命令: %1 (将被忽略)").arg(command));
    }
}

void BotManager::onBotGameCreateSuccess(Bot *bot)
{
    if (!bot) return;

    // 1. 更新状态
    bot->hostJoined = false;
    bot->state = BotState::Reserved;
    bot->gameInfo.createTime = QDateTime::currentMSecsSinceEpoch();

    QString lowerName = bot->gameInfo.gameName.toLower();
    if (!lowerName.isEmpty()) {
        if (m_activeGames.contains(lowerName)) {
            LOG_WARNING(QString("⚠️ 状态同步警告: 房间名 %1 已存在于列表中").arg(lowerName));
        }
        m_activeGames.insert(lowerName, bot);
        LOG_INFO(QString("✅ [全局注册] 房间名已锁定: %1 -> Bot-%2").arg(lowerName).arg(bot->id));
    }

    // 2. 获取房主 UUID
    QString clientId = bot->gameInfo.clientId;

    // 3. 打印头部日志
    LOG_INFO("🎮 [房间创建完成回调]");
    LOG_INFO(QString("   ├─ 🤖 执行实例: %1").arg(bot->username));
    LOG_INFO(QString("   ├─ 👤 归属用户: %1").arg(clientId));
    LOG_INFO(QString("   └─ 🏠 房间名称: %1").arg(bot->gameInfo.gameName));

    LOG_INFO(QString("🎮 [房间创建成功] 准备通知客户端进场: %1").arg(bot->gameInfo.gameName));

    // 4. 发送 TCP 控制指令让客户端进入
    if (m_netManager) {
        bool okToGameLoby = m_netManager->sendEnterRoomCommand(clientId, m_controlPort, bot->commandSource == From_Server);
        bool okToLauncher = m_netManager->sendMessageToClient(clientId, S_C_MESSAGE, MSG_HOST_CREATED_GAME);

        if (okToGameLoby && okToLauncher) {
            LOG_INFO(QString("   └─ 🚀 自动进入: 指令已发送 (目标端口: %1)").arg(m_controlPort));
        } else {
            LOG_ERROR("   └─ ❌ 自动进入: 发送失败 (目标用户不在线或未记录通道)");
        }
    } else {
        LOG_INFO("   └─ 🛑 系统错误: NetManager 未绑定，无法发送指令");
    }

    // 5. 广播状态变更
    emit botStateChanged(bot->id, bot->username, bot->state);
}

void BotManager::onBotGameCreateFail(Bot *bot, GameCreationStatus status)
{
    if (!bot) return;

    // 1. 将魔兽原始状态码转换为业务错误码
    ErrorCode finalErr = ERR_UNKNOWN;
    QString reasonStr = "未知错误";

    switch (status) {
    case 0x01: // GameCreate_NameExists
        finalErr = ERR_GAME_NAME_EXISTS;
        reasonStr = "房间名已经存在";
        break;
    case 0x02: // GameCreate_TypeUnavailable
        finalErr = ERR_MAP_NOT_SUPPORTED;
        reasonStr = "地图类型不支持";
        break;
    case 0x03: // GameCreate_Error
        finalErr = ERR_CREATE_ERROR;
        reasonStr = "游戏创建有错误";
        break;
    default:
        finalErr = ERR_UNKNOWN;
        reasonStr = QString("未知错误（0x%1）").arg(status, 0, 16);
        break;
    }

    // 2. 打印详细的树状日志
    LOG_ERROR(QString("❌ [创建失败] Bot-%1 (%2)").arg(bot->id).arg(bot->username));
    LOG_INFO(QString("   ├─ 👤 归属用户: %1").arg(bot->gameInfo.clientId.left(8)));
    LOG_INFO(QString("   ├─ 🏠 目标房间: %1").arg(bot->gameInfo.gameName));
    LOG_INFO(QString("   └─ 📝 失败原因: %1 (Raw Status: 0x%2)").arg(reasonStr).arg(status, 0, 16));

    // 3. 通知客户端
    if (!bot->gameInfo.clientId.isEmpty()) {
        m_netManager->sendMessageToClient(bot->gameInfo.clientId, S_C_ERROR, finalErr);
    }

    // 4. 清理资源并重置状态
    removeGame(bot);

    LOG_INFO("   └─ 🔄 [状态重置] 游戏信息已清除，Bot 已归还池中");
}

void BotManager::onBotVisualHostLeft(Bot *bot)
{
    if (!bot || !bot->client) return;

    QString newUuid = "";
    QString newHostName = "";

    // 1. 获取 Client 中的玩家映射表
    const QMap<quint8, PlayerData> &players = bot->client->getPlayers();

    // 2. 遍历寻找现在谁是 VisualHost
    for (auto it = players.begin(); it != players.end(); ++it) {
        const PlayerData &p = it.value();

        // 排除机器人自己 (PID 2)
        // 寻找那个在 Client::onPlayerDisconnected 中被设为 isVisualHost = true 的真人
        if (p.pid != 2 && p.isVisualHost) {
            newUuid = p.clientUuid;
            newHostName = p.name;
            break;
        }
    }

    // 3. 执行所有权交接或释放
    if (!newUuid.isEmpty()) {
        LOG_INFO(QString("👤 [所有权交接] 房主离开，Bot-%1 控制权移交给: %2 (UUID: %3)")
                     .arg(bot->id).arg(newHostName, newUuid));

        bot->gameInfo.clientId = newUuid;
        bot->gameInfo.hostName = newHostName;
    }
    else {
        LOG_INFO(QString("👤 [所有权释放] 房主离开，无有效 UUID 继承人。Bot-%1 现在变为公共状态")
                     .arg(bot->id));

        bot->gameInfo.clientId = "";
    }
}

void BotManager::onHostJoinedGame(Bot *bot, const QString &hostName)
{
    if (!bot) return;

    if (bot->state == BotState::Reserved) {

        // 1. 更新状态
        bot->hostJoined = true;
        bot->state = BotState::Waiting;

        // 2. 打印日志
        LOG_INFO(QString("👤 [房主进入] Bot-%1 | 玩家: %2 | 动作: 解锁房间 (State -> Waiting)")
                     .arg(bot->id).arg(hostName));

        // 3. 状态改变
        emit botStateChanged(bot->id, bot->username, bot->state);

        // 4. 回复状态
        quint64 joinedTime = QDateTime::currentMSecsSinceEpoch();
        m_netManager->sendMessageToClient(bot->gameInfo.clientId, S_C_MESSAGE, MSG_HOST_JOINED_GAME, joinedTime);
    }
}

void BotManager::onBotPendingTaskTimeout()
{
    quint64 now = QDateTime::currentMSecsSinceEpoch();
    const quint64 TIMEOUT_MS = 3000;
    const quint64 HOST_JOIN_TIMEOUT_MS = 5000;

    for (int i = 0; i < m_bots.size(); ++i) {
        Bot *bot = m_bots[i];

        // 只检查有挂起任务的
        if (bot->pendingTask.hasTask) {

            if (now - bot->pendingTask.requestTime > TIMEOUT_MS) {
                // 1. 打印根节点
                LOG_INFO(QString("🚨 [任务超时] Bot-%1 (%2)").arg(bot->id).arg(bot->username));
                LOG_INFO(QString("   ├─ ⏱️ 耗时: %1 ms (阈值: %2 ms)").arg(now - bot->pendingTask.requestTime).arg(TIMEOUT_MS));
                LOG_INFO(QString("   ├─ 📝 任务: %1").arg(bot->pendingTask.gameName));

                // 2. 清除标记
                bot->pendingTask.hasTask = false;

                // 3. 通知用户
                if (!bot->pendingTask.clientId.isEmpty()) {
                    m_netManager->sendMessageToClient(bot->pendingTask.clientId, S_C_ERROR, ERR_TASK_TIMEOUT);
                    LOG_INFO(QString("   ├─ 👤 通知用户: %1 (Reason: 1-Timeout)").arg(bot->pendingTask.clientId.left(8)));
                }

                // 4. 强制断线
                bot->state = BotState::Disconnected;
                LOG_INFO("   └─ 🛡️ [强制动作] 标记为 Disconnected");
            }
        }

        // 检查房主加入超时
        if (bot->state == BotState::Reserved && !bot->hostJoined) {

            const quint64 diff = now - bot->gameInfo.createTime;

            if (diff > HOST_JOIN_TIMEOUT_MS) {
                // 1. 打印日志
                LOG_INFO(QString("🚨 [加入超时] Bot-%1 (%2)").arg(bot->id).arg(bot->username));
                LOG_INFO(QString("   ├─ ⏱️ 耗时: %1 ms (阈值: %2 ms)").arg(diff).arg(HOST_JOIN_TIMEOUT_MS));
                LOG_INFO(QString("   ├─ 🏠 房间: %1").arg(bot->gameInfo.gameName));
                LOG_INFO(QString("   └─ 👤 等待房主: %1 (未出现)").arg(bot->gameInfo.hostName));

                // 2. 通知客户端
                if (!bot->gameInfo.clientId.isEmpty()) {
                    m_netManager->sendMessageToClient(bot->gameInfo.clientId, S_C_ERROR, ERR_WAIT_HOST_TIMEOUT);
                }

                // 3. 取消游戏
                removeGame(bot, false);
                bot->client->cancelGame();
            }
        }
    }
}

void BotManager::onBotError(Bot *bot, QString error)
{
    if (!bot) return;

    // 1. 打印异常根节点
    LOG_ERROR(QString("┌── ❌ [Bot异常中断] Bot-%1 (%2)").arg(bot->id).arg(bot->username));
    LOG_INFO(QString("├── 状态: %1 | 原因: %2").arg(static_cast<int>(bot->state)).arg(error));

    QString ownerId = bot->gameInfo.clientId;

    // 2. 针对不同业务阶段进行精确通知
    if (!ownerId.isEmpty()) {
        // 场景 A: 正在向魔兽引擎申请房间 (Creating) 或 已申请成功正等待房主 (Reserved)
        if (bot->state == BotState::Creating || bot->state == BotState::Reserved) {
            LOG_INFO(QString("├── 通知: 用户 %1 (Code: ERR_CREATE_INTERRUPTED)").arg(ownerId.left(8)));
            m_netManager->sendMessageToClient(ownerId, S_C_ERROR, ERR_CREATE_INTERRUPTED);
        }
        // 场景 B: 房主已在大厅 (Waiting)
        else if (bot->state == BotState::Waiting) {
            LOG_INFO(QString("├── 通知: 用户 %1 (Code: MSG_HOST_UNHOST_GAME)").arg(ownerId.left(8)));
            m_netManager->sendMessageToClient(ownerId, S_C_MESSAGE, MSG_HOST_UNHOST_GAME);
        }
        // 场景 C: 游戏内 (InGame)
        else if (bot->state == BotState::InGame) {
            LOG_INFO(QString("├── 警报: 正在进行的房间 [%1] 已由于Bot崩溃销毁").arg(bot->gameInfo.gameName));
        }
    }

    // 3. 处理挂起任务
    if (bot->pendingTask.hasTask && !bot->pendingTask.clientId.isEmpty()) {
        QString taskOwner = bot->pendingTask.clientId;
        bot->pendingTask.hasTask = false;
        m_netManager->sendMessageToClient(taskOwner, S_C_ERROR, ERR_TASK_INTERRUPTED);
        LOG_INFO(QString("├── 清理: 已取消用户 %1 的挂起任务: %2").arg(taskOwner.left(8), bot->pendingTask.gameName));
    }

    // 4. 执行资源回收与状态重置
    removeGame(bot, true);
    emit botStateChanged(bot->id, bot->username, bot->state);

    // 5. 自动重连计划
    if (bot->client && !bot->client->isConnected()) {
        int retryDelay = 5000 + (bot->id * 1000);
        LOG_INFO(QString("└── 动作: 计划于 %1ms 后尝试自动重连").arg(retryDelay));
        QTimer::singleShot(retryDelay, this, [this, bot]() {
            if (m_bots.contains(bot) && bot->client && !bot->client->isConnected()) {
                bot->client->connectToHost(m_targetServer, m_targetPort);
            }
        });
    } else {
        LOG_INFO("└── 动作: 流程结束 (无需重连)");
    }
}

void BotManager::onBotDisconnected(Bot *bot)
{
    if (!bot) return;

    // 1. 打印根节点
    LOG_INFO(QString("🔌 [断开连接] Bot-%1 (%2)").arg(bot->id).arg(bot->username));

    // 2. 资源清理
    removeGame(bot, true);

    // 3. 状态变更
    emit botStateChanged(bot->id, bot->username, bot->state);

    // 4. 闭环
    LOG_INFO("   └─ 🧹 [清理完成] 状态已更新为 Disconnected");
}

void BotManager::onBotGameCanceled(Bot *bot)
{
    if (!bot) return;

    // 1. 防止重复处理
    if (bot->state == BotState::Idle && bot->gameInfo.gameName.isEmpty()) {
        return;
    }

    LOG_INFO(QString("🔄 [状态同步] 收到 Client 取消信号: Bot-%1").arg(bot->id));

    // 2. 从全局活跃游戏列表 (m_activeGames) 中移除
    QString lowerName = bot->gameInfo.gameName.toLower();
    if (!lowerName.isEmpty()) {
        if (m_activeGames.value(lowerName) == bot) {
            m_activeGames.remove(lowerName);
            LOG_INFO(QString("   ├─ 🔓 释放房间名锁定: %1").arg(lowerName));
        }
    }

    // 3. 重置 Bot
    bot->resetGameState();

    // 4. 通知上层应用
    emit botStateChanged(bot->id, bot->username, bot->state);

    LOG_INFO("   └─ ✅ Bot 状态已更新为 Idle");
}

void BotManager::onBotGameStarted(Bot *bot)
{
    if (!bot) return;

    // 1. 防止重复处理
    if (bot->state == BotState::InGame) {
        return;
    }

    // 2. 更新 Bot 状态
    bot->state = BotState::InGame;

    // 3. 记录游戏开始时间
    bot->gameInfo.gameStartTime = QDateTime::currentMSecsSinceEpoch();

    // 4. 打印日志
    LOG_INFO(QString("⚔️ [游戏开始] Bot-%1 (%2) 进入游戏状态").arg(bot->id).arg(bot->username));
    LOG_INFO(QString("   ├─ 🏠 房间: %1").arg(bot->gameInfo.gameName));

    // 获取当前人数
    if (bot->client) {
        LOG_INFO(QString("   └─ 👥 玩家: %1").arg(bot->client->getSlotInfoString()));
    }

    emit botStateChanged(bot->id, bot->username, bot->state);
}

// === 辅助函数 ===

QChar BotManager::randomCase(QChar c)
{
    // 50% 概率大写，50% 概率小写
    return (QRandomGenerator::global()->bounded(2) == 0) ? c.toLower() : c.toUpper();
}

QString BotManager::generateRandomPassword(int length)
{
    if (length < 8) length = 8;

    // 1. 定义字符池
    const QString lower = "abcdefghijklmnopqrstuvwxyz";
    const QString upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const QString nums  = "0123456789";
    const QString special = "~!@#$%^&*()_+-=[]{}<>;:,'.";

    QString password;

    // 2. 每种类型至少包含一个
    password.append(lower.at(QRandomGenerator::global()->bounded(lower.length())));
    password.append(upper.at(QRandomGenerator::global()->bounded(upper.length())));
    password.append(nums.at(QRandomGenerator::global()->bounded(nums.length())));
    password.append(special.at(QRandomGenerator::global()->bounded(special.length())));

    // 3. 剩余长度从所有池子中随机混抽
    QString allChars = lower + upper + nums + special;
    int remaining = length - 4;

    for(int i = 0; i < remaining; ++i) {
        int index = QRandomGenerator::global()->bounded(allChars.length());
        password.append(allChars.at(index));
    }

    // 4. 使用 Fisher-Yates 算法打乱顺序
    for (int i = password.length() - 1; i > 0; --i) {
        int j = QRandomGenerator::global()->bounded(i + 1);
        QChar temp = password[i];
        password[i] = password[j];
        password[j] = temp;
    }

    return password;
}

QString BotManager::toLeetSpeak(const QString& input)
{
    QString output = input;
    // 30% 的概率不转换
    if (QRandomGenerator::global()->bounded(100) < 30) return output;

    for (int i = 0; i < output.length(); ++i) {
        QChar c = output[i].toLower();
        if (c == 'e') output[i] = '3';
        else if (c == 'a') output[i] = '4';
        else if (c == 'o') output[i] = '0';
        else if (c == 'i' || c == 'l') output[i] = '1';
        else if (c == 's') output[i] = '5';
        else if (c == 't') output[i] = '7';
    }
    return output;
}

QString BotManager::generateUniqueUsername()
{
    // 随机模式权重
    int mode = QRandomGenerator::global()->bounded(100);
    QString baseName;

    // --- 核心命名逻辑 ---

    if (mode < 25) {
        // [模式1: 经典组合] Adj + Noun (e.g., DarkKiller)
        QString adj = s_adjectives[QRandomGenerator::global()->bounded(s_adjectives.size())];
        QString noun = s_nouns[QRandomGenerator::global()->bounded(s_nouns.size())];
        baseName = adj + noun;
    }
    else if (mode < 45) {
        // [模式2: 动作型] Verb + Noun/Hero (e.g., GankPudge, EatTrees)
        QString verb = s_verbs[QRandomGenerator::global()->bounded(s_verbs.size())];
        QString noun = (QRandomGenerator::global()->bounded(2) == 0) ?
                           s_nouns[QRandomGenerator::global()->bounded(s_nouns.size())] :
                           s_wc3names[QRandomGenerator::global()->bounded(s_wc3names.size())];
        baseName = verb + noun;
    }
    else if (mode < 60) {
        // [模式3: 装饰型] xX_Name_Xx (e.g., xX_Arthas_Xx)
        QString core = s_wc3names[QRandomGenerator::global()->bounded(s_wc3names.size())];
        // 50% 概率转火星文
        if (QRandomGenerator::global()->bounded(2) == 0) core = toLeetSpeak(core);

        baseName = "xX_" + core + "_Xx";
    }
    else if (mode < 75) {
        // [模式4: 头衔型] Prefix + Name (e.g., DrThrall, iAmPro)
        QString prefix = s_prefixes[QRandomGenerator::global()->bounded(s_prefixes.size())];
        QString core;
        if (prefix == "The") { // "The" 后面接形容词+名词更自然
            QString adj = s_adjectives[QRandomGenerator::global()->bounded(s_adjectives.size())];
            core = adj; // e.g. TheSilent
        } else {
            core = s_wc3names[QRandomGenerator::global()->bounded(s_wc3names.size())];
        }
        baseName = prefix + core;
    }
    else if (mode < 90) {
        // [模式5: 后缀型] Name + Suffix (e.g., PudgeGG, ViperCN)
        QString core = s_wc3names[QRandomGenerator::global()->bounded(s_wc3names.size())];
        QString suffix = s_suffixes[QRandomGenerator::global()->bounded(s_suffixes.size())];

        // 50% 概率加下划线连接
        if (QRandomGenerator::global()->bounded(2) == 0) {
            baseName = core + "_" + suffix;
        } else {
            baseName = core + suffix;
        }
    }
    else {
        // [模式6: 纯火星文 ID] (e.g., N1ghtH4wk)
        QString adj = s_adjectives[QRandomGenerator::global()->bounded(s_adjectives.size())];
        QString noun = s_nouns[QRandomGenerator::global()->bounded(s_nouns.size())];
        baseName = toLeetSpeak(adj + noun);
    }

    // --- 后期微调 (增加随机性但不使用纯数字后缀) ---

    // 10% 概率全小写 (很多真人玩家懒得按 Shift)
    if (QRandomGenerator::global()->bounded(100) < 10) {
        baseName = baseName.toLower();
    }
    // 5% 概率全大写 (咆哮体)
    else if (QRandomGenerator::global()->bounded(100) < 5) {
        baseName = baseName.toUpper();
    }

    // 长度截断 (War3 ID 限制 15 字符)
    if (baseName.length() > 15) {
        baseName = baseName.left(15);
        // 清理截断后末尾可能残留的尴尬符号
        while (baseName.endsWith('_') || baseName.endsWith('X') || baseName.endsWith('x')) {
            baseName.chop(1);
        }
    }

    // 长度太短补点东西 (少于4字符很难注册)
    if (baseName.length() < 4) {
        baseName += "Pro";
    }

    return baseName;
}

bool BotManager::createBotAccountFilesIfNotExist(bool allowAutoGenerate, int targetListNumber)
{
    LOG_INFO("🔐 [账号管理] 启动账号文件检查流程");

    // 1. 确定配置目录
    QString configDir;
    QStringList searchPaths;

#ifdef Q_OS_LINUX
    searchPaths << "/etc/War3Bot/config";
#endif
    searchPaths << QCoreApplication::applicationDirPath() + "/config";
    searchPaths << QDir::currentPath() + "/config";

    bool foundExistingDir = false;
    for (const QString &path : qAsConst(searchPaths)) {
        QDir checkDir(path);
        if (checkDir.exists()) {
            configDir = path;
            foundExistingDir = true;
            break;
        }
    }

    // 情况 A: 目录没找到，且不允许自动生成 -> 报错并提前返回
    if (!foundExistingDir && !allowAutoGenerate) {
        LOG_ERROR("❌ [致命错误] 未找到配置目录，且自动生成已关闭 (auto_gen=false)。");
        LOG_INFO("   ├─ 请手动创建目录: config/");
        LOG_INFO("   └─ 或者在 war3bot.ini 中设置 [bots] auto_generate=true");
        return false;
    }

    // 情况 B: 目录没找到，但允许自动生成 -> 创建默认目录并继续
    if (!foundExistingDir && allowAutoGenerate) {
        configDir = QCoreApplication::applicationDirPath() + "/config";
        QDir dir(configDir);
        if (!dir.exists()) {
            if (dir.mkpath(".")) {
                LOG_INFO(QString("✨ [自动创建] 已创建配置目录: %1").arg(configDir));
            } else {
                LOG_ERROR(QString("❌ [致命错误] 无法创建目录 (权限不足?): %1").arg(configDir));
                return false;
            }
        }
    }

    LOG_INFO(QString("   ├─ 📂 配置目录: %1").arg(configDir));

    // 2. 检查文件是否存在
    // 定义 10 个文件，每个存放 100 个账号
    const int TOTAL_BOTS = 1000;
    const int BOTS_PER_FILE = 100;
    const int FILE_COUNT = TOTAL_BOTS / BOTS_PER_FILE;

    m_newAccountFilePaths.clear();
    m_allAccountFilePaths.clear();
    bool generatedAny = false;
    QSet<QString> generatedSet;

    for (int i = 1; i <= FILE_COUNT; ++i) {
        bool isTargetList = (i == targetListNumber);
        QString fileName = QString("bots_auto_%1.json").arg(i, 2, 10, QChar('0'));
        QString fullPath = configDir + "/" + fileName;

        // UI 格式化
        bool isLastItem = (i == FILE_COUNT);
        QString branch = isLastItem ? "   └─ " : "   ├─ ";
        QString indent = isLastItem ? "      " : "   │  ";

        // Case A: 文件已存在
        if (QFile::exists(fullPath)) {
            if (isTargetList) {
                LOG_INFO(QString("%1✅ [已选中] %2").arg(branch, fileName));
                m_allAccountFilePaths.append(fullPath);
            } else {
                LOG_INFO(QString("%1   [已忽略] %2 (非当前列表)").arg(branch, fileName));
            }
            continue;
        }

        // Case B: 文件不存在，检查开关
        if (!allowAutoGenerate) {
            if (isTargetList) {
                LOG_WARNING(QString("%1❌ [缺失] %2 (自动生成已关闭，无法启动)").arg(branch, fileName));
            }
            continue;
        }

        if (!isTargetList) continue;

        // Case C: 需要生成
        LOG_INFO(QString("%1🆕 [生成中] %2 (包含 %3 个账号)").arg(branch, fileName).arg(BOTS_PER_FILE));

        QJsonArray array;
        int count = 0;

        // 生成 100 个不重复的账号
        while (count < BOTS_PER_FILE) {
            QString user = generateUniqueUsername();

            // 确保不重复
            if (generatedSet.contains(user)) {
                continue;
            }
            generatedSet.insert(user);

            QJsonObject obj;
            obj["u"] = user;
            obj["p"] = generateRandomPassword(QRandomGenerator::global()->bounded(10, 15));
            array.append(obj);
            count++;
        }

        QJsonDocument doc(array);
        QByteArray jsonData = doc.toJson();

        // 写入运行目录文件
        QFile file(fullPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(jsonData);
            file.close();

            if (isTargetList) {
                m_allAccountFilePaths.append(fullPath);
                m_newAccountFilePaths.append(fullPath);
                LOG_INFO(QString("%1├─ 💾 写入并选中: %2").arg(indent, fileName));
            } else {
                LOG_INFO(QString("%1├─ 💾 写入成功: %2").arg(indent, fileName));
            }

            generatedAny = true;

#ifdef APP_SOURCE_DIR
            QString srcConfigDirStr = QString(APP_SOURCE_DIR) + "/config";
            QDir srcDir(srcConfigDirStr);
            if (srcDir.exists()) {
                QString destPath = srcDir.filePath(fileName);
                if (QFile::exists(destPath)) QFile::remove(destPath);

                if (QFile::copy(fullPath, destPath)) {
                    LOG_INFO(QString("%1└─ 🔄 [同步源码] 已回写到: %2").arg(indent, fileName));
                } else {
                    LOG_WARNING(QString("%1└─ ⚠️ [同步失败] 无法回写到源码目录").arg(indent));
                }
            }
#endif
        } else {
            LOG_ERROR(QString("%1└─ ❌ 写入失败 (%2)").arg(indent, file.errorString()));
        }
    }

    if (m_allAccountFilePaths.isEmpty()) {
        LOG_WARNING(QString("   └─ ⚠️ [警告] 最终加载列表为空！请检查 list_number=%1 是否有对应的 bots_auto_%2.json 文件").arg(targetListNumber).arg(targetListNumber, 2, 10, QChar('0')));
    } else {
        LOG_INFO(QString("   └─ 📋 最终加载列表包含 %1 个文件").arg(m_allAccountFilePaths.size()));
    }

    return generatedAny;
}

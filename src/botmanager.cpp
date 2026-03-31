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
    cleanup();
    qDeleteAll(m_bots);
    m_bots.clear();
}

void BotManager::initializeBots(quint32 initialCount, const QString &configPath)
{
    // 1. 打印初始化头部
    LOG_INFO("🤖 [BotManager] 初始化序列启动");

    // 2. 清理旧数据
    cleanup();
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
    m_tempRegistrationClient->setNetManager(m_netManager);
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
    bot->client->setNetManager(m_netManager);
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
    connect(bot->client, &Client::playerCountChanged, this, [this, bot](int count) { this->onBotPlayerCountChanged(bot, count); });
    connect(bot->client, &Client::hostJoinedGame, this, [this, bot](const QString &name) { this->onBotHostJoinedGame(bot, name); });
    connect(bot->client, &Client::gameCreateFail, this, [this, bot](GameCreationStatus status) { this->onBotGameCreateFail(bot, status); });
    connect(bot->client, &Client::roomPingsUpdated, this, [this, bot](const QMap<quint8, quint32> &pings) { this->onBotRoomPingsUpdated(bot, pings); });
    connect(bot->client, &Client::readyStateChanged, this, [this, bot](const QMap<quint8, QVariantMap> &readyData) { this->onBotReadyStateChanged(bot, readyData); });

    m_bots.append(bot);

    LOG_INFO(QString("   │  │  ├─ 🤖 实例化: %1 (ID: %2)").arg(username).arg(bot->id));
}

bool BotManager::createGame(const QString &hostName, const QString &gameName, const QString &gameMode, CommandSource commandSource, const QString &clientId)
{
    // --- 1. 打印任务请求头部 ---
    QString sourceStr = (commandSource == From_Client) ? "客户端聊天窗口" : "服务端命令窗口";

    LOG_INFO("🎮 [创建游戏任务启动]");
    LOG_INFO(QString("   ├─ 👤 虚拟房主: %1").arg(hostName));
    LOG_INFO(QString("   ├─ 📝 游戏名称: %1").arg(gameName));
    LOG_INFO(QString("   ├─ 📝 游戏模式: %1").arg(gameMode));
    LOG_INFO(QString("   ├─ 🆔 命令来源: %1 (%2)").arg(sourceStr, clientId));

    Bot *targetBot = nullptr;
    bool needConnect = false;

    // 1. 优先复用在线空闲的
    if (m_bots.isEmpty()) {
        LOG_ERROR("┌── ❌ [创建失败] 核心服务未运行");
        LOG_ERROR("└── 原因: 机器人池为空，没有任何 Bot 进程连接到服务端");

        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_NO_BOTS_AVAILABLE);
        return false;
    }

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
        targetBot->hostname = hostName;
        targetBot->commandSource = commandSource;
        targetBot->gameInfo.createTime = QDateTime::currentMSecsSinceEpoch();
        targetBot->gameInfo.clientId = clientId;
        targetBot->gameInfo.hostName = hostName;
        targetBot->gameInfo.gameName = gameName;
        targetBot->gameInfo.gameMode = gameMode;
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
        targetBot->hostname = hostName;
        targetBot->commandSource = commandSource;
        targetBot->gameInfo.createTime = QDateTime::currentMSecsSinceEpoch();
        targetBot->gameInfo.clientId = clientId;
        targetBot->gameInfo.hostName = hostName;
        targetBot->gameInfo.gameName = gameName;
        targetBot->gameInfo.gameMode = gameMode;

        // 2. 确保 Client 对象存在且信号已绑定
        if (!targetBot->client) {
            targetBot->client = new Client(this);
            targetBot->client->setGameTickInterval();
            targetBot->client->setNetManager(m_netManager);
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
            connect(targetBot->client, &Client::playerCountChanged, this, [this, targetBot](int count) { this->onBotPlayerCountChanged(targetBot, count); });
            connect(targetBot->client, &Client::hostJoinedGame, this, [this, targetBot](const QString &name) { this->onBotHostJoinedGame(targetBot, name); });
            connect(targetBot->client, &Client::gameCreateFail, this, [this, targetBot](GameCreationStatus status) { this->onBotGameCreateFail(targetBot, status); });
            connect(targetBot->client, &Client::roomPingsUpdated, this, [this, targetBot](const QMap<quint8, quint32> &pings) { this->onBotRoomPingsUpdated(targetBot, pings); });
            connect(targetBot->client, &Client::readyStateChanged, this, [this, targetBot](const QMap<quint8, QVariantMap> &readyData) { this->onBotReadyStateChanged(targetBot, readyData); });
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

void BotManager::cleanup()
{
    LOG_INFO("[BotManager] 停止所有机器人...");
    for (Bot *bot : qAsConst(m_bots)) {
        if (bot->client) {
            bot->client->disconnectFromHost();
        }
        bot->state = BotState::Disconnected;
    }
}

void BotManager::removeBotMappings(const QString &clientId, const QString &hostName)
{
    // 🚀 1. 清理 ClientId 映射 (m_clientIdToBotMap)
    if (!clientId.isEmpty()) {
        bool removedCid = m_clientIdToBotMap.remove(clientId) > 0;
        if (removedCid) {
            LOG_INFO(QString("   ├── 🆔 移除 UUID 映射: %1 -> ✅ 成功").arg(clientId));
        } else {
            LOG_INFO(QString("   ├── 🆔 移除 UUID 映射: %1 -> ⚠️ 未在表中").arg(clientId));
        }
    }

    // 🚀 2. 清理 房主名称 映射 (m_hostNameToBotMap)
    if (!hostName.isEmpty()) {
        QString lowerHostName = hostName.toLower();
        bool removedHost = m_hostNameToBotMap.remove(lowerHostName) > 0;
        if (removedHost) {
            LOG_INFO(QString("   ├── 👤 移除 房主 映射: %1 -> ✅ 成功").arg(lowerHostName));
        } else {
            LOG_INFO(QString("   ├── 👤 移除 房主 映射: %1 -> ⚠️ 未在表中").arg(lowerHostName));
        }
    }
}

void BotManager::removeGame(Bot *bot, bool disconnectFlag)
{
    if (!bot) return;

    LOG_INFO(QString("🧹 [清理任务] 准备回收机器人资源: Bot-%1").arg(bot->id));

    // 🚀 步骤 1：获取当前的 Key 并删除
    QString currentClientId = bot->gameInfo.clientId;
    QString currentHostName = bot->hostname;

    removeBotMappings(currentClientId, currentHostName);

    // 🚀 步骤 2：防范性全局扫描
    QMutableHashIterator<QString, Bot*> it(m_clientIdToBotMap);
    while (it.hasNext()) {
        it.next();
        if (it.value() == bot) {
            LOG_WARNING(QString("   ├── 🛡️ [补丁] 发现残留映射: Key[%1] -> 已强制抹除").arg(it.key()));
            it.remove();
        }
    }

    // 🚀 步骤 3：清理房主名映射
    if (!bot->hostname.isEmpty()) {
        m_hostNameToBotMap.remove(bot->hostname.toLower());
    }

    // 🚀 步骤 4：释放房间名锁定
    QString lowerName = bot->gameInfo.gameName.toLower();
    if (!lowerName.isEmpty()) {
        if (m_activeGames.value(lowerName) == bot) {
            m_activeGames.remove(lowerName);
            LOG_INFO(QString("   ├── 🔓 释放房间名锁定: %1").arg(lowerName));
        }
    }

    // 🚀 步骤 5：执行底层清理
    if (bot->client) {
        bot->client->cancelGame();
    }

    // 🚀 步骤 6：重置所有变量
    bot->resetGameState();

    if (disconnectFlag) {
        bot->state = BotState::Disconnected;
        LOG_INFO("   └─ ✅ Bot 状态已更新为 Disconnected");
    } else {
        bot->state = BotState::Idle;
        LOG_INFO("   └─ ✅ Bot 状态已更新为 Idle");
    }

    // 🚀 步骤 7. 通知上层应用
    emit botStateChanged(bot->id, bot->username, bot->state);
}

const QVector<Bot*>& BotManager::getAllBots() const
{
    return m_bots;
}

void BotManager::setServerPort(quint16 port)
{
    m_controlPort = port;
}

void BotManager::setNetManager(NetManager *netManager)
{
    m_netManager = netManager;
    if (m_netManager) {
        bool ok = connect(m_netManager, &NetManager::roomPingReceived,
                          this, &BotManager::onBotRoomPingReceived);

        if (ok) {
            LOG_INFO("🔗 [链路配置] 信号 roomPingReceived -> 槽 onBotRoomPingReceived 连接成功");
        } else {
            LOG_CRITICAL("❌ [链路配置] 信号连接失败！请检查函数签名和 Q_OBJECT 宏");
        }
    }
}

Bot *BotManager::findBotByHostName(const QString &hostName)
{
    if (hostName.isEmpty()) return nullptr;

    Bot *bot = m_hostNameToBotMap.value(hostName.toLower(), nullptr);

    if (bot && bot->state != BotState::Disconnected) {
        return bot;
    }

    return nullptr;
}

Bot *BotManager::findBotByClientId(const QString &clientId)
{
    if (clientId.isEmpty()) return nullptr;

    Bot *bot = m_clientIdToBotMap.value(clientId, nullptr);
    if (bot && bot->state != BotState::Disconnected) {
        return bot;
    }

    return nullptr;
}

bool BotManager::checkCooldown(const QString &clientId, const QString &command, qint64 now)
{
    // 1. 定义冷却规则 (也可以定义为类成员变量以提高性能)
    static QMap<QString, qint64> cooldownRules;
    if (cooldownRules.isEmpty()) {
        cooldownRules.insert("/host", 2000);
        cooldownRules.insert("/start", 3000);
        cooldownRules.insert("/join", 500);
        cooldownRules.insert("/unhost", 1000);
        cooldownRules.insert("/swap", 500);
    }
    const qint64 DEFAULT_COOLDOWN = 1000;

    qint64 requiredWait = cooldownRules.value(command, DEFAULT_COOLDOWN);

    // 2. 检查记录
    if (m_commandCooldowns.contains(clientId)) {
        qint64 lastExec = m_commandCooldowns[clientId].value(command, 0);
        qint64 diff = now - lastExec;

        if (diff < requiredWait) {
            quint32 remaining = static_cast<quint32>(requiredWait - diff);
            // 通知客户端进入冷却
            m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_COOLDOWN, remaining);

            LOG_WARNING(QString("⏳ [频率限制] UUID: %1 | 指令: %2 | 剩余: %3ms")
                            .arg(clientId, command).arg(remaining));
            return false;
        }
    }

    // 3. 更新时间戳并放行
    m_commandCooldowns[clientId][command] = now;
    return true;
}

void BotManager::handleHostCommand(const QString &userName, const QString &clientId, const QString &text)
{
    LOG_INFO("🎮 [创建房间流程]");

    // 1. 重复开房检查
    Bot *existingBotByCid = findBotByClientId(clientId);
    Bot *existingBotByName = findBotByHostName(userName);

    if (existingBotByCid != nullptr || existingBotByName != nullptr) {
        QString interceptReason = (existingBotByName != nullptr) ? "该游戏名/账号" : "当前客户端/设备";

        LOG_WARNING(QString("   └─ ⚠️ 拦截: %1 已拥有活跃房间 (拦截目标: %2)")
                        .arg(interceptReason)
                        .arg(userName));
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_ALREADY_IN_GAME);
        return;
    }

    // 2. 参数解析
    QStringList parts = text.split(" ", Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        LOG_ERROR("   └─ ❌ 错误: 参数不足 (用法: /host <模式> <房名>)");
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_PARAM_ERROR);
        return;
    }

    QString mapModel = parts[0].toLower();
    QString inputGameName = parts.mid(1).join(" ");

    // 3. 模式校验
    static const QVector<QString> allowModels = {
        "ap", "cm", "rd", "sd", "ar", "xl", "aptb", "cmtb", "rdtb", "sdtb", "artb", "xltb",
        "ap83", "cm83", "rd83", "sd83", "ar83", "xl83", "ap83tb", "cm83tb", "rd83tb", "sd83tb", "ar83tb", "xl83tb"
    };

    if (!allowModels.contains(mapModel)) {
        LOG_ERROR(QString("   └─ ❌ 错误: 不支持的模式 [%1]").arg(mapModel));
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_MAP_NOT_SUPPORTED);
        return;
    }

    // 4. 房名预处理 (截断与后缀)
    QString displayMode = mapModel.toUpper();
    QString baseName = inputGameName.trimmed();
    if (baseName.isEmpty()) baseName = QString("%1's Game").arg(userName);

    QString prefix = QString("[%1] ").arg(displayMode);
    const int MAX_BYTES = 31;
    int availableBytes = MAX_BYTES - prefix.toUtf8().size();

    QByteArray nameBytes = baseName.toUtf8();
    if (nameBytes.size() > availableBytes) {
        nameBytes = nameBytes.left(availableBytes);
        while (nameBytes.size() > 0) {
            QString tryStr = QString::fromUtf8(nameBytes);
            if (tryStr.toUtf8().size() == nameBytes.size() && !tryStr.contains(QChar::ReplacementCharacter)) break;
            nameBytes.chop(1);
        }
        LOG_INFO(QString("   ├─ ✂️ 房名过长，已执行 UTF-8 安全截断"));
    }

    QString finalGameName = prefix + QString::fromUtf8(nameBytes);

    // 5. 重名检查
    if (m_activeGames.contains(finalGameName.toLower())) {
        LOG_ERROR(QString("   └─ ❌ 错误: 房间名 [%1] 已存在").arg(finalGameName));
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_GAME_NAME_EXISTS);
        return;
    }

    // 6. 缓存指令信息
    CommandInfo info;
    info.clientId = clientId;
    info.text = text.trimmed();
    info.timestamp = QDateTime::currentMSecsSinceEpoch();
    m_commandInfos.insert(userName, info);

    LOG_INFO(QString("   ├─ ✅ 最终房名: [%1]").arg(finalGameName));
    LOG_INFO(QString("   └─ 🚀 提交任务: 调用 createGame()"));

    // 7. 执行创建
    if (!createGame(userName, finalGameName, displayMode, From_Client, clientId)) {
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_NO_BOTS_AVAILABLE);
    }
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

void BotManager::onBotClientExpired(const QString &clientId)
{
    Bot *bot = findBotByClientId(clientId);
    if (bot) {
        LOG_INFO(QString("🧹 [系统自动回收] 检测到 UUID %1 会话已过期，正在强制解散房间").arg(clientId));
        removeGame(bot, true);
        removeClientMapping(clientId);
    }
}

void BotManager::onBotCommandReceived(const QString &userName, const QString &clientId, const QString &command, const QString &text)
{
    const QString trimmedCommand = command.trimmed().toLower();
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // --- 1. 基础校验 ---
    if (!m_netManager->isClientRegistered(clientId)) {
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_PERMISSION_DENIED);
        return;
    }

    if (!checkCooldown(clientId, trimmedCommand, now)) return;

    LOG_INFO("📨 [收到用户指令]");
    LOG_INFO(QString("   ├─ 👤 发送者: %1 (UUID: %2)").arg(userName, clientId));
    LOG_INFO(QString("   └─ 💬 内容:   %1 %2").arg(trimmedCommand, text));

    // --- 2. 指令逻辑分发 ---
    if (trimmedCommand == "/host") {
        handleHostCommand(userName, clientId, text);
        return;
    }

    // --- 3. 处理准备/取消准备 (所有玩家可用) ---
    if (trimmedCommand == "/ready" || trimmedCommand == "/unready") {
        bool isReady = (trimmedCommand == "/ready");
        Bot *targetBot = nullptr;

        for (Bot *bot : qAsConst(m_bots)) {
            if (!bot->client) continue;
            if (bot->isOwner(clientId) || bot->client->hasPlayerByUuid(clientId)) {
                targetBot = bot;
                break;
            }
        }

        if (targetBot && targetBot->client) {
            targetBot->client->setPlayerReadyByUuid(clientId, isReady);
            targetBot->client->syncReadyStates();

            LOG_INFO(QString("   └─ 🚀 执行动作: 玩家 %1 状态更新 -> %2 (已触发即时同步)")
                         .arg(userName, isReady ? "已准备" : "已取消准备"));
        } else {
            LOG_WARNING(QString("   └─ ❌ 拒绝: 玩家 %1 当前不在任何房间内").arg(userName));
        }
        return;
    }

    // --- 4. 特殊处理 /unhost (仅房主) ---
    if (trimmedCommand == "/unhost") {
        Bot *myBot = findBotByClientId(clientId);
        if (myBot) {
            LOG_INFO("   └─ 🚀 执行动作: 解散房间");
            removeGame(myBot, false);
        } else {
            LOG_INFO("   └─ ℹ️ 房间已在断开时自动清理，指令仅做同步确认");
        }
        m_netManager->sendMessageToClient(clientId, S_C_MESSAGE, MSG_HOST_UNHOST_GAME);
        return;
    }

    // --- 5. 其他控制指令 (仅限房主/管理员) ---
    Bot *myBot = findBotByClientId(clientId);
    if (!myBot) {
        LOG_WARNING(QString("   └─ ❌ 拒绝: 用户名下当前没有活跃房间，无法执行 %1").arg(trimmedCommand));
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_PERMISSION_DENIED);
        return;
    }

    if (trimmedCommand == "/start") {
        if (myBot->state == BotState::Waiting && myBot->client) {
            // if (!myBot->client->isAllPlayersReady()) { ... }

            LOG_INFO("   └─ 🚀 执行动作: 启动游戏");
            myBot->client->startGame();
            myBot->state = BotState::Starting;
            emit botStateChanged(myBot->id, myBot->username, myBot->state);
        } else {
            LOG_WARNING(QString("   └─ ⚠️ 忽略: 状态 %1 不满足开始条件").arg(static_cast<int>(myBot->state)));
        }
    }
    else if (trimmedCommand == "/swap") {
        QStringList parts = text.split(" ", Qt::SkipEmptyParts);
        if (parts.size() >= 2 && myBot->client) {
            int s1 = parts[0].toInt();
            int s2 = parts[1].toInt();
            LOG_INFO(QString("   └─ 🚀 执行动作: 交换槽位 %1 <-> %2").arg(s1).arg(s2));
            myBot->client->swapSlots(s1, s2);
        }
    }
    else if (trimmedCommand == "/latency" || trimmedCommand == "/lat") {
        int val = text.toInt();
        if (val >= 10 && val <= 100 && myBot->client) {
            LOG_INFO(QString("   └─ 🚀 执行动作: 修改延迟为 %1ms").arg(val));
            myBot->client->setGameTickInterval((quint16)val);
        }
    }
    else {
        LOG_WARNING(QString("   └─ ❓ 未知命令: %1 (将被忽略)").arg(trimmedCommand));
    }
}

void BotManager::onBotPlayerCountChanged(Bot *bot, int count)
{
    if (!bot) return;

    bot->gameInfo.currentPlayerCount = count;

    LOG_INFO(QString("📊 [状态同步] 接收到 Bot-%1 的人数更新").arg(bot->id));
    LOG_INFO(QString("   ├── 🏠 房间: %1").arg(bot->gameInfo.gameName));
    LOG_INFO(QString("   └── ✅ 结果: 真人玩家数已同步为 %1").arg(count));
}

void BotManager::onBotGameCreateSuccess(Bot *bot)
{
    if (!bot || !bot->client) return;

    // 1. 更新内部状态
    bot->hostJoined = false;
    bot->state = BotState::Reserved;
    bot->gameInfo.createTime = QDateTime::currentMSecsSinceEpoch();

    if (!bot->hostname.isEmpty()) {
        m_hostNameToBotMap.insert(bot->hostname.toLower(), bot);
    }

    if (!bot->gameInfo.clientId.isEmpty()) {
        m_clientIdToBotMap.insert(bot->gameInfo.clientId, bot);
    }

    QString lowerName = bot->gameInfo.gameName.toLower();
    if (!lowerName.isEmpty()) {
        m_activeGames.insert(lowerName, bot);
    }

    // 2. 获取端口并进行严格诊断
    quint16 botListenPort = bot->client->getListenPort();
    quint32 serverMapCrc = bot->client->getMapCRC();
    QString clientId = bot->gameInfo.clientId;

    // 3. 打印详细树状日志
    LOG_INFO("🎮 [房间创建完成回调]");
    LOG_INFO(QString("   ├─ 🤖 机器人实例: %1 (ID: %2)").arg(bot->username).arg(bot->id));
    LOG_INFO(QString("   ├─ 👤 房主名称:  %1 (UUID:  %2)").arg(bot->hostname, clientId));
    LOG_INFO(QString("   ├─ 🏠 房间名称:  %1").arg(bot->gameInfo.gameName));
    LOG_INFO(QString("   ├─ 🚩 游戏模式:  %1").arg(bot->gameInfo.gameMode));
    LOG_INFO(QString("   ├─ 🛡️ 地图校验:  0x%1").arg(QString::number(serverMapCrc, 16).toUpper()));

    // 诊断端口状态
    if (botListenPort == 0) {
        LOG_ERROR("   ├─ ❌ 端口错误: 获取到 0！(检查 bindToRandomPort 是否成功)");
    } else {
        LOG_INFO(QString("   ├─ 🔌 分配端口:  %1").arg(botListenPort));
    }

    // 4. 发送 TCP 控制指令
    if (m_netManager) {
        quint16 botListenPort = bot->client->getListenPort();
        bool okToGameLoby = m_netManager->sendEnterRoomCommand(clientId, m_controlPort, bot->commandSource == From_Server);
        bool okToLauncher = m_netManager->sendMessageToClient(clientId, S_C_MESSAGE, MSG_HOST_CREATED_GAME, botListenPort);
        bool okToCheckCrc = m_netManager->sendMessageToClient(clientId, S_C_MESSAGE, MSG_CHECK_MAP_CRC, static_cast<quint64>(serverMapCrc));

        if (okToCheckCrc) {
            LOG_INFO("   └─ 🚀 校验指令: 已发送服务端 CRC 供客户端比对");
        } else {
            LOG_ERROR("   └─ ❌ 校验指令: 发送失败 (目标用户不在线或未记录通道)");
        }

        if (okToGameLoby && okToLauncher) {
            LOG_INFO(QString("   └─ 🚀 自动进入: 指令已发送 (目标端口: %1)").arg(m_controlPort));
        } else {
            LOG_ERROR("   └─ ❌ 自动进入: 发送失败 (目标用户不在线或未记录通道)");
        }
    } else {
        LOG_INFO("   └─ 🛑 系统错误: NetManager 未绑定，无法发送指令");
    }

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
    LOG_INFO(QString("   ├─ 👤 归属用户: %1").arg(bot->gameInfo.clientId));
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

    QString oldHostName = bot->gameInfo.hostName;
    QString oldUuid = bot->gameInfo.clientId;
    QString newUuid = "";
    QString newHostName = "";
    quint8 newHostPid = 0;

    const QMap<quint8, PlayerData> &players = bot->client->getPlayers();

    // 1. 寻找继承人
    for (auto it = players.begin(); it != players.end(); ++it) {
        if (it.key() != 2 && it.value().isVisualHost) {
            newUuid = it.value().clientUuid;
            newHostName = it.value().name;
            newHostPid = it.key();
            break;
        }
    }

    // 2. 执行所有权交接逻辑
    if (!newUuid.isEmpty()) {
        LOG_INFO(QString("👑 [房主移交] 目标: %1 | 新房主: %2 (PID: %3)")
                     .arg(bot->gameInfo.gameName, newHostName).arg(newHostPid));

        m_hostNameToBotMap.remove(oldHostName.toLower());
        m_hostNameToBotMap.insert(newHostName.toLower(), bot);

        m_clientIdToBotMap.remove(oldUuid);
        m_clientIdToBotMap.insert(newUuid, bot);

        bot->gameInfo.clientId = newUuid;
        bot->gameInfo.hostName = newHostName;
        bot->hostname = newHostName;

        // 3. 向全房间广播通知
        for (const auto &p : players) {
            if (p.pid != 2 && !p.clientUuid.isEmpty()) {
                m_netManager->sendMessageToClient(p.clientUuid, PacketType::S_C_MESSAGE, MSG_HOST_LEAVE_GAME, newHostPid);
            }
        }
    }
    else {
        LOG_INFO(QString("🚫 [房间关闭] 房主离开且无继承人"));
        removeGame(bot, false);
    }
}

void BotManager::onBotHostJoinedGame(Bot *bot, const QString &hostName)
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
                    LOG_INFO(QString("   ├─ 👤 通知用户: %1 (Reason: 1-Timeout)").arg(bot->pendingTask.clientId));
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

void BotManager::onBotRoomPingReceived(const QHostAddress &addr, quint16 port, const QString &identifier, quint64 clientTime, PingSearchMode mode)
{
    LOG_INFO("📩 [BotManager] 接收到 roomPingReceived 信号");

    Bot *bot = nullptr;
    QString modeTag;

    // 1. 根据模式执行查找逻辑
    switch (mode) {
    case ByClientId:
        bot = findBotByClientId(identifier);
        modeTag = "UUID Only";
        break;
    case ByBoth:
        bot = findBotByClientId(identifier);
        if (!bot) bot = findBotByHostName(identifier);
        modeTag = "Both (Name & UUID)";
        break;
    case ByHostName:
    default:
        bot = findBotByHostName(identifier);
        modeTag = "HostName Only";
        break;
    }

    // 2. 提取房间状态
    quint8 current = 0;
    quint8 max = 0;
    bool isHit = (bot != nullptr);

    if (isHit) {
        current = static_cast<quint8>(bot->gameInfo.currentPlayerCount);
        max     = static_cast<quint8>(bot->gameInfo.maxPlayers);
    }

    // 3. 树状日志记录
    LOG_INFO(QString("   ├── 👤 来源地址: %1:%2").arg(addr.toString()).arg(port));
    LOG_INFO(QString("   ├── 🔍 匹配模式: %1").arg(modeTag));
    LOG_INFO(QString("   ├── 🆔 搜索标识: %1").arg(identifier));

    if (isHit) {
        LOG_INFO(QString("   ├── 🎯 命中目标: %1").arg(bot->gameInfo.gameName));
        LOG_INFO(QString("   ├── 📊 房间状态: %1 / %2").arg(current).arg(max));
    } else {
        LOG_INFO("   ├── ⚠️  查找结果: 未找到匹配的活跃 Bot");
    }

    // 4. 根据模式准备发送给客户端的参数
    if (m_netManager) {
        QString outHost = "";
        QString outUuid = "";

        if (isHit) {
            switch (mode) {
            case ByHostName:
                outHost = bot->hostname;
                break;
            case ByClientId:
                outUuid = bot->gameInfo.clientId;
                break;
            case ByBoth:
                outHost = bot->hostname;
                outUuid = bot->gameInfo.clientId;
                break;
            }
        }

        m_netManager->sendRoomPong(addr, port, clientTime, current, max, outHost, outUuid);

        LOG_INFO(QString("   └── ✅ 动作执行: 已根据 %1 模式回发 Pong").arg(modeTag));
    }
}

void BotManager::onBotRoomPingsUpdated(Bot *bot, const QMap<quint8, quint32> &pings)
{
    if (!bot || !bot->client || !m_netManager) {
        return;
    }

    QVariantMap vMap;
    QString summary;
    for (auto it = pings.constBegin(); it != pings.constEnd(); ++it) {
        vMap.insert(QString::number(it.key()), it.value());
        if (!summary.isEmpty()) summary += ", ";
        summary += QString("P%1:%2ms").arg(it.key()).arg(it.value());
    }

    QSet<QString> targetUuids;

    // A. 强制加入房间创建者
    if (!bot->gameInfo.clientId.isEmpty()) {
        targetUuids.insert(bot->gameInfo.clientId);
    }

    // B. 加入房间内所有已识别的玩家
    const QMap<quint8, PlayerData> &roomPlayers = bot->client->getPlayers();
    for (const auto &player : roomPlayers) {
        if (!player.clientUuid.isEmpty()) {
            targetUuids.insert(player.clientUuid);
        }
    }

    LOG_INFO(QString("📡 [延迟同步序列] 房间: %1").arg(bot->gameInfo.gameName));
    LOG_INFO(QString("   ├── 📊 实时数据: [%1]").arg(summary.isEmpty() ? "空" : summary));
    LOG_INFO(QString("   ├── 👥 接收目标: %1 个实例").arg(targetUuids.size()));

    int sentCount = 0;
    int total = targetUuids.size();
    int i = 0;

    for (const QString &uuid : targetUuids) {
        i++;
        bool isLast = (i == total);
        QString branch = isLast ? "   └── " : "   ├── ";

        // 发送 TCP 指令
        bool ok = m_netManager->sendRoomPings(uuid, vMap);

        if (ok) {
            sentCount++;
            bool isRoomOwner = (uuid == bot->gameInfo.clientId);
            LOG_INFO(QString("%1🚀 下发至 %2 [UUID: %3]")
                         .arg(branch, isRoomOwner ? "👑 房主" : "👤 玩家", uuid));
        } else {
            LOG_WARNING(QString("%1❌ 失败: 客户端 %2 已断开控制链路").arg(branch, uuid));
        }
    }

    if (sentCount > 0) {
        LOG_INFO(QString("   ✨ 同步完成: 已成功通知 %1 个客户端").arg(sentCount));
    }
}

void BotManager::onBotReadyStateChanged(Bot *bot, const QMap<quint8, QVariantMap> &readyData)
{
    if (!bot || !m_netManager) return;

    LOG_INFO(QString("📡 [状态广播序列] 开始处理数据同步..."));
    LOG_INFO(QString("   ├─ 🏠 房间名称: %1").arg(bot->gameInfo.gameName));
    LOG_INFO(QString("   ├─ 🤖 负责机器人: Bot-%1 (%2)").arg(bot->id).arg(bot->username));

    LOG_INFO("   ├─ 📦 原始数据内容 (readyData):");

    QVariantMap vMap;
    if (readyData.isEmpty()) {
        LOG_INFO("   │  └─ ⚠️ [警告] readyData 映射表为空！");
    } else {
        for (auto it = readyData.constBegin(); it != readyData.constEnd(); ++it) {
            quint8 pid = it.key();
            QVariantMap innerData = it.value();

            bool isReady = innerData.value("r").toBool();
            int countdown = innerData.value("c").toInt();

            vMap.insert(QString::number(pid), innerData);

            bool isLast = (it == readyData.constEnd() - 1);
            QString prefix = isLast ? "   │  └─ " : "   │  ├─ ";
            LOG_INFO(QString("%1PID[%2]: { 准备状态(r): %3, 倒计时(c): %4s }")
                         .arg(prefix)
                         .arg(pid)
                         .arg(isReady ? "✅ READY" : "⏳ WAITING")
                         .arg(countdown));
        }
    }

    QSet<QString> targetUuids;

    if (!bot->gameInfo.clientId.isEmpty()) {
        targetUuids.insert(bot->gameInfo.clientId);
    }

    const QMap<quint8, PlayerData> &roomPlayers = bot->client->getPlayers();
    for (const auto &player : roomPlayers) {
        if (!player.clientUuid.isEmpty()) {
            targetUuids.insert(player.clientUuid);
        }
    }

    LOG_INFO(QString("   ├─ 📡 广播发送目标 (%1 个实例):").arg(targetUuids.size()));

    int sendCount = 0;
    if (targetUuids.isEmpty()) {
        LOG_INFO("   │  └─ ❌ [错误] 找不到任何有效的发送目标 (UUID 全空)！");
    } else {
        for (const QString &uuid : targetUuids) {
            m_netManager->sendRoomReadyStates(uuid, vMap);
            sendCount++;

            bool isLast = (sendCount == targetUuids.size());
            QString prefix = isLast ? "   │  └─ " : "   │  ├─ ";
            LOG_INFO(QString("%1发送至 UUID: %2").arg(prefix, uuid));
        }
    }

    LOG_INFO(QString("   └─ ✅ 任务完成。共计下发 %1 条 UI 状态更新。").arg(sendCount));
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
            LOG_INFO(QString("├── 通知: 用户 %1 (Code: ERR_CREATE_INTERRUPTED)").arg(ownerId));
            m_netManager->sendMessageToClient(ownerId, S_C_ERROR, ERR_CREATE_INTERRUPTED);
        }
        // 场景 B: 房主已在大厅 (Waiting)
        else if (bot->state == BotState::Waiting) {
            LOG_INFO(QString("├── 通知: 用户 %1 (Code: MSG_HOST_UNHOST_GAME)").arg(ownerId));
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
        LOG_INFO(QString("├── 清理: 已取消用户 %1 的挂起任务: %2").arg(taskOwner, bot->pendingTask.gameName));
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

    LOG_INFO(QString("🔌 [断开连接] Bot-%1 (%2)").arg(bot->id).arg(bot->username));

    removeGame(bot, true);

}

void BotManager::onBotGameCanceled(Bot *bot)
{
    if (!bot) return;

    LOG_INFO(QString("🔄 [状态同步] 收到 Client 取消信号: Bot-%1").arg(bot->id));

    removeGame(bot);
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

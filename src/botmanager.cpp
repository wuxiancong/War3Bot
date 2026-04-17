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
}

void BotManager::initializeBots(quint32 initialCount, const QString &configPath)
{
    // 1. 打印初始化头部
    LOG_INFO("🤖 [BotManager] 初始化序列启动");
    if (m_tempRegistrationClient) {
        m_tempRegistrationClient->disconnect(this);
        m_tempRegistrationClient->disconnectFromHost();
        m_tempRegistrationClient->deleteLater();
        m_tempRegistrationClient = nullptr;
    }

    // 2. 清理旧数据
    cleanup();
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
            LOG_WARNING("      └─ ⚠️ 异常警告: 队列为空 -> 降级为常规加载");
            loadMoreBots(initialCount);
            startAllBots();
        }

    } else {
        LOG_INFO("   └─ ✅ 模式切换: [常规加载模式]");
        LOG_INFO("      ├─ 📂 状态: 所有账号文件已存在，跳过注册");
        LOG_INFO(QString("      ├─ 📥 目标加载数: %1 个 Bot").arg(initialCount));

        loadMoreBots(initialCount);
        startAllBots();

        LOG_INFO(QString("      └─ 🚀 动作执行: 启动连接 (Bot总数: %1)").arg(m_bots.size()));
    }
}

int BotManager::loadMoreBots(int count)
{
    // 1. 打印头部
    LOG_INFO(QString("   ├─ 📥 [增量加载] 请求增加: %1 个机器人").arg(count));

    int loadedCount = 0;

    while (loadedCount < count) {
        // 资源耗尽检查
        if (m_currentFileIndex >= m_allAccountFilePaths.size()) {
            LOG_WARNING("   │  └─ ⚠️ [资源耗尽] 所有账号文件已全部加载完毕");
            LOG_WARNING(QString("   │     ├─ 📊 当前索引: %1").arg(m_currentFileIndex));
            LOG_WARNING(QString("   │     └─ 🗃️ 文件总数: %1").arg(m_allAccountFilePaths.size()));
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
            QString u = obj["u"].toString();
            QString p = obj["p"].toString();
            if (u.isEmpty() || p.isEmpty()) {
                LOG_WARNING(QString("   │  ⚠️ [数据跳过] 文件 %1 第 %2 项数据不完整").arg(fileName).arg(m_currentAccountIndex));
            } else {
                addBotInstance(u, p);
                loadedCount++;
                loadedFromThisFile++;
            }
            m_currentAccountIndex++;
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

bool BotManager::isBotValid(Bot *bot, const QString &context)
{
    // 1. 基础非空检查
    if (!bot) {
        if (!context.isEmpty()) LOG_INFO(QString("❌ [致命] %1 收到空 Bot 指针").arg(context));
        return false;
    }

    // 2. bot 是否还在管理池中
    if (!m_bots.contains(bot)) {
        if (!context.isEmpty()) LOG_WARNING(QString("⚠️ [拦截] %1 收到已失效/销毁的 Bot (ID:%2)").arg(context).arg(bot->id));
        return false;
    }

    return true;
}

bool BotManager::isBotActive(Bot *bot, const QString &context)
{
    // 1. 第一层：内存与容器校验
    if (!bot || !m_bots.contains(bot)) {
        if (!context.isEmpty()) LOG_INFO(QString("⚠️ [拦截] %1: Bot 实例已销毁或不在管理列表中").arg(context));
        return false;
    }

    // 2. 第二层：组件校验
    if (!bot->client) {
        if (!context.isEmpty()) LOG_INFO(QString("❌ [拒绝] %1: Bot-%2 (%3) 的网络组件 (Client) 为空")
                         .arg(context).arg(bot->id).arg(bot->username));
        return false;
    }

    return true;
}

const QVector<Bot*>& BotManager::getAllBots() const
{
    return m_bots;
}

void BotManager::startAllBots()
{
    int delay = 0;

    for (Bot *bot : qAsConst(m_bots)) {
        if (!isBotActive(bot, "StartAllBots")) continue;
        QTimer::singleShot(delay, this, [this, bot]() {
            if (isBotActive(bot, "StartAllBots")) {
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
            bot->client->disconnect(this);
            bot->client->disconnectFromHost();
            bot->client->deleteLater();
        }
        bot->state = BotState::Disconnected;
    }
    qDeleteAll(m_bots);
    m_bots.clear();
}

void BotManager::addBotInstance(const QString& username, const QString& password)
{
    LOG_INFO(QString("🔨 [Bot创建序列] 准备实例化 Bot-%1: %2").arg(m_globalBotIdCounter).arg(username));

    Bot *bot = new Bot(this, m_globalBotIdCounter++, username, password);

    // 先添加机器人，否则无法创建游戏。
    m_bots.append(bot);

    LOG_INFO(QString("   ├─ ⚙️ 步骤1: 执行 bot->setupClient..."));
    bot->setupClient(m_netManager, m_botDisplayName);

    LOG_INFO(QString("   ├─ 🔗 步骤2: 执行 setupBotConnections..."));
    setupBotConnections(bot);

    LOG_INFO(QString("   └─ 🎉 Bot-%1 (%2) 初始化序列完成").arg(bot->id).arg(username));
}

void BotManager::setupBotConnections(Bot *bot)
{
    // 1. 基础指针校验
    if (!bot) {
        LOG_ERROR("   │  ❌ [信号连接] 失败：接收到空的 Bot 指针");
        return;
    }

    // 2. 深度活跃性诊断
    LOG_INFO(QString("   │  🔍 [诊断] 正在校验 Bot-%1 的绑定权限...").arg(bot->id));
    if (!isBotActive(bot, "SetupBotConnections")) {
        LOG_ERROR(QString("   │  🚫 [核心拦截] Bot-%1 校验未通过，绑定流程已终止！").arg(bot->id));
        LOG_ERROR(QString("   │  👉 原因排查：1. bot->client 是否尚未创建？ 2. bot 是否尚未加入 m_bots 列表？"));
        return;
    }

    LOG_INFO(QString("🛠️ [信号连接] 权限校验通过，开始为 Bot-%1 建立信号隧道...").arg(bot->id));

    // 3. 定义内部追踪工具
    auto arrivalLog = [bot](const QString &sig) {
        LOG_INFO(QString("🔔 [信号捕获] BotManager: 捕获到 Bot-%1 的 %2 代理信号").arg(bot->id).arg(sig));
    };

    auto checkConnect = [bot](bool success, const char* signalName) {
        if (!success) {
            LOG_CRITICAL(QString("   │  ❌ [物理失败] Bot-%1 隧道建立失败: %2 (检查参数类型是否1:1对应)").arg(bot->id).arg(signalName));
        } else {
            LOG_DEBUG(QString("   │  🆗 [隧道开启] %1").arg(signalName));
        }
    };

    // 4. 开始执行 16 路信号绑定

    // 1. 认证成功
    checkConnect(connect(bot, &Bot::authenticated, this, [this, bot, arrivalLog]() {
                     arrivalLog("authenticated");
                     this->onBotAuthenticated(bot);
                 }), "authenticated");

    // 2. 进入聊天
    checkConnect(connect(bot, &Bot::enteredChat, this, [this, bot, arrivalLog]() {
                     arrivalLog("enteredChat");
                     this->onBotEnteredChat(bot);
                 }), "enteredChat");

    // 3. 游戏创建成功
    checkConnect(connect(bot, &Bot::gameCreateSuccess, this, [this, bot, arrivalLog](bool isHotRefresh) {
                     arrivalLog(QString("gameCreateSuccess (hotRefresh: %1)").arg(isHotRefresh));
                     this->onBotGameCreateSuccess(bot, isHotRefresh);
                 }), "gameCreateSuccess");

    // 4. 断开连接
    checkConnect(connect(bot, &Bot::disconnected, this, [this, bot, arrivalLog]() {
                     arrivalLog("disconnected");
                     this->onBotDisconnected(bot);
                 }), "disconnected");

    // 5. 游戏开始
    checkConnect(connect(bot, &Bot::gameStarted, this, [this, bot, arrivalLog]() {
                     arrivalLog("gameStarted");
                     this->onBotGameStarted(bot);
                 }), "gameStarted");

    // 6. 游戏取消
    checkConnect(connect(bot, &Bot::gameCancelled, this, [this, bot, arrivalLog]() {
                     arrivalLog("gameCancelled");
                     this->onBotGameCancelled(bot);
                 }), "gameCancelled");

    // 7. 账号创建
    checkConnect(connect(bot, &Bot::accountCreated, this, [this, bot, arrivalLog]() {
                     arrivalLog("accountCreated");
                     this->onBotAccountCreated(bot);
                 }), "accountCreated");

    // 8. 视觉房主离开
    checkConnect(connect(bot, &Bot::visualHostLeft, this, [this, bot, arrivalLog]() {
                     arrivalLog("visualHostLeft");
                     this->onBotVisualHostLeft(bot);
                 }), "visualHostLeft");

    // 9. Socket 错误
    checkConnect(connect(bot, &Bot::socketError, this, [this, bot, arrivalLog](QString error) {
                     arrivalLog("socketError (" + error + ")");
                     this->onBotError(bot, error);
                 }), "socketError");

    // 10. 玩家人数变动
    checkConnect(connect(bot, &Bot::playerCountChanged, this, [this, bot, arrivalLog](int count) {
                     arrivalLog("playerCountChanged");
                     this->onBotPlayerCountChanged(bot, count);
                 }), "playerCountChanged");

    // 11. 房主进入游戏
    checkConnect(connect(bot, &Bot::hostJoinedGame, this, [this, bot, arrivalLog](const QString &name) {
                     arrivalLog("hostJoinedGame");
                     this->onBotHostJoinedGame(bot, name);
                 }), "hostJoinedGame");

    // 12. 房主权限交接
    checkConnect(connect(bot, &Bot::roomHostChanged, this, [this, bot, arrivalLog](const quint8 heirId) {
                     arrivalLog("roomHostChanged");
                     this->onBotRoomHostChanged(bot, heirId);
                 }), "roomHostChanged");

    // 13. 游戏创建失败
    checkConnect(connect(bot, &Bot::gameCreateFail, this, [this, bot, arrivalLog](GameCreationStatus status) {
                     arrivalLog("gameCreateFail");
                     this->onBotGameCreateFail(bot, status);
                 }), "gameCreateFail");

    // 14. 房间 Ping 更新
    checkConnect(connect(bot, &Bot::roomPingsUpdated, this, [this, bot, arrivalLog](const QMap<quint8, quint32> &pings) {
                     arrivalLog("roomPingsUpdated");
                     this->onBotRoomPingsUpdated(bot, pings);
                 }), "roomPingsUpdated");

    // 15. 游戏状态改变
    checkConnect(connect(bot, &Bot::gameStateChanged, this, [this, bot, arrivalLog](const QString &clientId, GameState gameState) {
                     arrivalLog("gameStateChanged");
                     this->onBotGameStateChanged(bot, clientId, gameState);
                 }), "gameStateChanged");

    // 16. 准备状态改变
    checkConnect(connect(bot, &Bot::readyStateChanged, this, [this, bot, arrivalLog](const QVariantMap &readyData) {
                     arrivalLog("readyStateChanged");
                     this->onBotReadyStateChanged(bot, readyData);
                 }), "readyStateChanged");

    // 17. 重入拒绝通知
    checkConnect(connect(bot, &Bot::rejoinRejected, this, [this, bot, arrivalLog](const QString &clientId, quint32 remainingMs) {
                     arrivalLog("rejoinRejected");
                     this->onBotRejoinRejected(bot, clientId, remainingMs);
                 }), "rejoinRejected");

    LOG_INFO(QString("   │  ✅ Bot-%1 全部信号隧道建立完毕").arg(bot->id));
}

bool BotManager::createGame(const QString &hostName, const QString &gameName, const QString &gameMode, CommandSource commandSource, const QString &clientId)
{
    LOG_INFO("----------------------------------------");
    LOG_INFO("🎮 [创建游戏任务启动]");
    LOG_INFO(QString("   ├─ 👤 房主: %1 | 模式: %2").arg(hostName, gameMode));
    LOG_INFO(QString("   ├─ 📝 房间: %1").arg(gameName));
    LOG_INFO(QString("   └─ 🆔 来源: %1 (%2)").arg((commandSource == From_Client ? "Client" : "Server"), clientId));

    if (m_bots.isEmpty()) {
        LOG_ERROR("   ❌ [拒绝] 机器人池为空，无法执行任务");
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_NO_BOTS_AVAILABLE);
        return false;
    }

    LOG_INFO(QString("   📊 [机器人池快照] 当前存量: %1").arg(m_bots.size()));

    for (int i = 0; i < m_bots.size(); ++i) {
        Bot *bot = m_bots[i];

        SignalAudit authAudit = bot->getAudit(SIGNAL(authenticated()));
        SignalAudit chatAudit = bot->getAudit(SIGNAL(enteredChat()));
        QString connectString = (bot->client && bot->client->isConnected()) ? "🌐 Online" : "🔌 Offline";
        LOG_INFO(QString("   │  [%1] %2 | %3 | 状态: %4 | 任务: %5")
                     .arg(i + 1, 2)
                     .arg(bot->username.leftJustified(12), connectString,
                          bot->botStateToString(bot->state), bot->pendingTask.hasTask ? "🔴 Busy" : "🟢 Free"));

        QString auditString = QString("   👉 [信号审计] 认证: (连:%1, 发:%2) | 大厅: (连:%3, 发:%4)")
                                  .arg(authAudit.physicalLinks).arg(authAudit.triggerCount)
                                  .arg(chatAudit.physicalLinks).arg(chatAudit.triggerCount);

        if (authAudit.physicalLinks == 0) LOG_ERROR(auditString + " ❌ 物理断路！");
        else LOG_INFO(auditString);
    }

    Bot *targetBot = nullptr;

    for (Bot *bot : qAsConst(m_bots)) {
        bool isProtocolReady = (bot->state == BotState::Idle ||
                                bot->state == BotState::InLobby ||
                                bot->state == BotState::Authenticated);

        if (isProtocolReady && bot->client && bot->client->isConnected() && !bot->pendingTask.hasTask) {
            targetBot = bot;
            LOG_INFO(QString("   ✅ [P1-就绪指派] 选中在线机器人: [%1]").arg(targetBot->username));
            break;
        }
    }

    if (!targetBot) {
        LOG_INFO("   🔍 [P1-结果] 无直接可用资源，尝试 P2 抢占/复用逻辑...");
        for (Bot *bot : qAsConst(m_bots)) {
            bool isInterruptible = (bot->state != BotState::InGame &&
                                    bot->state != BotState::Starting &&
                                    bot->state != BotState::Finishing);

            if (isInterruptible && !bot->pendingTask.hasTask) {
                targetBot = bot;
                LOG_INFO(QString("   🎯 [P2-抢占成功] 捕获机器人: [%1] (当前状态: %2)")
                             .arg(targetBot->username, bot->botStateToString(targetBot->state)));
                break;
            }
        }

        if (!targetBot) {
            LOG_INFO("   🔍 [P2-结果] 池内机器人均在忙，尝试 P3 动态扩容...");
            if (loadMoreBots(1) > 0) {
                targetBot = m_bots.last();
                LOG_INFO(QString("   📂 [P3-扩容成功] 载入新实例: [%1]").arg(targetBot->username));
            } else {
                LOG_ERROR("   ❌ [P3-结果] 扩容失败：账号文件已耗尽");
                m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_NO_BOTS_AVAILABLE);
                return false;
            }
        } else {
            LOG_INFO(QString("   ⏭️ [P3-跳过] 已获得 P2 抢占资源 [%1]").arg(targetBot->username));
        }
    } else {
        LOG_INFO(QString("   ⏭️ [P2/P3-跳过] 已获得 P1 优质资源 [%1]").arg(targetBot->username));
    }

    // --- 2. 任务执行 (Task Execution) ---

    if (targetBot) {
        targetBot->state = BotState::Creating;
        targetBot->setupGameInfo(hostName, gameName, gameMode, commandSource, clientId);
        if (targetBot->client && targetBot->client->isConnected()) {
            if (targetBot->state == BotState::Authenticated ||
                targetBot->state == BotState::Creating ||
                targetBot->state == BotState::InLobby ||
                targetBot->state == BotState::Idle) {
                targetBot->client->setHost(hostName);
                targetBot->client->setBotDisplayName(m_botDisplayName);
                targetBot->client->createGame(gameName, "", Provider_TFT_New,
                                              Game_TFT_Custom, SubType_None, Ladder_None,
                                              commandSource);

                LOG_INFO(QString("   🚀 [同步执行] 链路已就绪，直接发送 0x1C | 端口: %1 [%2]")
                             .arg(targetBot->gameInfo.port).arg(targetBot->username));
            }
            else {
                targetBot->setupPendingTask(hostName, gameName, gameMode, clientId, commandSource);
                LOG_INFO(QString("   ⏳ [异步预约] 机器人 [%1] 正在拨号/认证中，任务已挂载，成功后自动触发").arg(targetBot->username));
            }
        }
        else {
            targetBot->setupPendingTask(hostName, gameName, gameMode, clientId, commandSource);
            targetBot->setupClient(m_netManager, m_botDisplayName);
            targetBot->state = BotState::Connecting;
            targetBot->client->connectToHost(m_targetServer, m_targetPort);

            LOG_INFO(QString("   🔌 [重新拨号] 链路不在线，发起全新连接请求 [%1]").arg(targetBot->username));
        }

        LOG_INFO("----------------------------------------");
        return true;
    }

    return false;
}

void BotManager::removeGame(Bot *bot, bool disconnectFlag, const QString &reason)
{
    if (!isBotValid(bot, "RemoveGame")) return;

    // --- 1. 状态锁：细分拦截逻辑 ---

    if (bot->activeOperations > 0) {
        LOG_INFO(QString("⏳ Bot-%1 正在执行关键操作，标记为延迟移除").arg(bot->id));
        bot->pendingRemoval = true;
        bot->pendingDisconnectFlag = disconnectFlag;
        bot->pendingRemovalReason = reason;
        return;
    }

    if (bot->state == BotState::Connecting) {
        LOG_INFO(QString("🛡️ [状态锁] Bot-%1 正在发起网络连接，已拦截来自 (%2) 的清理请求")
                     .arg(bot->id).arg(reason));
        return;
    }

    if (bot->state == BotState::Unregistered) {
        LOG_INFO(QString("🛡️ [状态锁] Bot-%1 尚未完成账号注册，已拦截来自 (%2) 的清理请求")
                     .arg(bot->id).arg(reason));
        return;
    }

    if (bot->state == BotState::Authenticated) {
        LOG_INFO(QString("🛡️ [状态锁] Bot-%1 已通过身份验证但尚未就绪，已拦截来自 (%2) 的清理请求")
                     .arg(bot->id).arg(reason));
        return;
    }

    if (bot->state == BotState::Creating) {
        LOG_INFO(QString("🛡️ [状态锁] Bot-%1 正在执行房间创建指令，已拦截来自 (%2) 的清理请求")
                     .arg(bot->id).arg(reason));
        return;
    }

    // --- 2. 递归保护 ---
    if (bot->state == BotState::Finishing) {
        LOG_INFO(QString("🛡️ [递归拦截] Bot-%1 已经在清理序列中，跳过重复请求 (%2)")
                     .arg(bot->id).arg(reason));
        return;
    }

    // --- 3. 执行清理序列 ---

    bot->state = BotState::Finishing;
    bot->finishingStartTime = QDateTime::currentMSecsSinceEpoch();

    LOG_INFO(QString("🧹 [执行清理] 机器人: %1 | 原因: %2").arg(bot->username, reason));

    // A. 注销全局映射
    unregisterBotMappings(bot->gameInfo.clientId, bot->hostname, bot->gameInfo.gameName);

    // B. 执行机器人内部重置
    bot->resetGame(disconnectFlag, reason);

    // C. 向外部发射状态变更信号
    emit botStateChanged(bot->id, bot->username, bot->state);

    // D. 执行最后的协议层面取消动作
    if (bot->client) {
        LOG_INFO(QString("   └─ 📡 协议动作: 调用底层 Client::cancelGame"));
        bot->client->cancelGame();
    }
}

void BotManager::rejectCommandWithNotice(Bot *bot, const QString &clientId, const QString &command)
{
    LOG_INFO(QString("🚫 [指令拒绝序列] 捕捉到非法操作: %1").arg(command));

    if (isBotActive(bot, "Reject Command With Notice")) {
        quint8 myPid = bot->client->getPidByClientId(clientId);
        if (myPid != 0) {
            LOG_INFO(QString("   ├─ 👤 目标确认: %1 (PID: %2) @ %3")
                         .arg(bot->client->getPlayerNameByPid(myPid))
                         .arg(myPid)
                         .arg(bot->username));
            bot->client->sendAccessDeniedMessage(myPid, command);
        } else {
            LOG_INFO(QString("   ├─ 👤 目标定位失败: ClientId [%1] 当前不在房间中").arg(clientId));
        }
    } else {
        LOG_WARNING("   ├─ ⚠️ 动作受限: 机器人实例未激活，跳过游戏内通知");
    }

    if (m_netManager) {
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_PERMISSION_DENIED);
        LOG_INFO(QString("   └─ 📡 平台同步: 已向客户端 %1 发送权限拒绝码").arg(clientId));
    }
}

void BotManager::registerBotMappings(Bot *bot)
{
    if (!bot) return;

    LOG_INFO("🔗 [建立全局映射关系]");

    // 1. 映射房主名称
    if (!bot->hostname.isEmpty()) {
        QString lowerName = bot->hostname.toLower();
        m_hostNameToBotMap.insert(lowerName, bot);
        LOG_INFO(QString("   ├─ 👤 映射房主: %1 -> Bot:%2").arg(lowerName, bot->username));
    } else {
        LOG_INFO("   ├─ 👤 房主名为空，跳过映射建立");
    }

    // 2. 映射 ClientId
    if (!bot->gameInfo.clientId.isEmpty()) {
        m_clientIdToBotMap.insert(bot->gameInfo.clientId, bot);
        LOG_INFO(QString("   ├─ 🆔 映射 ID: %1 -> Bot:%2").arg(bot->gameInfo.clientId, bot->username));
    } else {
        LOG_INFO("   ├─ 🆔 ClientId 为空，跳过映射建立");
    }

    // 3. 映射房间名
    QString lowerRoom = bot->gameInfo.gameName.toLower();
    if (!lowerRoom.isEmpty()) {
        m_activeGames.insert(lowerRoom, bot);
        LOG_INFO(QString("   └─ 🏠 映射房间: %1 -> Bot:%2").arg(lowerRoom, bot->username));
    } else {
        LOG_INFO("   └─ 🏠 房间名为空，跳过映射建立");
    }
}

void BotManager::unregisterBotMappings(const QString &clientId, const QString &hostName, const QString &roomName)
{
    LOG_INFO("🧹 [清理全局映射关系]");

    // 1. 清理 ClientId 映射
    if (!clientId.isEmpty()) {
        if (m_clientIdToBotMap.remove(clientId) > 0) {
            LOG_INFO(QString("   ├─ 🆔 移除 ClientId: %1 -> ✅ 成功").arg(clientId));
        } else {
            LOG_INFO(QString("   ├─ 🆔 移除 ClientId: %1 -> ⚪ 无需清理").arg(clientId));
        }
    } else {
        LOG_INFO("   ├─ 🆔 ClientId 为空，跳过映射清理");
    }

    // 2. 清理房主名称映射
    if (!hostName.isEmpty()) {
        QString lowerName = hostName.toLower();
        if (m_hostNameToBotMap.remove(lowerName) > 0) {
            LOG_INFO(QString("   ├─ 👤 移除房主名: %1 -> ✅ 成功").arg(lowerName));
        } else {
            LOG_INFO(QString("   ├─ 👤 移除房主名: %1 -> ⚪ 无需清理").arg(lowerName));
        }
    } else {
        LOG_INFO("   ├─ 👤 房主名为空，跳过映射清理");
    }

    // 3. 清理房间映射
    if (!roomName.isEmpty()) {
        QString lowerRoom = roomName.toLower();
        if (m_activeGames.remove(lowerRoom) > 0) {
            LOG_INFO(QString("   └─ 🏠 移除房间名: %1 -> ✅ 成功").arg(lowerRoom));
        } else {
            LOG_INFO(QString("   └─ 🏠 移除房间名: %1 -> ⚪ 无需清理").arg(lowerRoom));
        }
    } else {
        LOG_INFO("   └─ 🏠 房间名为空，跳过映射清理");
    }
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

bool BotManager::isGhostRoom(Bot *bot, const QString &mappedName)
{
    if (!bot) return true;

    bool invalidState = (bot->state == BotState::Idle ||
                         bot->state == BotState::Disconnected ||
                         bot->state == BotState::Finishing);

    bool dataMismatch = (bot->gameInfo.hostName.isEmpty() ||
                         bot->gameInfo.gameName.isEmpty() ||
                         bot->gameInfo.gameName.compare(mappedName, Qt::CaseInsensitive) != 0);

    return invalidState || dataMismatch;
}

bool BotManager::isRoomExist(const QString &roomName)
{
    if (roomName.isEmpty()) return false;

    QString lowerName = roomName.toLower();

    if (!m_activeGames.contains(lowerName)) {
        return false;
    }

    Bot *bot = m_activeGames.value(lowerName);

    if (isGhostRoom(bot, lowerName)) {
        LOG_WARNING(QString("   🧹 [懒清理] 发现房间名 [%1] 的映射已失效 (Bot状态: %2)，正在自动移除...")
                        .arg(lowerName, bot ? bot->botStateToString(bot->state) : "Null"));

        m_activeGames.remove(lowerName);
        return false;
    }

    return true;
}

void BotManager::cleanupGhostRooms()
{
    LOG_INFO("🔍 [系统巡检] 正在扫描并清理幽灵房间映射...");

    int count = 0;
    auto it = m_activeGames.begin();

    while (it != m_activeGames.end()) {
        if (isGhostRoom(it.value(), it.key())) {
            LOG_WARNING(QString("   🧹 移除失效房名映射: [%1] -> Bot: %2 (状态: %3)")
                            .arg(it.key(),
                                 it.value() ? it.value()->username : "Null",
                                 it.value() ? it.value()->botStateToString(it.value()->state) : "N/A"));

            it = m_activeGames.erase(it);
            count++;
        } else {
            ++it;
        }
    }

    if (count > 0) {
        LOG_INFO(QString("   ✅ 清理完成，共移除 %1 条幽灵映射").arg(count));
    }
}

void BotManager::cleanupGhostBots()
{
    quint64 now = QDateTime::currentMSecsSinceEpoch();
    const quint64 GHOST_TIMEOUT_MS = 10000;

    for (int i = m_bots.size() - 1; i >= 0; --i) {
        Bot *bot = m_bots[i];

        if (bot->state == BotState::Finishing) {
            if (now - bot->finishingStartTime > 5000) {
                LOG_ERROR(QString("👻 [幽灵清理] Bot-%1 长期卡在 Finishing").arg(bot->id));
                unregisterBotMappings(bot->gameInfo.clientId, bot->hostname, bot->gameInfo.gameName);
                bot->resetGame(false, "Cleanup-Finishing-Timeout");
            }
        }

        else if (bot->state == BotState::Idle) {
            if (!bot->hostname.isEmpty()) {
                quint64 idleElapsed = now - bot->firstResetGameTime;

                if (idleElapsed > GHOST_TIMEOUT_MS) {
                    LOG_WARNING(QString("🧹 [状态纠错] Bot-%1 处于 %2 状态已停留 %3ms 且残留房主 [%4]，判定为创建失败，执行强制重置")
                                    .arg(bot->id)
                                    .arg(bot->botStateToString(bot->state))
                                    .arg(idleElapsed)
                                    .arg(bot->hostname));

                    unregisterBotMappings(bot->gameInfo.clientId, bot->hostname, bot->gameInfo.gameName);
                    bot->resetGame(false, "Cleanup-Idle-Zombie");
                }
            }
        }
    }
}

Bot *BotManager::findBotByHostName(const QString &hostName, bool onlyOccupied)
{
    if (hostName.isEmpty()) return nullptr;
    QString lowerName = hostName.toLower();

    Bot *bot = m_hostNameToBotMap.value(lowerName, nullptr);

    if (bot) {
        if (bot->gameInfo.gameName.isEmpty() || bot->gameInfo.hostName.isEmpty()) {
            return nullptr;
        }

        if (onlyOccupied && !bot->isOccupied()) {
            return nullptr;
        }
        return bot;
    }

    return nullptr;
}

Bot *BotManager::findBotByMemberClientId(const QString &clientId, bool onlyOccupied)
{
    if (clientId.isEmpty()) return nullptr;

    Bot *ownedBot = m_clientIdToBotMap.value(clientId, nullptr);
    if (ownedBot) {
        bool stateValid = !onlyOccupied || ownedBot->isOccupied();
        if (stateValid && !ownedBot->gameInfo.gameName.isEmpty() && !ownedBot->gameInfo.hostName.isEmpty()) {
            return ownedBot;
        }
    }

    for (Bot *bot : qAsConst(m_bots)) {
        if (isBotActive(bot, "FindMemberByClientId-Guest")) {
            bool stateValid = !onlyOccupied || bot->isOccupied();
            if (stateValid && !bot->gameInfo.gameName.isEmpty() && !bot->gameInfo.hostName.isEmpty()) {
                if (bot->client->hasPlayerByClientId(clientId)) {
                    return bot;
                }
            }
        }
    }

    return nullptr;
}

Bot *BotManager::findBotByMemberUserName(const QString &userName, bool onlyOccupied)
{
    if (userName.isEmpty()) return nullptr;
    QString lowerName = userName.toLower();

    Bot *ownedBot = m_hostNameToBotMap.value(lowerName, nullptr);
    if (ownedBot) {
        bool stateValid = !onlyOccupied || ownedBot->isOccupied();
        if (stateValid && !ownedBot->gameInfo.gameName.isEmpty() && !ownedBot->gameInfo.hostName.isEmpty()) {
            return ownedBot;
        }
    }

    for (Bot *bot : qAsConst(m_bots)) {
        if (isBotActive(bot, "FindMemberByUserName-Guest")) {
            bool stateValid = !onlyOccupied || bot->isOccupied();
            if (stateValid && !bot->gameInfo.gameName.isEmpty() && !bot->gameInfo.hostName.isEmpty()) {
                if (bot->client->hasPlayerByUserName(userName)) {
                    return bot;
                }
            }
        }
    }

    return nullptr;
}

Bot *BotManager::findBotByOwnerClientId(const QString &clientId, bool onlyOccupied)
{
    if (clientId.isEmpty()) return nullptr;

    Bot *bot = m_clientIdToBotMap.value(clientId, nullptr);

    if (bot) {
        if (onlyOccupied && !bot->isOccupied()) {
            return nullptr;
        }
        return bot;
    }

    return nullptr;
}

Bot *BotManager::findBotByOwnerUserName(const QString &userName, bool onlyOccupied)
{
    if (userName.isEmpty()) return nullptr;

    QString lowerName = userName.toLower();

    Bot *bot = m_hostNameToBotMap.value(lowerName, nullptr);

    if (bot) {
        if (onlyOccupied && !bot->isOccupied()) {
            return nullptr;
        }
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
        cooldownRules.insert("/leave", 500);
        cooldownRules.insert("/swap", 500);
        cooldownRules.insert("/ready", 1000);
        cooldownRules.insert("/unready", 1000);
        cooldownRules.insert("/swapself", 10);
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

            LOG_INFO(QString("⏳ [频率限制] ClientId: %1 | 指令: %2 | 剩余: %3ms")
                         .arg(clientId, command).arg(remaining));
            return false;
        }
    }

    // 3. 更新时间戳并放行
    m_commandCooldowns[clientId][command] = now;
    return true;
}

ErrorCode BotManager::checkStartCondition(Bot *bot, const QString &clientId, quint8 &current, quint8 &required)
{
    if (!isBotActive(bot, "CheckStartCondition")) return ERR_UNKNOWN;

    // 1. 状态与权限检查
    if (bot->state != BotState::Waiting) return ERR_UNKNOWN;
    if (!bot->isOwner(clientId)) return ERR_PERMISSION_DENIED;

    // 2. 初始化统计变量
    int sentinelHumans = 0;
    int scourgeHumans = 0;
    bool allReady = true;

    const QVector<GameSlot> &gameSlots = bot->client->getGameSlots();
    const QMap<quint8, PlayerData> &playerDataMap = bot->client->getPlayers();

    for (const GameSlot &gameSlot : gameSlots) {
        if (gameSlot.slotStatus == Occupied && gameSlot.computer == Human && gameSlot.pid != 0 && gameSlot.pid != 2) {

            // A. 统计队伍人数
            if (gameSlot.team == 0) sentinelHumans++;
            else if (gameSlot.team == 1) scourgeHumans++;

            // B. 检查准备状态
            if (playerDataMap.contains(gameSlot.pid)) {
                if (!playerDataMap[gameSlot.pid].isReady) {
                    allReady = false;
                }
            }
        }
    }

    current = sentinelHumans + scourgeHumans;
    QString mode = bot->gameInfo.gameMode.toLower();

    // 3. 执行人数与对阵判定
    if (mode.contains("solo")) {
        required = 2;
        if (sentinelHumans < 1 || scourgeHumans < 1) {
            MultiLangMsg msg;
            msg.add("zh_CN", "无法开始游戏。Solo模式需要对阵双方各就各位。")
                .add("en", "Cannot start. Solo mode requires one player on each side.");

            bot->client->broadcastChatMessage(msg);
            return ERR_NOT_ENOUGH_PLAYERS;
        }
    } else {
        required = 10;
        if (current < required) {
            MultiLangMsg msg;
            msg.add("zh_CN", QString("无法开始游戏。当前人数不足 (当前: %1, 所需: %2)。").arg(current).arg(required))
                .add("en", QString("Cannot start. Not enough players (Current: %1, Required: %2).").arg(current).arg(required));

            bot->client->broadcastChatMessage(msg);
            return ERR_NOT_ENOUGH_PLAYERS;
        }
    }

    // 4. 执行全员准备判定
    if (!allReady) {
        QStringList unreadyNames;
        for (const GameSlot &slot : gameSlots) {
            if (slot.slotStatus == Occupied && slot.pid != 0 && slot.pid != 2) {
                if (playerDataMap.contains(slot.pid) && !playerDataMap[slot.pid].isReady) {
                    unreadyNames << bot->client->getColoredTextByState(slot.pid, playerDataMap[slot.pid].name, true);
                }
            }
        }

        MultiLangMsg msg;
        QString namesText = unreadyNames.join(", ");

        msg.add("zh_CN", "无法开始游戏，请等待以下玩家准备: " + namesText)
            .add("en", "Cannot start. Waiting for: " + namesText);

        // 广播给所有人
        bot->client->broadcastChatMessage(msg);

        return ERR_PLAYERS_NOT_READY;
    }

    return ERR_OK;
}

void BotManager::handleStartWar3(const QString& clientId, const QString& roomName)
{
    LOG_INFO(QString("🚀 [War3启动申请] ClientId: %1 | 房间: %2").arg(clientId, roomName));

    Bot *bot = findBotByOwnerClientId(clientId);
    if (!bot || bot->gameInfo.gameName != roomName) {
        LOG_WARNING("   └─ ❌ 申请拒绝: 找不到关联的房间或机器人已释放");
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_NOT_IN_ROOM);
        return;
    }

    quint8 current = 0, required = 0;
    ErrorCode err = checkStartCondition(bot, clientId, current, required);

    if (err == ERR_OK) {
        LOG_INFO(QString("   └─ ✅ 验证通过: 允许 Launcher 唤起 War3.exe (当前:%1/所需:%2)")
                     .arg(current).arg(required));
        m_netManager->sendMessageToClient(clientId, S_C_START_WAR3, ERR_OK,
                                          (quint64)current << 8 | required);
    }
    else {
        LOG_ERROR(QString("   └─ ❌ 验证失败: 错误码 %1 | 状态: %2 | 人数: %3/%4")
                      .arg(err)
                      .arg(bot->botStateToString(bot->state))
                      .arg(current).arg(required));
        m_netManager->sendMessageToClient(clientId, S_C_START_WAR3, err,
                                          (quint64)current << 8 | required);
    }
}

void BotManager::handleHostCommand(const QString &userName, const QString &clientId, const QString &text)
{
    LOG_INFO("🎮 [创建房间流程]");
    cleanupGhostRooms();
    cleanupGhostBots();

    // --- 1. 重复开房/在房检查 ---
    Bot *existingBot = findBotByMemberClientId(clientId);

    if (!existingBot) {
        existingBot = findBotByHostName(userName);
    }

    if (existingBot) {
        LOG_WARNING("   ├── 🛡️ [开房拦截] 玩家已在游戏中或账号已占用机器人");

        bool isOwner = existingBot->isOwner(clientId) || (existingBot->hostname == userName);
        LOG_INFO(QString("   │   ├─ 👤 玩家身份: %1").arg(isOwner ? "房主 (Owner)" : "成员 (Guest)"));
        LOG_INFO(QString("   │   ├─ 🏠 所在房间: [%1]").arg(existingBot->gameInfo.gameName));
        LOG_INFO(QString("   │   └─ 🤖 占用机器人: %1").arg(existingBot->username));

        LOG_WARNING(QString("   └── 🚫 动作: 拒绝创建，请先通过 /leave 或 /unhost 退出当前房间"));
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_ALREADY_IN_GAME);
        return;
    }

    // --- 2. 指令参数解析 ---
    QStringList parts = text.split(" ", Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        LOG_ERROR("   └─ ❌ 错误: 参数不足 (用法: /host <模式> <房名>)");
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_PARAM_ERROR);
        return;
    }

    QString mapModel = parts[0].toLower();
    QString inputGameName = parts.mid(1).join(" ");

    // --- 3. 游戏模式校验 ---
    static const QVector<QString> allowModels = {
        "ap", "cm", "rd", "sd", "ar", "xl", "aptb", "cmtb", "rdtb", "sdtb", "artb", "xltb", "solo",
        "ap83", "cm83", "rd83", "sd83", "ar83", "xl83", "solo83", "ap83tb", "cm83tb", "rd83tb", "sd83tb", "ar83tb", "xl83tb"
    };

    if (!allowModels.contains(mapModel)) {
        LOG_ERROR(QString("   └─ ❌ 错误: 不支持的模式 [%1]").arg(mapModel));
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_MAP_NOT_SUPPORTED);
        return;
    }

    // --- 4. 房间名预处理 ---
    QString displayMode = mapModel.toUpper();
    QString baseName = inputGameName.trimmed().isEmpty() ? QString("%1's Game").arg(userName) : inputGameName.trimmed();

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
        LOG_INFO("   ├─ ✂️ 房名过长，已执行 UTF-8 安全截断");
    }

    QString finalGameName = prefix + QString::fromUtf8(nameBytes);

    // --- 5. 重名房间检查 ---
    if (isRoomExist(finalGameName)) {
        LOG_ERROR(QString("   └─ ❌ 错误: 房间名 [%1] 已存在").arg(finalGameName));
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_GAME_NAME_EXISTS);
        return;
    }

    // --- 6. 执行创建游戏 ---
    CommandInfo info;
    info.clientId = clientId;
    info.text = text.trimmed();
    info.timestamp = QDateTime::currentMSecsSinceEpoch();
    m_commandInfos.insert(userName, info);

    LOG_INFO(QString("   ├─ ✅ 验证通过: 最终房名 [%1]").arg(finalGameName));
    LOG_INFO(QString("   └─ 🚀 正在分配机器人并创建游戏..."));

    if (!createGame(userName, finalGameName, displayMode, From_Client, clientId)) {
        LOG_ERROR("   └─ ❌ 失败: 没有可用的空闲机器人");
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_NO_BOTS_AVAILABLE);
    }
}

void BotManager::onBotAuthenticated(Bot *bot)
{
    if (!isBotActive(bot, "Authenticated")) return;

    bot->state = BotState::Authenticated;
    LOG_INFO(QString("🔑 [状态更新] Bot-%1 (%2) 验证通过，准备进入大厅").arg(bot->id).arg(bot->username));

    emit botStateChanged(bot->id, bot->username, bot->state);

    bot->client->enterChat();
}

void BotManager::onBotAccountCreated(Bot *bot)
{
    if (!isBotValid(bot, "AccountCreated")) return;
    LOG_INFO(QString("   └─ 🆕 [%1] 账号注册成功，正在尝试登录...").arg(bot->username));
}

void BotManager::onBotClientExpired(const QString &clientId)
{
    Bot *bot = findBotByOwnerClientId(clientId);
    if (bot) {
        LOG_INFO(QString("🧹 [系统自动回收] 检测到 ClientId %1 会话已过期，正在强制解散房间").arg(clientId));
        removeGame(bot, true, "Session Expired");
    }
}

void BotManager::onBotCommandReceived(const QString &userName,
                                      const QString &clientId,
                                      const QString &command,
                                      const QString &text) {
    const QString trimmedCommand = command.trimmed().toLower();
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // --- 1. 基础校验 ---
    if (!m_netManager->isClientRegistered(clientId)) {
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_PERMISSION_DENIED);
        return;
    }

    if (!checkCooldown(clientId, trimmedCommand, now))
        return;

    LOG_INFO("📨 [收到用户指令]");
    LOG_INFO(QString("   ├─ 👤 发送者: %1 (ClientId: %2)").arg(userName, clientId));
    LOG_INFO(QString("   └─ 💬 内容:   %1 %2").arg(trimmedCommand, text));

    // --- 2. 全局指令 ---
    if (trimmedCommand == "/host") {
        handleHostCommand(userName, clientId, text);
        return;
    }

    // --- 3. 统一房间匹配 ---
    Bot *targetBot = findBotByMemberClientId(clientId);
    if (!targetBot) {
        targetBot = findBotByMemberUserName(userName);
    }
    if (!targetBot) {
        LOG_INFO(QString("   └─ ❌ 拒绝: 玩家 %1 不在任何房间内，无法处理指令 [%2]")
                     .arg(userName, trimmedCommand));
        if (trimmedCommand == "/unhost" || trimmedCommand == "/leave" || trimmedCommand == "/swapself") return;
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_NOT_IN_ROOM);
        return;
    }

    Client *client = targetBot->client;
    if (!client) return;

    // --- 4. 成员级别指令 ---
    if (trimmedCommand == "/ready" || trimmedCommand == "/unready") {
        bool isReady = (trimmedCommand == "/ready");
        LOG_DEBUG(QString("   └─ ⚙️ 执行状态变更: [%1] -> %2").arg(userName, isReady ? "READY" : "UNREADY"));

        client->setPlayerReadyStates(clientId, userName, isReady);
        client->syncPlayerReadyStates();

        LOG_INFO(QString("   └─ ✅ 执行成功: 玩家 %1 状态已同步为 [%2]")
                     .arg(userName, isReady ? "已准备" : "已取消准备"));
        return;
    }
    else if (trimmedCommand == "/leave") {
        LOG_INFO(QString("   └─ 🚪 玩家 %1 请求离开房间 [%2]").arg(userName, targetBot->username));

        bool success = client->disconnectPlayerByClientId(clientId);
        if (!success) {
            LOG_INFO("   ├─ 🔍 ClientId 匹配失败，尝试通过 UserName 匹配...");
            success = client->disconnectPlayerByUserName(userName);
        }

        m_netManager->sendMessageToClient(clientId, S_C_MESSAGE, MSG_PLAYER_LEAVE_GAME, success ? 1 : 0);
        LOG_INFO(QString("   └─ %1 执行结果: %2").arg(success ? "✅" : "❌", success ? "已断开" : "未找到玩家"));
        return;
    }
    else if (trimmedCommand == "/swapself") {
        quint8 myPid = client->getPidByClientId(clientId);
        if (myPid == 0) myPid = client->getPidByPlayerName(userName);

        if (myPid != 0) {
            int mySlotIndex = client->getSlotIndexByPid(myPid);
            if (mySlotIndex != -1) {
                client->swapSlots(mySlotIndex + 1, mySlotIndex + 1);
                LOG_INFO(QString("   └─ ✅ 执行成功: 玩家 %1 状态已刷新").arg(userName));
            }
        }
        return;
    }
    else if (trimmedCommand == "/swap") {
        QStringList parts = text.split(" ", Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            LOG_WARNING("   └─ ⚠️ 参数错误: /swap 指令格式不正确");
            return;
        }

        int s1 = parts[0].toInt();
        int s2 = parts[1].toInt();
        quint8 myPid = client->getPidByPlayerName(userName);
        int myUserFriendlySlot = client->getSlotIndexByPid(myPid) + 1;

        bool isOwner = targetBot->isOwner(clientId);
        bool involvesMe = (s1 == myUserFriendlySlot || s2 == myUserFriendlySlot);
        bool targetOccupied = (s1 == myUserFriendlySlot) ? client->isSlotOccupied(s2) : client->isSlotOccupied(s1);

        // 权限判定：房主随意换，普通玩家只能互换自己所在的空位
        bool permissionAllowed = isOwner || (involvesMe && !targetOccupied);

        if (permissionAllowed) {
            client->swapSlots(s1, s2);
            LOG_INFO(QString("   └── ✅ 执行成功: 槽位对调完成 (%1 <-> %2)").arg(s1).arg(s2));
        } else {
            QString reason = !involvesMe ? "只能交换自己" : "目标槽位已被占用";
            LOG_INFO(QString("   └── 🚫 权限拒绝: %1").arg(reason));
            rejectCommandWithNotice(targetBot, clientId, "/swap");
        }
        return;
    }
    else if (trimmedCommand == "/unhost") {
        if (targetBot->isOwner(clientId)) {
            LOG_INFO(QString("🧹 [指令] 房主 %1 执行解散房间").arg(userName));
            removeGame(targetBot, false, "User /unhost Command");
            m_netManager->sendMessageToClient(clientId, S_C_MESSAGE, MSG_HOST_UNHOST_GAME);
        } else {
            LOG_DEBUG(QString("   └─ ℹ️ 忽略: 玩家 %1 (Guest) 尝试解散他人房间，已静默拦截").arg(userName));
        }
        return;
    }

    // --- 5. 房主/管理员权限校验 ---
    if (!targetBot->isOwner(clientId)) {
        LOG_INFO(QString("   └─ 🛑 拦截: 玩家 %1 (Guest) 试图执行管理指令 %2")
                     .arg(userName, trimmedCommand));
        rejectCommandWithNotice(targetBot, clientId, trimmedCommand);
        return;
    }

    // --- 6. 房主级别指令 ---
    if (trimmedCommand == "/start") {
        quint8 current = 0;
        quint8 required = 0;
        ErrorCode err = checkStartCondition(targetBot, clientId, current, required);

        if (err == ERR_OK) {
            LOG_INFO(QString("   └─ 🚀 准入校验通过，正在启动游戏..."));
            client->startGame();
            targetBot->state = BotState::Starting;
            emit botStateChanged(targetBot->id, targetBot->username, targetBot->state);
            m_netManager->sendMessageToClient(clientId, S_C_MESSAGE, MSG_GAME_STATE_CHANGE, (quint64)GAME_STATE_LOADING);
        }
        else {
            LOG_INFO(QString("   └─ ❌ 启动被拒绝: 错误码 %1 (当前:%2/所需:%3)")
                         .arg(err).arg(current).arg(required));
        }
        return;
    }
    else if (trimmedCommand == "/latency" || trimmedCommand == "/lat") {
        int val = text.toInt();
        if (val >= 10 && val <= 100) {
            LOG_INFO(QString("   └─ 🚀 执行动作: 修改延迟为 %1ms").arg(val));
            client->setGameTickInterval((quint16)val);
        }
    }
    else {
        LOG_INFO(QString("   └─ ❓ 未知命令: %1 (忽略)").arg(trimmedCommand));
    }
}

void BotManager::onBotPlayerCountChanged(Bot *bot, int count)
{
    if (!isBotValid(bot, "PlayerCountChanged")) return;

    bot->gameInfo.currentPlayerCount = count;

    LOG_INFO(QString("📊 [状态同步] 接收到 Bot-%1 的人数更新").arg(bot->id));
    LOG_INFO(QString("   ├── 🏠 房间: %1").arg(bot->gameInfo.gameName));
    LOG_INFO(QString("   └── ✅ 结果: 真人玩家数已同步为 %1").arg(count));
}

void BotManager::onBotGameCreateSuccess(Bot *bot, bool isHotRefresh)
{
    if (!isBotActive(bot, "GameCreateSuccess")) return;

    // 1. 更新机器人内部属性
    bot->hostJoined = isHotRefresh;
    bot->state = isHotRefresh ? BotState::Waiting : BotState::Reserved;
    bot->gameInfo.createTime = QDateTime::currentMSecsSinceEpoch();

    // 2. 注册全局搜索映射表
    if (!isHotRefresh) {
        registerBotMappings(bot);
    }

    // 3. 提取网络指令参数
    quint16 botListenPort = bot->client->getListenPort();
    quint32 serverMapCrc = bot->client->getMapCRC();
    QString clientId = bot->gameInfo.clientId;

    // 4. 打印详细树状日志
    LOG_INFO(isHotRefresh ? "♻️ [房间看板热更新完成]" : "🎮 [房间创建完成回调]");
    LOG_INFO(QString("   ├─ 🤖 机器人实例: %1 (ID: %2)").arg(bot->username).arg(bot->id));
    LOG_INFO(QString("   ├─ 👤 房主名称:  %1 (ClientId: %2)").arg(bot->hostname, clientId));
    LOG_INFO(QString("   ├─ 🏠 房间名称:  %1").arg(bot->gameInfo.gameName));
    LOG_INFO(QString("   ├─ 🚩 游戏模式:  %1").arg(bot->gameInfo.gameMode));
    LOG_INFO(QString("   ├─ 🛡️ 地图校验:  0x%1").arg(QString::number(serverMapCrc, 16).toUpper()));
    LOG_INFO(QString("   └─ ⚙️ 当前状态:  %1").arg(bot->botStateToString(bot->state)));

    if (!m_netManager) {
        LOG_CRITICAL("   └── ❌ 系统错误: NetManager 未绑定，无法同步指令消息");
        return;
    }

    if (isHotRefresh) {
        // --- 场景 A: 房主交接热更新 ---
        LOG_INFO("   🚀 [状态同步序列] 正在为新房主同步 UI 状态...");

        // A-1. 发送 CRC 校验
        bool okCrc = m_netManager->sendMessageToClient(clientId, S_C_MESSAGE, MSG_CHECK_MAP_CRC, (quint64)serverMapCrc);

        // A-2. 发送 JOINED
        bool okSync = m_netManager->sendMessageToClient(clientId, S_C_MESSAGE, MSG_HOST_JOINED_GAME, 1);

        if (okCrc && okSync) {
            LOG_INFO("   └─ ✅ 结果: 房主交接成功。已发送热更新标志 (data=1)");
        } else {
            LOG_ERROR("   └── ❌ 警告: 状态同步消息下发失败 (用户可能突然离线)");
        }

        emit botStateChanged(bot->id, bot->username, bot->state);
        return;
    }

    // --- 场景 B: 全新创建房间 ---
    if (botListenPort == 0) {
        LOG_ERROR("   ├── ❌ 端口错误: 获取到 0！(检查 bindToRandomPort 是否成功)");
    } else {
        LOG_INFO(QString("   ├── 🔌 分配端口:  %1").arg(botListenPort));
    }

    LOG_INFO("   🚀 [控制指令下发] 正在通知 Launcher 进入房间...");

    // B-1. 发送控制命令 (ENTER_ROOM 会触发 Launcher 端的魔兽进入房间逻辑)
    bool okCmd = m_netManager->sendEnterRoomCommand(clientId, bot->gameInfo.gameName, bot->commandSource == From_Server);

    // B-2. 发送创建成功消息 (触发 Launcher 端的 Socket 准备)
    bool okCreated = m_netManager->sendMessageToClient(clientId, S_C_MESSAGE, MSG_HOST_CREATED_GAME, botListenPort);

    // B-3. 发送地图校验
    bool okCrc = m_netManager->sendMessageToClient(clientId, S_C_MESSAGE, MSG_CHECK_MAP_CRC, (quint64)serverMapCrc);

    if (okCmd && okCreated && okCrc) {
        LOG_INFO(QString("   └── ✅ 结果: 初始指令已全部送达 (目标控制端口: %1)").arg(m_controlPort));
    } else {
        LOG_ERROR("   └── ❌ 结果: 部分控制指令发送失败");
    }

    emit botStateChanged(bot->id, bot->username, bot->state);
}

void BotManager::onBotGameCreateFail(Bot *bot, GameCreationStatus status)
{
    if (!isBotValid(bot, "CreateFail")) return;

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
    removeGame(bot, false, "Game Creation Failed");

    LOG_INFO("   └─ 🔄 [状态重置] 游戏信息已清除，Bot 已归还池中");
}

void BotManager::onBotRoomHostChanged(Bot *bot, const quint8 heirPid)
{
    if (!isBotActive(bot, "RoomHostChanged")) return;

    if (bot->state == BotState::Finishing || bot->pendingRemoval) {
        LOG_INFO(QString("🛡️ [静默拦截] Bot-%1 正在解散或已标记为待移除，丢弃房主交接广播").arg(bot->id));
        return;
    }

    const auto &players = bot->client->getPlayers();
    if (!players.contains(heirPid)) return;

    QString oldClientId = bot->gameInfo.clientId;
    QString oldHostName = bot->hostname;

    QString newHostName = players[heirPid].name;
    QString newClientId = players[heirPid].clientId;

    // 1. 注销全局搜索映射表
    unregisterBotMappings(oldClientId, oldHostName, "");

    // 2. 更新机器人内部属性
    bot->hostJoined = true;
    bot->hostname = newHostName;
    bot->gameInfo.clientId = newClientId;
    bot->gameInfo.hostName = newHostName;

    // 3. 注册全局搜索映射表
    registerBotMappings(bot);

    LOG_INFO(QString("👑 [房主权限交接] Bot-%1: 从 %2 转移至 %3")
                 .arg(bot->id).arg(oldHostName, newHostName));

    // 4. 通知房间内所有玩家房主变了
    for (auto it = players.begin(); it != players.end(); ++it) {
        if (it.key() == 2) continue;
        m_netManager->sendMessageToClient(it.value().clientId, S_C_MESSAGE, MSG_ROOM_HOST_CHANGE, heirPid);
    }
}

void BotManager::onBotVisualHostLeft(Bot *bot)
{
    if (!isBotActive(bot, "VisualHostLeft")) return;

    if (bot->state == BotState::Finishing || bot->pendingRemoval) {
        LOG_INFO(QString("🛡️ [静默拦截] Bot-%1 正在解散或已标记为待移除，中止寻找继承人").arg(bot->id));
        return;
    }

    // 进入关键操作保护区
    bot->enterCriticalOperation();

    QString oldHostName = bot->gameInfo.hostName;
    QString newHostName = "";
    QString newClientId = "";
    quint8 newHostPid = 0;

    const QMap<quint8, PlayerData> &players = bot->client->getPlayers();

    // 1. 寻找继承人 (在 Client 侧已经处理过 isVisualHost 标记)
    for (auto it = players.begin(); it != players.end(); ++it) {
        if (it.key() != 2 && it.value().isVisualHost) {
            newClientId = it.value().clientId;
            newHostName = it.value().name;
            newHostPid = it.key();
            break;
        }
    }

    // 2. 执行所有权交接逻辑
    if (!newClientId.isEmpty()) {
        LOG_INFO(QString("👑 [房主移交成功] 目标: %1 | 新房主: %2 (PID: %3)")
                     .arg(bot->gameInfo.gameName, newHostName).arg(newHostPid));

        // 3. 向全房间广播通知
        for (const auto &p : players) {
            if (p.pid != 2 && !p.clientId.isEmpty()) {
                m_netManager->sendMessageToClient(p.clientId, PacketType::S_C_MESSAGE, MSG_HOST_LEAVE_GAME, newHostPid);
            }
        }

        LOG_INFO(QString("   └─ ✅ 房间 [%1] 已由玩家 %2 成功接管，所有映射已同步").arg(bot->gameInfo.gameName, newHostName));
    }
    else {
        LOG_INFO(QString("🚫 [房间关闭] 房主 %1 离开且无继承人，正在解散房间: %2").arg(oldHostName, bot->gameInfo.gameName));
        removeGame(bot, false, "VisualHostLeft: No Heir");
    }

    bot->leaveCriticalOperation();
}

void BotManager::onBotHostJoinedGame(Bot *bot, const QString &hostName)
{
    if (!isBotValid(bot, "JoinedGame")) return;

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
    const quint64 TASK_TIMEOUT_MS = 12000;               // 12秒创建超时
    const quint64 HOST_JOIN_TIMEOUT_MS = 10000;          // 10秒进房超时

    QVector<Bot*> botsCopy = m_bots;

    for (Bot *bot : qAsConst(botsCopy)) {
        if (!isBotActive(bot, "PendingTaskTimeout")) continue;

        // --- A. 任务执行超时逻辑 ---
        if (bot->pendingTask.hasTask) {
            quint64 elapsed = now - bot->pendingTask.requestTime;

            if (elapsed > TASK_TIMEOUT_MS) {
                bot->pendingTask.hasTask = false;

                LOG_INFO(QString("🚨 [任务超时] Bot-%1 (%2)").arg(bot->id).arg(bot->username));
                LOG_INFO(QString("   ├─ ⏱️ 耗时: %1 ms (阈值: %2 ms)").arg(elapsed).arg(TASK_TIMEOUT_MS));
                LOG_INFO(QString("   ├─ 📝 任务: %1").arg(bot->pendingTask.gameName));

                if (!bot->pendingTask.clientId.isEmpty()) {
                    m_netManager->sendMessageToClient(bot->pendingTask.clientId, S_C_ERROR, ERR_TASK_TIMEOUT);
                }
                removeGame(bot, true, "Create Game Task Timeout");
                continue;
            }
        }

        // --- B. 房主进场超时逻辑 ---
        if (bot->state == BotState::Reserved && !bot->hostJoined) {
            quint64 diff = now - bot->gameInfo.createTime;

            if (diff > HOST_JOIN_TIMEOUT_MS) {
                LOG_INFO(QString("🚨 [加入超时] Bot-%1 (%2)").arg(bot->id).arg(bot->username));
                LOG_INFO(QString("   ├─ 🏠 房间: %1").arg(bot->gameInfo.gameName));

                if (!bot->gameInfo.clientId.isEmpty()) {
                    m_netManager->sendMessageToClient(bot->gameInfo.clientId, S_C_ERROR, ERR_WAIT_HOST_TIMEOUT);
                }
                removeGame(bot, false, "Waiting Host Join Timeout");
            }
        }
    }
    cleanupGhostBots();
}

void BotManager::onBotRoomPingReceived(const QHostAddress &addr, quint16 port, const QString &identifier, quint64 clientTime, PingSearchMode mode)
{
    LOG_INFO("📩 [BotManager] 接收到 roomPingReceived 信号");

    Bot *bot = nullptr;
    QString modeTag;

    // 1. 根据模式执行查找逻辑
    switch (mode) {
    case ByClientId:
        bot = findBotByMemberClientId(identifier);
        modeTag = "ClientId Only";
        break;
    case ByBoth:
        bot = findBotByMemberClientId(identifier);
        if (!bot) bot = findBotByHostName(identifier);
        modeTag = "Both (Name & ClientId)";
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
        LOG_WARNING("   ├── ⚠️  查找结果: 未找到匹配的活跃 Bot");
    }

    // 4. 根据模式准备发送给客户端的参数
    if (m_netManager) {
        QString outHost = "";
        QString outClientId = "";

        if (isHit) {
            outHost = bot->hostname;
            outClientId = bot->gameInfo.clientId;
        }

        m_netManager->sendRoomPong(addr, port, clientTime, current, max, outHost, outClientId);

        LOG_INFO(QString("   └── ✅ 动作执行: 已回发 Pong (Host:%1 | ClientId:%2)")
                     .arg(outHost.isEmpty() ? "N/A" : outHost, outClientId.isEmpty() ? "N/A" : outClientId));
    }
}

void BotManager::onBotRejoinRejected(Bot *bot, const QString &clientId, quint32 remainingMs)
{
    if (!isBotValid(bot, "RejoinRejected")) return;

    LOG_INFO(QString("📢 [重入通知] 房间: %1 | 通知 ClientId: %2 | 冷却: %3ms")
                 .arg(bot->gameInfo.gameName, clientId).arg(remainingMs));

    if (m_netManager && !clientId.isEmpty()) {
        m_netManager->sendMessageToClient(clientId, S_C_MESSAGE, MSG_REJECT_REJOIN, static_cast<quint64>(remainingMs));
    }
}

void BotManager::onBotRoomPingsUpdated(Bot *bot, const QMap<quint8, quint32> &pings)
{
    if(!isBotActive(bot, "RoomPingsUpdated")) return;

    QVariantMap vMap;
    QString summary;
    for (auto it = pings.constBegin(); it != pings.constEnd(); ++it) {
        vMap.insert(QString::number(it.key()), it.value());
        if (!summary.isEmpty()) summary += ", ";
        summary += QString("P%1:%2ms").arg(it.key()).arg(it.value());
    }

    QSet<QString> targetClientIds;

    // A. 强制加入房间创建者
    if (!bot->gameInfo.clientId.isEmpty()) {
        targetClientIds.insert(bot->gameInfo.clientId);
    }

    // B. 加入房间内所有已识别的玩家
    const QMap<quint8, PlayerData> &players = bot->client->getPlayers();
    for (const auto &player : players) {
        if (!player.clientId.isEmpty()) {
            targetClientIds.insert(player.clientId);
        }
    }

    LOG_INFO(QString("📡 [延迟同步序列] 房间: %1").arg(bot->gameInfo.gameName));
    LOG_INFO(QString("   ├── 📊 实时数据: [%1]").arg(summary.isEmpty() ? "空" : summary));
    LOG_INFO(QString("   ├── 👥 接收目标: %1 个实例").arg(targetClientIds.size()));

    int sentCount = 0;
    int total = targetClientIds.size();
    int i = 0;

    for (const QString &clientId : targetClientIds) {
        i++;
        bool isLast = (i == total);
        QString branch = isLast ? "   └── " : "   ├── ";

        // 发送 TCP 指令
        if (m_netManager) {
            bool ok = m_netManager->sendRoomPings(clientId, vMap);

            if (ok) {
                sentCount++;
                bool isRoomOwner = (clientId == bot->gameInfo.clientId);
                LOG_INFO(QString("%1🚀 下发至 %2 [ClientId: %3]")
                             .arg(branch, isRoomOwner ? "👑 房主" : "👤 玩家", clientId));
            } else {
                LOG_INFO(QString("%1❌ 失败: 客户端 %2 已断开控制链路").arg(branch, clientId));
            }
        }
    }

    if (sentCount > 0) {
        LOG_INFO(QString("   ✨ 同步完成: 已成功通知 %1 个客户端").arg(sentCount));
    }
}

void BotManager::onBotGameStateChanged(Bot *bot, const QString &clientId, GameState state)
{
    if (!isBotActive(bot, "GameStateChanged")) return;

    QSet<QString> targetsClientId;

    if (!clientId.isEmpty()) {
        // 定向发送给某个人
        targetsClientId.insert(clientId);
    } else {
        // 广播给所有人（房主 + 所有成员）
        if (!bot->gameInfo.clientId.isEmpty()) targetsClientId.insert(bot->gameInfo.clientId);
        const QMap<quint8, PlayerData> &players = bot->client->getPlayers();
        for (const auto &player : players) {
            if (!player.clientId.isEmpty()) targetsClientId.insert(player.clientId);
        }
    }

    // 执行发送
    for (const QString &targetClientId : targetsClientId) {
        m_netManager->sendMessageToClient(targetClientId, S_C_MESSAGE, MSG_GAME_STATE_CHANGE, (quint64)state);
    }

    LOG_INFO(QString("📡 [状态变更] 房间: %1 | 状态: %2 | 目标: %3")
                 .arg(bot->gameInfo.gameName)
                 .arg(state)
                 .arg(clientId.isEmpty() ? "全员广播" : clientId));
}

void BotManager::onBotReadyStateChanged(Bot *bot, const QVariantMap &readyData)
{
    if(!isBotActive(bot, "ReadyStateChanged")) return;

    LOG_INFO(QString("📡 [状态广播序列] 开始处理数据同步 (双索引模式)..."));
    LOG_INFO(QString("   ├─ 🏠 房间名称: %1").arg(bot->gameInfo.gameName));
    LOG_INFO(QString("   ├─ 🤖 负责机器人: Bot-%1 (%2)").arg(bot->id).arg(bot->username));

    QVariantMap vMap = readyData;

    LOG_INFO("   ├─ 📦 待下发玩家状态列表:");

    if (vMap.isEmpty()) {
        LOG_WARNING("   │  └─ ⚠️ [警告] 状态列表为空！");
    } else {
        int loggedCount = 0;
        int totalPlayers = 0;

        // 先计算总人数（键数/2）
        totalPlayers = vMap.size() / 2;

        for (auto it = vMap.constBegin(); it != vMap.constEnd(); ++it) {
            QString key = it.key();

            // 检查键是否为数字
            bool isPidKey;
            key.toInt(&isPidKey);
            if (!isPidKey) continue;

            QVariantMap innerData   = it.value().toMap();
            bool isReady            = innerData.value("r").toBool();
            int countdown           = innerData.value("c").toInt();
            QString pName           = innerData.value("name").toString();
            int realPid             = innerData.value("pid").toInt();

            loggedCount++;
            bool isLast = (loggedCount >= totalPlayers);
            QString prefix = isLast ? "   │  └─ " : "   │  ├─ ";

            LOG_INFO(QString("%1[%2] PID:%3 -> { 状态: %4, 倒计时: %5s }")
                         .arg(prefix)
                         .arg(pName, -15)
                         .arg(realPid)
                         .arg(isReady ? "✅ 已就绪" : "⏳ 未准备")
                         .arg(countdown));
        }
    }

    // 2. 收集发送目标
    QSet<QString> targetClientIds;
    LOG_INFO("   ├─ 🎯 正在构建广播目标列表...");

    // 房主保底
    if (!bot->gameInfo.clientId.isEmpty()) {
        targetClientIds.insert(bot->gameInfo.clientId);
        LOG_INFO(QString("   │  ├─ 🏠 房主(Creator) ID 已加入: %1").arg(bot->gameInfo.clientId));
    } else {
        LOG_WARNING("   │  ├─ ⚠️ [警告] 机器人 gameInfo 中没有房主 ClientId");
    }

    const QMap<quint8, PlayerData> &players = bot->client->getPlayers();
    for (const auto &player : players) {
        if (player.pid == 2) continue;

        if (!player.clientId.isEmpty()) {
            targetClientIds.insert(player.clientId);
            LOG_INFO(QString("   │  ├─ 👤 玩家 %1 (PID:%2) ID 已加入: %3")
                         .arg(player.name, -15).arg(player.pid).arg(player.clientId));
        } else {
            LOG_ERROR(QString("   │  ├─ ❌ [失败] 无法获取玩家 %1 (PID:%2) 的 ClientId，他将无法收到同步！")
                          .arg(player.name).arg(player.pid));
        }
    }

    LOG_INFO(QString("   ├─ 📡 最终广播发送目标: %1 个去重后的终端").arg(targetClientIds.size()));

    if (m_netManager && !targetClientIds.isEmpty()) {
        m_netManager->sendRoomReadyStates(targetClientIds, vMap);
    }
}

void BotManager::onBotError(Bot *bot, QString error)
{
    if (!isBotActive(bot, "Error")) return;

    if (bot->state == BotState::Connecting) {
        LOG_WARNING(QString("⚠️ [连接失败] Bot-%1 拨号过程中遇到错误: %2").arg(bot->id).arg(error));
    } else {
        LOG_ERROR(QString("┌── ❌ [Bot异常中断] Bot-%1 (%2)").arg(bot->id).arg(bot->username));
        LOG_INFO(QString("├── 状态: %1 | 原因: %2").arg(bot->botStateToString(bot->state), error));

        QString ownerId = bot->gameInfo.clientId;

        if (!ownerId.isEmpty()) {
            if (bot->state == BotState::Creating || bot->state == BotState::Reserved) {
                LOG_INFO(QString("├── 通知: 用户 %1 (Code: ERR_CREATE_INTERRUPTED)").arg(ownerId));
                m_netManager->sendMessageToClient(ownerId, S_C_ERROR, ERR_CREATE_INTERRUPTED);
            }
            else if (bot->state == BotState::Waiting) {
                LOG_INFO(QString("├── 通知: 用户 %1 (Code: MSG_HOST_UNHOST_GAME)").arg(ownerId));
                m_netManager->sendMessageToClient(ownerId, S_C_MESSAGE, MSG_HOST_UNHOST_GAME);
            }
            else if (bot->state == BotState::InGame) {
                LOG_INFO(QString("├── 警报: 正在进行的房间 [%1] 已由于Bot崩溃销毁").arg(bot->gameInfo.gameName));
            }
        }

        if (bot->pendingTask.hasTask && !bot->pendingTask.clientId.isEmpty()) {
            QString taskOwner = bot->pendingTask.clientId;
            bot->pendingTask.hasTask = false;
            m_netManager->sendMessageToClient(taskOwner, S_C_ERROR, ERR_TASK_INTERRUPTED);
            LOG_INFO(QString("├── 清理: 已取消用户 %1 的挂起任务: %2").arg(taskOwner, bot->pendingTask.gameName));
        }

        removeGame(bot, true, "Bot Error: " + error);
    }

    if (bot->state == BotState::Disconnected && (!bot->client || !bot->client->isConnected())) {

        int retryDelay = 5000 + (bot->id * 1000);

        LOG_INFO(QString("└── 动作: 计划于 %1ms 后尝试自动重连").arg(retryDelay));

        QTimer::singleShot(retryDelay, this, [this, bot]() {
            if (m_bots.contains(bot) && bot->state == BotState::Disconnected) {
                LOG_INFO(QString("🔄 [重连尝试] Bot-%1 (%2) 重新发起连接...").arg(bot->id).arg(bot->username));
                bot->state = BotState::Connecting;
                bot->client->connectToHost(m_targetServer, m_targetPort);
            }
        });
    } else {
        LOG_INFO("└── 动作: 流程结束 (Bot 已在线或状态不符合重连条件)");
    }
}

void BotManager::onBotDisconnected(Bot *bot)
{
    if (!isBotValid(bot, "Disconnected")) return;

    if (bot->state == BotState::Creating ||
        bot->state == BotState::Connecting ||
        bot->state == BotState::Unregistered ||
        bot->state == BotState::Authenticated
        ) {

        LOG_INFO(QString("🛡️ [安全策略] 拦截延迟断开信号: Bot-%1 正在执行新任务 (%2)")
                     .arg(bot->id)
                     .arg(bot->botStateToString(bot->state)));
        return;
    }

    LOG_INFO(QString("🔌 [链路断开] Bot-%1 (%2) 物理连接已失效").arg(bot->id).arg(bot->username));

    removeGame(bot, true, "Network Socket Closed");
}

void BotManager::onBotEnteredChat(Bot *bot)
{
    if (!isBotActive(bot, "EnteredChat")) return;

    bot->state = BotState::InLobby;
    LOG_INFO(QString("💬 [状态更新] Bot-%1 (%2) 已进入聊天大厅").arg(bot->id).arg(bot->username));

    if (bot->pendingTask.hasTask) {
        LOG_INFO(QString("   ├─ 🎮 [任务触发] Bot-%1 立即执行挂起任务").arg(bot->id));

        bot->setupGameInfo(
            bot->pendingTask.hostName,
            bot->pendingTask.gameName,
            bot->pendingTask.gameMode,
            bot->pendingTask.commandSource,
            bot->pendingTask.clientId
            );

        bot->state = BotState::Creating;

        // 立即下发创建指令
        bot->client->setHost(bot->gameInfo.hostName);
        bot->client->setBotDisplayName(m_botDisplayName);
        bot->client->createGame(
            bot->gameInfo.gameName,
            "",
            Provider_TFT_New,
            Game_TFT_Custom,
            SubType_None,
            Ladder_None,
            bot->gameInfo.commandSource
            );

        // 清理任务标记
        bot->pendingTask.hasTask = false;

    } else {
        bot->state = BotState::Idle;
    }
    emit botStateChanged(bot->id, bot->username, bot->state);
}

void BotManager::onBotGameCancelled(Bot *bot)
{
    if (!isBotValid(bot, "GameCancelled")) return;

    // --- 1. 递归保护：防止 removeGame 过程中再次触发信号 ---
    if (bot->state == BotState::Finishing) {
        LOG_INFO(QString("🛡️ [递归拦截] Bot-%1 正在执行清理流程，已忽略重复信号").arg(bot->id));
        return;
    }

    // --- 2. 空闲保护：防止对已在大厅的机器人重复操作 ---
    if (bot->state == BotState::Idle) {
        LOG_INFO(QString("🛡️ [状态忽略] Bot-%1 已经是空闲状态 (Idle)，无需处理取消信号").arg(bot->id));
        return;
    }

    // --- 3. 断线保护：防止对已下线的机器人发送协议包 ---
    if (bot->state == BotState::Disconnected) {
        LOG_INFO(QString("🛡️ [断线拦截] Bot-%1 链路已断开，已忽略取消信号").arg(bot->id));
        return;
    }

    // --- 4. 流程保护：防止旧房间的取消信号破坏新房间的创建 ---
    if (bot->state == BotState::Creating) {
        LOG_INFO(QString("🛡️ [流程拦截] Bot-%1 正在开启新房间，已拦截旧局延迟信号").arg(bot->id));
        return;
    }

    // --- 5. 状态确认：记录当前正在处理的活跃取消动作 ---
    LOG_INFO(QString("🔄 [状态同步] 收到 Client 取消信号: Bot-%1 | 触发清理序列 (当前状态: %2)")
                 .arg(bot->id).arg(bot->botStateToString(bot->state)));

    // 执行核心清理逻辑
    removeGame(bot, false, "Game Cancelled");
}

void BotManager::onBotGameStarted(Bot *bot)
{
    if (!isBotActive(bot, "GameStarted")) return;

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
    LOG_INFO(QString("   └─ 👥 玩家: %1").arg(bot->client->getSlotInfoString()));

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
        startAllBots();
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
    if (m_tempRegistrationClient) {
        m_tempRegistrationClient->disconnect(this);
        m_tempRegistrationClient->deleteLater();
    }
    m_tempRegistrationClient = new Client(this);
    m_tempRegistrationClient->setNetManager(m_netManager);
    m_tempRegistrationClient->setBotDisplayName(m_botDisplayName);
    m_tempRegistrationClient->setCredentials(user, pass, Protocol_SRP_0x53);

    // 连接成功 -> 发送注册包 (延迟50ms确保握手完成)
    connect(m_tempRegistrationClient, &Client::connected, m_tempRegistrationClient, [this]() {
        QTimer::singleShot(50, m_tempRegistrationClient, &Client::createAccount);
    });

    // 定义通用结束步骤 (Lambda)
    auto isFinished = std::make_shared<bool>(false);
    auto finishStep = [this, user, isFinished](bool success, QString msg) {
        if (*isFinished) return;
        *isFinished = true;
        if (!success) {
            LOG_ERROR(QString("      │  ❌ [注册失败] 用户: %1 | 原因: %2").arg(user, msg));
        }

        if (m_tempRegistrationClient) {
            m_tempRegistrationClient->disconnect(this);
            m_tempRegistrationClient->disconnectFromHost();
        }

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
    QTimer::singleShot(5000, m_tempRegistrationClient, [finishStep, isFinished]() {
        if (!*isFinished) {
            finishStep(false, "Timeout (超时)");
        }
    });

    m_tempRegistrationClient->connectToHost(m_targetServer, m_targetPort);
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
                LOG_INFO(QString("%1❌ [缺失] %2 (自动生成已关闭，无法启动)").arg(branch, fileName));
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

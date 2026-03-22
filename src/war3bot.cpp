#include "logger.h"
#include "war3bot.h"
#include <QDir>
#include <QCoreApplication>

War3Bot::War3Bot(QObject *parent)
    : QObject(parent)
    , m_forcePortReuse(false)
    , m_botManager(nullptr)
    , m_netManager(nullptr)
    , m_client(nullptr)
{
    m_client = new Client(this);
    m_botManager = new BotManager(this);
    connect(m_client, &Client::authenticated, this, &War3Bot::onBnetAuthenticated);
    connect(m_client, &Client::gameCreateSuccess, this, &War3Bot::onGameCreateSuccess);
}

War3Bot::~War3Bot()
{
    stopServer();
    if (m_client) {
        m_client->deleteLater();
    }
    if (m_botManager) {
        m_botManager->stopAll();
    }
}

bool War3Bot::startServer(quint16 port, const QString &configFile)
{
    LOG_INFO("🚀 [System] 开始启动 War3Bot 服务器...");

    // 1. 配置文件搜索逻辑
    LOG_INFO("├── 📂 配置文件检索...");
    m_configPath = configFile;
    bool foundConfig = false;

    if (m_configPath.isEmpty()) {
        QStringList searchPaths;
#ifdef Q_OS_LINUX
        searchPaths << "/etc/War3Bot/config/war3bot.ini";
        searchPaths << "/etc/War3Bot/war3bot.ini";
#endif
        searchPaths << QCoreApplication::applicationDirPath() + "/config/war3bot.ini";
        searchPaths << "config/war3bot.ini";

        for (const QString &path : qAsConst(searchPaths)) {
            if (QFile::exists(path)) {
                m_configPath = path;
                foundConfig = true;
                LOG_INFO(QString("│   ├── ✅ 匹配到有效配置: %1").arg(QDir::toNativeSeparators(m_configPath)));
                break;
            }
        }
    } else {
        foundConfig = QFile::exists(m_configPath);
        if (foundConfig) LOG_INFO(QString("│   ├── ✅ 使用手动指定配置: %1").arg(m_configPath));
    }

    if (!foundConfig) {
        m_configPath = "config/war3bot.ini";
        LOG_WARNING("│   └── ⚠️ 未找到任何配置文件，回退至默认路径: config/war3bot.ini");
    }

    // 2. 运行状态检查
    if (m_netManager && m_netManager->isRunning()) {
        LOG_WARNING("└── ⛔ 启动中止: 服务器实例已在运行中");
        return true;
    }

    // 3. 核心组件初始化
    LOG_INFO("├── 🔧 核心组件初始化...");
    if (!m_netManager) {
        m_netManager = new NetManager(this);
        m_botManager->setNetManager(m_netManager);

        // 关键依赖注入
        if (m_client) m_client->setNetManager(m_netManager);

        connect(m_netManager, &NetManager::commandReceived, m_botManager, &BotManager::onCommandReceived);
        LOG_INFO("│   ├── NetManager: 已实例化");
        LOG_INFO("│   └── 🔗 指令链路: NetManager -> BotManager [已绑定]");
    }

    // 4. 网络服务启动
    LOG_INFO(QString("├── 🌐 网络服务绑定 (Port: %1)...").arg(port));
    bool success = m_netManager->startServer(port, m_configPath);

    if (!success) {
        LOG_ERROR("└── ❌ 致命错误: 服务器端口绑定失败 (可能被占用)");
        return false;
    }
    LOG_INFO("│   └── ✅ 监听成功");

    // 5. 业务参数解析与机器人集群启动
    if (QFile::exists(m_configPath)) {
        QSettings settings(m_configPath, QSettings::IniFormat);
        LOG_INFO(QString("├── 📑 解析业务配置: [%1]").arg(QFileInfo(m_configPath).fileName()));

        QString botDisplayName = settings.value("bots/display_name", "CC.Dota.XXX").toString();
        int botCount = settings.value("bots/init_count", 10).toInt();

        if (m_client) {
            m_client->setBotDisplayName(botDisplayName);
            LOG_INFO(QString("│   ├── 👤 主机器人显示名: %1").arg(botDisplayName));
        }

        LOG_INFO(QString("└── 🤖 机器人集群初始化 (目标在线: %1)...").arg(botCount));

        // 执行集群启动
        m_botManager->setServerPort(port);
        m_botManager->initializeBots(botCount, m_configPath);

        LOG_INFO("    ✨ [System] War3Bot 服务器已准备就绪");
    } else {
        LOG_ERROR("└── ❌ 致命错误: 无法打开配置文件进行业务逻辑初始化");
        return false;
    }

    return true;
}

void War3Bot::connectToBattleNet(QString hostname, quint16 port, QString user, QString pass)
{
    if (!m_client) return;

    // 1. 如果参数为空，尝试从配置文件读取默认值
    if (hostname.isEmpty() || port == 0 || user.isEmpty() || pass.isEmpty()) {
        if (QFile::exists(m_configPath)) {
            QSettings settings(m_configPath, QSettings::IniFormat);

            if (hostname.isEmpty()) hostname = settings.value("bnet/server", "139.155.155.166").toString();
            if (port == 0)          port     = settings.value("bnet/port", 6112).toInt();
            if (user.isEmpty())     user     = settings.value("bnet/username", "").toString();
            if (pass.isEmpty())     pass     = settings.value("bnet/password", "").toString();

            QString botDisplayName = settings.value("bots/display_name", "CC.Dota.XXX").toString();
            m_client->setBotDisplayName(botDisplayName);

            LOG_INFO("已从配置文件补全缺失的连接参数。");
        } else {
            LOG_WARNING("配置文件不存在，无法补全参数。");
        }
    }

    // 2. 检查最终参数是否有效
    if (user.isEmpty()) {
        LOG_ERROR("❌ 连接失败：用户名为空（未在参数或配置中找到）。");
        return;
    }

    LOG_INFO(QString("正在连接战网: %1@%2:%3").arg(user, hostname).arg(port));

    m_client->setCredentials(user, pass);
    m_client->connectToHost(hostname, port);
}

void War3Bot::createGame(const QString &gameName, const QString &gamePassword,
                         const QString &username, const QString &userPassword)
{
    if (!m_client) return;

    // 1. 如果已经连接且已认证，直接创建
    if (m_client->isConnected()) {
        LOG_INFO(QString("🎮 [单用户] 正在请求创建游戏: %1").arg(gameName));

        m_client->createGame(
            gameName,
            gamePassword,
            ProviderVersion::Provider_TFT_New,
            ComboGameType::Game_TFT_Custom,
            SubGameType::SubType_Internet,
            LadderType::Ladder_None,
            CommandSource::From_Server);

        // 清空可能残留的挂起任务
        m_pendingGameName.clear();
        m_pendingGamePassword.clear();
    }
    // 2. 如果未连接，先保存意图，然后发起连接
    else {
        LOG_INFO(QString("⏳ [单用户] 战网未连接，正在尝试连接并排队创建游戏: %1").arg(gameName));

        // A. 暂存游戏信息
        m_pendingGameName = gameName;
        m_pendingGamePassword = gamePassword;

        // B. 发起连接
        connectToBattleNet("", 0, username, userPassword);
    }
}

void War3Bot::cancelGame()
{
    // 彻底取消：清除挂起任务
    m_pendingGameName.clear();
    m_pendingGamePassword.clear();

    if (m_client && m_client->isConnected()) {
        LOG_INFO("❌ [单用户] 请求 Cancel (销毁房间)...");
        m_client->cancelGame();
    } else {
        LOG_INFO("ℹ️ [单用户] 战网未连接，仅清除了本地挂起的创建任务。");
    }
}

void War3Bot::stopAdv()
{
    // 1. 无论当前是否连接，先清除所有待创建的挂起任务
    m_pendingGameName.clear();
    m_pendingGamePassword.clear();

    // 2. 如果已连接，发送停止广播协议
    if (m_client && m_client->isConnected()) {
        LOG_INFO("🛑 [单用户] 正在请求停止游戏 (取消主机广播)...");
        m_client->stopAdv();
    } else {
        LOG_INFO("ℹ️ [单用户] 战网未连接，仅清除了挂起的创建任务。");
    }
}

void War3Bot::stopServer()
{
    if (m_netManager) {
        m_netManager->stopServer();
        LOG_INFO("War3Bot 服务器已停止");
    }
}

bool War3Bot::isRunning() const
{
    return m_netManager && m_netManager->isRunning();
}

void War3Bot::onPeerRegistered(const QString &peerId, const QString &clientUuid, int size)
{
    LOG_INFO(QString("新对等节点已连接 - peerID: %1, 客户端ID: %2 共计 %3 个客户端").arg(peerId, clientUuid).arg(size));
}

void War3Bot::onPeerRemoved(const QString &peerId)
{
    LOG_INFO(QString("对等节点已断开连接 - peerID: %1").arg(peerId));
}

void War3Bot::onPunchRequested(const QString &sourcePeerId, const QString &targetPeerId)
{
    LOG_INFO(QString("打洞请求已处理 - %1 -> %2").arg(sourcePeerId, targetPeerId));
}

void War3Bot::onBnetAuthenticated()
{
    LOG_INFO("✅ 战网认证通过");

    // 分支 A: 有挂起的建房任务 -> 直接建房
    if (!m_pendingGameName.isEmpty()) {
        LOG_INFO(QString("🚀 执行挂起任务: 创建游戏 [%1]").arg(m_pendingGameName));

        m_client->createGame(
            m_pendingGameName,
            m_pendingGamePassword,
            ProviderVersion::Provider_TFT_New,
            ComboGameType::Game_TFT_Custom,
            SubGameType::SubType_Internet,
            LadderType::Ladder_None,
            CommandSource::From_Server);

        m_pendingGameName.clear();
        m_pendingGamePassword.clear();
    }
    // 分支 B: 无任务 -> 进入聊天频道
    else {
        LOG_INFO("💬 无挂起任务，请求进入聊天大厅...");
        m_client->enterChat();
    }
}

void War3Bot::onGameCreateSuccess()
{
    LOG_INFO("房间创建成功");
}

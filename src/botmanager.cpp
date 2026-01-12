#include "botmanager.h"
#include "logger.h"
#include <QDir>
#include <QThread>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QCoreApplication>

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
    qDebug().noquote() << "🤖 [BotManager] 初始化序列启动";

    // 2. 清理旧数据
    stopAll();
    qDeleteAll(m_bots);
    m_bots.clear();
    m_currentFileIndex = 0;
    m_currentAccountIndex = 0;
    m_globalBotIdCounter = 1;
    m_allAccountFilePaths.clear();
    m_newAccountFilePaths.clear();

    qDebug().noquote() << "   ├─ 🧹 环境清理: 完成";

    // 3. 读取配置文件
    QSettings settings(configPath, QSettings::IniFormat);
    m_targetServer = settings.value("bnet/server", "127.0.0.1").toString();
    m_targetPort = settings.value("bnet/port", 6112).toUInt();
    m_norepeatChars = settings.value("bots/norepeat", "abcd").toString();

    m_initialLoginCount = initialCount;

    qDebug().noquote() << QString("   ├─ ⚙️ 加载配置: %1").arg(QFileInfo(configPath).fileName());
    qDebug().noquote() << QString("   │  ├─ 🖥️ 服务器: %1:%2").arg(m_targetServer).arg(m_targetPort);
    qDebug().noquote() << QString("   │  └─ 🔐 种子码: %1").arg(m_norepeatChars);

    // 4. 生成或加载文件
    bool isNewFiles = createBotAccountFilesIfNotExist();

    // 5. 根据文件状态决定流程
    if (isNewFiles) {
        qDebug().noquote() << "   └─ 🆕 模式切换: [批量注册模式]";

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
                qDebug().noquote() << QString("      ├─ 📄 加载新文件: %1").arg(QFileInfo(path).fileName());
            }
        }

        m_totalRegistrationCount = m_registrationQueue.size();
        qDebug().noquote() << QString("      ├─ 🔢 待注册总数: %1 个").arg(m_totalRegistrationCount);

        // 双重检查
        if (m_totalRegistrationCount > 0) {
            m_isMassRegistering = true;
            qDebug().noquote() << "      └─ 🚀 动作执行: 启动注册队列 (间隔防止封IP)";
            processNextRegistration();
        } else {
            qDebug().noquote() << "      └─ ⚠️ 异常警告: 队列为空 -> 降级为常规加载";
            loadMoreBots(initialCount);
            startAll();
        }

    } else {
        qDebug().noquote() << "   └─ ✅ 模式切换: [常规加载模式]";
        qDebug().noquote() << "      ├─ 📂 状态: 所有账号文件已存在，跳过注册";
        qDebug().noquote() << QString("      ├─ 📥 目标加载数: %1 个 Bot").arg(initialCount);

        loadMoreBots(initialCount);
        startAll();

        qDebug().noquote() << QString("      └─ 🚀 动作执行: 启动连接 (Bot总数: %1)").arg(m_bots.size());
    }
}

void BotManager::processNextRegistration()
{
    // 1. 队列为空 -> 流程结束 (闭环)
    if (m_registrationQueue.isEmpty()) {
        qDebug().noquote() << "      └─ 🎉 [批量注册] 流程结束";
        qDebug().noquote() << QString("         ├─ 📊 总计处理: %1 个账号").arg(m_totalRegistrationCount);
        qDebug().noquote() << QString("         └─ 🚀 状态切换: 正在启动前 %1 个机器人...").arg(m_initialLoginCount);

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
        qDebug().noquote() << QString("      ├─ ⏳ [注册进度] %1/%2 (%3%) -> 当前: %4")
                                  .arg(current, 3) // 占位符对齐
                                  .arg(m_totalRegistrationCount)
                                  .arg(percent, 2)
                                  .arg(user);
    }

    // 3. 执行注册逻辑
    m_tempRegistrationClient = new Client(this);
    m_tempRegistrationClient->setCredentials(user, pass, Protocol_SRP_0x53);

    // 连接成功 -> 发送注册包 (延迟50ms确保握手完成)
    connect(m_tempRegistrationClient, &Client::connected, m_tempRegistrationClient, [this]() {
        QTimer::singleShot(50, m_tempRegistrationClient, &Client::createAccount);
    });

    // 定义通用结束步骤 (Lambda)
    auto finishStep = [this, user](bool success, QString msg) {
        if (!success) {
            qDebug().noquote() << QString("      │  ❌ [注册失败] 用户: %1 | 原因: %2").arg(user, msg);
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
    qDebug().noquote() << QString("   ├─ 📥 [增量加载] 请求增加: %1 个机器人").arg(count);

    int loadedCount = 0;

    while (loadedCount < count) {
        // 资源耗尽检查
        if (m_currentFileIndex >= m_allAccountFilePaths.size()) {
            qDebug().noquote() << "   │  └─ ⚠️ [资源耗尽] 所有账号文件已全部加载完毕";
            break;
        }

        QString currentFullPath = m_allAccountFilePaths[m_currentFileIndex];
        QString fileName = QFileInfo(currentFullPath).fileName(); // 只显示文件名，保持日志整洁

        QFile file(currentFullPath);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug().noquote() << QString("   │  ❌ [读取失败] %1 -> 跳过").arg(fileName);
            m_currentFileIndex++;
            m_currentAccountIndex = 0;
            continue;
        }

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        if (!doc.isArray()) {
            qDebug().noquote() << QString("   │  ❌ [格式错误] %1 不是 JSON 数组 -> 跳过").arg(fileName);
            m_currentFileIndex++;
            continue;
        }

        QJsonArray array = doc.array();
        int totalInFile = array.size();
        int loadedFromThisFile = 0;

        // 打印文件节点信息
        qDebug().noquote() << QString("   │  ├─ 📂 读取来源: %1 (当前进度: %2/%3)")
                                  .arg(fileName)
                                  .arg(m_currentAccountIndex)
                                  .arg(totalInFile);
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
            qDebug().noquote() << QString("   │  │  ├─ ➕ 提取账号: %1 个").arg(loadedFromThisFile);
        }

        // 文件切换/状态更新
        if (m_currentAccountIndex >= totalInFile) {
            qDebug().noquote() << "   │  │  └─ 🏁 文件读完: 切换下一个";
            m_currentFileIndex++;
            m_currentAccountIndex = 0;
        } else {
            // 还没读完，只是满足了本次 count 需求
            qDebug().noquote() << QString("   │  │  └─ ⏸️ 暂停读取: 剩余 %1 个账号待用").arg(totalInFile - m_currentAccountIndex);
        }
    }

    // 2. 打印总结
    QString statusIcon = (loadedCount >= count) ? "✅" : "⚠️";
    qDebug().noquote() << QString("   └─ %1 [加载统计] 实际增加: %2 / 目标: %3")
                              .arg(statusIcon).arg(loadedCount).arg(count);

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
    bot->client->setCredentials(username, password, Protocol_SRP_0x53);

    // === 信号绑定 ===

    // 基础状态信号
    connect(bot->client, &Client::disconnected, this, [this, bot]() { this->onBotDisconnected(bot); });
    connect(bot->client, &Client::gameCanceled, this, [this, bot]() { this->onBotGameCanceled(bot); });
    connect(bot->client, &Client::authenticated, this, [this, bot]() { this->onBotAuthenticated(bot); });
    connect(bot->client, &Client::accountCreated, this, [this, bot]() { this->onBotAccountCreated(bot); });
    connect(bot->client, &Client::gameCreateFail, this, [this, bot]() { this->onBotGameCreateFail(bot); });
    connect(bot->client, &Client::gameCreateSuccess, this, [this, bot]() { this->onBotGameCreateSuccess(bot); });
    connect(bot->client, &Client::socketError, this, [this, bot](QString error) { this->onBotError(bot, error); });
    connect(bot->client, &Client::hostJoinedGame, this, [this, bot](const QString &name) { this->onHostJoinedGame(bot, name); });

    m_bots.append(bot);

    qDebug().noquote() << QString("   │  │  ├─ 🤖 实例化: %1 (ID: %2)").arg(username).arg(bot->id);
}

bool BotManager::createGame(const QString &hostName, const QString &gameName, CommandSource commandSource, const QString &clientId)
{
    // --- 1. 打印任务请求头部 ---
    QString sourceStr = (commandSource == From_Client) ? "客户端聊天窗口" : "服务端命令窗口";

    qDebug().noquote() << "🎮 [创建游戏任务启动]";
    qDebug().noquote() << QString("   ├─ 👤 虚拟房主: %1").arg(hostName);
    qDebug().noquote() << QString("   ├─ 📝 游戏名称: %1").arg(gameName);
    qDebug().noquote() << QString("   ├─ 🆔 命令来源: %1 (%2)").arg(sourceStr, clientId.left(8));

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
        qDebug().noquote() << QString("   ├─ ✅ 执行动作: 指派在线空闲机器人 [%1] 创建房间").arg(targetBot->username);

        // 更新 Bot 基础信息
        targetBot->hostJoined = false;
        targetBot->commandSource = commandSource;
        targetBot->gameInfo.createTime = QDateTime::currentMSecsSinceEpoch();
        targetBot->gameInfo.clientId = clientId;
        targetBot->gameInfo.hostName = hostName;
        targetBot->gameInfo.gameName = gameName;
        targetBot->client->setHost(hostName);
        targetBot->state = BotState::Creating;
        targetBot->client->createGame(gameName, "", Provider_TFT_New, Game_TFT_Custom, SubType_None, Ladder_None, commandSource);

        qDebug().noquote() << "   └─ 🚀 执行动作: 立即发送 CreateGame 指令";
        return true;
    }

    // 2. 复用离线的
    if (!targetBot) {
        for (Bot *bot : qAsConst(m_bots)) {
            // 只要不是 Connecting, Creating, Idle (即 Disconnected 或 Error) 都可以复用
            if (bot->state == BotState::Disconnected) {
                targetBot = bot;
                needConnect = true;
                qDebug().noquote() << QString("   ├─ ♻️ 资源复用: 唤醒离线机器人 [%1] (ID: %2)").arg(targetBot->username).arg(targetBot->id);
                break;
            }
        }
    }

    // 3. 动态扩容
    if (!targetBot) {
        qDebug().noquote() << "   ├─ ⚠️ 当前池中无可用 Bot，尝试从文件加载...";

        if (loadMoreBots(1) > 0) {
            targetBot = m_bots.last();
            needConnect = true;
            qDebug().noquote() << QString("   ├─ 📂 动态扩容成功: 从文件加载了 [%1]").arg(targetBot->username);
        } else {
            qDebug().noquote() << "   └─ ❌ 动态扩容失败: 所有账号文件已耗尽！";
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
            targetBot->client = new class Client(this);
            targetBot->client->setCredentials(targetBot->username, targetBot->password, Protocol_SRP_0x53);

            // 绑定信号
            connect(targetBot->client, &Client::disconnected, this, [this, targetBot]() { this->onBotDisconnected(targetBot); });
            connect(targetBot->client, &Client::gameCanceled, this, [this, targetBot]() { this->onBotGameCanceled(targetBot); });
            connect(targetBot->client, &Client::authenticated, this, [this, targetBot]() { this->onBotAuthenticated(targetBot); });
            connect(targetBot->client, &Client::accountCreated, this, [this, targetBot]() { this->onBotAccountCreated(targetBot); });
            connect(targetBot->client, &Client::gameCreateFail, this, [this, targetBot]() { this->onBotGameCreateFail(targetBot); });
            connect(targetBot->client, &Client::gameCreateSuccess, this, [this, targetBot]() { this->onBotGameCreateSuccess(targetBot); });
            connect(targetBot->client, &Client::socketError, this, [this, targetBot](QString error) { this->onBotError(targetBot, error); });
            connect(targetBot->client, &Client::hostJoinedGame, this, [this, targetBot](const QString &name) { this->onHostJoinedGame(targetBot, name); });
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

        qDebug().noquote() << QString("   └─ ⏳ 执行动作: 启动连接流程 [%1] (任务已挂起，等待登录)").arg(targetBot->username);
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
    qDebug().noquote() << QString("🔑 [认证成功] Bot-%1 (%2)").arg(bot->id).arg(bot->username);

    // 2. 检查是否有挂起的任务 (Pending Task)
    if (bot->pendingTask.hasTask) {
        // 分支 A: 处理任务
        qDebug().noquote() << "   ├─ 🎮 [发现挂起任务]";
        qDebug().noquote() << QString("   │  ├─ 👤 虚拟房主: %1").arg(bot->pendingTask.hostName);
        qDebug().noquote() << QString("   │  └─ 📝 目标房名: %1").arg(bot->pendingTask.gameName);

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
        qDebug().noquote() << "   └─ 🚀 [动作执行] 发送 CreateGame 指令 -> 状态切换: Creating";

    } else {
        // 分支 B: 无任务
        bot->state = BotState::Idle;

        // 闭环日志
        qDebug().noquote() << "   └─ 💤 [状态切换] 无挂起任务 -> 进入 Idle (空闲待机)";
    }
}

void BotManager::onBotAccountCreated(Bot *bot)
{
    if (!bot) return;
    LOG_INFO(QString("   └─ 🆕 [%1] 账号注册成功，正在尝试登录...").arg(bot->username));
}

void BotManager::onCommandReceived(const QString &userName, const QString &clientId, const QString &command, const QString &text)
{

    // 1. 全局前置检查：是否已经拥有房间？
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

    // 2. 频率限制 (Cooldown) - 防止恶意刷屏
    const qint64 CREATE_COOLDOWN_MS = 5000;
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

    // 3. 权限检查
    if (!m_netManager->isClientRegistered(clientId)) {
        m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_PERMISSION_DENIED);
        LOG_WARNING(QString("   └─ ⚠️ 忽略未注册用户的指令: %1 (%2)").arg(userName, clientId));
        return;
    }

    QString fullCmd = command + (text.isEmpty() ? "" : " " + text);

    qDebug().noquote() << "📨 [收到用户指令]";
    qDebug().noquote() << QString("   ├─ 👤 发送者: %1 (UUID: %2...)").arg(userName, clientId.left(8));
    qDebug().noquote() << QString("   └─ 💬 内容:   %1").arg(fullCmd);

    // 4. 处理 /host 指令
    if (command == "/host") {
        qDebug().noquote() << "🎮 [创建房间请求记录]";

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
        QVector<QString> allowModels = {"ar83", "sd83", "rd83", "ap83", "xl83", "ar83tb", "sd83tb", "rd83tb", "ap83tb", "xl83tb"};
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

        qDebug().noquote() << QString("   ├─ 👤 用户: %1").arg(userName);
        qDebug().noquote() << QString("   ├─ 🆔 UUID: %1").arg(clientId);
        qDebug().noquote() << QString("   └─ 💾 已存入 HostMap (当前缓存数: %1)").arg(m_commandInfos.size());

        // 4.2 房名预处理与截断
        qDebug().noquote() << "🎮 [创建房间基本信息]";
        QString baseName = text.trimmed();
        if (baseName.isEmpty()) {
            baseName = QString("%1's Game").arg(userName);
            qDebug().noquote() << QString("   ├─ ℹ️ 自动命名: %1").arg(baseName);
        } else {
            qDebug().noquote() << QString("   ├─ 📝 指定名称: %1").arg(baseName);
        }

        QString suffix = QString(" (%1/%2)").arg(1).arg(10);

        const int MAX_BYTES = 31;
        int suffixBytes = suffix.toUtf8().size();
        int availableBytes = MAX_BYTES - suffixBytes;

        qDebug().noquote() << QString("   ├─ 📏 空间计算: 总限 %1 Bytes | 后缀占用 %2 Bytes | 剩余可用 %3 Bytes")
                                  .arg(MAX_BYTES).arg(suffixBytes).arg(availableBytes);

        if (availableBytes <= 0) {
            qDebug().noquote() << "   └─ ❌ 失败: 后缀过长，无空间容纳房名";
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
            qDebug().noquote() << QString("   ├─ ✂️ 触发截断: 原始 %1 Bytes -> 截断后 %2 Bytes")
                                      .arg(originalSize).arg(nameBytes.size());
        }

        qDebug().noquote() << QString("   ├─ ✅ 最终房名: [%1]").arg(finalGameName);
        qDebug().noquote() << "   └─ 🚀 执行动作: 调用 createGame()";

        // 4.4 执行创建
        bool scheduled = createGame(userName, finalGameName, From_Client, clientId);
        if (!scheduled) {
            // 暂时无法创建房间，请稍后再试。
            m_netManager->sendMessageToClient(clientId, S_C_ERROR, ERR_NO_BOTS_AVAILABLE);
        }
    }
    // ==================== 处理 /unhost ====================
    else if (command == "/unhost") {
        qDebug().noquote() << "🛑 [取消房间流程]";
        qDebug().noquote() << "   └─ 🚀 执行动作: 返回游戏大厅";
    }
    // ==================== 处理 /bot ====================
    else if (command == "/bot") {
        qDebug().noquote() << "🤖 [Bot 切换流程]";
        qDebug().noquote() << "   └─ 🚀 执行动作: 切换 Bot 状态/所有者";
    }
    // ==================== 未知指令 ====================
    else {
        qDebug().noquote() << "⚠️ [指令未处理]";
        qDebug().noquote() << QString("   └─ ❓ 未知命令: %1 (将被忽略)").arg(command);
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
    qDebug().noquote() << "🎮 [房间创建完成回调]";
    qDebug().noquote() << QString("   ├─ 🤖 执行实例: %1").arg(bot->username);
    qDebug().noquote() << QString("   ├─ 👤 归属用户: %1").arg(clientId);
    qDebug().noquote() << QString("   └─ 🏠 房间名称: %1").arg(bot->gameInfo.gameName);

    // 4. 发送 TCP 控制指令让客户端进入
    if (m_netManager) {
        bool ok = m_netManager->sendEnterRoomCommand(clientId, m_controlPort, bot->commandSource == From_Server);

        if (ok) {
            qDebug().noquote() << QString("   └─ 🚀 自动进入: 指令已发送 (目标端口: %1)").arg(m_controlPort);
        } else {
            qDebug().noquote() << "   └─ ❌ 自动进入: 发送失败 (目标用户不在线或未记录通道)";
        }
    } else {
        qDebug().noquote() << "   └─ 🛑 系统错误: NetManager 未绑定，无法发送指令";
    }

    // 5. 广播状态变更
    emit botStateChanged(bot->id, bot->username, bot->state);
}

void BotManager::onBotGameCreateFail(Bot *bot)
{
    if (!bot) return;

    // 1. 打印根节点
    qDebug().noquote() << QString("❌ [创建失败] Bot-%1 (%2)").arg(bot->id).arg(bot->username);

    // 2. 通知客户端
    if (!bot->gameInfo.clientId.isEmpty()) {
        m_netManager->sendMessageToClient(bot->gameInfo.clientId, S_C_ERROR, ERR_CREATE_FAILED, 1);
        qDebug().noquote() << QString("   ├─ 👤 通知用户: %1 (Code: ERR_CREATE_FAILED)").arg(bot->gameInfo.clientId.left(8));
    }

    // 3. 清理资源
    removeGame(bot);

    // 4. 闭环日志
    qDebug().noquote() << "   └─ 🔄 [状态重置] 游戏信息已清除";
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
                qDebug().noquote() << QString("🚨 [任务超时] Bot-%1 (%2)").arg(bot->id).arg(bot->username);
                qDebug().noquote() << QString("   ├─ ⏱️ 耗时: %1 ms (阈值: %2 ms)").arg(now - bot->pendingTask.requestTime).arg(TIMEOUT_MS);
                qDebug().noquote() << QString("   ├─ 📝 任务: %1").arg(bot->pendingTask.gameName);

                // 2. 清除标记
                bot->pendingTask.hasTask = false;

                // 3. 通知用户 (1 表示超时)
                if (!bot->pendingTask.clientId.isEmpty()) {
                    m_netManager->sendMessageToClient(bot->pendingTask.clientId, S_C_ERROR, ERR_CREATE_FAILED, 3);
                    qDebug().noquote() << QString("   ├─ 👤 通知用户: %1 (Reason: 1-Timeout)").arg(bot->pendingTask.clientId.left(8));
                }

                // 4. 强制断线
                bot->state = BotState::Disconnected;
                qDebug().noquote() << "   └─ 🛡️ [强制动作] 标记为 Disconnected";
            }
        }

        // 检查房主加入超时
        if (bot->state == BotState::Reserved && !bot->hostJoined) {

            const quint64 diff = now - bot->gameInfo.createTime;

            if (diff > HOST_JOIN_TIMEOUT_MS) {
                // 1. 打印日志
                qDebug().noquote() << QString("🚨 [加入超时] Bot-%1 (%2)").arg(bot->id).arg(bot->username);
                qDebug().noquote() << QString("   ├─ ⏱️ 耗时: %1 ms (阈值: %2 ms)").arg(diff).arg(HOST_JOIN_TIMEOUT_MS);
                qDebug().noquote() << QString("   ├─ 🏠 房间: %1").arg(bot->gameInfo.gameName);
                qDebug().noquote() << QString("   └─ 👤 等待房主: %1 (未出现)").arg(bot->gameInfo.hostName);

                // 2. 通知客户端
                if (!bot->gameInfo.clientId.isEmpty()) {
                    m_netManager->sendMessageToClient(bot->gameInfo.clientId, S_C_ERROR, ERR_CREATE_FAILED, 4);
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

    // 1. 打印根节点 (错误详情)
    qDebug().noquote() << QString("❌ [Bot错误] Bot-%1 (%2)").arg(bot->id).arg(bot->username);
    qDebug().noquote() << QString("   ├─ 📄 原因: %1").arg(error);

    // 2. 检查是否需要通知用户 (任务失败)
    if (bot->pendingTask.hasTask && !bot->pendingTask.clientId.isEmpty()) {
        bot->pendingTask.hasTask = false;
        m_netManager->sendMessageToClient(bot->pendingTask.clientId, S_C_ERROR, ERR_CREATE_FAILED, 2);
        qDebug().noquote() << QString("   ├─ 👤 [任务中断] 通知用户: %1 (挂起任务)").arg(bot->pendingTask.clientId.left(8));
        qDebug().noquote() << QString("   │  └─ 📝 取消任务: %1").arg(bot->pendingTask.gameName);
    }
    else if (bot->state == BotState::Creating && !bot->gameInfo.clientId.isEmpty()) {
        m_netManager->sendMessageToClient(bot->gameInfo.clientId, S_C_ERROR, ERR_CREATE_FAILED, 2);
        qDebug().noquote() << QString("   ├─ 👤 [创建中断] 通知用户: %1 (创建中途掉线)").arg(bot->gameInfo.clientId.left(8));
    }

    // 3. 清理资源 (强制断线模式)
    removeGame(bot, true);

    // 4. 发出状态变更信号
    emit botStateChanged(bot->id, bot->username, bot->state);

    // 5. 自动重连逻辑
    if (bot->client && !bot->client->isConnected()) {
        int retryDelay = 5000 + (bot->id * 1000);
        qDebug().noquote() << QString("   └─ 🔄 [自动重连] 计划于 %1 ms 后执行...").arg(retryDelay);

        QTimer::singleShot(retryDelay, this, [this, bot]() {
            if (m_bots.contains(bot) && bot->client && !bot->client->isConnected()) {
                bot->client->connectToHost(m_targetServer, m_targetPort);
            }
        });
    } else {
        qDebug().noquote() << "   └─ 🛑 [流程结束] 无需重连或已连接";
    }
}

void BotManager::onBotDisconnected(Bot *bot)
{
    if (!bot) return;

    // 1. 打印根节点
    qDebug().noquote() << QString("🔌 [断开连接] Bot-%1 (%2)").arg(bot->id).arg(bot->username);

    // 2. 资源清理
    removeGame(bot, true);

    // 3. 状态变更
    emit botStateChanged(bot->id, bot->username, bot->state);

    // 4. 闭环
    qDebug().noquote() << "   └─ 🧹 [清理完成] 状态已更新为 Disconnected";
}

void BotManager::onBotGameCanceled(Bot *bot)
{
    if (!bot) return;

    // 1. 防止重复处理
    if (bot->state == BotState::Idle && bot->gameInfo.gameName.isEmpty()) {
        return;
    }

    qDebug().noquote() << QString("🔄 [状态同步] 收到 Client 取消信号: Bot-%1").arg(bot->id);

    // 2. 从全局活跃游戏列表 (m_activeGames) 中移除
    QString lowerName = bot->gameInfo.gameName.toLower();
    if (!lowerName.isEmpty()) {
        if (m_activeGames.value(lowerName) == bot) {
            m_activeGames.remove(lowerName);
            qDebug().noquote() << QString("   ├─ 🔓 释放房间名锁定: %1").arg(lowerName);
        }
    }

    // 3. 重置 Bot
    bot->resetGameState();

    // 4. 通知上层应用
    emit botStateChanged(bot->id, bot->username, bot->state);

    qDebug().noquote() << "   └─ ✅ Bot 状态已更新为 Idle";
}

// === 辅助函数 ===

QChar BotManager::randomCase(QChar c)
{
    // 50% 概率大写，50% 概率小写
    return (QRandomGenerator::global()->bounded(2) == 0) ? c.toLower() : c.toUpper();
}

QString BotManager::generateRandomSuffix(int length)
{
    const QString chars("abcdefghijklmnopqrstuvwxyz0123456789");
    QString randomString;
    for(int i=0; i<length; ++i) {
        int index = QRandomGenerator::global()->bounded(chars.length());
        randomString.append(chars.at(index));
    }
    return randomString;
}

QString BotManager::generateUniqueUsername()
{
    // 1. 获取基础字符集
    QString raw = m_norepeatChars;
    int prefixLen = raw.length();

    // 2. 转换为 QList 进行洗牌
    QList<QChar> charList;
    for (QChar c : raw) {
        charList.append(c);
    }

    for (int i = charList.size() - 1; i > 0; --i) {
        int j = QRandomGenerator::global()->bounded(i + 1);
        charList.swapItemsAt(i, j);
    }

    QString prefix;
    for (QChar c : charList) {
        prefix.append(randomCase(c));
    }

    // 3. 计算随机目标长度
    const int MIN_LEN = 5;
    const int MAX_LEN = 15;

    int safeMin = qMax(MIN_LEN, prefixLen + 1);
    int safeMax = qMax(MAX_LEN, prefixLen + 1);

    // 随机生成一个总长度
    int targetTotalLen = QRandomGenerator::global()->bounded(safeMin, safeMax + 1);

    // 计算还需要补多少位
    int suffixLen = targetTotalLen - prefixLen;

    // 如果计算出来不需要补，至少补1位保证随机性
    if (suffixLen < 1) suffixLen = 1;

    // 4. 生成后缀
    QString suffix = generateRandomSuffix(suffixLen);

    return prefix + suffix;
}

bool BotManager::createBotAccountFilesIfNotExist()
{
    qDebug().noquote() << "🔐 [账号管理] 启动账号文件检查流程";

    // 1. 确定配置目录 (configDir)
    QString configDir;
    QStringList searchPaths;

#ifdef Q_OS_LINUX
    searchPaths << "/etc/War3Bot/config";
#endif
    // 只检查当前运行目录相关的路径
    searchPaths << QCoreApplication::applicationDirPath() + "/config";
    searchPaths << QDir::currentPath() + "/config";

    bool foundExistingDir = false;
    for (const QString &path : qAsConst(searchPaths)) {
        QDir checkDir(path);
        // 只要目录存在且不为空，就认为找到了
        if (checkDir.exists() && !checkDir.isEmpty()) {
            configDir = path;
            foundExistingDir = true;
            break;
        }
    }

    // 2. 如果没找到，尝试初始化目录 (询问创建 或 从系统复制)
    if (!foundExistingDir) {
        QString defaultDir = QCoreApplication::applicationDirPath() + "/config";

        qDebug().noquote() << "------------------------------------------------------------";
        qDebug().noquote() << "⚠️  [警告] 未在标准搜索路径中找到配置文件。";
        qDebug().noquote() << QString("❓ 建议路径: %1").arg(defaultDir);
        qDebug().noquote() << "❓ 是否直接在该路径生成新账号文件? (y/n): ";

        fflush(stdout);
        QTextStream qin(stdin);
        QString answer = qin.readLine().trimmed().toLower();

        if (answer == "y" || answer == "yes" || answer.isEmpty()) {
            configDir = defaultDir;
            QDir dir(configDir);
            if (!dir.exists()) dir.mkpath(".");
        }
        else {
#ifdef Q_OS_LINUX
            // --- 尝试从系统目录复制 ---
            QString sysConfigPath = "/etc/War3Bot/config";
            QDir sysDir(sysConfigPath);

            if (sysDir.exists() && !sysDir.isEmpty()) {
                qDebug().noquote() << QString("🔍 检测到系统配置: %1").arg(sysConfigPath);
                qDebug().noquote() << "❓ 是否复制到运行目录? (y/n): ";

                if (qin.readLine().trimmed().toLower().startsWith("y")) {
                    QDir destDir(defaultDir);
                    if (!destDir.exists()) destDir.mkpath(".");

                    // 确保 .service 也被复制
                    QStringList filters;
                    filters << "*.ini" << "*.json" << "*.service";

                    QFileInfoList files = sysDir.entryInfoList(filters, QDir::Files);
                    if (files.isEmpty()) {
                        qDebug().noquote() << "   ⚠️ 系统目录为空，无法复制。";
                        return false;
                    }

                    for (const QFileInfo &fileInfo : files) {
                        QString destFile = destDir.filePath(fileInfo.fileName());
                        if (QFile::exists(destFile)) QFile::remove(destFile);
                        QFile::copy(fileInfo.absoluteFilePath(), destFile);
                        qDebug().noquote() << QString("   │  ✅ 复制成功: %1").arg(fileInfo.fileName());
                    }
                    configDir = defaultDir; // 设定成功
                } else {
                    return false; // 用户放弃
                }
            } else {
                return false; // 无系统配置
            }
#else
            return false;
#endif
        }
    }

    qDebug().noquote() << QString("   ├─ 📂 配置目录: %1").arg(configDir);

    // 3. 检查具体账号文件 (JSON)
    QString seed = m_norepeatChars.trimmed();
    if (seed.isEmpty()) seed = "default";

    QStringList files;
    files << QString("bots_%1_part1.json").arg(seed);
    files << QString("bots_%1_part2.json").arg(seed);

    bool generatedAny = false;
    m_newAccountFilePaths.clear();
    m_allAccountFilePaths.clear();

    for (int i = 0; i < files.size(); ++i) {
        QString fileName = files[i];
        QString fullPath = configDir + "/" + fileName;

        // 生成对应的 copy 文件名
        QString copyFileName = fileName;
        copyFileName.replace(".json", "_copy.json");
        QString copyPath = configDir + "/" + copyFileName;

        m_allAccountFilePaths.append(fullPath);

        // 树状图 UI
        bool isLastItem = (i == files.size() - 1);
        QString branch = isLastItem ? "   └─ " : "   ├─ ";
        QString indent = isLastItem ? "      " : "   │  ";

        // Case A: 文件已存在
        if (QFile::exists(fullPath)) {
            qDebug().noquote() << QString("%1✅ [已就绪] %2").arg(branch, fileName);
            continue;
        }

        // Case B: 需要生成
        qDebug().noquote() << QString("%1🆕 [生成中] %2").arg(branch, fileName);

        // 1. 生成数据
        QJsonArray array;
        for (int k = 0; k < 100; ++k) {
            QJsonObject obj;
            obj["u"] = generateUniqueUsername();
            obj["p"] = generateRandomSuffix(8);
            array.append(obj);
        }

        QJsonDocument doc(array);
        QByteArray jsonData = doc.toJson();

        // 2. 写入主文件
        QFile file(fullPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(jsonData);
            file.close();

            m_newAccountFilePaths.append(fullPath);
            generatedAny = true;

            qDebug().noquote() << QString("%1├─ 💾 主文件: 写入成功").arg(indent);

            // 3. 写入备份
            QFile copyFile(copyPath);
            if (copyFile.open(QIODevice::WriteOnly)) {
                copyFile.write(jsonData);
                copyFile.close();
                qDebug().noquote() << QString("%1└─ 📋 备份文件: 写入成功").arg(indent);
            } else {
                qDebug().noquote() << QString("%1└─ ❌ 备份文件: 写入失败").arg(indent);
            }

            // 将生成的文件回写到源代码目录 (包含 copy 文件)
#ifdef APP_SOURCE_DIR
            // APP_SOURCE_DIR 来自 CMake 定义
            QString srcConfigDirStr = QString(APP_SOURCE_DIR) + "/config";
            QDir srcDir(srcConfigDirStr);

            // 只有当源码目录真的存在时才执行 (防止非开发环境误操作)
            if (srcDir.exists()) {
                qDebug().noquote() << QString("%1   🚀 [同步源码] 正在回写到: %2").arg(indent, srcConfigDirStr);

                // Lambda: 复制并覆盖的辅助函数
                auto copyToSource = [&](QString srcFilePath, QString fileNameLog) {
                    QString destPath = srcDir.filePath(QFileInfo(srcFilePath).fileName());

                    // 如果目标存在，先删除，确保由 QFile::copy 进行全新复制
                    if (QFile::exists(destPath)) {
                        QFile::remove(destPath);
                    }

                    if (QFile::copy(srcFilePath, destPath)) {
                        qDebug().noquote() << QString("%1      ✅ 同步成功: %2").arg(indent, fileNameLog);
                    } else {
                        qDebug().noquote() << QString("%1      ⚠️ 同步失败: %2").arg(indent, fileNameLog);
                    }
                };

                // A. 回写主文件
                copyToSource(fullPath, fileName);

                // B. 回写 Copy 文件
                if (QFile::exists(copyPath)) {
                    copyToSource(copyPath, copyFileName);
                }
            }
#endif

        } else {
            qDebug().noquote() << QString("%1└─ ❌ 主文件: 打开失败 (%2)").arg(indent, file.errorString());
        }
    }

    return generatedAny;
}

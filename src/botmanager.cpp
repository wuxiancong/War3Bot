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
    timer->start(2000);
}

BotManager::~BotManager()
{
    stopAll();
    qDeleteAll(m_bots);
    m_bots.clear();
}

void BotManager::initializeBots(quint32 initialCount, const QString &configPath)
{
    // 1. 清理旧数据
    stopAll();
    qDeleteAll(m_bots);
    m_bots.clear();
    m_currentFileIndex = 0;
    m_currentAccountIndex = 0;
    m_globalBotIdCounter = 1;
    m_allAccountFilePaths.clear();
    m_newAccountFilePaths.clear(); // 【新增】清空新文件列表

    // 2. 读取配置文件
    QSettings settings(configPath, QSettings::IniFormat);
    m_targetServer = settings.value("bnet/server", "127.0.0.1").toString();
    m_targetPort = settings.value("bnet/port", 6112).toUInt();
    m_norepeatChars = settings.value("bots/norepeat", "abcd").toString();

    m_initialLoginCount = initialCount;

    // 3. 生成或加载文件 (函数内部会填充 m_newAccountFilePaths)
    bool isNewFiles = createBotAccountFilesIfNotExist();

    if (isNewFiles) {
        LOG_INFO("🆕 检测到新生成的账号文件！准备开始批量注册...");
        LOG_INFO("⏳ 注册过程为了防止被封IP设置了间隔，请耐心等待...");

        // 2. 只加载 "新生成的文件" 到注册队列
        m_registrationQueue.clear();

        for (const QString &path : qAsConst(m_newAccountFilePaths)) {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                QJsonArray array = doc.array();
                for (const QJsonValue &val : qAsConst(array)) {
                    QJsonObject obj = val.toObject();
                    m_registrationQueue.enqueue(qMakePair(obj["u"].toString(), obj["p"].toString()));
                }
                file.close();
            }
        }

        m_totalRegistrationCount = m_registrationQueue.size();

        // 双重检查：虽然 flag 是 true，但如果队列空了(异常情况)，直接启动
        if (m_totalRegistrationCount > 0) {
            m_isMassRegistering = true;
            processNextRegistration();
        } else {
            LOG_WARNING("⚠️ 标记为新文件，但解析出的账号队列为空，直接启动...");
            loadMoreBots(initialCount);
            startAll();
        }

    } else {
        LOG_INFO("📂 所有账号文件均已存在，跳过注册，直接加载...");
        loadMoreBots(initialCount);
        startAll();
    }
}

void BotManager::processNextRegistration()
{
    // 1. 检查队列是否为空
    if (m_registrationQueue.isEmpty()) {
        LOG_INFO(QString("🎉 批量注册完成！共处理 %1 个账号。").arg(m_totalRegistrationCount));
        m_isMassRegistering = false;

        if (m_tempRegistrationClient) {
            m_tempRegistrationClient->deleteLater();
            m_tempRegistrationClient = nullptr;
        }

        LOG_INFO(QString("🚀 正在启动前 %1 个机器人...").arg(m_initialLoginCount));
        loadMoreBots(m_initialLoginCount);
        startAll();
        return;
    }

    // 2. 取出下一个账号
    QPair<QString, QString> account = m_registrationQueue.dequeue();
    QString user = account.first;
    QString pass = account.second;

    int current = m_totalRegistrationCount - m_registrationQueue.size();
    if (current % 10 == 0) {
        LOG_INFO(QString("⏳ 注册进度: %1/%2 ...").arg(current).arg(m_totalRegistrationCount));
    }

    // 3. 创建一次性 Client
    m_tempRegistrationClient = new Client(this);
    m_tempRegistrationClient->setCredentials(user, pass, Protocol_SRP_0x53);

    // 连接成功 -> 发送注册包 (延迟50ms确保握手完成)
    connect(m_tempRegistrationClient, &Client::connected, m_tempRegistrationClient, [this]() {
        QTimer::singleShot(50, m_tempRegistrationClient, &Client::createAccount);
    });

    // 结束处理 Lambda
    auto finishStep = [this](bool success, QString msg) {
        if (!success) {
            LOG_WARNING(msg);
        }
        m_tempRegistrationClient->disconnectFromHost();

        // 延迟 200ms 后处理下一个
        QTimer::singleShot(200, this, [this]() {
            if (m_tempRegistrationClient) {
                m_tempRegistrationClient->deleteLater();
                m_tempRegistrationClient = nullptr;
            }
            this->processNextRegistration();
        });
    };

    connect(m_tempRegistrationClient, &Client::accountCreated, this, [finishStep]() { finishStep(true, "成功"); });
    connect(m_tempRegistrationClient, &Client::socketError, this, [finishStep, user](QString err) {
        LOG_ERROR(QString("注册 [%1] 网络错误: %2").arg(user, err));
        finishStep(false, err);
    });

    // 超时保护
    QTimer::singleShot(5000, m_tempRegistrationClient, [this, finishStep, user]() {
        if (m_tempRegistrationClient && m_tempRegistrationClient->isConnected()) {
            LOG_WARNING(QString("注册 [%1] 超时，跳过").arg(user));
            finishStep(false, "Timeout");
        }
    });

    m_tempRegistrationClient->connectToHost(m_targetServer, m_targetPort);
}

int BotManager::loadMoreBots(int count)
{
    int loadedCount = 0;
    while (loadedCount < count) {
        if (m_currentFileIndex >= m_allAccountFilePaths.size()) {
            LOG_WARNING("⚠️ 所有账号文件已全部加载完毕，无法再增加更多机器人！");
            break;
        }

        QString currentFileName = m_allAccountFilePaths[m_currentFileIndex];
        QFile file(currentFileName);
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_ERROR(QString("❌ 无法读取文件: %1").arg(currentFileName));
            m_currentFileIndex++;
            m_currentAccountIndex = 0;
            continue;
        }

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        if (!doc.isArray()) {
            m_currentFileIndex++;
            continue;
        }

        QJsonArray array = doc.array();
        int totalInFile = array.size();

        while (loadedCount < count && m_currentAccountIndex < totalInFile) {
            QJsonObject obj = array[m_currentAccountIndex].toObject();
            addBotInstance(obj["u"].toString(), obj["p"].toString());
            loadedCount++;
            m_currentAccountIndex++;
        }

        if (m_currentAccountIndex >= totalInFile) {
            LOG_INFO(QString("📂 文件 [%1] 读取完毕，切换下一个...").arg(currentFileName));
            m_currentFileIndex++;
            m_currentAccountIndex = 0;
        }
    }
    return loadedCount;
}

void BotManager::addBotInstance(const QString& username, const QString& password)
{
    Bot *bot = new Bot(m_globalBotIdCounter++, username, password);
    bot->client = new class Client(this);
    bot->commandSource = From_Server;
    bot->client->setCredentials(username, password, Protocol_SRP_0x53);

    connect(bot->client, &Client::authenticated, this, [this, bot]() { this->onBotAuthenticated(bot); });
    connect(bot->client, &Client::accountCreated, this, [this, bot]() { this->onBotAccountCreated(bot); });
    connect(bot->client, &Client::gameCreateSuccess, this, [this, bot]() { this->onBotGameCreateSuccess(bot); });
    connect(bot->client, &Client::socketError, this, [this, bot](QString err) { this->onBotError(bot, err); });
    connect(bot->client, &Client::disconnected, this, [this, bot]() { this->onBotDisconnected(bot); });
    connect(bot->client, &Client::hostJoinedGame, this, [this, bot](QString name) {
        if (bot->state == BotState::Reserved) {
            bot->state = BotState::Waiting;
            LOG_INFO(QString("Bot-%1: 房主 %2 已就位").arg(bot->id).arg(name));
            emit botStateChanged(bot->id, bot->username, bot->state);
        }
    });

    m_bots.append(bot);
    qDebug().noquote() << QString("🆕 加载机器人: %1 (ID: %2)").arg(username).arg(bot->id);
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
        targetBot->commandSource = commandSource;
        targetBot->gameInfo.clientId = clientId;
        targetBot->gameInfo.hostName = hostName;
        targetBot->gameInfo.gameName = gameName;
        targetBot->state = BotState::Creating;
        targetBot->client->setHost(hostName);
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
        targetBot->commandSource = commandSource;
        targetBot->gameInfo.clientId = clientId;
        targetBot->gameInfo.hostName = hostName;
        targetBot->gameInfo.gameName = gameName;

        // 2. 确保 Client 对象存在且信号已绑定
        if (!targetBot->client) {
            targetBot->client = new class Client(this);
            targetBot->client->setCredentials(targetBot->username, targetBot->password, Protocol_SRP_0x53);

            // 绑定信号
            connect(targetBot->client, &Client::authenticated, this, [this, targetBot]() { this->onBotAuthenticated(targetBot); });
            connect(targetBot->client, &Client::accountCreated, this, [this, targetBot]() { this->onBotAccountCreated(targetBot); });
            connect(targetBot->client, &Client::gameCreateSuccess, this, [this, targetBot]() { this->onBotGameCreateSuccess(targetBot); });
            connect(targetBot->client, &Client::gameCreateFail, this, [this, targetBot]() { this->onBotGameCreateFail(targetBot); });
            connect(targetBot->client, &Client::socketError, this, [this, targetBot](QString e) { this->onBotError(targetBot, e); });
            connect(targetBot->client, &Client::disconnected, this, [this, targetBot]() { this->onBotDisconnected(targetBot); });
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

void BotManager::removeGameName(Bot *bot, bool disconnectFlag)
{
    if (!bot) return;

    // 1. 从全局活跃房间表中移除
    QString lowerName = bot->gameInfo.gameName.toLower();
    if (!lowerName.isEmpty() && m_activeGames.contains(lowerName)) {
        // 确保移除的是当前 Bot 的记录
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
    qDebug().noquote() << QString("✅ [%1] 登录成功").arg(bot->username);

    // 1. 检查是否有挂起的任务 (Pending Task)
    if (bot->pendingTask.hasTask) {
        qDebug().noquote() << QString("🎮 [处理挂起任务] Bot: %1 | 任务: %2")
                                  .arg(bot->username, bot->pendingTask.gameName);

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
    } else {
        // 无任务，标记为空闲
        bot->state = BotState::Idle;
        qDebug().noquote() << QString("💤 [%1] 进入空闲待机模式").arg(bot->username);
    }
}

void BotManager::onBotAccountCreated(Bot *bot)
{
    if (!bot) return;
    LOG_INFO(QString("🆕 [%1] 账号注册成功，正在尝试登录...").arg(bot->username));
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
            LOG_WARNING(QString("⚠️ 拦截重复开房请求: 用户 %1 已在 Bot-%2 中").arg(userName).arg(bot->id));
            m_netManager->sendErrorToClient(clientId, C_S_COMMAND, CMD_ERR_ALREADY_IN_GAME);
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
            m_netManager->sendErrorToClient(clientId, C_S_COMMAND, CMD_ERR_COOLDOWN, remaining);
            return;
        }
    }

    // 更新最后操作时间
    m_lastHostTime.insert(clientId, now);

    // 3. 权限检查
    if (!m_netManager->isClientRegistered(clientId)) {
        m_netManager->sendErrorToClient(clientId, C_S_COMMAND, CMD_ERR_PERMISSION_DENIED);
        LOG_WARNING(QString("⚠️ 忽略未注册用户的指令: %1 (%2)").arg(userName, clientId));
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
            m_netManager->sendErrorToClient(clientId, C_S_COMMAND, CMD_ERR_PARAM_ERROR);
            return;
        }

        QString mapModel = parts[0].toLower();
        QString inputGameName = parts.mid(1).join(" ");

        // 4.1 检查地图模式
        QVector<QString> allowModels = {"ar83", "sd83", "rd83", "ap83", "xl83", "ar83tb", "sd83tb", "rd83tb", "ap83tb", "xl83tb"};
        if (!allowModels.contains(mapModel)) {
            // 不支持的地图模式
            m_netManager->sendErrorToClient(clientId, C_S_COMMAND, CMD_ERR_MAP_NOT_SUPPORTED);
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
            m_netManager->sendErrorToClient(clientId, C_S_COMMAND, CMD_ERR_NAME_TOO_LONG);
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
            m_netManager->sendErrorToClient(clientId, C_S_COMMAND, CMD_ERR_GAME_NAME_EXISTS);
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
            m_netManager->sendErrorToClient(clientId, C_S_COMMAND, CMD_ERR_NO_BOTS_AVAILABLE);
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
    bot->state = BotState::Reserved;

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
    LOG_ERROR(QString("❌ Bot-%1 创建游戏失败").arg(bot->id));

    if (!bot) return;

    if (!bot->gameInfo.clientId.isEmpty()) {
        // 房间创建失败
        m_netManager->sendErrorToClient(bot->gameInfo.clientId, C_S_COMMAND, CMD_ERR_CREATE_FAILED);
    }

    removeGameName(bot);

    LOG_INFO(QString("Bot-%1 状态已经重置").arg(bot->id));
}

void BotManager::onBotPendingTaskTimeout()
{
    quint64 now = QDateTime::currentMSecsSinceEpoch();
    const quint64 TIMEOUT_MS = 3000;

    for (int i = 0; i < m_bots.size(); ++i) {
        Bot *bot = m_bots[i];

        // 只检查有挂起任务的
        if (bot->pendingTask.hasTask) {

            if (now - bot->pendingTask.requestTime > TIMEOUT_MS) {
                qDebug().noquote() << QString("🚨 [任务超时] Bot: %1 | 耗时: %2 ms | 任务: %3")
                                          .arg(bot->username)
                                          .arg(now - bot->pendingTask.requestTime)
                                          .arg(bot->pendingTask.gameName);

                // 清除任务
                bot->pendingTask.hasTask = false;

                // 1 表示超时
                if (!bot->pendingTask.clientId.isEmpty()) {
                    m_netManager->sendErrorToClient(bot->pendingTask.clientId, C_S_COMMAND, CMD_ERR_CREATE_FAILED, 1);
                }

                bot->state = BotState::Disconnected;
            }
        }
    }
}

void BotManager::onBotError(Bot *bot, QString error)
{
    if (!bot) return;
    removeGameName(bot, true);
    emit botStateChanged(bot->id, bot->username, bot->state);
    LOG_WARNING(QString("❌ [%1] 错误: %2").arg(bot->username, error));

    if (bot->client && !bot->client->isConnected()) {
        int retryDelay = 5000 + (bot->id * 1000);
        LOG_INFO(QString("🔄 [%1] 将在 %2 毫秒后尝试重连...").arg(bot->username).arg(retryDelay));
        QTimer::singleShot(retryDelay, this, [this, bot]() {
            if (m_bots.contains(bot) && bot->client && !bot->client->isConnected()) {
                bot->client->connectToHost(m_targetServer, m_targetPort);
            }
        });
    }

    if (bot->pendingTask.hasTask && !bot->pendingTask.clientId.isEmpty()) {
        // 2 表示连接中断
        bot->pendingTask.hasTask = false;
        qDebug() << "❌ [任务失败] 连接错误，取消挂起任务:" << bot->pendingTask.gameName;
        m_netManager->sendErrorToClient(bot->pendingTask.clientId, C_S_COMMAND, CMD_ERR_CREATE_FAILED, 2);
    } else if (bot->state == BotState::Creating && !bot->gameInfo.clientId.isEmpty()) {
        // 如果已经在创建中(Creating)状态下报错
        m_netManager->sendErrorToClient(bot->gameInfo.clientId, C_S_COMMAND, CMD_ERR_CREATE_FAILED, 2);
    }
}

void BotManager::onBotDisconnected(Bot *bot)
{
    if (!bot) return;
    removeGameName(bot, true);
    LOG_INFO(QString("🔌 [%1] 断开连接").arg(bot->username));
    emit botStateChanged(bot->id, bot->username, bot->state);
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
    QString appDir = QCoreApplication::applicationDirPath();
    QString configDir = appDir + "/config";
    QDir dir(configDir);
    if (!dir.exists()) dir.mkpath(".");

    QStringList files = {"bots_part1.json", "bots_part2.json"};
    bool generatedAny = false;

    m_newAccountFilePaths.clear();

    for (const QString &fileName : files) {
        QString fullPath = configDir + "/" + fileName;

        m_allAccountFilePaths.append(fullPath);

        if (QFile::exists(fullPath)) {
            continue;
        }

        LOG_INFO(QString("正在生成账号文件: %1 (基于 norepeat: %2)...").arg(fullPath, m_norepeatChars));

        // 生成 100 个随机账号
        QJsonArray array;
        for (int i = 0; i < 100; ++i) {
            QJsonObject obj;
            obj["u"] = generateUniqueUsername();
            obj["p"] = generateRandomSuffix(8);
            array.append(obj);
        }

        QJsonDocument doc(array);
        QFile file(fullPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            file.close();
            LOG_INFO(QString("✅ 已生成账号文件: %1").arg(fullPath));

            m_newAccountFilePaths.append(fullPath);
            generatedAny = true;
        }
    }

    return generatedAny;
}

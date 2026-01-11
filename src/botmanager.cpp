#include "botmanager.h"
#include "logger.h"
#include <QThread>

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

void BotManager::initializeBots(quint32 count, const QString &configPath)
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
    for (quint32 i = 0; i < count; ++i) {
        // 生成用户名：前缀 + ID (例如 bot1, bot2)
        QString fullUsername = (i == 0) ? QString("%1").arg(userPrefix) : QString("%1%2").arg(userPrefix).arg(i);

        Bot *bot = new Bot(i, fullUsername, password);

        // 创建 Client 实例
        bot->client = new class Client(this);

        // 设置命令来源
        bot->commandSource = From_Server;

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
        connect(bot->client, &Client::gameCreateSuccess, this, [this, bot]() {
            this->onBotGameCreateSuccess(bot);
        });

        // 4. 错误处理
        connect(bot->client, &Client::socketError, this, [this, bot](QString err) {
            this->onBotError(bot, err);
        });

        // 5. 房主加入
        connect(bot->client, &Client::hostJoinedGame, this, [this, bot](QString name) {
            if (bot->state == BotState::Reserved) {
                bot->state = BotState::Waiting;
                LOG_INFO(QString("Bot-%1: 房主 %2 已就位，房间状态切换为 Waiting").arg(bot->id).arg(name));
                emit botStateChanged(bot->id, bot->username, bot->state);
            }
        });

        m_bots.append(bot);
    }
    LOG_INFO(QString("初始化完成，共创建 %1 个机器人对象").arg(m_bots.size()));
}

bool BotManager::createGame(const QString &hostName, const QString &gameName, CommandSource commandSource, const QString &clientId)
{
    // --- 1. 打印任务请求头部 ---
    QString sourceStr = (commandSource == From_Client) ? "客户端聊天窗口" : "服务端命令窗口";

    qDebug().noquote() << "🎮 [创建游戏任务启动]";
    qDebug().noquote() << QString("   ├─ 👤 虚拟房主: %1").arg(hostName);
    qDebug().noquote() << QString("   ├─ 📝 游戏名称: %1").arg(gameName);
    qDebug().noquote() << QString("   ├─ 🆔 来源信息: %1 (%2)").arg(sourceStr, clientId.left(8));

    Bot *targetBot = nullptr;
    bool needConnect = false; // 标记是否需要发起 TCP 连接

    // 阶段 1: 优先寻找 [已登录 && 空闲] 的 Bot (最快路径)
    for (Bot *bot : qAsConst(m_bots)) {
        if (bot->state == BotState::Idle && bot->client && bot->client->isConnected()) {
            targetBot = bot;
            break;
        }
    }

    // 如果找到在线空闲的，直接执行创建指令 (分支 B)
    if (targetBot) {
        qDebug().noquote() << QString("   ├─ ✅ 执行动作: 指派在线空闲机器人 [%1] 创建房间").arg(targetBot->username);

        // 更新 Bot 属性
        targetBot->commandSource = commandSource;
        targetBot->gameInfo.clientId = clientId;
        targetBot->gameInfo.hostName = hostName;
        targetBot->gameInfo.gameName = gameName;
        targetBot->state = BotState::Creating;

        // 设置虚拟房主并发送指令
        targetBot->client->setHost(hostName);
        targetBot->client->createGame(gameName, "", Provider_TFT_New, Game_TFT_Custom, SubType_None, Ladder_None, commandSource);

        qDebug().noquote() << "   └─ 🚀 执行动作: 立即发送 CreateGame 指令";
        return true;
    }

    // 阶段 2: 寻找 [未登录/离线] 的现有 bot (资源复用)
    if (!targetBot) {
        for (Bot *bot : qAsConst(m_bots)) {
            // 只要不是 Connecting, Creating, Idle (即 Disconnected 或 Error) 都可以复用
            if (bot->state == BotState::Disconnected) {
                targetBot = bot;
                needConnect = true; // 需要发起连接
                qDebug().noquote() << QString("   ├─ ♻️ 资源复用: 唤醒离线机器人 [%1] (ID: %2)").arg(targetBot->username).arg(targetBot->id);
                break;
            }
        }
    }

    // 阶段 3: 如果还是没有，m_bots 外的 bot (动态扩容)
    if (!targetBot) {
        quint32 maxId = 0;
        for (Bot *bot : qAsConst(m_bots)) {
            if (bot->id > maxId) maxId = bot->id;
        }
        quint32 newId = maxId + 1;
        QString newUsername = QString("%1%2").arg(m_userPrefix).arg(newId);

        qDebug().noquote() << "   ├─ ⚠️ 资源状态: 无可用 Bot -> 触发动态扩容";
        qDebug().noquote() << QString("   ├─ 🤖 新建实例: [%1] (ID: %2)").arg(newUsername).arg(newId);

        targetBot = new Bot(newId, newUsername, m_defaultPassword);
        m_bots.append(targetBot);
        needConnect = true;
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
            targetBot->client->setCredentials(targetBot->username, m_defaultPassword, Protocol_SRP_0x53);

            // 绑定信号
            connect(targetBot->client, &Client::authenticated, this, [this, targetBot]() { this->onBotAuthenticated(targetBot); });
            connect(targetBot->client, &Client::accountCreated, this, [this, targetBot]() { this->onBotAccountCreated(targetBot); });
            connect(targetBot->client, &Client::gameCreateSuccess, this, [this, targetBot]() { this->onBotGameCreateSuccess(targetBot); });
            connect(targetBot->client, &Client::gameCreateFail, this, [this, targetBot]() { this->onBotGameCreateFail(targetBot); });
            connect(targetBot->client, &Client::socketError, this, [this, targetBot](QString e) { this->onBotError(targetBot, e); });
            connect(targetBot->client, &Client::disconnected, this, [this, targetBot]() { this->onBotDisconnected(targetBot); });
        } else {
            targetBot->client->setCredentials(targetBot->username, m_defaultPassword, Protocol_SRP_0x53);
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

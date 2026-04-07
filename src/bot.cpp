#include "bot.h"
#include "logger.h"
#include "botmanager.h"

Bot::Bot(BotManager *manager, quint32 id, QString username, QString password)
    : QObject((QObject*)manager), id(id), username(username),
    password(password), state(BotState::Disconnected), m_manager(manager)
{
}

Bot::~Bot()
{
    if (client) {
        client->disconnectFromHost();
        client->deleteLater();
    }
}

void Bot::resetGameState(bool disconnectFlag, const QString &context)
{
    // 1. 根节点日志：记录来源和核心标志
    LOG_INFO(QString("🧹 [状态重置] Bot-%1 | 来源: %2").arg(this->id).arg(context));
    LOG_INFO(QString("   ├─ ⚙️ 配置: 物理断开: %1")
                 .arg(disconnectFlag ? "✅ 是" : "❌ 否"));

    if (this->client) {
        if (disconnectFlag) {
            // 场景：报错或强制下线
            if (this->client->isConnected()) {
                LOG_INFO(QString("   ├── 🔌 动作: 正在强制切断 Client-%1 的 TCP 链路...").arg(this->id));
                this->client->disconnectFromHost();
            }
        }
    }

    // 2. 内存数据彻底清空
    this->gameInfo = GameInfo();
    this->pendingTask = PendingTask();
    this->m_triggerCounts.clear();

    this->commandSource = From_Server;
    this->hostJoined = false;
    this->pendingRemoval = false;
    this->activeOperations = 0;
    this->pendingRemovalReason = "None";
}

bool Bot::isOwner(const QString &senderClientId) const
{
    bool isMatched = !gameInfo.clientId.isEmpty() && (gameInfo.clientId == senderClientId);

    if (isMatched) {
        LOG_INFO(QString("🛡️ [鉴权成功] 确认 UUID: [%1] 为 Bot-%2 的合法拥有者").arg(senderClientId).arg(id));
    } else {
        if (!gameInfo.clientId.isEmpty()) {
            LOG_WARNING(QString("🛡️ [鉴权拒绝] 身份核对不一致 (Bot-%1)").arg(id));
            LOG_INFO(QString("   ├── 🏠 所在房间: [%1]").arg(gameInfo.gameName));
            LOG_INFO(QString("   ├── 👑 记录主人: %1 (UUID: %2)").arg(hostname, gameInfo.clientId));
            LOG_INFO(QString("   └── 🚫 请求来源: %1").arg(senderClientId));
        } else {
            LOG_INFO(QString("🛡️ [鉴权跳过] 房间当前暂无绑定的所有者 UUID (Bot-%1)").arg(id));
        }
    }

    return isMatched;
}

void Bot::enterCriticalOperation()
{
    activeOperations++;
}

void Bot::leaveCriticalOperation()
{
    activeOperations--;

    if (activeOperations > 0) return;
    if (pendingRemoval) {
        QString finalReason = QString("关键操作结束，履行排队指令 (原始原因: %1)")
                                  .arg(pendingRemovalReason.isEmpty() ? "未知" : pendingRemovalReason);

        LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        LOG_INFO(QString("⏳ [清理履约] 机器人: %1 (ID:%2)").arg(username).arg(id));
        LOG_INFO(QString("   └── 🔍 溯源原因: %1").arg(pendingRemovalReason));
        LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

        if (m_manager) {
            QMetaObject::invokeMethod(m_manager, "removeGame", Qt::QueuedConnection,
                                      Q_ARG(Bot*, this),
                                      Q_ARG(bool, this->pendingDisconnectFlag),
                                      Q_ARG(QString, finalReason));
        }
    }
}

void Bot::setupClient(NetManager *netManager, const QString &displayName)
{
    LOG_INFO(QString("🏗️ [代理初始化] 正在为 Bot-%1 构建底层 Client 引擎...").arg(this->id));

    if (client) {
        LOG_INFO(QString("   ├── 🧹 正在清理旧引擎 (Bot-%1)...").arg(this->id));
        client->disconnect();
        client->disconnectFromHost();
        client->deleteLater();
        client = nullptr;
    }

    client = new Client(this);
    client->setBotFlag(true);
    client->setGameTickInterval(50);
    client->setNetManager(netManager);
    client->setBotDisplayName(displayName);
    client->setCredentials(username, password, Protocol_SRP_0x53);

    auto relayLog = [this](const QString &sig) {
        LOG_INFO(QString("📢 [信号中转] Bot-%1: 收到底层 Client::%2 -> 转发代理信号").arg(this->id).arg(sig));
    };

    auto checkRelay = [this](bool success, const char* name) {
        if (!success) {
            LOG_CRITICAL(QString("   │  ❌ [物理故障] Bot-%1 中转隧道建立失败: %2").arg(this->id).arg(name));
        } else {
            LOG_DEBUG(QString("   │  🆗 [隧道开启] %1").arg(name));
        }
    };

    LOG_INFO(QString("   ├── 📡 正在铺设信号中转隧道 (Client -> Bot)..."));

    // --- 16 路信号中转绑定 ---

    // 1. 认证
    checkRelay(connect(client, &Client::authenticated, this, [this, relayLog](){
                   this->incrementSignalCount("authenticated");
                   relayLog("authenticated");
                   emit authenticated();
               }), "authenticated");

    // 2. 进入聊天
    checkRelay(connect(client, &Client::enteredChat, this, [this, relayLog](){
                   this->incrementSignalCount("enteredChat");
                   relayLog("enteredChat");
                   emit enteredChat();
               }), "enteredChat");

    // 3. 断开
    checkRelay(connect(client, &Client::disconnected, this, [this, relayLog](){
                   this->incrementSignalCount("disconnected");
                   relayLog("disconnected");
                   emit disconnected();
               }), "disconnected");

    // 4. 游戏开始
    checkRelay(connect(client, &Client::gameStarted, this, [this, relayLog](){
                   this->incrementSignalCount("gameStarted");
                   relayLog("gameStarted");
                   emit gameStarted();
               }), "gameStarted");

    // 5. 游戏取消
    checkRelay(connect(client, &Client::gameCancelled, this, [this, relayLog](){
                   this->incrementSignalCount("gameCancelled");
                   relayLog("gameCancelled");
                   emit gameCancelled();
               }), "gameCancelled");

    // 6. 账号创建
    checkRelay(connect(client, &Client::accountCreated, this, [this, relayLog](){
                   this->incrementSignalCount("accountCreated");
                   relayLog("accountCreated");
                   emit accountCreated();
               }), "accountCreated");

    // 7. 虚拟房主离开
    checkRelay(connect(client, &Client::visualHostLeft, this, [this, relayLog](){
                   this->incrementSignalCount("visualHostLeft");
                   relayLog("visualHostLeft");
                   emit visualHostLeft();
               }), "visualHostLeft");

    // 8. 创建房间成功
    checkRelay(connect(client, &Client::gameCreateSuccess, this, [this, relayLog](CommandSource source, bool isHotRefresh){
                   this->incrementSignalCount("gameCreateSuccess");
                   QString srcStr = (source == From_Client) ? "Client" :  (source == From_Server) ? "Server" : "Unknown";
                   QString hotStr = isHotRefresh ? "true" : "false";
                   relayLog(QString("gameCreateSuccess (source: %2 hotRefresh: %3)").arg(this->id).arg(srcStr, hotStr));
                   emit gameCreateSuccess(isHotRefresh);
               }), "gameCreateSuccess");

    // 9. Socket错误
    checkRelay(connect(client, &Client::socketError, this, [this, relayLog](const QString &error){
                   this->incrementSignalCount("socketError");
                   relayLog("socketError");
                   emit socketError(error);
               }), "socketError");

    // 10. 人数变动
    checkRelay(connect(client, &Client::playerCountChanged, this, [this, relayLog](int count){
                   this->incrementSignalCount("playerCountChanged");
                   relayLog("playerCountChanged");
                   emit playerCountChanged(count);
               }), "playerCountChanged");

    // 11. 房主进场
    checkRelay(connect(client, &Client::hostJoinedGame, this, [this, relayLog](const QString &name){
                   this->incrementSignalCount("hostJoinedGame");
                   relayLog("hostJoinedGame");
                   emit hostJoinedGame(name);
               }), "hostJoinedGame");

    // 12. 房主更迭
    checkRelay(connect(client, &Client::roomHostChanged, this, [this, relayLog](const quint8 heirPid){
                   this->incrementSignalCount("roomHostChanged");
                   relayLog("roomHostChanged");
                   emit roomHostChanged(heirPid);
               }), "roomHostChanged");

    // 13. 创建失败
    checkRelay(connect(client, &Client::gameCreateFail, this, [this, relayLog](GameCreationStatus status){
                   this->incrementSignalCount("gameCreateFail");
                   relayLog("gameCreateFail");
                   emit gameCreateFail(status);
               }), "gameCreateFail");

    // 14. Pings 更新
    checkRelay(connect(client, &Client::roomPingsUpdated, this, [this, relayLog](const QMap<quint8, quint32> &pings){
                   this->incrementSignalCount("roomPingsUpdated");
                   relayLog("roomPingsUpdated");
                   emit roomPingsUpdated(pings);
               }), "roomPingsUpdated");

    // 15. 准备状态
    checkRelay(connect(client, &Client::readyStateChanged, this, [this, relayLog](const QVariantMap &readyData){
                   this->incrementSignalCount("readyStateChanged");
                   relayLog("readyStateChanged");
                   emit readyStateChanged(readyData);
               }), "readyStateChanged");

    // 16. 重入拦截
    checkRelay(connect(client, &Client::rejoinRejected, this, [this, relayLog](const QString &clientId, quint32 remainingMs){
                   this->incrementSignalCount("rejoinRejected");
                   relayLog("rejoinRejected");
                   emit rejoinRejected(clientId, remainingMs);
               }), "rejoinRejected");

    LOG_INFO(QString("   └── ✅ Bot-%1 引擎构建及信号代理完成").arg(this->id));
}

void Bot::setupPendingTask(const QString &host, const QString &name, const QString &clientId, CommandSource source)
{
    LOG_INFO(QString("⏳ [任务挂起] Bot-%1: 正在预约异步创建任务...").arg(this->id));

    this->pendingTask.hasTask = true;
    this->pendingTask.hostName = host;
    this->pendingTask.gameName = name;
    this->pendingTask.clientId = clientId;
    this->pendingTask.commandSource = source;
    this->pendingTask.requestTime = QDateTime::currentMSecsSinceEpoch();

    LOG_INFO(QString("   ├─ 👤 预约房主: %1").arg(host));
    LOG_INFO(QString("   ├─ 📝 预约房名: %1").arg(name));
    LOG_INFO(QString("   └─ 📅 戳记时间: %1").arg(this->pendingTask.requestTime));
}

void Bot::setupGameInfo(const QString &host, const QString &name, const QString &mode, CommandSource source, const QString &clientId)
{
    LOG_INFO(QString("📋 [元数据设置] Bot-%1: 正在初始化房间配置...").arg(this->id));

    resetGameState(false, "Setup Game Info");

    this->hostname = host;
    this->commandSource = source;

    this->gameInfo.hostName = host;
    this->gameInfo.gameName = name;
    this->gameInfo.gameMode = mode;
    this->gameInfo.clientId = clientId;
    this->gameInfo.createTime = QDateTime::currentMSecsSinceEpoch();

    if (client && client->getUdpSocket()) {
        this->gameInfo.port = client->getUdpSocket()->localPort();
        LOG_INFO(QString("   ├─ 🔌 端口捕获: %1 (来自底层 Socket)").arg(this->gameInfo.port));
    } else {
        this->gameInfo.port = 0;
        LOG_WARNING(QString("   ├─ 🔌 端口捕获: 0 (⚠️ 警告: 底层 Socket 尚未就绪)"));
    }

    LOG_INFO(QString("   ├─ 🏠 房间名称: %1").arg(name));
    LOG_INFO(QString("   └─ 👤 视觉房主: %1 (UUID: %2)").arg(host, clientId));
}

SignalAudit Bot::getAudit(const char* signalSignature)
{
    SignalAudit signalAudit;
    signalAudit.physicalLinks = this->receivers(signalSignature);

    QString sig = QString::fromUtf8(signalSignature);
    QString name = sig.section('(', 0, 0);
    int i = 0;
    while (i < name.length() && name.at(i).isDigit()) i++;
    name = name.mid(i);

    signalAudit.triggerCount = m_triggerCounts.value(name, 0);
    return signalAudit;
}

void Bot::incrementSignalCount(const QString &sigName) {
    m_triggerCounts[sigName]++;
}

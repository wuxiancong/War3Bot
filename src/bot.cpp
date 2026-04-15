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

bool Bot::isOccupied()
{
    return (state != BotState::Idle &&
            state != BotState::Disconnected &&
            state != BotState::Finishing);
}

void Bot::resetGame(bool disconnectFlag, const QString &context)
{
    if (!client) return;

    LOG_INFO(QString("🧹 重置 Bot-%2 | 来源: %3").arg(QString::number(id), context));

    // 1. 更新状态
    state = disconnectFlag ? BotState::Disconnected : BotState::Idle;

    // 2. 驱动底层 Client 重置
    client->resetGame();

    // 3. 处理 BNET 链路
    if (disconnectFlag && client->isConnected()) {
        LOG_INFO(QString("   ├── 🔌 动作: 正在强制切断 Client-%1 的 BNET 链路...").arg(id));
        client->disconnectFromHost();
    }

    // 4. 重置 Bot 自身的业务数据
    gameInfo                = GameInfo();
    pendingTask             = PendingTask();

    hostJoined              = false;
    pendingRemoval          = false;
    commandSource           = From_Server;
    activeOperations        = 0;
    pendingRemovalReason    = "None";

    m_triggerCounts.clear();

    LOG_INFO(QString("   └─ ✅ Bot-%1 业务数据已完全归零").arg(id));
}

bool Bot::isOwner(const QString &senderClientId) const
{
    bool isMatched = !gameInfo.clientId.isEmpty() && (gameInfo.clientId == senderClientId);

    if (isMatched) {
        LOG_INFO(QString("🛡️ [鉴权成功] 确认 ClientId: [%1] 为 Bot-%2 的合法拥有者").arg(senderClientId).arg(id));
    } else {
        if (!gameInfo.clientId.isEmpty()) {
            LOG_WARNING(QString("🛡️ [鉴权拒绝] 身份核对不一致 (Bot-%1)").arg(id));
            LOG_INFO(QString("   ├── 🏠 所在房间: [%1]").arg(gameInfo.gameName));
            LOG_INFO(QString("   ├── 👑 记录主人: %1 (ClientId: %2)").arg(hostname, gameInfo.clientId));
            LOG_INFO(QString("   └── 🚫 请求来源: %1").arg(senderClientId));
        } else {
            LOG_INFO(QString("🛡️ [鉴权跳过] 房间当前暂无绑定的所有者 ClientId (Bot-%1)").arg(id));
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
                                      Q_ARG(bool, pendingDisconnectFlag),
                                      Q_ARG(QString, finalReason));
        }
    }
}

void Bot::setupClient(NetManager *netManager, const QString &displayName)
{
    LOG_INFO(QString("🏗️ [代理初始化] 正在为 Bot-%1 构建底层 Client 引擎...").arg(id));

    if (client) {
        LOG_INFO(QString("   ├── 🧹 正在清理旧引擎 (Bot-%1)...").arg(id));
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
        LOG_INFO(QString("📢 [信号中转] Bot-%1: 收到底层 Client::%2 -> 转发代理信号").arg(id).arg(sig));
    };

    auto checkRelay = [this](bool success, const char* name) {
        if (!success) {
            LOG_CRITICAL(QString("   │  ❌ [物理故障] Bot-%1 中转隧道建立失败: %2").arg(id).arg(name));
        } else {
            LOG_DEBUG(QString("   │  🆗 [隧道开启] %1").arg(name));
        }
    };

    LOG_INFO(QString("   ├── 📡 正在铺设信号中转隧道 (Client -> Bot)..."));

    // --- 17 路信号中转绑定 ---

    // 1. 认证
    checkRelay(connect(client, &Client::authenticated, this, [this, relayLog](){
                   incrementSignalCount("authenticated");
                   relayLog("authenticated");
                   emit authenticated();
               }), "authenticated");

    // 2. 进入聊天
    checkRelay(connect(client, &Client::enteredChat, this, [this, relayLog](){
                   incrementSignalCount("enteredChat");
                   relayLog("enteredChat");
                   emit enteredChat();
               }), "enteredChat");

    // 3. 断开
    checkRelay(connect(client, &Client::disconnected, this, [this, relayLog](){
                   incrementSignalCount("disconnected");
                   relayLog("disconnected");
                   emit disconnected();
               }), "disconnected");

    // 4. 游戏开始
    checkRelay(connect(client, &Client::gameStarted, this, [this, relayLog](){
                   incrementSignalCount("gameStarted");
                   relayLog("gameStarted");
                   emit gameStarted();
               }), "gameStarted");

    // 5. 游戏取消
    checkRelay(connect(client, &Client::gameCancelled, this, [this, relayLog](){
                   incrementSignalCount("gameCancelled");
                   relayLog("gameCancelled");
                   emit gameCancelled();
               }), "gameCancelled");

    // 6. 账号创建
    checkRelay(connect(client, &Client::accountCreated, this, [this, relayLog](){
                   incrementSignalCount("accountCreated");
                   relayLog("accountCreated");
                   emit accountCreated();
               }), "accountCreated");

    // 7. 虚拟房主离开
    checkRelay(connect(client, &Client::visualHostLeft, this, [this, relayLog](){
                   incrementSignalCount("visualHostLeft");
                   relayLog("visualHostLeft");
                   emit visualHostLeft();
               }), "visualHostLeft");

    // 8. 创建房间成功
    checkRelay(connect(client, &Client::gameCreateSuccess, this, [this, relayLog](CommandSource source, bool isHotRefresh){
                   incrementSignalCount("gameCreateSuccess");
                   QString srcStr = (source == From_Client) ? "Client" :  (source == From_Server) ? "Server" : "Unknown";
                   QString hotStr = isHotRefresh ? "true" : "false";
                   relayLog(QString("gameCreateSuccess (source: %1 hotRefresh: %2)").arg(id).arg(srcStr, hotStr));
                   emit gameCreateSuccess(isHotRefresh);
               }), "gameCreateSuccess");

    // 9. Socket错误
    checkRelay(connect(client, &Client::socketError, this, [this, relayLog](const QString &error){
                   incrementSignalCount("socketError");
                   relayLog("socketError");
                   emit socketError(error);
               }), "socketError");

    // 10. 人数变动
    checkRelay(connect(client, &Client::playerCountChanged, this, [this, relayLog](int count){
                   incrementSignalCount("playerCountChanged");
                   relayLog("playerCountChanged");
                   emit playerCountChanged(count);
               }), "playerCountChanged");

    // 11. 房主进场
    checkRelay(connect(client, &Client::hostJoinedGame, this, [this, relayLog](const QString &name){
                   incrementSignalCount("hostJoinedGame");
                   relayLog("hostJoinedGame");
                   emit hostJoinedGame(name);
               }), "hostJoinedGame");

    // 12. 房主更迭
    checkRelay(connect(client, &Client::roomHostChanged, this, [this, relayLog](const quint8 heirPid){
                   incrementSignalCount("roomHostChanged");
                   relayLog("roomHostChanged");
                   emit roomHostChanged(heirPid);
               }), "roomHostChanged");

    // 13. 创建失败
    checkRelay(connect(client, &Client::gameCreateFail, this, [this, relayLog](GameCreationStatus status){
                   incrementSignalCount("gameCreateFail");
                   relayLog("gameCreateFail");
                   emit gameCreateFail(status);
               }), "gameCreateFail");

    // 14. Pings 更新
    checkRelay(connect(client, &Client::roomPingsUpdated, this, [this, relayLog](const QMap<quint8, quint32> &pings){
                   incrementSignalCount("roomPingsUpdated");
                   relayLog("roomPingsUpdated");
                   emit roomPingsUpdated(pings);
               }), "roomPingsUpdated");

    // 15. 游戏状态
    checkRelay(connect(client, &Client::gameStateChanged, this, [this, relayLog](const QString &clientId, GameState gameState){
                   incrementSignalCount("gameStateChanged");
                   relayLog("gameStateChanged");
                   emit gameStateChanged(clientId, gameState);
               }), "gameStateChanged");

    // 16. 准备状态
    checkRelay(connect(client, &Client::readyStateChanged, this, [this, relayLog](const QVariantMap &readyData){
                   incrementSignalCount("readyStateChanged");
                   relayLog("readyStateChanged");
                   emit readyStateChanged(readyData);
               }), "readyStateChanged");

    // 17. 重入拦截
    checkRelay(connect(client, &Client::rejoinRejected, this, [this, relayLog](const QString &clientId, quint32 remainingMs){
                   incrementSignalCount("rejoinRejected");
                   relayLog("rejoinRejected");
                   emit rejoinRejected(clientId, remainingMs);
               }), "rejoinRejected");

    LOG_INFO(QString("   └── ✅ Bot-%1 引擎构建及信号代理完成").arg(id));
}

void Bot::setupPendingTask(const QString &host, const QString &name, const QString &mode, const QString &clientId, CommandSource source)
{
    LOG_INFO(QString("⏳ [任务挂起] Bot-%1: 正在预约异步创建任务...").arg(id));

    pendingTask.hasTask = true;
    pendingTask.hostName = host;
    pendingTask.gameName = name;
    pendingTask.gameMode = mode;
    pendingTask.clientId = clientId;
    pendingTask.commandSource = source;
    pendingTask.requestTime = QDateTime::currentMSecsSinceEpoch();

    LOG_INFO(QString("   ├─ 👤 预约房主: %1").arg(host));
    LOG_INFO(QString("   ├─ 📝 预约房名: %1").arg(name));
    LOG_INFO(QString("   ├─ 🚩 预约模式: %1").arg(mode));
    LOG_INFO(QString("   └─ 📅 戳记时间: %1").arg(pendingTask.requestTime));
}

void Bot::setupGameInfo(const QString &host, const QString &name, const QString &mode, CommandSource source, const QString &clientId)
{
    LOG_INFO(QString("📋 [元数据设置] Bot-%1: 正在初始化房间配置...").arg(id));

    resetGame(false, "Setup Game Info");

    hostname = host;
    commandSource = source;

    gameInfo.hostName = host;
    gameInfo.gameName = name;
    gameInfo.gameMode = mode;
    gameInfo.clientId = clientId;
    pendingTask.commandSource = source;
    gameInfo.createTime = QDateTime::currentMSecsSinceEpoch();

    if (client && client->getUdpSocket()) {
        gameInfo.port = client->getUdpSocket()->localPort();
        LOG_INFO(QString("   ├─ 🔌 端口捕获: %1 (来自底层 Socket)").arg(gameInfo.port));
    } else {
        gameInfo.port = 0;
        LOG_WARNING(QString("   ├─ 🔌 端口捕获: 0 (⚠️ 警告: 底层 Socket 尚未就绪)"));
    }

    LOG_INFO(QString("   ├─ 🏠 房间名称: %1").arg(name));
    LOG_INFO(QString("   └─ 👤 视觉房主: %1 (ClientId: %2)").arg(host, clientId));
}

QString Bot::botStateToString(BotState state)
{
    switch (state) {
    case BotState::Disconnected:  return "Disconnected (已断开)";
    case BotState::Connecting:    return "Connecting (连接中)";
    case BotState::Unregistered:  return "Unregistered (未注册)";
    case BotState::Authenticated: return "Authenticated (已认证)";
    case BotState::InLobby:       return "InLobby (大厅中)";
    case BotState::Idle:          return "Idle (空闲待机)";
    case BotState::Creating:      return "Creating (创建中)";
    case BotState::Reserved:      return "Reserved (已预留)";
    case BotState::Waiting:       return "Waiting (等待房主)";
    case BotState::Starting:      return "Starting (启动中)";
    case BotState::InGame:        return "InGame (游戏内)";
    case BotState::Finishing:     return "Finishing (清理中)";
    default:                      return QString("Unknown (%1)").arg(static_cast<int>(state));
    }
}

SignalAudit Bot::getAudit(const char* signalSignature)
{
    SignalAudit signalAudit;
    signalAudit.physicalLinks = receivers(signalSignature);

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

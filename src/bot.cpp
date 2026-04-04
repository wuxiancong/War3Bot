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

void Bot::resetGameState()
{
    gameInfo = GameInfo();
    pendingTask = PendingTask();
    commandSource = From_Server;
    state = BotState::Idle;
    hostJoined = false;
    pendingRemoval = false;
    pendingDisconnectFlag = false;
    activeOperations = 0;
    pendingRemovalReason = "Unspecified";
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
    if (client) {
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

    connect(client, &Client::authenticated,      this, &Bot::authenticated);
    connect(client, &Client::gameCreateSuccess,  this, &Bot::gameCreateSuccess);
    connect(client, &Client::gameCreateFail,     this, &Bot::gameCreateFail);
    connect(client, &Client::disconnected,       this, &Bot::disconnected);
    connect(client, &Client::socketError,        this, &Bot::socketError);
    connect(client, &Client::enteredChat,        this, &Bot::enteredChat);
    connect(client, &Client::gameStarted,        this, &Bot::gameStarted);
    connect(client, &Client::gameCancelled,      this, &Bot::gameCancelled);
    connect(client, &Client::playerCountChanged, this, &Bot::playerCountChanged);
    connect(client, &Client::hostJoinedGame,     this, &Bot::hostJoinedGame);
    connect(client, &Client::roomHostChanged,    this, &Bot::roomHostChanged);
    connect(client, &Client::visualHostLeft,     this, &Bot::visualHostLeft);
    connect(client, &Client::roomPingsUpdated,   this, &Bot::roomPingsUpdated);
    connect(client, &Client::readyStateChanged,  this, &Bot::readyStateChanged);
    connect(client, &Client::rejoinRejected,     this, &Bot::rejoinRejected);
    connect(client, &Client::accountCreated,     this, &Bot::accountCreated);
}

void Bot::setupPendingTask(const QString &host, const QString &name, const QString &clientId, CommandSource source)
{
    this->pendingTask.hasTask = true;
    this->pendingTask.hostName = host;
    this->pendingTask.gameName = name;
    this->pendingTask.clientId = clientId;
    this->pendingTask.commandSource = source;
    this->pendingTask.requestTime = QDateTime::currentMSecsSinceEpoch();
}

void Bot::setupGameInfo(const QString &host, const QString &name, const QString &mode, CommandSource source, const QString &clientId)
{
    resetGameState();

    this->hostname = host;
    this->commandSource = source;

    this->gameInfo.hostName = host;
    this->gameInfo.gameName = name;
    this->gameInfo.gameMode = mode;
    this->gameInfo.clientId = clientId;
    this->gameInfo.createTime = QDateTime::currentMSecsSinceEpoch();

    if (client && client->getUdpSocket()) {
        this->gameInfo.port = client->getUdpSocket()->localPort();
    } else {
        this->gameInfo.port = 0;
    }
}

#ifndef BOT_H
#define BOT_H

#include <QString>
#include <QObject>
#include <QDateTime>
#include "client.h"

class BotManager;
class NetManager;

// === 1. 机器人状态枚举 ===
enum class BotState {
    Disconnected,
    Connecting,
    InLobby,
    Unregistered,
    Authenticated,
    Idle,
    Creating,
    Reserved,
    Waiting,
    Starting,
    InGame,
    Finishing
};

// === 2. 挂起任务结构体 ===
struct PendingTask {
    bool hasTask = false;
    qint64 requestTime = 0;
    QString hostName;
    QString clientId;
    QString gameName;
    CommandSource commandSource;
};

// === 3. 房间信息结构体 ===
struct GameInfo {
    QString gameName        = "";
    QString gameMode        = "";
    QString mapName         = "";
    QString mapPath         = "";
    QString hostName        = "";
    QString clientId        = "";

    quint16 port            = 0;
    int maxPlayers          = 10;
    int currentPlayerCount  = 0;

    qint64 createTime       = 0;
    qint64 gameStartTime    = 0;
    qint64 gameEndTime      = 0;
};

// === 4. 指令信息结构体 ===
struct CommandInfo {
    QString text;
    QString clientId;
    quint32 botId;
    qint64 timestamp;
};

// === 4. 信号审计结构体 ===
struct SignalAudit {
    int physicalLinks       = 0;
    int triggerCount        = 0;
};

class Bot : public QObject {
    Q_OBJECT
public:
    explicit Bot(BotManager *manager, quint32 id, QString username, QString password);
    ~Bot();

    quint32 id;
    QString username;
    QString password;
    QString hostname;
    BotState state;
    GameInfo gameInfo;
    PendingTask pendingTask;
    Client *client = nullptr;

    bool hostJoined = false;
    CommandSource commandSource = From_Server;

    int activeOperations = 0;
    bool pendingRemoval = false;
    bool pendingDisconnectFlag = false;
    QString pendingRemovalReason = "Unspecified";

    void resetGameState(bool disconnectFlag, const QString &context);
    void incrementSignalCount(const QString &sigName);
    bool isOwner(const QString &senderClientId) const;
    SignalAudit getAudit(const char* signalSignature);
    void enterCriticalOperation();
    void leaveCriticalOperation();
    void resetAuditCounts();

    void setupClient(NetManager* netManager, const QString& displayName);
    void setupPendingTask(const QString &host, const QString &name, const QString &clientId, CommandSource source);
    void setupGameInfo(const QString &host, const QString &name, const QString &mode, CommandSource source, const QString &clientId);

signals:
    void enteredChat();
    void gameStarted();
    void disconnected();
    void gameCancelled();
    void authenticated();
    void accountCreated();
    void visualHostLeft();
    void socketError(QString error);
    void playerCountChanged(int count);
    void hostJoinedGame(const QString &name);
    void gameCreateSuccess(bool isHotRefresh);
    void roomHostChanged(const quint8 heirPid);
    void gameCreateFail(GameCreationStatus status);
    void roomPingsUpdated(const QMap<quint8, quint32> &pings);
    void readyStateChanged(const QVariantMap &readyData);
    void rejoinRejected(const QString &clientId, quint32 remainingMs);

private:
    BotManager *m_manager;
    QMap<QString, int> m_triggerCounts;
};

#endif // BOT_H

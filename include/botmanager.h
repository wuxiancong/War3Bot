#ifndef BOTMANAGER_H
#define BOTMANAGER_H

#include <QQueue>
#include <QObject>
#include <QVector>
#include <QSettings>
#include <QPair>

#include "bot.h"
#include "client.h"
#include "netmanager.h"

class BotManager : public QObject
{
    Q_OBJECT
public:
    explicit BotManager(QObject *parent = nullptr);
    ~BotManager();

    void cleanup();
    void startAllBots();
    int loadMoreBots(int count);
    void processNextRegistration();
    void setServerPort(quint16 port);
    const QVector<Bot*> &getAllBots() const;
    void setNetManager(NetManager *netManager);
    Bot *findBotByHostName(const QString &hostName);
    Bot *findBotByClientId(const QString &clientId);
    void initializeBots(quint32 initialCount, const QString &configPath);
    void removeBotMappings(const QString &clientId, const QString &hostName);
    bool checkCooldown(const QString &clientId, const QString &command, qint64 now);
    bool createBotAccountFilesIfNotExist(bool allowAutoGenerate, int targetListNumber);
    void handleHostCommand(const QString &userName, const QString &clientId, const QString &text);
    void removeGame(Bot *bot, bool disconnectFlag = false, const QString &reason = "Unspecified");
    bool createGame(const QString &hostName, const QString &gameName, const QString &gameMode, CommandSource commandSource, const QString &clientUuid);

signals:
    void botStateChanged(int botId, QString username, BotState newState);
    
public slots:
    void onBotCommandReceived(const QString &userName, const QString &clientUuid, const QString &command, const QString &text);
    void onBotClientExpired(const QString &clientId);

private slots:
    void onBotRoomPingReceived(const QHostAddress &addr, quint16 port, const QString &identifier, quint64 clientTime, PingSearchMode mode = ByHostName);
    void onBotRejoinRejected(Bot *bot, const QString &clientId, quint32 remainingMs);
    void onBotRoomPingsUpdated(Bot *bot, const QMap<quint8, quint32> &pings);
    void onBotReadyStateChanged(Bot *bot, const QVariantMap &readyData);
    void onBotGameCreateFail(Bot *bot, GameCreationStatus status);
    void onBotHostJoinedGame(Bot *bot, const QString &hostName);
    void onBotRoomHostChanged(Bot *bot, const quint8 heirPid);
    void onBotGameCreateSuccess(Bot *bot, bool isHotRefresh);
    void onBotPlayerCountChanged(Bot *bot, int count);
    void onBotError(Bot *bot, QString error);
    void onBotVisualHostLeft(Bot *bot);
    void onBotAccountCreated(Bot *bot);
    void onBotAuthenticated(Bot *bot);
    void onBotGameCancelled(Bot *bot);
    void onBotDisconnected(Bot *bot);
    void onBotEnteredChat(Bot *bot);
    void onBotGameStarted(Bot *bot);
    void onBotPendingTaskTimeout();

private:
    void addBotInstance(const QString &username, const QString &password);
    bool isBotActive(Bot *bot, const QString &context);
    bool isBotValid(Bot *bot, const QString &context);
    QString generateRandomPassword(int length);
    QString toLeetSpeak(const QString &input);
    QString botStateToString(BotState state);
    void setupBotConnections(Bot *bot);
    QString generateUniqueUsername();
    QChar randomCase(QChar c);

private:
    QVector<Bot*>                           m_bots;
    QString                                 m_botDisplayName            = "CC.Dota.XXX";

    QStringList                             m_allAccountFilePaths;
    QStringList                             m_newAccountFilePaths;
    QQueue<QPair<QString, QString>>         m_registrationQueue;
    Client                                  *m_tempRegistrationClient   = nullptr;

    int                                     m_currentFileIndex          = 0;
    int                                     m_currentAccountIndex       = 0;
    int                                     m_initialLoginCount         = 0;
    int                                     m_totalRegistrationCount    = 0;
    quint32                                 m_globalBotIdCounter        = 1;
    bool                                    m_isMassRegistering         = false;

    QString                                 m_targetServer;
    QString                                 m_configPath;
    quint16                                 m_controlPort               = 0;
    quint16                                 m_targetPort                = 6112;
    NetManager                              *m_netManager               = nullptr;

    QMap<QString, Bot*>                     m_activeGames;
    QMap<QString, qint64>                   m_lastHostTime;
    QMap<QString, CommandInfo>              m_commandInfos;
    QHash<QString, Bot*>                    m_hostNameToBotMap;
    QHash<QString, Bot*>                    m_clientIdToBotMap;
    QMap<QString, QMap<QString, qint64>>    m_commandCooldowns;
};

#endif // BOTMANAGER_H

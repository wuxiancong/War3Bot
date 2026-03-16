#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSet>
#include <QtSql>
#include <QObject>
#include <QReadWriteLock>

class DbManager : public QObject
{
    Q_OBJECT
public:
    static DbManager &instance();

    /**
     * @brief 通用初始化函数
     * @param driver 驱动类型 ("QSQLMYSQL" 或 "QSQLITE")
     * @param dbName 数据库名 (MySQL为库名，SQLite为文件路径)
     * @param tables 键为表名，值为建表 SQL 的 Map
     */
    bool init(const QString &driver, const QString &dbName,
              const QMap<QString, QString> &tables,
              const QString &host = "139.155.155.166", int port = 3306,
              const QString &user = "pvpgn", const QString &pass = "Wxc@2409154");

    // 执行 SQL 脚本
    bool executeSchema(const QMap<QString, QString> &tables);

    bool isHardwareIdBanned(const QString &hwid);
    bool banHardwareId(const QString &hwid, const QString &username,
                       const QString &reason, uint days = 0);
    bool unbanHardwareId(const QString &hwid);

    QString getHwidFromHistory(const QString &username);
    void updateUserHwid(const QString &username, const QString &hwid);

    void syncBannedList();
    void checkConnection();

private:
    explicit DbManager(QObject *parent = nullptr);
    ~DbManager();

    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;

    QSqlDatabase    m_db;
    QSet<QString>   m_bannedCache;
    QReadWriteLock  m_cacheLock;
    QStringList     m_targetTables = {
        "users", "temp_users", "ladder_stats", "matches",
        "match_results", "player_hero_stats",
        "player_mode_stats", "friendships",
        "chat_logs", "banned_hwids"
    };
};

#endif // DBMANAGER_H

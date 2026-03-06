#include "databasemanager.h"
#include "logger.h"
#include <QDebug>
#include <QDateTime>

DatabaseManager &DatabaseManager::instance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent) {
    m_db = QSqlDatabase::addDatabase("QSQLMYSQL");
}

DatabaseManager::~DatabaseManager() {
    if (m_db.isOpen()) m_db.close();
}

bool DatabaseManager::init(const QString &driver, const QString &dbName,
                           const QMap<QString, QString> &tables,
                           const QString &host, int port,
                           const QString &user, const QString &pass)
{
    // 1. 动态设置驱动
    m_db = QSqlDatabase::addDatabase(driver);

    if (driver == "QSQLITE") {
        m_db.setDatabaseName(dbName);
    } else {
        // MySQL 逻辑
        m_db.setHostName(host);
        m_db.setPort(port);
        m_db.setUserName(user);
        m_db.setPassword(pass);

        // 先连服务器创建库 (MySQL 特有步骤)
        if (!m_db.open()) return false;
        QSqlQuery query;
        query.exec(QString("CREATE DATABASE IF NOT EXISTS `%1` DEFAULT CHARACTER SET utf8mb4").arg(dbName));
        m_db.setDatabaseName(dbName);
        m_db.close();
    }

    if (!m_db.open()) {
        LOG_CRITICAL(QString("❌ 数据库打开失败: %1").arg(m_db.lastError().text()));
        return false;
    }

    // 2. 初始化所有表结构
    return executeSchema(tables);
}

bool DatabaseManager::executeSchema(const QMap<QString, QString> &tables) {
    QSqlQuery query;
    for (auto it = tables.begin(); it != tables.end(); ++it) {
        if (!query.exec(it.value())) {
            LOG_CRITICAL(QString("❌ 表初始化失败 [%1]: %2").arg(it.key(), query.lastError().text()));
            return false;
        }
    }
    return true;
}

void DatabaseManager::syncBannedList() {
    QSqlQuery query("SELECT hwid FROM banned_hwids WHERE expires_at > NOW() OR expires_at IS NULL");

    QWriteLocker locker(&m_cacheLock);
    m_bannedCache.clear();
    while (query.next()) {
        m_bannedCache.insert(query.value(0).toString().toUpper());
    }
    LOG_INFO(QString("♻️ [DB] 缓存同步完成，黑名单数量: %1").arg(m_bannedCache.size()));
}

bool DatabaseManager::isHardwareIdBanned(const QString &hwid) {
    QReadLocker locker(&m_cacheLock);
    return m_bannedCache.contains(hwid.toUpper());
}

bool DatabaseManager::banHardwareId(const QString &hwid, const QString &username,
                                    const QString &reason, uint days)
{
    QSqlQuery query;
    query.prepare("INSERT INTO banned_hwids (hwid, username, reason, banned_at, expires_at) "
                  "VALUES (:hwid, :user, :reason, NOW(), "
                  ":expiry) ON DUPLICATE KEY UPDATE "
                  "username=:user, reason=:reason, expires_at=:expiry");

    query.bindValue(":hwid", hwid.toUpper());
    query.bindValue(":user", username);
    query.bindValue(":reason", reason);

    if (days > 0)
        query.bindValue(":expiry", QDateTime::currentDateTime().addDays(days));
    else
        query.bindValue(":expiry", QVariant());

    if (query.exec()) {
        QWriteLocker locker(&m_cacheLock);
        m_bannedCache.insert(hwid.toUpper());
        return true;
    }
    LOG_INFO(QString("❌ [DB] 封禁失败: %1").arg(query.lastError().text()));
    return false;
}

bool DatabaseManager::unbanHardwareId(const QString &hwid) {
    QSqlQuery query;
    query.prepare("DELETE FROM banned_hwids WHERE hwid = :hwid");
    query.bindValue(":hwid", hwid.toUpper());

    if (query.exec()) {
        QWriteLocker locker(&m_cacheLock);
        m_bannedCache.remove(hwid.toUpper());
        return true;
    }
    return false;
}

QString DatabaseManager::getHwidFromHistory(const QString &username)
{
    QSqlQuery query;
    query.prepare("SELECT last_hwid FROM users WHERE username = :user LIMIT 1");
    query.bindValue(":user", username);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "";
}

void DatabaseManager::updateUserHwid(const QString &username, const QString &hwid)
{
    QSqlQuery query;
    query.prepare("INSERT INTO users (username, last_hwid, last_login) "
                  "VALUES (:user, :hwid, NOW()) "
                  "ON DUPLICATE KEY UPDATE last_hwid = :hwid, last_login = NOW()");
    query.bindValue(":user", username);
    query.bindValue(":hwid", hwid);
    query.exec();
}

void DatabaseManager::checkConnection() {
    if (!m_db.isOpen() || !m_db.isValid()) {
        LOG_INFO(QString("📡 [DB] 连接断开，尝试重连..."));
        if (m_db.open()) {
            syncBannedList();
        }
    } else {
        QSqlQuery query("SELECT 1");
        (void)query;
    }
}

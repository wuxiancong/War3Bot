#include "databasemanager.h"
#include "logger.h"
#include <QDebug>
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

// --- 日志辅助工具 ---
static int g_indent = 0;
void LOG_TREE(const QString &msg, bool isError = false, bool isLast = false) {
    QString prefix = "";
    for (int i = 0; i < g_indent; ++i) {
        prefix += "│   ";
    }
    prefix += (isLast ? "└── " : "├── ");

    if (isError) LOG_CRITICAL(prefix + msg);
    else LOG_INFO(prefix + msg);
}

#define SCOPE_LEVEL TreeScope _scope
class TreeScope {
public:
    TreeScope() { g_indent++; }
    ~TreeScope() { g_indent--; }
};
// ------------------

DatabaseManager &DatabaseManager::instance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent) {

}

DatabaseManager::~DatabaseManager() {
    if (m_db.isOpen()) m_db.close();
}

bool DatabaseManager::init(const QString &driver, const QString &dbName,
                           const QMap<QString, QString> &tables,
                           const QString &host, int port,
                           const QString &user, const QString &pass)
{
    LOG_INFO("📂 [开始] 数据库系统初始化项目");
    SCOPE_LEVEL;

    // 1. 检查驱动支持
    LOG_TREE(QString("驱动检查: %1").arg(driver));
    if (!QSqlDatabase::isDriverAvailable(driver)) {
        LOG_TREE("❌ 驱动不可用！可用列表: " + QSqlDatabase::drivers().join(","), true, true);
        return false;
    }

    const QString connName = QSqlDatabase::defaultConnection;
    if (QSqlDatabase::contains(connName)) {
        LOG_TREE("清理旧的数据库连接句柄");
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connName);
    }

    m_db = QSqlDatabase::addDatabase(driver, connName);

    // 2. 环境差异化配置
    if (driver == "QSQLITE") {
        LOG_TREE("环境: Windows/SQLite");
        m_db.setDatabaseName(dbName);
        LOG_TREE("路径: " + dbName, false, true);
    } else {
        LOG_TREE(QString("环境: Linux/MySQL (%1:%2)").arg(host).arg(port));
        m_db.setHostName(host);
        m_db.setPort(port);
        m_db.setUserName(user);
        m_db.setPassword(pass);

        LOG_TREE("正在建立物理连接...");
        if (!m_db.open()) {
            LOG_TREE("❌ 物理连接失败: " + m_db.lastError().text(), true, true);
            return false;
        }

        LOG_TREE("尝试创建/确认数据库实例: " + dbName);
        QSqlQuery query(m_db);
        if(!query.exec(QString("CREATE DATABASE IF NOT EXISTS `%1` DEFAULT CHARACTER SET utf8mb4").arg(dbName))) {
            LOG_TREE("⚠️ 创建数据库命令异常: " + query.lastError().text());
        }

        m_db.close();
        m_db.setDatabaseName(dbName);
        LOG_TREE("正在重载到目标业务库...");
    }

    // 3. 打开业务连接
    if (!m_db.open()) {
        LOG_TREE("❌ 最终连接打开失败: " + m_db.lastError().text(), true, true);
        return false;
    }
    LOG_TREE("✅ 数据库物理通道连接成功");

    // 4. 执行建表脚本
    bool schemaOk = executeSchema(tables);
    LOG_INFO(schemaOk ? "✨ [完成] 数据库初始化成功" : "💥 [终止] 数据库初始化失败");
    return schemaOk;
}

bool DatabaseManager::executeSchema(const QMap<QString, QString> &tables) {
    LOG_TREE("📑 正在同步表结构...");
    SCOPE_LEVEL;

    QSqlQuery query(m_db);

    QStringList order = {"users", "matches", "ladder_stats", "match_results", "chat_logs", "banned_hwids"};

    for (const QString &tableName : order) {
        if (!tables.contains(tableName)) continue;

        LOG_TREE(QString("正在处理表: %1").arg(tableName));
        if (!query.exec(tables[tableName])) {
            LOG_TREE(QString("❌ 创建失败! 详细错误: %1").arg(query.lastError().text()), true);
            LOG_TREE("失败 SQL: " + tables[tableName], true, true);
            return false;
        }
    }
    LOG_TREE("✅ 所有表结构已同步", false, true);
    return true;
}

void DatabaseManager::syncBannedList() {
    LOG_TREE("♻️ 正在同步黑名单缓存...");
    SCOPE_LEVEL;

    QSqlQuery query("SELECT hwid FROM banned_hwids WHERE expires_at > NOW() OR expires_at IS NULL", m_db);

    QWriteLocker locker(&m_cacheLock);
    m_bannedCache.clear();
    int count = 0;
    while (query.next()) {
        m_bannedCache.insert(query.value(0).toString().toUpper());
        count++;
    }
    LOG_TREE(QString("✅ 同步完成，缓存数量: %1").arg(count), false, true);
}

bool DatabaseManager::isHardwareIdBanned(const QString &hwid) {
    QReadLocker locker(&m_cacheLock);
    bool isBanned = m_bannedCache.contains(hwid.toUpper());
    if (isBanned) {
        LOG_TREE(QString("🛡️ 拦截命中: HWID [%1] 在黑名单中").arg(hwid));
    }
    return isBanned;
}

bool DatabaseManager::banHardwareId(const QString &hwid, const QString &username, const QString &reason, uint days) {
    LOG_TREE(QString("🚫 执行封禁: 用户[%1] HWID[%2]").arg(username, hwid));
    SCOPE_LEVEL;

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO banned_hwids (hwid, username, reason, banned_at, expires_at) "
                  "VALUES (:hwid, :user, :reason, NOW(), :expiry) "
                  "ON DUPLICATE KEY UPDATE username=:user, reason=:reason, expires_at=:expiry");

    query.bindValue(":hwid", hwid.toUpper());
    query.bindValue(":user", username);
    query.bindValue(":reason", reason);
    if (days > 0) query.bindValue(":expiry", QDateTime::currentDateTime().addDays(days));
    else query.bindValue(":expiry", QVariant());

    if (query.exec()) {
        QWriteLocker locker(&m_cacheLock);
        m_bannedCache.insert(hwid.toUpper());
        LOG_TREE("✅ 封禁数据已持久化并同步缓存", false, true);
        return true;
    }
    LOG_TREE("❌ 封禁写入失败: " + query.lastError().text(), true, true);
    return false;
}

bool DatabaseManager::unbanHardwareId(const QString &hwid) {
    LOG_TREE(QString("🔓 执行解封: HWID[%1]").arg(hwid));
    SCOPE_LEVEL;

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM banned_hwids WHERE hwid = :hwid");
    query.bindValue(":hwid", hwid.toUpper());

    if (query.exec()) {
        QWriteLocker locker(&m_cacheLock);
        m_bannedCache.remove(hwid.toUpper());
        LOG_TREE("✅ 解封完成", false, true);
        return true;
    }
    LOG_TREE("❌ 解封失败: " + query.lastError().text(), true, true);
    return false;
}

QString DatabaseManager::getHwidFromHistory(const QString &username) {
    LOG_TREE(QString("🔍 查询历史记录: 用户[%1]").arg(username));
    QSqlQuery query(m_db);
    query.prepare("SELECT last_hwid FROM users WHERE username = :user LIMIT 1");
    query.bindValue(":user", username);

    if (query.exec() && query.next()) {
        QString res = query.value(0).toString();
        LOG_TREE("└── 结果: " + (res.isEmpty() ? "空" : res));
        return res;
    }
    return "";
}

void DatabaseManager::updateUserHwid(const QString &username, const QString &hwid) {
    LOG_TREE(QString("📝 更新用户指纹: %1 -> %2").arg(username, hwid));
    QSqlQuery query(m_db);
    query.prepare("UPDATE users SET last_hwid = :hwid, lastlogin_time = UNIX_TIMESTAMP() WHERE username = :user");
    query.bindValue(":user", username);
    query.bindValue(":hwid", hwid);
    if (!query.exec()) {
        LOG_TREE("└── ❌ 更新失败: " + query.lastError().text(), true);
    }
}

void DatabaseManager::checkConnection() {
    if (!m_db.isOpen() || !m_db.isValid()) {
        LOG_TREE("📡 [DB] 连接断开，触发重连流程...");
        SCOPE_LEVEL;
        if (m_db.open()) {
            LOG_TREE("✅ 重连成功，重新加载缓存", false, true);
            syncBannedList();
        } else {
            LOG_TREE("❌ 重连失败: " + m_db.lastError().text(), true, true);
        }
    } else {
        // 轻量级心跳
        QSqlQuery query("SELECT 1", m_db);
        if (query.lastError().isValid()) {
            LOG_TREE("⚠️ 心跳探测异常: " + query.lastError().text());
        }
    }
}

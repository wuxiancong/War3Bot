#include "dbmanager.h"
#include "botmanager.h"
#include "netmanager.h"
#include "calculate.h"
#include "war3map.h"
#include "logger.h"
#include <QDir>
#include <QTimer>
#include <QPointer>
#include <QFileInfo>
#include <QDateTime>
#include <QDataStream>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QNetworkDatagram>
#include <QRandomGenerator>
#include <QCryptographicHash>

#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

NetManager::NetManager(QObject *parent)
    : QObject(parent)
    , m_isRunning(false)
    , m_enableBroadcast(false)
    , m_cleanupInterval(60000)
    , m_broadcastInterval(30000)
    , m_peerTimeout(300000)
    , m_listenPort(0)
    , m_broadcastPort(6112)
    , m_settings(nullptr)
    , m_cleanupTimer(nullptr)
    , m_broadcastTimer(nullptr)
    , m_udpSocket(nullptr)
    , m_tcpServer(nullptr)
    , m_nextSessionId(1000)
    , m_serverSeq(0)
{
    // 1. 优先尝试标准的缓存目录 (Linux: ~/.cache/..., Win: AppData/Local/...)
    QString writeRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    // 2. 如果 CacheLocation 不可用，尝试 AppLocalData
    if (writeRoot.isEmpty()) {
        writeRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }

    // 3. 定义 CRC 存放路径
    m_crcRootPath = writeRoot + "/war3files/crc";

    // 4. 尝试创建目录
    QDir dir;
    if (!dir.mkpath(m_crcRootPath)) {
        // --- 降级方案 1: 尝试程序运行目录 ---
        QString fallbackPath = QCoreApplication::applicationDirPath() + "/war3files/crc";
        if (dir.mkpath(fallbackPath)) {
            LOG_WARNING(QString("⚠️ [权限警告] 标准路径不可写，已回退至运行目录: %1").arg(fallbackPath));
            m_crcRootPath = fallbackPath;
        }
        else {
            // --- 降级方案 2: 尝试系统临时目录 (/tmp/...) ---
            fallbackPath = QDir::tempPath() + "/war3files/crc";
            if (dir.mkpath(fallbackPath)) {
                LOG_WARNING(QString("⚠️ [权限警告] 运行目录不可写，已回退至临时目录: %1").arg(fallbackPath));
                m_crcRootPath = fallbackPath;
            } else {
                LOG_CRITICAL("❌ [致命错误] 没有任何目录可写！CRC 功能将失效。");
                m_crcRootPath = QCoreApplication::applicationDirPath() + "/war3files/crc";
            }
        }
    } else {
        LOG_INFO(QString("✅ CRC 缓存目录已就绪: %1").arg(m_crcRootPath));
    }
}

NetManager::~NetManager()
{
    stopServer();
}

// ==================== 数据库管理与初始化 ====================

bool NetManager::setupDatabase()
{
    LOG_INFO("│   ├── 🛠️ [setupDatabase] 准备数据库配置...");

    // 1. 根据平台环境选择驱动和基础配置
#ifdef Q_OS_WIN
    QString driver = "QSQLITE";
    QString dbName = QCoreApplication::applicationDirPath() + "/pvpgn.db";
    QString autoInc = "AUTOINCREMENT";
#else
    // Ubuntu/Linux 生产环境：驱动名称 -> QMYSQL
    QString driver = "QMYSQL";
    QString dbName = "pvpgn";
    QString autoInc = "AUTO__INCREMENT";
#endif

    // 2. 定义本项目需要的所有表结构
    QMap<QString, QString> myTables;

    // a. 用户基础表
    myTables["users"] =
        "CREATE TABLE IF NOT EXISTS users ("
        // --- [第一部分：PvPGN 原生字段 (28个)] ---
        "uid int NOT NULL AUTO_INCREMENT COMMENT '战网唯一ID',"
        "acct_username varchar(32) DEFAULT NULL,"
        "username varchar(32) NOT NULL PRIMARY KEY COMMENT '平台登录名',"
        "acct_userid int DEFAULT NULL,"
        "acct_passhash1 varchar(40) DEFAULT NULL,"
        "acct_email varchar(128) DEFAULT NULL,"
        "auth_admin varchar(6) DEFAULT 'false',"
        "auth_normallogin varchar(6) DEFAULT 'true',"
        "auth_changepass varchar(6) DEFAULT 'true',"
        "auth_changeprofile varchar(6) DEFAULT 'true',"
        "auth_botlogin varchar(6) DEFAULT 'false',"
        "auth_operator varchar(6) DEFAULT 'false',"
        "new_at_team_flag int DEFAULT 0,"
        "auth_lock varchar(6) DEFAULT 'false',"
        "auth_locktime int DEFAULT 0,"
        "auth_lockreason varchar(128) DEFAULT NULL,"
        "auth_mute varchar(6) DEFAULT 'false',"
        "auth_mutetime int DEFAULT 0,"
        "auth_mutereason varchar(128) DEFAULT NULL,"
        "auth_command_groups varchar(16) DEFAULT '1',"
        "acct_lastlogin_time int DEFAULT 0,"
        "acct_lastlogin_owner varchar(128) DEFAULT NULL,"
        "acct_lastlogin_clienttag varchar(4) DEFAULT NULL,"
        "acct_lastlogin_ip varchar(45) DEFAULT NULL COMMENT '支持IPv6',"
        "acct_ctime varchar(128) DEFAULT NULL,"
        "acct_userlang varchar(128) DEFAULT NULL,"
        "acct_verifier varchar(128) DEFAULT NULL,"
        "acct_salt varchar(128) DEFAULT NULL,"

        // --- [第二部分：CC 平台扩展字段] ---
        "nickname VARCHAR(32) DEFAULT '' COMMENT '显示昵称',"
        "last_clientId VARCHAR(40) DEFAULT '' COMMENT '软件安装实例ClientId',"
        "last_hwid VARCHAR(64) DEFAULT '' COMMENT '物理硬件指纹',"

        // --- [双重验证与来源追踪] ---
        "web_password_hash VARCHAR(255) DEFAULT NULL COMMENT '网页端登录密码Hash',"
        "login_source VARCHAR(20) DEFAULT 'web' COMMENT '登录来源: web, launcher, game',"
        "is_web_verified TINYINT(1) DEFAULT 0 COMMENT '网页端验证状态',"

        // --- [认证与安全相关字段] ---
        "tellphone_number VARCHAR(20) DEFAULT NULL COMMENT '绑定手机号',"
        "is_email_verified TINYINT(1) DEFAULT 0 COMMENT '邮箱是否已激活',"
        "is_phone_verified TINYINT(1) DEFAULT 0 COMMENT '手机是否已绑定',"
        "total_ingame_time INT DEFAULT 0 COMMENT '游戏内累计时长(分钟)',"
        "cheat_records_count INT DEFAULT 0 COMMENT '作弊记录次数',"
        "report_records_count INT DEFAULT 0 COMMENT '被举报次数',"

        // --- [Token 验证相关字段] ---
        "email_verify_token VARCHAR(64) DEFAULT NULL COMMENT '邮箱验证Token',"
        "email_verify_expires DATETIME DEFAULT NULL COMMENT '邮箱验证链接过期时间',"
        "password_reset_token VARCHAR(64) DEFAULT NULL COMMENT '密码重置Token',"
        "password_reset_expires DATETIME DEFAULT NULL COMMENT '密码重置链接过期时间',"

        "register_ip VARCHAR(45) DEFAULT '' COMMENT '初始注册IP (支持IPv6)',"
        "register_location VARCHAR(64) DEFAULT '未知' COMMENT '注册地理位置',"
        "register_time DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '平台注册时间',"
        "deleted_time DATETIME DEFAULT NULL COMMENT '注销时间(软删除)',"

        "role TINYINT DEFAULT 0 COMMENT '0玩家, 1管理, 2超管',"
        "status TINYINT DEFAULT 0 COMMENT '0离线, 1大厅, 2房间, 3战斗',"
        "total_online_time INT DEFAULT 0 COMMENT '累计在线分钟',"
        "avatar_url VARCHAR(255) DEFAULT '' COMMENT '用户头像路径',"
        "banned_until DATETIME DEFAULT NULL COMMENT 'Web端封禁截止时间',"

        "coins_current_balance DECIMAL(10,2) DEFAULT 0.00 COMMENT 'CC币余额',"
        "coins_total_recharged DECIMAL(10,2) DEFAULT 0.00 COMMENT '累计充值总额',"

        "is_vip BOOLEAN DEFAULT FALSE COMMENT '是否是VIP',"
        "vip_expires_at DATETIME DEFAULT NULL COMMENT 'VIP到期时间',"

        // 增加唯一键索引
        "UNIQUE KEY idx_uid (uid),"
        "INDEX idx_hwid (last_hwid),"
        "INDEX idx_email (acct_email),"
        "INDEX idx_phone (tellphone_number)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    // b. 用户临时表
    myTables["temp_users"] =
        "CREATE TABLE IF NOT EXISTS temp_users ("
        "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY COMMENT '临时表自增ID',"
        "username VARCHAR(32) NOT NULL COMMENT '平台登录名',"
        "acct_email VARCHAR(128) NOT NULL COMMENT '注册邮箱',"
        "tellphone_number VARCHAR(20) DEFAULT NULL COMMENT '绑定手机号',"

        // --- [密码与安全相关] ---
        "web_password_hash VARCHAR(255) NOT NULL COMMENT '网页端登录密码Hash',"
        "encrypted_password VARCHAR(255) NOT NULL COMMENT '加密的明文密码用于PvPGN注册',"
        "acct_salt VARCHAR(128) DEFAULT NULL COMMENT '战网密码盐值',"
        "acct_verifier VARCHAR(128) DEFAULT NULL COMMENT '战网密码验证器',"

        // --- [邮箱验证相关] ---
        "email_verify_token VARCHAR(64) NOT NULL COMMENT '邮箱验证Token',"
        "email_verify_expires DATETIME NOT NULL COMMENT '邮箱验证链接过期时间',"

        // --- [注册信息] ---
        "register_ip VARCHAR(45) DEFAULT '' COMMENT '初始注册IP (支持IPv6)',"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '临时记录创建时间',"

        // --- [索引] ---
        "UNIQUE KEY idx_temp_username (username),"
        "UNIQUE KEY idx_temp_email (acct_email),"
        "INDEX idx_temp_token (email_verify_token)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    // c. 天梯数据表
    myTables["ladder_stats"] =
        "CREATE TABLE IF NOT EXISTS ladder_stats ("
        "username VARCHAR(32) PRIMARY KEY,"

        // --- 核心等级与分数 ---
        // 0:-, 1:D, 2:C, 3:B, 4:A, 5:S, 6:SS, 7:SSS
        "rank_level TINYINT DEFAULT 0 COMMENT '等级索引',"
        "score INT DEFAULT 1000 COMMENT '天梯积分',"

        // --- 基础胜负统计 ---
        "wins INT DEFAULT 0,"
        "losses INT DEFAULT 0,"
        "draws INT DEFAULT 0,"
        "escapes INT DEFAULT 0 COMMENT '逃跑/秒退',"

        // --- 表现数据 (用于计算 KDA, GPM) ---
        "total_kills INT DEFAULT 0,"
        "total_deaths INT DEFAULT 0,"
        "total_assists INT DEFAULT 0,"
        "total_gold INT DEFAULT 0,"
        "mvp_count INT DEFAULT 0,"

        // --- 连胜系统 ---
        "current_win_streak INT DEFAULT 0,"
        "max_win_streak INT DEFAULT 0,"

        // --- 时间统计 ---
        "total_play_time_mins INT DEFAULT 0 COMMENT '游戏总时长',"

        "FOREIGN KEY (username) REFERENCES users(username) ON DELETE CASCADE,"
        "INDEX idx_score (score)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    // d. 游戏比赛主表
    myTables["matches"] =
        "CREATE TABLE IF NOT EXISTS matches ("
        "match_id BIGINT AUTO_INCREMENT PRIMARY KEY, "
        "map_name VARCHAR(128), "
        "game_mode VARCHAR(32) COMMENT '如: -ap, -rd, -cm', "
        "winner_team TINYINT COMMENT '1: 近卫, 2: 天灾', "
        "start_time DATETIME, "
        "duration_seconds INT COMMENT '比赛时长', "
        "match_type TINYINT COMMENT '1: 天梯积分赛, 2: 普通练习赛' "
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    // e. 比赛结算详情
    myTables["match_results"] =
        "CREATE TABLE IF NOT EXISTS match_results ("
        "id BIGINT AUTO_INCREMENT PRIMARY KEY, "
        "match_id BIGINT, "
        "username VARCHAR(32), "
        "team_id TINYINT COMMENT '1:近卫, 2:天灾', "
        "is_winner BOOLEAN, "

        // --- 英雄信息 ---
        "hero_name VARCHAR(64) COMMENT '英雄全名: 如 影魔 - 奈文摩尔', "
        "hero_level TINYINT DEFAULT 1, "

        // --- 基础战斗 (KDA) ---
        "kills INT DEFAULT 0, "
        "deaths INT DEFAULT 0, "
        "assists INT DEFAULT 0, "

        // --- 经济与资源 ---
        "last_hits INT DEFAULT 0 COMMENT '正补', "
        "denies INT DEFAULT 0 COMMENT '反补', "
        "gold_earned INT DEFAULT 0 COMMENT '总获得金钱', "
        "xp_earned INT DEFAULT 0 COMMENT '总获得经验 (用于计算XPM)', "

        // --- MVP 计算核心字段 (新增) ---
        "hero_damage INT DEFAULT 0 COMMENT '对英雄造成的总伤害', "
        "tower_damage INT DEFAULT 0 COMMENT '对建筑造成的总伤害', "
        "damage_taken INT DEFAULT 0 COMMENT '承受的总伤害 (坦克指标)', "
        "hero_healing INT DEFAULT 0 COMMENT '治疗量 (辅助指标)', "
        "wards_placed INT DEFAULT 0 COMMENT '插眼数量 (辅助指标)', "
        "courier_kills INT DEFAULT 0 COMMENT '杀鸡次数', "
        "apm INT DEFAULT 0, "

        // --- 最终评价 ---
        "is_mvp BOOLEAN DEFAULT FALSE COMMENT '是否为本场MVP', "
        "score_point DECIMAL(5,2) DEFAULT 0.00 COMMENT '系统评分: 如 9.8', "

        "FOREIGN KEY (match_id) REFERENCES matches(match_id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    // f. 英雄汇总统计表
    myTables["player_hero_stats"] =
        "CREATE TABLE IF NOT EXISTS player_hero_stats ("
        "id BIGINT AUTO_INCREMENT PRIMARY KEY,"
        "username VARCHAR(32),"
        "hero_name VARCHAR(64),"
        "games_played INT DEFAULT 0,"
        "wins INT DEFAULT 0,"
        "total_kills INT DEFAULT 0,"
        "total_deaths INT DEFAULT 0,"
        "total_assists INT DEFAULT 0,"
        "max_kills INT DEFAULT 0 COMMENT '该英雄单场最高击杀',"
        "max_duration_secs INT DEFAULT 0 COMMENT '该英雄最长比赛时间',"
        "UNIQUE KEY idx_user_hero (username, hero_name),"
        "FOREIGN KEY (username) REFERENCES users(username) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    // g. 游戏模式汇总表
    myTables["player_mode_stats"] =
        "CREATE TABLE IF NOT EXISTS player_mode_stats ("
        "id BIGINT AUTO_INCREMENT PRIMARY KEY,"
        "username VARCHAR(32),"
        "game_mode VARCHAR(32),"
        "games_played INT DEFAULT 0,"
        "wins INT DEFAULT 0,"
        "UNIQUE KEY idx_user_mode (username, game_mode),"
        "FOREIGN KEY (username) REFERENCES users(username) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    // h. 好友关系表
    myTables["friendships"] =
        "CREATE TABLE IF NOT EXISTS friendships ("
        "user_id VARCHAR(32),"
        "friend_id VARCHAR(32),"
        "status TINYINT DEFAULT 0 COMMENT '0:申请中, 1:已是好友, 2:黑名单',"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY (user_id, friend_id),"
        "FOREIGN KEY (user_id) REFERENCES users(username) ON DELETE CASCADE,"
        "FOREIGN KEY (friend_id) REFERENCES users(username) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    // i. 邮件发送记录表
    myTables["email_logs"] =
        "CREATE TABLE IF NOT EXISTS email_logs ("
        "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY COMMENT '自增ID', "
        "email VARCHAR(128) NOT NULL COMMENT '邮箱地址', "
        "sent_at DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '发送时间', "
        "INDEX idx_email_sent (email, sent_at)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    // j. 聊天记录表
    myTables["chat_logs"] =
        "CREATE TABLE IF NOT EXISTS chat_logs ("
        "id BIGINT AUTO_INCREMENT PRIMARY KEY, "
        "sender VARCHAR(32), "
        "receiver VARCHAR(32) DEFAULT 'GLOBAL' COMMENT 'GLOBAL或私聊用户名', "
        "message TEXT, "
        "sent_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    // k. 黑名单表
    myTables["banned_hwids"] =
        "CREATE TABLE IF NOT EXISTS banned_hwids ("
        "hwid VARCHAR(64) PRIMARY KEY, "
        "username VARCHAR(32), "
        "reason TEXT, "
        "banned_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "expires_at DATETIME"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    LOG_INFO(QString("│   │   ├── 📝 已加载表结构定义: %1 张表").arg(myTables.size()));

    // 3. 读取连接参数
    QString host = m_settings ? m_settings->value("mysql/host", "127.0.0.1").toString() : "127.0.0.1";
    int port     = m_settings ? m_settings->value("mysql/port", 3306).toInt() : 3306;
    QString user = m_settings ? m_settings->value("mysql/user", "pvpgn").toString() : "pvpgn";
    QString pass = m_settings ? m_settings->value("mysql/pass", "Wxc@2409154").toString() : "Wxc@2409154";

    if (driver == "QSQLMYSQL") {
        LOG_INFO(QString("│   │   ├── 🌐 远程服务器: %1:%2").arg(host).arg(port));
        LOG_INFO(QString("│   │   └── 👤 数据库用户: %1").arg(user));
    }

    // 4. 调用通用的 DbManager 进行初始化
    LOG_INFO(QString("│   │   └── 🚀 移交权限至 DbManager 进行物理初始化..."));

    if (!DbManager::instance().init(driver, dbName, myTables, host, port, user, pass)) {
        LOG_CRITICAL("│   ├── ❌ [错误] 数据库初始化关键环节失败！");
        return false;
    }

    // 5. 后续同步操作
    LOG_INFO("│   ├── ♻️  正在执行数据预热与同步...");
    DbManager::instance().syncBannedList();

    LOG_INFO("│   └── ✅ 数据库环境部署就绪");
    return true;
}

// ==================== Socket 管理与启动 ====================

bool NetManager::startServer(quint64 port, const QString &configFile)
{
    if (m_isRunning) return true;

    // 1. 根节点：启动流程
    LOG_INFO("📂 [NetManager] 开始全系统启动流程...");

    // 2. 配置文件加载
    LOG_INFO(QString("├── 📄 正在读取配置文件: %1").arg(configFile));
    m_settings = new QSettings(configFile, QSettings::IniFormat, this);
    loadConfiguration();
    LOG_INFO("│   └── ✅ 配置信息已载入内存");

    // 3. 进入数据库核心初始化
    LOG_INFO("├── 🔧 启动数据库子系统...");
    if (!setupDatabase()) {
        LOG_CRITICAL("│   └── ❌ 数据库初始化异常中断，数据无法保存到服务器！");
    }

    // 4. UDP 网络配置
    LOG_INFO(QString("├── 📡 正在配置 UDP 链路 (目标端口: %1)...").arg(port));
    m_udpSocket = new QUdpSocket(this);
    if (!bindSocket(port)) {
        LOG_ERROR("│   └── ❌ UDP 端口绑定失败");
        cleanupResources();
        return false;
    }
    setupSocketOptions();
    LOG_INFO(QString("│   └── ✅ UDP 链路就绪: %1").arg(m_udpSocket->localPort()));

    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetManager::onUDPReadyRead);

    // 5. TCP 监听配置
    LOG_INFO(QString("├── 🤝 正在开启 TCP 服务监听 (端口: %1)...").arg(port));
    m_tcpServer = new QTcpServer(this);
    if (!m_tcpServer->listen(QHostAddress::AnyIPv4, port)) {
        LOG_ERROR(QString("│   └── ❌ TCP 监听失败: %1").arg(m_tcpServer->errorString()));
        cleanupResources();
        return false;
    }

    LOG_INFO(QString("├── 🤝 TCP 服务监听中..."));
    LOG_INFO(QString("│   ├── 绑定地址: %1").arg(m_tcpServer->serverAddress().toString()));
    LOG_INFO(QString("│   ├── 绑定端口: %1").arg(m_tcpServer->serverPort()));
    LOG_INFO(QString("│   └── 状态: 等待客户端连接..."));

    connect(m_tcpServer, &QTcpServer::newConnection, this, &NetManager::onNewTcpConnection);
    LOG_INFO("│   └── ✅ TCP 服务已在线");

    // 6. 最终完成
    m_listenPort = m_udpSocket->localPort();
    m_isRunning = true;
    setupTimers();

    LOG_INFO(QString("└── 🎉 服务端启动圆满完成 - UDP/TCP 端口: %1").arg(m_listenPort));

    emit serverStarted(port);
    return true;
}

void NetManager::loadConfiguration()
{
    m_peerTimeout = m_settings->value("server/peer_timeout", 300000).toInt();
    m_cleanupInterval = m_settings->value("server/cleanup_interval", 60000).toInt();
    m_enableBroadcast = m_settings->value("server/enable_broadcast", false).toBool();
}

bool NetManager::setupSocketOptions()
{
    int fd = m_udpSocket->socketDescriptor();
    if (fd == -1) return false;
    int reuse = 1;
#ifdef Q_OS_WIN
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
#else
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
    return true;
}

bool NetManager::bindSocket(quint64 port)
{
    if (!m_udpSocket->bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress)) {
        LOG_ERROR(QString("Bind Error: %1").arg(m_udpSocket->errorString()));
        return false;
    }
    return true;
}

void NetManager::setupTimers()
{
    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &NetManager::onCleanupTimeout);
    m_cleanupTimer->start(m_cleanupInterval);

    if (m_enableBroadcast) {
        m_broadcastTimer = new QTimer(this);
        connect(m_broadcastTimer, &QTimer::timeout, this, &NetManager::onBroadcastTimeout);
        m_broadcastTimer->start(30000);
    }
}

void NetManager::stopServer()
{
    if (!m_isRunning) return;
    m_isRunning = false;
    cleanupResources();
    m_registerInfos.clear();
    emit serverStopped();
}

// ==================== 二进制发送逻辑 ====================

qint64 NetManager::sendUdpPacket(const QHostAddress &target, quint64 port, PacketType type, const void *payload, quint64 payloadLen)
{
    if (!m_udpSocket) {
        LOG_ERROR("❌ [UDP] 发送失败: UDP Socket 未初始化");
        return -1;
    }

    // 1. 准备 Buffer
    int totalSize = sizeof(PacketHeader) + payloadLen;
    QByteArray buffer;
    buffer.resize(totalSize);

    // 2. 填充 Header
    PacketHeader *header = reinterpret_cast<PacketHeader*>(buffer.data());
    header->magic = PROTOCOL_MAGIC;
    header->version = PROTOCOL_VERSION;
    header->command = static_cast<quint8>(type);
    header->sessionId = 0;
    header->seq = ++m_serverSeq;
    header->payloadLen = payloadLen;
    header->checksum = 0;
    memset(header->signature, 0, 16);

    // 3. 填充 Payload
    if (payloadLen > 0 && payload != nullptr) {
        memcpy(buffer.data() + sizeof(PacketHeader), payload, payloadLen);
    }

    // 4. 计算 CRC
    header->checksum = calculateStandardCRC16(buffer);

    // 5. 后计算安全签名
    QByteArray secret = getAppSecret();
    QByteArray signSource = buffer + secret;
    QByteArray signature = QCryptographicHash::hash(signSource, QCryptographicHash::Sha256);
    memcpy(header->signature, signature.constData(), 16);

    // 6. 发送
    qint64 sent = m_udpSocket->writeDatagram(buffer, target, port);

    QString typeStr = packetTypeToString(type);

    if (sent < 0) {
        LOG_ERROR(QString("❌ [UDP] 发送失败 -> %1:%2 | Cmd: %3 | Error: %4")
                      .arg(target.toString()).arg(port).arg(typeStr, m_udpSocket->errorString()));
    } else {
        if (type == PacketType::C_S_HEARTBEAT || type == PacketType::S_C_PONG) {
            LOG_DEBUG(QString("📤 [UDP] %1 -> %2:%3 (Len: %4)").arg(typeStr, target.toString()).arg(port).arg(sent));
        } else {
            LOG_INFO(QString("📤 [UDP] %1 -> %2:%3 (Len: %4)").arg(typeStr, target.toString()).arg(port).arg(sent));
        }
    }
    return sent;
}

bool NetManager::sendTcpPacket(QTcpSocket *socket, PacketType type, const void *payload, quint64 payloadLen)
{
    // 0. 基础指针空校验
    if (!socket) {
        LOG_ERROR("❌ [TCP] 发送失败: Socket 指针为空");
        return false;
    }

    if (socket->thread() != QThread::currentThread()) {
        QByteArray data((const char*)payload, (int)payloadLen);
        QMetaObject::invokeMethod(this, [this, socket, type, data]() {
            this->sendTcpPacket(socket, type, data.constData(), data.size());
        }, Qt::QueuedConnection);
        return true;
    }

    // 1. 状态数值有效性校验
    int s = static_cast<int>(socket->state());
    if (s < 0 || s > 6) {
        LOG_ERROR(QString("❌ [TCP] 内存风险拦截: 检测到非法状态码 (%1)！指针可能已失效，放弃操作防止崩溃").arg(s));
        return false;
    }

    // 2. 业务连接状态校验
    if (socket->state() != QAbstractSocket::ConnectedState) {
        LOG_WARNING(QString("❌ [TCP] 发送失败: Socket 当前未连接 (当前状态: %1)").arg(s));
        return false;
    }

    // 3. 准备数据 Buffer
    int totalSize = sizeof(PacketHeader) + payloadLen;
    QByteArray buffer;
    buffer.resize(totalSize);

    // 4. 填充 Header
    PacketHeader *header = reinterpret_cast<PacketHeader*>(buffer.data());
    header->magic = PROTOCOL_MAGIC;
    header->version = PROTOCOL_VERSION;
    header->command = static_cast<quint8>(type);

    QVariant sidProp = socket->property("sessionId");
    QVariant cidProp = socket->property("clientId");
    quint32 sid = sidProp.isValid() ? sidProp.toUInt() : 0;
    QString clientId = cidProp.isValid() ? cidProp.toString() : "Unknown";

    header->sessionId = sid;
    header->seq = ++m_serverSeq;
    header->payloadLen = payloadLen;
    header->checksum = 0;
    memset(header->signature, 0, 16);

    // 5. 填充负载
    if (payloadLen > 0 && payload != nullptr) {
        memcpy(buffer.data() + sizeof(PacketHeader), payload, payloadLen);
    }

    // 6. 计算校验
    header->checksum = calculateStandardCRC16(buffer);
    QByteArray secret = getAppSecret();
    QByteArray signature = QCryptographicHash::hash(buffer + secret, QCryptographicHash::Sha256);
    memcpy(header->signature, signature.constData(), 16);

    // 7. 执行发送
    QString peerInfo = QString("%1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort());

    qint64 sent = socket->write(buffer);
    socket->flush();

    // 8. 结果判断
    if (sent == totalSize) {
        LOG_INFO(QString("🚀 [TCP] %1 -> %2 (SID: %3 | CID: %4 | Len: %5)")
                     .arg(packetTypeToString(type), peerInfo)
                     .arg(sid)
                     .arg(clientId)
                     .arg(sent));
        return true;
    } else {
        LOG_ERROR(QString("❌ [TCP] 写入不完整 -> %1 | Cmd: %2 | 计划: %3 / 实际: %4")
                      .arg(peerInfo, packetTypeToString(type))
                      .arg(totalSize).arg(sent));
        return false;
    }
}

// ==================== 二进制接收逻辑 ====================

void NetManager::onUDPReadyRead()
{
    while (m_udpSocket && m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        handleIncomingDatagram(datagram);
    }
}

void NetManager::handleIncomingDatagram(const QNetworkDatagram &datagram)
{
    QString senderInfo = QString("%1:%2").arg(datagram.senderAddress().toString()).arg(datagram.senderPort());
    QByteArray data = datagram.data();

    // --- 1. 进入函数 ---
    qDebug() << "┌── [收到数据包] 来自:" << senderInfo;
    qDebug() << "│   ├── 原始大小:" << data.size() << "字节";

    PacketHeader *header = reinterpret_cast<PacketHeader*>(data.data());
    PacketType packetType = static_cast<PacketType>(header->command);

    // 看门狗检查
    if (!m_watchdog.checkUdpPacket(datagram.senderAddress(), data.size(), packetType, header->sessionId)) {
        qDebug() << "│   └── ❌ [拒绝] 未通过看门狗流量检查";
        return;
    }

    // 长度预检
    if (data.size() < (int)sizeof(PacketHeader)) {
        qDebug() << "│   └── ❌ [错误] 数据长度小于包头最小长度";
        return;
    }

    // --- 2. 基础协议校验 ---
    qDebug() << "│   ├── [1. 协议头校验]";
    if (header->magic != PROTOCOL_MAGIC || header->version != PROTOCOL_VERSION) {
        qDebug() << "│   │   ├── 魔数:" << QString("0x%1").arg(header->magic, 0, 16).toUpper()
                 << "(期望:" << QString("0x%1").arg(PROTOCOL_MAGIC, 0, 16).toUpper() << ")";
        qDebug() << "│   │   ├── 版本:" << header->version << "(期望:" << PROTOCOL_VERSION << ")";
        qDebug() << "│   │   └── ❌ 结果: 魔数或版本不匹配，丢弃";
        return;
    }

    if (data.size() != static_cast<int>(sizeof(PacketHeader) + header->payloadLen)) {
        qDebug() << "│   │   ├── 声明负载长度:" << header->payloadLen;
        qDebug() << "│   │   ├── 实际总长度:" << data.size();
        qDebug() << "│   │   └── ❌ 结果: 数据包完整性校验失败 (长度不匹配)";
        return;
    }
    qDebug() << "│   │   └── ✅ 基础校验通过";

    if ((packetType == C_S_PING || packetType == C_S_HEARTBEAT) && header->sessionId == 0) {
        handleUdpPing(header, datagram.senderAddress(), datagram.senderPort());
        return;
    }

    // 暂存校验位用于比对
    quint16 receivedChecksum = header->checksum;
    char receivedSignature[16];
    memcpy(receivedSignature, header->signature, 16);

    // 重置校验位进行二次计算
    memset(header->signature, 0, 16);
    header->checksum = 0;

    // --- 3. 安全签名校验 ---
    qDebug() << "│   ├── [2. 签名校验]";
    QByteArray secret = getAppSecret();
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    hasher.addData(data);
    hasher.addData(secret);
    QByteArray expectedHash = hasher.result();

    if (memcmp(receivedSignature, expectedHash.constData(), 16) != 0) {
        qDebug() << "│   │   ├── 收到签名:" << QByteArray(receivedSignature, 16).toHex().toUpper();
        qDebug() << "│   │   ├── 期望签名:" << expectedHash.toHex().toUpper();
        qDebug() << "│   │   └── ❌ 结果: 签名验证失败 (密钥可能不一致)";
        return;
    }
    qDebug() << "│   │   └── ✅ 签名验证通过";

    // --- 4. CRC 校验 ---
    qDebug() << "│   ├── [3. 内容校验]";
    memcpy(header->signature, receivedSignature, 16);
    quint16 calculatedCrc = calculateStandardCRC16(data);

    if (calculatedCrc != receivedChecksum) {
        qDebug() << "│   │   ├── 收到 CRC: 0x" << QString::number(receivedChecksum, 16).toUpper();
        qDebug() << "│   │   ├── 计算 CRC: 0x" << QString::number(calculatedCrc, 16).toUpper();
        qDebug() << "│   │   └── ❌ 结果: CRC 校验失败 (数据可能在传输中损坏)";
        return;
    }
    qDebug() << "│   │   └── ✅ CRC 校验通过";

    // --- 5. 指令分发 ---
    char *payload = data.data() + sizeof(PacketHeader);

    qDebug() << "│   └── [4. 指令分发] 命令ID:" << packetType << "序列号:" << header->seq;

    QString currentClientId;
    {
        QReadLocker locker(&m_registerInfosLock);
        currentClientId = m_sessionIndex.value(header->sessionId);
    }

    switch (packetType) {
    case C_S_REGISTER:
        if (header->payloadLen >= sizeof(CSRegisterPacket)) {
            LOG_INFO("📥 [UDP 接收] C_S_REGISTER (注册请求)");
            handleRegister(header, reinterpret_cast<CSRegisterPacket*>(payload), datagram.senderAddress(), datagram.senderPort());
        }
        break;

    case C_S_ROOM_PING:
        LOG_INFO("📥 [UDP 接收] C_S_ROOM_PING (房间列表Ping)");
        handleRoomPing(header, payload, datagram.senderAddress(), datagram.senderPort());
        break;

    case C_S_CHECKMAPCRC:
        if (header->payloadLen >= sizeof(CSCheckMapCRCPacket)) {
            LOG_INFO("📥 [UDP 接收] C_S_CHECKMAPCRC (地图校验检查)");
            handleCheckMapCRC(header, reinterpret_cast<CSCheckMapCRCPacket*>(payload),
                              datagram.senderAddress(), datagram.senderPort());
        }
        break;

    default:
        LOG_INFO("❓ [UDP 未知指令]");
        LOG_INFO(QString("   └─ 🔢 Command: %1 (0x%2)")
                     .arg(packetType)
                     .arg(QString::number(packetType, 16).toUpper()));
        break;
    }
}

// ==================== 具体业务处理器 ====================

void NetManager::handleRegister(const PacketHeader *header, const CSRegisterPacket *packet, const QHostAddress &senderAddr, quint64 senderPort)
{
    // 1. 数据解析
    QString hardwareId = QString::fromUtf8(packet->hardwareId).trimmed();
    QString clientId   = QString::fromUtf8(packet->clientId).trimmed();
    QString username   = QString::fromUtf8(packet->username).trimmed();
    QString localIp    = QString::fromUtf8(packet->localIp).trimmed();
    QString reportedPublicIp = QString::fromUtf8(packet->publicIp).trimmed();
    QString actualPublicIp = cleanAddress(senderAddr);
    QString natStr = natTypeToString(static_cast<NATType>(packet->natType));

    // 2. 安全检查
    if (clientId.isEmpty() || hardwareId.isEmpty() || username.isEmpty()) {
        // a. 打印根节点和来源
        LOG_WARNING(QString("┌── [注册失败] 关键协议字段缺失"));
        LOG_WARNING(QString("├── 来源地址: %1:%2").arg(actualPublicIp).arg(senderPort));

        // b. 细化每个字段的具体状态
        QString uidStatus = clientId.isEmpty()   ? "EMPTY" : "OK";
        QString hwidStatus = hardwareId.isEmpty() ? "EMPTY" : "OK";
        QString userStatus = username.isEmpty()   ? "EMPTY" : "OK";

        LOG_WARNING(QString("├── 字段校验: UID[%1] | HWID[%2] | User[%3]")
                        .arg(uidStatus, hwidStatus, userStatus));

        // c. 打印截断的原始数据快照
        LOG_WARNING(QString("├── 原始快照: User(%1) | UID(%2)...")
                        .arg(username.isEmpty() ? "EMPTY" : username, clientId.isEmpty() ? "EMPTY" : clientId));

        LOG_WARNING(QString("└── 动作执行: 丢弃该非法数据包"));
        return;
    }

    if (DbManager::instance().isHardwareIdBanned(hardwareId) ||
        DbManager::instance().isUsernameBanned(username)) {

        LOG_WARNING(QString("🚫 [登录拒绝] 封禁名单匹配: 用户:%1 | 硬件:%2")
                        .arg(username, hardwareId));

        SCRegisterPacket resp;
        resp.sessionId = 0;
        resp.status = 0;
        sendUdpPacket(senderAddr, senderPort, S_C_REGISTER, &resp, sizeof(resp));
        return;
    }

    QWriteLocker locker(&m_registerInfosLock);
    for (const auto &peer : qAsConst(m_registerInfos)) {
        if (peer.hardwareId == hardwareId && peer.username != username) {
            LOG_WARNING(QString("┌─ [注册拦截] 拒绝多开"));
            LOG_WARNING(QString("└─ 机器 %1 已有账号 %2 在线，当前请求: %3").arg(hardwareId, peer.username, username));
            return;
        }
    }

    // 3. 业务处理
    quint32 newSessionId = 0;
    do { newSessionId = QRandomGenerator::global()->generate(); } while (newSessionId == 0 || m_sessionIndex.contains(newSessionId));

    if (m_registerInfos.contains(clientId)) {
        m_sessionIndex.remove(m_registerInfos[clientId].sessionId);
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    RegisterInfo info;
    info.clientId = clientId; info.hardwareId = hardwareId; info.username = username;
    info.localIp = localIp; info.localPort = packet->localPort;
    info.publicIp = actualPublicIp; info.publicPort = senderPort;
    info.sessionId = newSessionId; info.lastSeen = now; info.firstSeen = now;
    info.isRegistered = true; info.natType = packet->natType; info.lastSeq = header->seq;

    m_registerInfos[clientId] = info;
    m_watchdog.markSessionActive(senderAddr, newSessionId);

    m_sessionIndex[newSessionId] = clientId;
    DbManager::instance().updateUserHwid(username, hardwareId);

    // 4. 打印简洁树状日志
    LOG_INFO(QString("┌── [用户注册] 来自: %1:%2").arg(actualPublicIp).arg(senderPort));
    LOG_INFO(QString("├── 身份: %1 (SID: %2)").arg(username).arg(newSessionId));
    LOG_INFO(QString("├── 设备: ClientId: %1 | HWID: %2").arg(clientId, hardwareId));
    LOG_INFO(QString("├── 本地: %1:%2").arg(localIp).arg(packet->localPort));

    // 对比汇报与实际
    QString ipStatus = (reportedPublicIp == actualPublicIp) ? "一致" : "不一致/尚未获取";
    LOG_INFO(QString("├── 公网: %1:%2 (汇报: %3:%4 | %5)")
                 .arg(actualPublicIp).arg(senderPort).arg(reportedPublicIp.isEmpty() ? "None" : reportedPublicIp).arg(packet->publicPort).arg(ipStatus));

    LOG_INFO(QString("└── NAT : %1 | Seq: %2 | 状态: 注册成功").arg(natStr).arg(header->seq));

    // 5. 发送响应
    SCRegisterPacket resp;
    resp.sessionId = newSessionId;
    resp.status = 2; // Registered
    sendUdpPacket(senderAddr, senderPort, S_C_REGISTER, &resp, sizeof(resp));
}

void NetManager::handleUnregister(const PacketHeader *header)
{
    if (header->sessionId == 0) return;

    QWriteLocker locker(&m_registerInfosLock);
    if (!m_sessionIndex.contains(header->sessionId)) {
        LOG_WARNING(QString("┌─ [注销失败] 未找到会话"));
        LOG_WARNING(QString("└─ SessionID: %1").arg(header->sessionId));
        return;
    }

    QString clientId = m_sessionIndex.take(header->sessionId);
    if (m_registerInfos.contains(clientId)) {
        RegisterInfo info = m_registerInfos.take(clientId);

        // 计算在线时长
        qint64 durationSec = (QDateTime::currentMSecsSinceEpoch() - info.firstSeen) / 1000;
        QString durationStr = QString("%1h %2m %3s").arg(durationSec / 3600).arg((durationSec % 3600) / 60).arg(durationSec % 60);

        // 打印简洁日志
        LOG_INFO(QString("┌── [用户注销] 用户: %1").arg(info.username));
        LOG_INFO(QString("├── 会话: SID: %1 | Seq: %2").arg(header->sessionId).arg(header->seq));
        LOG_INFO(QString("├── 时长: %1").arg(durationStr));
        LOG_INFO(QString("└── 状态: 已离线 (内存资源已回收)"));
    } else {
        LOG_ERROR(QString("└── [系统异常] 索引与主表不同步 (ClientId: %1)").arg(clientId));
    }
}

quint8 NetManager::updateSessionState(quint32 sessionId, const QHostAddress &addr, quint64 port, bool *outIpChanged)
{
    if (sessionId == 0) return Unregistered;

    m_watchdog.markSessionActive(addr, sessionId);

    QWriteLocker locker(&m_registerInfosLock);

    if (!m_sessionIndex.contains(sessionId)) {
        return Unregistered;
    }

    QString clientId = m_sessionIndex.value(sessionId);
    if (!m_registerInfos.contains(clientId)) {
        return Unregistered;
    }

    RegisterInfo &info = m_registerInfos[clientId];

    // 1. 更新活跃时间
    info.lastSeen = QDateTime::currentMSecsSinceEpoch();

    // 2. 检测并更新 IP
    QString cleanIp = cleanAddress(addr);
    if (outIpChanged) *outIpChanged = false;

    if (info.publicIp != cleanIp || info.publicPort != port) {
        if (outIpChanged) *outIpChanged = true;
        info.publicIp = cleanIp;
        info.publicPort = port;
    }

    return Registered;
}

void NetManager::handleTcpPing(QTcpSocket *socket)
{
    while (socket->bytesAvailable() >= (qint64)sizeof(PacketHeader)) {
        PacketHeader header;
        socket->peek(reinterpret_cast<char*>(&header), sizeof(PacketHeader));

        if (header.magic == PROTOCOL_MAGIC && header.command == C_S_PING && header.sessionId == 0) {

            socket->read(sizeof(PacketHeader));

            SCPongPacket pong;
            pong.status = 2;

            sendTcpPacket(socket, PacketType::S_C_PONG, &pong, sizeof(pong));

            LOG_DEBUG(QString("⚡ [FastPath] 已响应来自 %1 的连通性探测")
                          .arg(socket->peerAddress().toString()));
        } else {
            break;
        }
    }
}

void NetManager::handleUdpPing(const PacketHeader *header, const QHostAddress &senderAddr, quint64 senderPort)
{
    bool ipChanged = false;

    int status = updateSessionState(header->sessionId, senderAddr, senderPort, &ipChanged);

    SCPongPacket pongPkt;
    pongPkt.status = status;
    sendUdpPacket(senderAddr, senderPort, S_C_PONG, &pongPkt, sizeof(pongPkt));

    if (status == 2) {
        LOG_DEBUG(QString("🏓 [PING] Session:%1 <-> %2:%3").arg(header->sessionId).arg(senderAddr.toString()).arg(senderPort));
    } else {
        LOG_DEBUG(QString("⚠️ [PING] 未注册探测 <-> %1:%2").arg(senderAddr.toString()).arg(senderPort));
    }
}

void NetManager::handleRoomPing(const PacketHeader *header, const char *payload, const QHostAddress &addr, quint16 port)
{
    // 1. 长度校验
    if (header->payloadLen < sizeof(CSRoomPingPacket)) {
        LOG_WARNING(QString("   └── ❌ 校验失败: 负载长度不足 (%1 < %2)")
                        .arg(header->payloadLen).arg(sizeof(CSRoomPingPacket)));
        return;
    }

    const CSRoomPingPacket *ping = reinterpret_cast<const CSRoomPingPacket*>(payload);

    // 2. 提取标识符
    QString hostName = QString::fromUtf8(ping->targetHostName, strnlen(ping->targetHostName, 32)).trimmed();
    QString clientId = QString::fromUtf8(ping->targetClientId, strnlen(ping->targetClientId, 64)).trimmed();

    QString finalIdentifier;
    PingSearchMode mode;

    // 3. 优先级决策
    if (!hostName.isEmpty()) {
        finalIdentifier = hostName;
        mode = ByHostName;
        LOG_INFO(QString("   ├── 🔍 识别模式: ByHostName [%1]").arg(hostName));
    } else if (!clientId.isEmpty()) {
        finalIdentifier = clientId;
        mode = ByClientId;
        LOG_INFO(QString("   ├── 🔍 识别模式: ByClientId [%1]").arg(clientId));
    } else {
        LOG_WARNING("   └── ❌ 识别失败: 数据包中 HostName 和 ClientId 均为空");
        return;
    }

    // 4. 发射信号
    LOG_INFO(QString("   └── 📢 发射信号: roomPingReceived -> 准备移交 BotManager"));

    emit roomPingReceived(addr, port, finalIdentifier, ping->clientSendTime, mode);
}

void NetManager::handleCommand(const PacketHeader *header, const CSCommandPacket *packet)
{
    QWriteLocker locker(&m_registerInfosLock);

    // 1. 首先根据 SessionID 查找对应的 ClientID (ClientId)
    if (!m_sessionIndex.contains(header->sessionId)) {
        LOG_WARNING(QString("⚠️ [指令拒绝] 未知的 SessionID: %1").arg(header->sessionId));
        return;
    }

    QString recordedClientId = m_sessionIndex.value(header->sessionId);

    // 2. 检查数据一致性
    if (!m_registerInfos.contains(recordedClientId)) {
        LOG_WARNING(QString("⚠️ [指令拒绝] 数据不一致: ClientId %1 的注册信息已丢失").arg(recordedClientId));
        return;
    }

    RegisterInfo &info = m_registerInfos[recordedClientId];

    // 3. 执行封禁检查
    if (DbManager::instance().isHardwareIdBanned(info.hardwareId) ||
        DbManager::instance().isUsernameBanned(info.username)) {
        LOG_WARNING(QString("🛡️ [安全拦截] 封禁用户试图执行指令: %1").arg(info.username));
        sendMessageToClient(recordedClientId, PacketType::S_C_ERROR, ERR_PERMISSION_DENIED);
        return;
    }

    // 4. 序列号/防重放检查
    if (header->seq <= info.lastSeq) {
        LOG_WARNING(QString("🛡️ [重放拦截] 收到重复/过期的包, Seq: %1").arg(header->seq));
        return;
    }
    info.lastSeq = header->seq;

    // 5. 提取数据包内容
    QString pktClientId = QString::fromUtf8(packet->clientId, strnlen(packet->clientId, sizeof(packet->clientId)));
    QString user = QString::fromUtf8(packet->username, strnlen(packet->username, sizeof(packet->username)));
    QString cmd  = QString::fromUtf8(packet->command, strnlen(packet->command, sizeof(packet->command)));
    QString text = QString::fromUtf8(packet->text, strnlen(packet->text, sizeof(packet->text)));

    // 6. 安全校验：防伪造检查
    if (pktClientId != recordedClientId) {
        LOG_INFO("🚫 [安全拦截] 客户端 ID 不匹配 (疑似伪造包)");
        LOG_INFO(QString("   ├─ 🔒 Session 绑定: %1").arg(recordedClientId));
        LOG_INFO(QString("   └─ 🔓 数据包声称:   %1").arg(pktClientId));
        return;
    }

    // 7. 打印成功日志
    QString fullCmd = cmd + (text.isEmpty() ? "" : " " + text);
    LOG_INFO("🤖 [收到用户指令]");
    LOG_INFO(QString("   ├─ 👤 用户: %1").arg(user));
    LOG_INFO(QString("   ├─ 💬 内容: %1").arg(fullCmd));
    LOG_INFO(QString("   └─ 🔑 验证: 通过 (Session: %1)").arg(header->sessionId));

    // 8. 向上层分发
    emit commandReceived(user, recordedClientId, cmd, text);
}

void NetManager::hardwareBan(const QString &targetUser, const QString &reason, uint days)
{
    QString hwid = getHwidByUsername(targetUser);

    if (hwid.isEmpty()) {
        LOG_ERROR("无法封禁：找不到该用户的硬件记录");
        return;
    }

    DbManager::instance().banHardwareId(hwid, targetUser, reason, days);

    kickUserIfOnline(targetUser);

    LOG_INFO(QString("🚫 管理员已封禁用户: %1 (机器码: %2)").arg(targetUser, hwid));
}

void NetManager::handleCheckMapCRC(const PacketHeader *header, const CSCheckMapCRCPacket *packet, const QHostAddress &senderAddr, quint64 senderPort)
{
    Q_UNUSED(header);
    QString crcHex = QString::fromUtf8(packet->crcHex, strnlen(packet->crcHex, sizeof(packet->crcHex))).trimmed();

    QString scriptDir = QCoreApplication::applicationDirPath() + "/war3files/crc/" + crcHex;
    QDir dir(scriptDir);
    bool exists = dir.exists() && QFile::exists(scriptDir + "/common.j"); // 简化检查

    // 构造响应
    SCCheckMapCRCPacket resp;
    memset(&resp, 0, sizeof(resp));
    strncpy(resp.crcHex, crcHex.toStdString().c_str(), sizeof(resp.crcHex) - 1);
    resp.exists = exists ? 1 : 0;

    // 如果不存在，加入待上传白名单
    if (!exists) {
        QWriteLocker locker(&m_tokenLock);
        m_pendingUploadTokens.insert(crcHex, header->sessionId);
        LOG_INFO(QString("🔍 请求CRC %1 不存在，等待上传 (Session: %2)")
                     .arg(crcHex).arg(header->sessionId));
    } else {
        LOG_INFO(QString("✅ 请求CRC %1 已存在").arg(crcHex));
    }

    sendUdpPacket(senderAddr, senderPort, S_C_CHECKMAPCRC, &resp, sizeof(resp));
}

// ==================== TCP ====================

TcpConnType NetManager::identifyTcpProtocol(QTcpSocket *socket)
{
    qint64 available = socket->bytesAvailable();

    // 1. 最小长度检查
    if (available < 4) {
        LOG_INFO(QString("   ├── ⏳ 等待识别: 缓冲区数据过短 (%1 字节)").arg(available));
        return Tcp_Unknown;
    }

    QString peerInfo = QString("%1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort());

    // 2. 预览头部数据
    QByteArray head = socket->peek(8);
    const PacketHeader *pHeader = reinterpret_cast<const PacketHeader*>(head.constData());

    LOG_INFO("🔌 [TCP 协议识别阶段]");
    LOG_INFO(QString("   ├── 👤 连接来源: %1").arg(peerInfo));

    // --- 识别分支 ---

    // 分支 A: 检查是否为地图上传通道
    if (head.startsWith("W3UP")) {
        socket->setProperty("ConnType", Tcp_Upload);
        LOG_INFO("   ├── 🏷️  特征匹配: ASCII [W3UP]");
        LOG_INFO("   └── ✅ 判定结果: 文件上传通道 (UPLOAD)");
        return Tcp_Upload;
    }

    // 分支 B: 检查是否为控制指令通道
    if (pHeader->magic == PROTOCOL_MAGIC) {
        socket->setProperty("ConnType", Tcp_Custom);
        LOG_INFO(QString("   ├── 🏷️  特征匹配: 指令魔数 0x%1").arg(QString::number(PROTOCOL_MAGIC, 16).toUpper()));
        LOG_INFO("   └── ✅ 判定结果: 控制指令通道 (COMMAND/UI)");
        return Tcp_Custom;
    }

    // 分支 C: 检查是否为魔兽争霸3 原始游戏连接
    if (static_cast<unsigned char>(head[0]) == 0xF7) {
        socket->setProperty("ConnType", Tcp_W3GS);
        LOG_INFO("   ├── 🏷️  特征匹配: W3GS 引导符 0xF7");
        LOG_INFO("   └── ✅ 判定结果: 魔兽游戏连接 (W3GS)");
        return Tcp_W3GS;
    }

    // 分支 D: 非法协议或垃圾数据
    if (available >= 8) {
        QString hexData = head.toHex(' ').toUpper();
        LOG_INFO("   ├── ❌ 协议识别失败: 头部数据不匹配任何已知模式");
        LOG_INFO(QString("   └──  📦 原始字节 (Peek): %1").arg(hexData));
        LOG_ERROR("   └── 🛡️ 安全动作: 判定为非法连接，正在强制断开...");

        socket->disconnectFromHost();
    }

    return Tcp_Unknown;
}

void NetManager::onTcpReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        LOG_ERROR("❌ [onTcpReadyRead] 异常：无法获取信号发送者 Socket");
        return;
    }

    // 逻辑提前：先探测是否为无需 Session 的 Ping 探测包 (SessionID == 0)
    if (socket->bytesAvailable() >= (qint64)sizeof(PacketHeader)) {
        PacketHeader header;
        socket->peek(reinterpret_cast<char*>(&header), sizeof(PacketHeader));
        // 识别特征：私有魔数 + Ping 指令 + Session 为 0
        if (header.magic == PROTOCOL_MAGIC && (header.command == C_S_PING ||
                                               header.command == C_S_HEARTBEAT) && header.sessionId == 0) {
            handleTcpPing(socket);
            // 如果处理完所有 Ping 包后，缓冲区已经没有足够数据，直接退出，不干扰业务日志
            if (socket->bytesAvailable() < (qint64)sizeof(PacketHeader)) return;
        }
    }

    // 获取当前连接类型
    QVariant typeProp = socket->property("ConnType");
    int connType = typeProp.isValid() ? typeProp.toInt() : Tcp_Unknown;

    LOG_DEBUG(QString("📥 [TCP 接收] 来源: %1:%2 | 当前通道类型: %3")
                  .arg(socket->peerAddress().toString())
                  .arg(socket->peerPort())
                  .arg(connType == Tcp_Unknown ? "未知 (需识别)" : (connType == Tcp_Upload ? "文件上传" : "指令控制")));

    // 1. 如果类型未知，先进行协议识别
    if (connType == Tcp_Unknown) {
        LOG_INFO("├── 🔍 检测到未知协议连接，启动识别程序...");
        connType = identifyTcpProtocol(socket);

        if (connType == Tcp_Unknown) {
            LOG_INFO("└── ⏳ 识别结果: 数据仍不足以判断，等待后续报文...");
            return;
        } else {
            LOG_INFO(QString("└── ✅ 识别成功: 已切换至 %1 模式")
                         .arg(connType == Tcp_Upload ? "UPLOAD" : "COMMAND"));
        }
    }

    // 2. 根据识别到的类型进行逻辑分发
    switch (static_cast<TcpConnType>(connType)) {
    case Tcp_Upload:
        LOG_DEBUG("└── 🚀 分发数据至 handleTcpUploadMessage");
        handleTcpUploadMessage(socket);
        break;

    case Tcp_Custom:
        LOG_DEBUG("└── 🚀 分发数据至 handleTcpCustomMessage");
        handleTcpCustomMessage(socket);
        break;

    default:
        LOG_WARNING(QString("└── ⚠️ 异常分支：未定义的 TcpConnType 枚举值: %1").arg(connType));
        break;
    }
}

void NetManager::handleTcpUploadMessage(QTcpSocket *socket)
{
    // 建立数据流
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_5_15);

    while (socket->bytesAvailable() > 0) {

        // ==================== 阶段 1: 解析头部 ====================
        if (!socket->property("HeaderParsed").toBool()) {
            const int MIN_HEADER_SIZE = 16;
            if (socket->bytesAvailable() < MIN_HEADER_SIZE) return;
            QByteArray headerPeep = socket->peek(MIN_HEADER_SIZE);
            QDataStream peepStream(headerPeep);
            peepStream.skipRawData(12); // 跳过 Magic(4) + Token(8)
            quint32 nameLenPreview;
            peepStream >> nameLenPreview;

            if (nameLenPreview > 256) {
                LOG_WARNING("❌ TCP 拒绝: 文件名过长");
                socket->disconnectFromHost();
                return;
            }

            if (socket->bytesAvailable() < MIN_HEADER_SIZE + nameLenPreview) {
                return;
            }

            // 1. 验证 Magic
            QByteArray magic = socket->read(4);
            if (magic != "W3UP") {
                sendUploadResult(socket, "", "Magic not match", false, UPLOAD_ERR_MAGIC);
                LOG_WARNING("❌ TCP 非法连接: 魔数错误");
                socket->disconnectFromHost();
                return;
            }

            // 2. 读取并验证 CRC Token
            QByteArray tokenBytes = socket->read(8);
            QString crcToken = QString::fromLatin1(tokenBytes).trimmed();
            quint32 linkedSessionId = 0;

            {
                QReadLocker locker(&m_tokenLock);
                if (m_pendingUploadTokens.contains(crcToken)) {
                    linkedSessionId = m_pendingUploadTokens.value(crcToken);
                } else {
                    sendUploadResult(socket, crcToken, "Unauthorized", false, UPLOAD_ERR_TOKEN);
                    LOG_WARNING(QString("❌ TCP 拒绝上传: 未授权的 Token (%1)").arg(crcToken));
                    socket->disconnectFromHost();
                    return;
                }
            }

            // 保存 SessionID
            socket->setProperty("CrcToken", crcToken);
            socket->setProperty("SessionId", linkedSessionId);

            // 3. 读取文件名长度
            quint32 nameLen;
            in >> nameLen;

            // 4. 读取文件名
            QByteArray nameBytes = socket->read(nameLen);
            QString rawFileName = QString::fromUtf8(nameBytes);
            QString fileName = QFileInfo(rawFileName).fileName();

            if (!isValidFileName(fileName)) {
                sendUploadResult(socket, crcToken, fileName, false, UPLOAD_ERR_FILENAME);
                LOG_WARNING(QString("❌ TCP 拒绝: 非法文件名 %1").arg(rawFileName));
                socket->disconnectFromHost();
                return;
            }

            // 5. 读取文件大小
            if (socket->bytesAvailable() < 8) return;
            qint64 fileSize;
            in >> fileSize;

            if (fileSize <= 0 || fileSize > 2 * 1024 * 1024) {
                sendUploadResult(socket, crcToken, fileName, false, UPLOAD_ERR_SIZE);
                LOG_WARNING("❌ TCP 拒绝: 文件过大");
                socket->disconnectFromHost();
                return;
            }

            // 6. 准备文件
            QString saveDir = m_crcRootPath + "/" + crcToken;

            QDir dir;
            if (!dir.mkpath(saveDir)) {
                LOG_ERROR("❌ 无法创建目录 (权限不足?): " + saveDir);
                return;
            }

            QString savePath = saveDir + "/" + fileName;
            QFile *file = new QFile(savePath);

            if (!file->open(QIODevice::WriteOnly)) {
                LOG_ERROR("❌ 无法创建文件: " + savePath + " 原因: " + file->errorString());
                sendUploadResult(socket, crcToken, fileName, false, UPLOAD_ERR_IO);
                delete file;
                socket->disconnectFromHost();
                return;
            }

            socket->setProperty("FilePtr", QVariant::fromValue((void*)file));
            socket->setProperty("BytesTotal", fileSize);
            socket->setProperty("BytesWritten", (qint64)0);
            socket->setProperty("HeaderParsed", true);
            socket->setProperty("FileName", fileName);

            LOG_INFO(QString("📥 [TCP] 开始接收文件: %1 (CRC: %2)").arg(fileName, crcToken));
        }

        // ==================== 阶段 2: 接收文件内容 ====================
        if (socket->property("HeaderParsed").toBool()) {
            QFile *file = static_cast<QFile*>(socket->property("FilePtr").value<void*>());
            qint64 total = socket->property("BytesTotal").toLongLong();
            qint64 current = socket->property("BytesWritten").toLongLong();
            qint64 remaining = total - current;
            qint64 bytesToRead = qMin(remaining, socket->bytesAvailable());

            QString currentCrcToken = socket->property("CrcToken").toString();
            QString currentFileName = socket->property("FileName").toString();

            if (bytesToRead > 0) {
                QByteArray chunk = socket->read(bytesToRead);
                file->write(chunk);
                current += chunk.size();
                socket->setProperty("BytesWritten", current);

                if (current > total) {
                    file->remove();
                    socket->disconnectFromHost();
                    return;
                }

                if (current == total) {
                    file->close();
                    file->deleteLater();

                    // =======================================================
                    // 4: 使用 SessionID 更新状态
                    // =======================================================
                    quint32 sid = socket->property("SessionId").toUInt();
                    bool updated = false;
                    QString clientName = "";

                    {
                        QWriteLocker locker(&m_registerInfosLock);
                        // 1. 先查索引
                        if (m_sessionIndex.contains(sid)) {
                            QString clientId = m_sessionIndex.value(sid);
                            // 2. 再查主表
                            if (m_registerInfos.contains(clientId)) {
                                m_registerInfos[clientId].crcToken = currentCrcToken;
                                clientName = m_registerInfos[clientId].username;
                                updated = true;
                                LOG_INFO(QString("🗺️ 已更新用户 %1 的地图CRC: %2 (via SessionID)")
                                             .arg(clientName, currentCrcToken));
                            }
                        }

                        if (!updated) {
                            QString senderIp = cleanAddress(socket->peerAddress().toString());
                            for (auto it = m_registerInfos.begin(); it != m_registerInfos.end(); ++it) {
                                if (it.value().publicIp == senderIp) {
                                    it.value().crcToken = currentCrcToken;
                                    updated = true;
                                    LOG_INFO(QString("🗺️ 已更新用户 %1 的地图CRC (via IP)").arg(it.value().username));
                                    break;
                                }
                            }
                        }
                    }

                    if (!updated) {
                        LOG_WARNING(QString("⚠️ 文件接收完成，但找不到对应用户 (Session: %1)").arg(sid));
                    }

                    sendUploadResult(socket, currentCrcToken, currentFileName, true, UPLOAD_OK);

                    updateMostFrequentCrc();
                    LOG_INFO("✅ [TCP] 接收完成");
                    socket->disconnectFromHost();
                    return;
                }
            } else {
                break;
            }
        }
    }
}

void NetManager::handleTcpCustomMessage(QTcpSocket *socket)
{
    // 安全过滤：只有标记为 Tcp_Custom 的连接才允许进入此解析器
    int connType = socket->property("ConnType").toInt();
    if (connType != Tcp_Custom) {
        LOG_ERROR(QString("🛡️ [安全拦截] handleTcpCustomMessage 拒绝处理非指令类型连接 (Type: %1)").arg(connType));
        return;
    }

    // 循环读取，处理粘包
    while (socket->bytesAvailable() > 0) {

        // 1. 检查头部是否完整
        if (socket->bytesAvailable() < (qint64)sizeof(PacketHeader)) return;

        // 2. 预读头部
        PacketHeader header;
        socket->peek(reinterpret_cast<char*>(&header), sizeof(PacketHeader));

        // 3. 校验魔数
        if (header.magic != PROTOCOL_MAGIC) {
            QString peerInfo = QString("%1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort());
            LOG_ERROR("🛑 [TCP 协议违规] 魔数不匹配！");
            LOG_ERROR(QString("   ├─ 🔌 来源: %1").arg(peerInfo));
            LOG_ERROR(QString("   ├─ ❌ 收到 Magic: 0x%1").arg(QString::number(header.magic, 16).toUpper()));
            LOG_ERROR(QString("   └─ 🎯 预期 Magic: 0x%1").arg(QString::number(PROTOCOL_MAGIC, 16).toUpper()));
            socket->disconnectFromHost();
            return;
        }

        // 4. 检查包体是否已完全到达
        qint64 totalPacketSize = sizeof(PacketHeader) + header.payloadLen;
        if (socket->bytesAvailable() < totalPacketSize) {
            return;
        }

        // 5. 正式读取完整数据包
        QByteArray packetData = socket->read(totalPacketSize);
        const PacketHeader *pHeader = reinterpret_cast<const PacketHeader*>(packetData.constData());
        const char *payload = packetData.constData() + sizeof(PacketHeader);

        LOG_INFO(QString("📩 [TCP 流量] 收到包 ID: 0x%1 | 负载长度: %2")
                     .arg(QString::number(pHeader->command, 16).toUpper())
                     .arg(pHeader->payloadLen));

        // A. 先尝试获取当前已绑定的 ID
        QString currentClientId = socket->property("clientId").toString();

        // B. 如果未绑定，且包里有 SessionID，尝试进行首次绑定
        if (currentClientId.isEmpty() && pHeader->sessionId != 0) {
            QWriteLocker locker(&m_registerInfosLock);
            if (m_sessionIndex.contains(pHeader->sessionId)) {
                QString clientId = m_sessionIndex.value(pHeader->sessionId);

                int connType = socket->property("ConnType").toInt();

                if (connType == Tcp_Custom) {
                    if (m_tcpClients.contains(clientId)) {
                        QPointer<QTcpSocket> oldSocket = m_tcpClients.value(clientId);
                        if (oldSocket && oldSocket != socket) {
                            LOG_INFO(QString("🔄 [TCP] 发现重复的控制台连接，替换旧通道: %1").arg(clientId));
                            oldSocket->disconnectFromHost();
                        }
                    }

                    m_tcpClients.insert(clientId, socket);
                    socket->setProperty("clientId", clientId);
                    socket->setProperty("sessionId", pHeader->sessionId);
                    currentClientId = clientId;

                    emit controlLinkEstablished(clientId);
                    LOG_INFO(QString("🔗 [TCP 控制通道绑定] ClientId: %1 | 状态: 绑定成功").arg(clientId));
                }
                else if (connType == Tcp_W3GS) {
                    socket->setProperty("clientId", clientId);
                    socket->setProperty("sessionId", pHeader->sessionId);
                    currentClientId = clientId;

                    LOG_INFO(QString("🎮 [TCP 游戏通道识别] 玩家: %1 (UID:%2) 已识别为 W3GS 连接")
                                 .arg(m_registerInfos[clientId].username, clientId));
                }
            } else {
                LOG_ERROR(QString("⚠️ [TCP 绑定失败] 无效的 Session: %1").arg(pHeader->sessionId));
            }
        }

        // 6. 处理具体指令
        switch (static_cast<PacketType>(pHeader->command)) {
        case PacketType::C_S_PREJOINROOM: {
            LOG_INFO("📨 [TCP 接收] C_S_PREJOINROOM (加入意向申报)");

            // 1. 协议长度预检
            if (pHeader->payloadLen < sizeof(CSPreJoinRoomPacket)) {
                LOG_ERROR(QString("   └── ❌ 校验失败: 负载长度不足 (%1 < %2)")
                              .arg(pHeader->payloadLen).arg(sizeof(CSPreJoinRoomPacket)));
                break;
            }

            const CSPreJoinRoomPacket *info = reinterpret_cast<const CSPreJoinRoomPacket*>(payload);

            // 2. 提取并清理字段
            QString userName = QString::fromUtf8(info->userName, strnlen(info->userName, 32)).trimmed();
            QString roomName = QString::fromUtf8(info->roomName, strnlen(info->roomName, 32)).trimmed();
            QString hostName = QString::fromUtf8(info->hostName, strnlen(info->hostName, 32)).trimmed();
            QString clientId = QString::fromUtf8(info->clientId, strnlen(info->clientId, 64)).trimmed();

            // 3. 准备回执包
            SCPreJoinRoomPacket resp;
            memset(&resp, 0, sizeof(resp));
            qstrncpy(resp.userName, info->userName, sizeof(resp.userName));
            qstrncpy(resp.hostName, info->hostName, sizeof(resp.hostName));

            auto recordPreJoinSuccess = [&]() {
                QWriteLocker locker(&m_preJoinLock);
                m_preJoins.insert(userName.toLower(), clientId);

                resp.status = 1;
                resp.errorCode = ERR_OK;

                LOG_INFO(QString("   ├── 👤 玩家名称: %1").arg(userName));
                LOG_INFO(QString("   ├── 🆔 客户端ID: %1").arg(clientId));
                LOG_INFO(QString("   └── ✅ 状态: 意向记录成功，允许物理连接"));
            };

            // 4. 业务逻辑拦截
            if (userName.isEmpty() || clientId.isEmpty()) {
                resp.status = 0;
                resp.errorCode = ERR_PARAM_ERROR;
                LOG_ERROR("   └── ❌ 申报拒绝: 关键参数缺失 (UserName 或 ClientId 为空)");
            }
            else if (DbManager::instance().isHardwareIdBanned(clientId) ||
                     DbManager::instance().isUsernameBanned(userName)) {

                resp.status = 0;
                resp.errorCode = ERR_BANNED_USER;

                LOG_WARNING(QString("   ├── 🛡️ [黑名单拦截] 封禁玩家试图进入房间"));
                LOG_INFO(QString("   ├── 👤 玩家名称: %1 %2").arg(userName, DbManager::instance().isUsernameBanned(userName) ? "[账号封印]" : ""));
                LOG_INFO(QString("   ├── 🆔 客户端ID: %1 %2").arg(clientId, DbManager::instance().isHardwareIdBanned(clientId) ? "[硬件封印]" : ""));
                LOG_INFO(QString("   └── 🚫 动作执行: 拒绝申报并下发错误码"));
            }
            else {
                if (roomName.isEmpty()) {
                    LOG_INFO(QString("   └── ℹ️ 动作执行: 房间名为空，检测到特殊环境加入，依然执行记录以确保后续状态同步能找到 ClientId"));
                    recordPreJoinSuccess();
                }
                else {
                    if (m_botManager && !m_botManager->isRoomExist(roomName)) {
                        resp.status = 0;
                        resp.errorCode = ERR_GAME_NOT_FOUND;
                        LOG_WARNING(QString("   └── 🛑 申报拒绝: 目标房间 [%1] 不存在").arg(roomName));
                    }
                    else {
                        recordPreJoinSuccess();
                    }
                }
            }

            // 5. 发送确认包
            bool ok = sendTcpPacket(socket, PacketType::S_C_PREJOINROOM, &resp, sizeof(resp));
            if (ok) {
                LOG_INFO(QString("   └── 🚀 动作执行: S_C_PREJOINROOM 已回发确认"));
            } else {
                LOG_ERROR("   └── ❌ 动作失败: 无法向客户端推送确认包");
            }
        }
        break;

        case PacketType::C_S_COMMAND:
            LOG_INFO("🚩 [命中] 正在进入 TCP C_S_COMMAND 分支...");
            if (pHeader->payloadLen >= sizeof(CSCommandPacket)) {
                const CSCommandPacket *cmdPkt = reinterpret_cast<const CSCommandPacket*>(payload);
                QString text = QString::fromUtf8(cmdPkt->text, strnlen(cmdPkt->text, sizeof(cmdPkt->text)));
                QString cmdStr = QString::fromUtf8(cmdPkt->command, strnlen(cmdPkt->command, sizeof(cmdPkt->command)));
                QString user = QString::fromUtf8(cmdPkt->username, strnlen(cmdPkt->username, sizeof(cmdPkt->username)));

                if (currentClientId.isEmpty()) {
                    LOG_INFO("🛑 [TCP 指令拒绝]");
                    LOG_ERROR("   ├─ ❌ 原因: 未鉴权连接 (无有效 SessionID)");
                    if (cmdStr == "/host") {
                        LOG_INFO("   └─ 🛡️ 动作: 发送 RETRY_HOST 指令");
                        sendRetryCommand(socket);
                    }
                    break;
                }

                LOG_INFO("🎮 [TCP 指令接收]");
                LOG_INFO(QString("   ├─ 👤 发送者: %1").arg(user));
                LOG_INFO(QString("   ├─ 🔗 来源ID: %1").arg(currentClientId));
                LOG_INFO(QString("   ├─ 💬 指令:   %1").arg(cmdStr));
                if (!text.isEmpty()) LOG_INFO(QString("   ├─ 📄 参数:   %1").arg(text));
                LOG_INFO("   └─ 🚀 动作:    分发至 BotManager");

                emit commandReceived(user, currentClientId, cmdStr, text);
            }
            break;

        default:
            LOG_INFO("❓ [TCP 未知指令]");
            LOG_INFO(QString("   └─ 🔢 Command: %1").arg(pHeader->command));
            break;
        }
    }
}

void NetManager::onNewTcpConnection()
{
    LOG_INFO("🌐 [TCP Server] 检测到新的挂起连接...");

    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        if (!socket) {
            LOG_ERROR("   └── ❌ 异常: nextPendingConnection 返回空指针");
            continue;
        }

        QString peerIp = socket->peerAddress().toString();
        quint16 peerPort = socket->peerPort();
        LOG_INFO(QString("   ├── 📥 握手请求: %1:%2").arg(peerIp).arg(peerPort));

        // 1. 看门狗拦截检查
        if (!m_watchdog.checkTcpConnection(socket->peerAddress())) {
            LOG_WARNING(QString("   ├── 🛡️ [安全拦截] 该 IP %1 触发频率限制，强制断开").arg(peerIp));
            socket->abort();
            socket->deleteLater();
            continue;
        }

        // 1. 看门狗拦截检查
        if (!m_watchdog.checkTcpConnection(socket->peerAddress())) {
            LOG_WARNING(QString("   ├── 🛡️ [安全拦截] 该 IP %1 触发频率限制，强制断开").arg(peerIp));
            socket->abort();
            socket->deleteLater();
            continue;
        }

        // 2. 绑定信号
        auto c1 = connect(socket, &QTcpSocket::readyRead, this, &NetManager::onTcpReadyRead);
        auto c2 = connect(socket, &QTcpSocket::disconnected, this, &NetManager::onTcpDisconnected);
        auto c3 = connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
                          this, [ peerIp](QAbstractSocket::SocketError err){
                              LOG_ERROR(QString("   └── ❗ [Socket错误] 源:%1 | Code:%2").arg(peerIp).arg(err));
                          });

        if (c1 && c2) {
            LOG_INFO(QString("   ├── ✅ 信号绑定成功: readyRead & disconnected"));
            LOG_INFO(QString("   └── 📝 状态: 连接已进入监听队列 (SocketID: %1)").arg(socket->socketDescriptor()));
        } else {
            LOG_CRITICAL("   └── ❌ [致命] 信号槽绑定失败！请检查函数签名。");
            socket->deleteLater();
        }
    }
}

void NetManager::onTcpDisconnected() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        QVariant fileVar = socket->property("FilePtr");
        if (fileVar.isValid()) {
            QFile *file = static_cast<QFile*>(fileVar.value<void*>());
            if (file) {
                if (file->isOpen()) file->close();
                delete file;
            }
        }
        QString clientId = socket->property("clientId").toString();
        if (!clientId.isEmpty()) {
            m_tcpClients.remove(clientId);
            LOG_INFO(QString("🧹 已从控制列表移除断开的客户端: %1").arg(clientId));
        }
        socket->deleteLater();
    }
}

bool NetManager::sendEnterRoomCommand(const QString &clientId, quint64 port, bool isServerCmd)
{
    if(isServerCmd){
        return true;
    }

    // 1. 检查 TCP 连接是否存在
    if (!m_tcpClients.contains(clientId)) {
        LOG_INFO("🛑 [指令发送失败]");
        LOG_INFO(QString("   ├─ 🎯 目标: %1").arg(clientId));
        LOG_ERROR("   └─ ❌ 原因: 用户没有记录");
        return false;
    }

    QPointer<QTcpSocket> socket = m_tcpClients.value(clientId);

    if (socket.isNull()) {
        LOG_ERROR(QString("🛑 [指令发送失败] 目标 %1 已离线 (QPointer 为空)").arg(clientId));
        m_tcpClients.remove(clientId); // 清理失效键
        return false;
    }

    // 2. 检查 Socket 状态
    if (socket->state() != QAbstractSocket::ConnectedState) {
        m_tcpClients.remove(clientId); // 清理死链接
        LOG_INFO("🛑 [指令发送失败]");
        LOG_INFO(QString("   ├─ 🎯 目标: %1").arg(clientId));
        LOG_ERROR("   └─ ❌ 原因: Socket 连接已断开 (清理僵尸连接)");
        return false;
    }

    // 3. 构造二进制 payload
    SCCommandPacket pkt;
    memset(&pkt, 0, sizeof(pkt));

    // 填充 ClientID (截断保护)
    strncpy(pkt.clientId, clientId.toUtf8().constData(), sizeof(pkt.clientId) - 1);

    // 填充指令
    const char *cmd = "ENTER_ROOM";
    strncpy(pkt.command, cmd, sizeof(pkt.command) - 1);

    // 填充参数 (端口)
    QString portStr = QString::number(port);
    strncpy(pkt.text, portStr.toUtf8().constData(), sizeof(pkt.text) - 1);

    // 4. 发送 PacketType::S_C_COMMAND
    bool ok = sendTcpPacket(socket, PacketType::S_C_COMMAND, &pkt, sizeof(pkt));

    // 5. 打印结果日志
    if (ok) {
        LOG_INFO("🚀 [自动进入指令分发]");
        LOG_INFO(QString("   ├─ 👤 目标用户: %1").arg(clientId));
        LOG_INFO(QString("   ├─ 🚪 房间端口: %1").arg(port));
        LOG_INFO("   └─ ✅ 发送状态: 成功 (TCP)");
    } else {
        LOG_INFO("🛑 [指令发送失败]");
        LOG_INFO(QString("   ├─ 🎯 目标: %1").arg(clientId));
        LOG_ERROR("   └─ ❌ 原因: TCP Socket 写入错误");
    }

    return ok;
}

bool NetManager::sendRetryCommand(QTcpSocket *socket)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return false;

    SCCommandPacket pkt;
    memset(&pkt, 0, sizeof(pkt));

    // 指令: RETRY_HOST
    const char *cmd = "RETRY_HOST";
    strncpy(pkt.command, cmd, sizeof(pkt.command) - 1);

    // 参数: 提示信息 (可选)
    const char *msg = "Need Auth";
    strncpy(pkt.text, msg, sizeof(pkt.text) - 1);

    // 发送 PacketType::S_C_COMMAND
    return sendTcpPacket(socket, PacketType::S_C_COMMAND, &pkt, sizeof(pkt));
}

bool NetManager::sendMessageToClient(const QString &clientId, PacketType type, quint8 code, quint64 data, bool isUdp)
{
    // 1. 构造错误包
    SCMessagePacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = type;
    pkt.code = code;
    pkt.data = data;

    // 2. 逻辑分支 A: 优先尝试 TCP 发送 (默认行为)
    if (!isUdp) {
        if (m_tcpClients.contains(clientId)) {
            QTcpSocket *socket = m_tcpClients[clientId];
            if (socket && socket->state() == QAbstractSocket::ConnectedState) {

                bool ok = sendTcpPacket(socket, type, &pkt, sizeof(pkt));

                if (ok) {
                    LOG_INFO(QString("🚫 [TCP Error] -> %1 | Code: %2").arg(clientId).arg(code));
                    return true;
                } else {
                    LOG_WARNING(QString("⚠️ [TCP Error] 发送失败 -> %1 (Socket错误), 尝试回退到 UDP").arg(clientId));
                    // 发送失败，向下穿透到 UDP 逻辑
                }
            }
        } else {
            LOG_DEBUG(QString("ℹ️ [TCP Error] 目标无TCP连接 -> %1, 尝试回退到 UDP").arg(clientId));
        }
    }

    // 3. 逻辑分支 B: UDP 发送 (回退方案或强制指定)
    QReadLocker locker(&m_registerInfosLock);
    if (m_registerInfos.contains(clientId)) {
        const RegisterInfo &info = m_registerInfos[clientId];
        QHostAddress targetAddr(info.publicIp);
        quint64 targetPort = info.publicPort;

        locker.unlock();

        sendUdpPacket(targetAddr, targetPort, type, &pkt, sizeof(pkt));

        LOG_INFO(QString("🚫 [UDP Error] -> %1:%2 | Code: %3").arg(targetAddr.toString()).arg(targetPort).arg(code));
        return true;
    }

    LOG_ERROR(QString("❌ [Error发送失败] 找不到目标用户: %1").arg(clientId));
    return false;
}

bool NetManager::sendToClient(const QString &clientId, const QByteArray &data)
{
    if (!m_tcpClients.contains(clientId)) {
        LOG_WARNING(QString("❌ 发送 TCP 失败: 找不到在线的 clientId %1").arg(clientId));
        return false;
    }

    QTcpSocket *socket = m_tcpClients[clientId];
    if (socket->state() != QAbstractSocket::ConnectedState) {
        m_tcpClients.remove(clientId); // 清理死链接
        return false;
    }

    qint64 written = socket->write(data);
    socket->flush();

    LOG_INFO(QString("🚀 TCP 发送 %1 字节 -> %2").arg(written).arg(clientId));
    return true;
}

void NetManager::sendRoomPong(const QHostAddress &targetAddr, quint16 targetPort, quint64 clientTime, quint8 current, quint8 max,
                              const QString &hostName, const QString &clientId)
{
    SCRoomPongPacket resp;
    memset(&resp, 0, sizeof(resp));

    // 赋值核心字段
    resp.clientSendTime = clientTime;
    resp.currentPlayers = current;
    resp.maxPlayers     = max;

    // 填充房主名
    QByteArray nameBytes = hostName.toUtf8();
    memcpy(resp.targetHostName, nameBytes.constData(), qMin(nameBytes.size(), 31));

    // 填充 ClientId
    QByteArray idBytes = clientId.toUtf8();
    memcpy(resp.targetClientId, idBytes.constData(), qMin(idBytes.size(), 63));

    LOG_INFO("📤 [UDP] 下发 RoomPong 响应包");
    LOG_INFO(QString("   ├── 📍 目标: %1:%2").arg(targetAddr.toString()).arg(targetPort));
    LOG_INFO(QString("   ├── 👤 房主: %1").arg(hostName.isEmpty() ? "N/A" : hostName));
    LOG_INFO(QString("   ├── 🆔 ClientId: %1").arg(clientId.isEmpty() ? "N/A" : clientId));
    LOG_INFO(QString("   ├── ⏱️ 原始时间戳: %1").arg(clientTime));
    LOG_INFO(QString("   ├── 👥 当前人数:   %1").arg(current));
    LOG_INFO(QString("   └── 📏 最大人数:   %1").arg(max));

    sendUdpPacket(targetAddr, targetPort, PacketType::S_C_ROOM_PONG, &resp, sizeof(resp));
}

bool NetManager::sendRoomPings(const QString &clientId, const QVariantMap &pings)
{
    QPointer<QTcpSocket> socket = m_tcpClients.value(clientId);

    if (socket.isNull()) {
        LOG_WARNING(QString("   ⚠️ [Ping下发中止] 客户端 %1 已断开").arg(clientId));
        m_tcpClients.remove(clientId);
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromVariant(pings);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    if (payload.isEmpty()) payload = "{}";

    if (socket->state() == QAbstractSocket::ConnectedState) {
        return sendTcpPacket(socket.data(), PacketType::S_C_PING_LIST, payload.data(), payload.size());
    }

    return false;
}

void NetManager::sendRoomReadyStates(const QString &clientId, const QVariantMap &readyStates)
{
    // 1. 获取对应的 Socket
    QPointer<QTcpSocket> socket = m_tcpClients.value(clientId);

    if (socket.isNull()) {
        LOG_ERROR(QString("   ⚠️ [准备状态下发中止] 客户端 %1 映射不存在或已离线").arg(clientId));
        return;
    }

    // 2.检查 Socket 类型标签
    int connType = socket->property("ConnType").toInt();
    if (connType != Tcp_Custom) {
        LOG_CRITICAL(QString("🛡️ [安全拦截] 企图向非控制通道 (%1) 发送 JSON 状态包！已拦截防止掉线。").arg(clientId));
        return;
    }

    // 3. 构建 JSON 数据流
    QJsonDocument doc = QJsonDocument::fromVariant(readyStates);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    if (payload.isEmpty()) payload = "{}";

    // 4. 发送 TCP 数据包
    if (sendTcpPacket(socket.data(), PacketType::S_C_READY_LIST, payload.data(), payload.size())) {
        LOG_INFO(QString("🚀 [TCP 下发] 准备状态表 -> Client:%1 (Len: %2)")
                     .arg(clientId).arg(payload.size()));
    }
}

void NetManager::sendUploadResult(QTcpSocket* socket, const QString& crc, const QString& fileName, bool success, UploadErrorCode reason)
{
    if (!socket || !m_udpSocket) return;

    QString senderIp = cleanAddress(socket->peerAddress().toString());
    QHostAddress targetAddr;
    quint64 targetPort = 0;
    bool found = false;
    quint32 fallbackSessionId = socket->property("SessionId").toUInt(); // 获取 SessionID

    QReadLocker locker(&m_registerInfosLock);

    // ---------------------------------------------------------
    // 策略 1: 优先尝试通过 IP 匹配
    // ---------------------------------------------------------
    for (const auto &info : qAsConst(m_registerInfos)) {
        // 如果 SessionID 存在，优先匹配 SessionID (最准确)
        if (fallbackSessionId != 0 && info.sessionId == fallbackSessionId) {
            targetAddr = QHostAddress(info.publicIp);
            targetPort = info.publicPort;
            found = true;
            // 如果 IP 变了，顺便打印个日志
            if (info.publicIp != senderIp) {
                LOG_INFO(QString("🔄 [TCP/UDP关联] IP不一致 (TCP:%1 vs UDP:%2)，使用 SessionID:%3 修正")
                             .arg(senderIp, info.publicIp).arg(fallbackSessionId));
            }
            break;
        }

        // 如果没有 SessionID，才尝试 IP 匹配
        if (fallbackSessionId == 0 && info.publicIp == senderIp) {
            targetAddr = QHostAddress(info.publicIp);
            targetPort = info.publicPort;
            found = true;
            break;
        }
    }

    // ---------------------------------------------------------
    // 策略 2: 如果遍历完还没找到
    // ---------------------------------------------------------
    if (!found && fallbackSessionId != 0) {
        // 尝试通过索引直接查找
        if (m_sessionIndex.contains(fallbackSessionId)) {
            QString clientId = m_sessionIndex.value(fallbackSessionId);
            if (m_registerInfos.contains(clientId)) {
                const RegisterInfo &info = m_registerInfos[clientId];
                targetAddr = QHostAddress(info.publicIp);
                targetPort = info.publicPort;
                found = true;
                LOG_INFO(QString("✅ 通过索引找回用户: %1 (Session: %2)").arg(clientId).arg(fallbackSessionId));
            }
        }
    }

    locker.unlock(); // 尽早解锁

    if (!found) {
        LOG_WARNING(QString("⚠️ 上传结束，无法找到 UDP 用户 (IP: %1, SessionID: %2)")
                        .arg(senderIp)
                        .arg(fallbackSessionId));
        return;
    }

    SCUploadResultPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    strncpy(pkt.crcHex, crc.toUtf8().constData(), sizeof(pkt.crcHex) - 1);
    strncpy(pkt.fileName, fileName.toUtf8().constData(), sizeof(pkt.fileName) - 1);
    pkt.status = success ? 1 : 0;
    pkt.reason = static_cast<quint8>(reason);

    sendUdpPacket(targetAddr, targetPort, S_C_UPLOADRESULT, &pkt, sizeof(pkt));

    LOG_INFO(QString("📤 发送上传回执 -> %1:%2 (Session: %5) | 文件: %3 | 结果: %4")
                 .arg(targetAddr.toString()).arg(targetPort)
                 .arg(fileName, success ? "成功" : "失败")
                 .arg(fallbackSessionId));
}

// ==================== 定时任务 ====================

void NetManager::onCleanupTimeout()
{
    cleanupExpiredClients();
    DbManager::instance().checkConnection();
}

void NetManager::onBroadcastTimeout()
{
    broadcastServerInfo();
}

void NetManager::broadcastServerInfo()
{
    if (!m_enableBroadcast || !m_udpSocket) return;
    QByteArray msg = QString("WAR3BOT_SERVER|%1").arg(m_listenPort).toUtf8();
    m_udpSocket->writeDatagram(msg, QHostAddress::Broadcast, m_broadcastPort);
}

void NetManager::updateMostFrequentCrc()
{
    m_crcCounts.clear();

    {
        QReadLocker locker(&m_registerInfosLock);
        for (const RegisterInfo &peer : qAsConst(m_registerInfos)) {
            if (!peer.crcToken.isEmpty()) {
                m_crcCounts[peer.crcToken]++;
            }
        }
    }

    QString maxCrcToken;
    int maxCount = 0;

    QMapIterator<QString, int> i(m_crcCounts);
    while (i.hasNext()) {
        i.next();
        if (i.value() > maxCount) {
            maxCount = i.value();
            maxCrcToken = i.key();
        }
    }

    if (!maxCrcToken.isEmpty()) {
        QString path = m_crcRootPath + "/" + maxCrcToken;
        QDir dir(path);

        // 确保该目录确实存在 .j 文件，否则设置了也没用
        if (dir.exists() && QFile::exists(path + "/common.j")) {
            War3Map::setPriorityCrcDirectory(path);
            LOG_INFO(QString("🔥 更新热门地图 CRC: %1 (在线人数: %2)").arg(maxCrcToken).arg(maxCount));
        } else {
            // 目录不完整，回退
            LOG_WARNING(QString("⚠️ 热门 CRC %1 数据缺失，回退默认").arg(maxCrcToken));
            War3Map::setPriorityCrcDirectory("");
        }
    } else {
        // 没有热门地图，回退
        War3Map::setPriorityCrcDirectory("");
    }
}

// ==================== 清理函数 ====================

void NetManager::cleanupResources()
{
    if (m_cleanupTimer) m_cleanupTimer->deleteLater();
    if (m_broadcastTimer) m_broadcastTimer->deleteLater();
    if (m_udpSocket) m_udpSocket->disconnect(this);
    if (m_tcpServer) m_tcpServer->disconnect(this);
    if (m_udpSocket) m_udpSocket->deleteLater();
    if (m_tcpServer) m_tcpServer->deleteLater();
    if (m_settings) m_settings->deleteLater();
    m_cleanupTimer = nullptr;
    m_broadcastTimer = nullptr;
    m_udpSocket = nullptr;
    m_tcpServer = nullptr;
    m_settings = nullptr;
}

void NetManager::cleanupExpiredClients()
{
    QWriteLocker locker(&m_registerInfosLock);
    quint64 now = QDateTime::currentMSecsSinceEpoch();

    // 定义临时结构体存储待删除信息，避免在 remove 时丢失用户名等信息
    struct ExpiredClient {
        QString clientId;
        QString username;
        quint64 silenceDuration;
    };
    QList<ExpiredClient> expiredList;

    // 1. 扫描阶段
    for (auto it = m_registerInfos.begin(); it != m_registerInfos.end(); ++it) {
        quint64 silence = now - it.value().lastSeen;
        if (silence > m_peerTimeout) {
            expiredList.append({it.key(), it.value().username, silence});
        }
    }

    // 2. 如果没有过期用户，直接返回，保持日志清爽
    if (expiredList.isEmpty()) {
        return;
    }

    // 3. 执行清理并打印树状日志
    LOG_INFO("🗑️ [会话超时清理任务]");
    LOG_INFO(QString("   ├─ ⏱️ 超时阈值: %1 ms").arg(m_peerTimeout));
    LOG_INFO(QString("   ├─ 📉 清理数量: %1 人").arg(expiredList.size()));

    for (int i = 0; i < expiredList.size(); ++i) {
        const auto &client = expiredList.at(i);
        bool isLast = (i == expiredList.size() - 1);
        QString prefix = isLast ? "   └─ " : "   ├─ ";

        // 打印详情：显示用户名、ClientId前8位、沉默秒数
        LOG_INFO(QString("%1🚫 移除: %2 (ClientId: %3...) | 已沉默: %4s")
                     .arg(prefix, client.username, client.clientId)
                     .arg(client.silenceDuration / 1000.0, 0, 'f', 1));

        // 执行移除
        removeClientInternal(client.clientId);
        emit clientExpired(client.clientId);
    }
}

void NetManager::removeClientInternal(const QString &clientId)
{
    if (!m_registerInfos.contains(clientId)) return;

    RegisterInfo info = m_registerInfos[clientId];
    m_watchdog.markSessionInactive(QHostAddress(info.publicIp), info.sessionId);

    // 1. 获取 SessionID
    quint32 sid = m_registerInfos[clientId].sessionId;

    // 2. 删索引
    m_sessionIndex.remove(sid);

    // 3. 删主表
    m_registerInfos.remove(clientId);
}

// ==================== 工具函数 ====================

QList<RegisterInfo> NetManager::getOnlinePlayers() const
{
    QReadLocker locker(&m_registerInfosLock);
    return m_registerInfos.values();
}

void NetManager::kickUserIfOnline(const QString &username)
{
    QWriteLocker locker(&m_registerInfosLock);
    for (auto it = m_registerInfos.begin(); it != m_registerInfos.end(); ++it) {
        if (it.value().username == username) {
            sendMessageToClient(it.key(), PacketType::S_C_ERROR, ERR_PERMISSION_DENIED, 0, true);
            QString clientId = it.key();
            m_sessionIndex.remove(it.value().sessionId);
            m_registerInfos.erase(it);
            LOG_INFO("👞 已将在线的被封禁用户踢下线: " + username);
            break;
        }
    }
}

QString NetManager::getHwidByUsername(const QString &username)
{
    {
        QReadLocker locker(&m_registerInfosLock);
        for (const auto &info : qAsConst(m_registerInfos)) {
            if (info.username == username) {
                LOG_INFO(QString("🔍 [在线匹配] 找到用户 %1 的 HWID: %2").arg(username, info.hardwareId));
                return info.hardwareId;
            }
        }
    }

    QString hwid = DbManager::instance().getHwidFromHistory(username);

    if (!hwid.isEmpty()) {
        LOG_INFO(QString("🔍 [数据库匹配] 找到离线用户 %1 的历史 HWID: %2").arg(username, hwid));
    } else {
        LOG_WARNING(QString("❌ [匹配失败] 找不到用户 %1 的任何记录").arg(username));
    }

    return hwid;
}

QString NetManager::getClientIdByPreJoinName(const QString &pName)
{
    if (pName.isEmpty()) return "";

    QString lowerName = pName.toLower();
    QWriteLocker locker(&m_preJoinLock);

    if (m_preJoins.contains(lowerName)) {
        QString clientId = m_preJoins.take(lowerName);
        LOG_INFO(QString("🎯 [ClientId 匹配成功] 玩家: %1 -> ClientId: %2")
                     .arg(pName, clientId));
        return clientId;
    }

    return "";
}

bool NetManager::isValidFileName(const QString &name)
{
    QString safe = QFileInfo(name).fileName();
    return (safe.compare("common.j", Qt::CaseInsensitive) == 0 ||
            safe.compare("blizzard.j", Qt::CaseInsensitive) == 0 ||
            safe.compare("war3map.j", Qt::CaseInsensitive) == 0);
}

bool NetManager::isClientRegistered(const QString &clientId) const
{
    if (clientId.isEmpty()) return false;
    QReadLocker locker(&m_registerInfosLock);
    return m_registerInfos.contains(clientId);
}

QString NetManager::cleanAddress(const QHostAddress &address) {
    QString ip = address.toString();
    return ip.startsWith("::ffff:") ? ip.mid(7) : ip;
}

QString NetManager::cleanAddress(const QString &address) {
    return address.startsWith("::ffff:") ? address.mid(7) : address;
}

QString NetManager::natTypeToString(NATType type)
{
    switch (type) {
    case NAT_UNKNOWN:
        return QStringLiteral("未知");
    case NAT_OPEN_INTERNET:
        return QStringLiteral("开放互联网");
    case NAT_FULL_CONE:
        return QStringLiteral("完全锥形NAT");
    case NAT_RESTRICTED_CONE:
        return QStringLiteral("限制锥形NAT");
    case NAT_PORT_RESTRICTED_CONE:
        return QStringLiteral("端口限制锥形NAT");
    case NAT_SYMMETRIC:
        return QStringLiteral("对称型NAT");
    case NAT_SYMMETRIC_UDP_FIREWALL:
        return QStringLiteral("对称型UDP防火墙");
    case NAT_BLOCKED:
        return QStringLiteral("被阻挡");
    case NAT_DOUBLE_NAT:
        return QStringLiteral("双重NAT");
    case NAT_CARRIER_GRADE:
        return QStringLiteral("运营商级NAT");
    case NAT_IP_RESTRICTED:
        return QStringLiteral("IP限制型NAT");
    default:
        return QStringLiteral("未知类型 (%1)").arg(type);
    }
}

QString NetManager::packetTypeToString(PacketType type)
{
    switch (type) {
    case PacketType::C_S_HEARTBEAT:         return "C_S_HEARTBEAT";
    case PacketType::C_S_REGISTER:          return "C_S_REGISTER";
    case PacketType::S_C_REGISTER:          return "S_C_REGISTER";
    case PacketType::C_S_UNREGISTER:        return "C_S_UNREGISTER";
    case PacketType::S_C_UNREGISTER:        return "S_C_UNREGISTER";
    case PacketType::C_S_GAMEDATA:          return "C_S_GAMEDATA";
    case PacketType::S_C_GAMEDATA:          return "S_C_GAMEDATA";
    case PacketType::C_S_COMMAND:           return "C_S_COMMAND";
    case PacketType::S_C_COMMAND:           return "S_C_COMMAND";
    case PacketType::C_S_CHECKMAPCRC:       return "C_S_CHECKMAPCRC";
    case PacketType::S_C_CHECKMAPCRC:       return "S_C_CHECKMAPCRC";
    case PacketType::C_S_PING:              return "C_S_PING";
    case PacketType::S_C_PONG:              return "S_C_PONG";
    case PacketType::S_C_ERROR:             return "S_C_ERROR";
    case PacketType::S_C_MESSAGE:           return "S_C_MESSAGE";
    case PacketType::S_C_UPLOADRESULT:      return "S_C_UPLOADRESULT";
    case PacketType::S_C_PING_LIST:         return "S_C_PING_LIST";
    case PacketType::S_C_READY_LIST:        return "S_C_READY_LIST";
    case PacketType::C_S_PREJOINROOM:       return "C_S_PREJOINROOM";
    case PacketType::S_C_PREJOINROOM:       return "S_C_PREJOINROOM";
    case PacketType::C_S_ROOM_PING:         return "C_S_ROOM_PING";
    case PacketType::S_C_ROOM_PONG:         return "S_C_ROOM_PONG";
    default: return QString("UNKNOWN(0x%1)").arg(static_cast<int>(type), 2, 16, QChar('0')).toUpper();
    }
}

QByteArray NetManager::getAppSecret() const { return "CC_War3_@#_Platform_2026_SecureKey"; }

bool NetManager::isRunning() const { return m_isRunning; }

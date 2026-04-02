#ifndef SECURITYWATCHDOG_H
#define SECURITYWATCHDOG_H

#include <QHostAddress>
#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QHash>
#include <QSet>

#include "protocol.h"

// ================= 配置常量 =================
#define BAN_BASE_TIME_MS 60000          // 基础封禁1分钟
#define MAX_IP_STATS_SIZE 100000        // 最大记录数
#define RECORD_TIMEOUT_MS 600000        // 10分钟无活动清理
#define CLEANUP_INTERVAL_MS 30000       // 30秒清理一次

// --- 频率限制 (基于单个 Session) ---
#define BASE_UDP_PER_SEC 150            // 握手/基础 UDP 频率
#define EXTRA_UDP_PER_SESSION 50        // 每个有效在线 Session 增加的配额
#define MAX_TCP_PER_MIN_BASE 60         // 基础 TCP 频率
#define EXTRA_TCP_PER_SESSION 20        // 每个有效在线 Session 增加的配额

struct IpStats {
    qint64 lastActivityTime = 0;

    // Session 追踪 (解决 NAT 同 IP 问题)
    QSet<quint32> activeSessions;

    // UDP 统计
    qint64 lastUdpResetTime = 0;
    int udpPacketCount = 0;

    // TCP 统计
    qint64 lastTcpResetTime = 0;
    int tcpConnCount = 0;

    // 封禁状态
    bool isBanned = false;
    qint64 banExpireTime = 0;
    int violationCount = 0;
};

class SecurityWatchdog : public QObject
{
    Q_OBJECT
public:
    explicit SecurityWatchdog(QObject *parent = nullptr);

    // --- 业务同步接口 ---
    // 当 NetManager 验证 Session 成功或心跳活跃时调用
    void markSessionActive(const QHostAddress &ip, quint32 sessionId);
    // 当 NetManager 清理过期 Session 时调用
    void markSessionInactive(const QHostAddress &ip, quint32 sessionId);

    // --- 核心检查函数 ---
    bool checkUdpPacket(const QHostAddress &sender, int packetSize, PacketType packetType, quint32 sessionId = 0);
    bool checkTcpConnection(const QHostAddress &sender);

    // --- 管理接口 ---
    void addWhitelist(const QString &ip);
    void addBlacklist(const QString &ip);
    void unban(const QString &ip);

private slots:
    void cleanupStaleRecords();

private:
    bool isIpBanned(quint32 ipInt, const QString &ipStr, IpStats &stats, qint64 now);
    void triggerBan(quint32 ipInt, const QString &ipStr, IpStats &stats, const QString &reason);
    quint32 parseIpToInt(const QHostAddress &addr) const;

    // 获取动态阈值
    int getDynamicUdpLimit(const IpStats &stats) const;
    int getDynamicTcpLimit(const IpStats &stats) const;

private:
    QMutex m_mutex;
    QTimer *m_cleanupTimer;

    QSet<quint32> m_whitelistInt;
    QSet<quint32> m_blacklistInt;
    QHash<quint32, IpStats> m_ipStats;
    QHash<QString, IpStats> m_ipStatsFallback; // IPv6
};

#endif // SECURITYWATCHDOG_H

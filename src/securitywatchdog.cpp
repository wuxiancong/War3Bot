#include "securitywatchdog.h"
#include "logger.h"
#include <QDebug>

SecurityWatchdog::SecurityWatchdog(QObject *parent) : QObject(parent)
{
    addWhitelist("127.0.0.1");
    addWhitelist("::1");

    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &SecurityWatchdog::cleanupStaleRecords);
    m_cleanupTimer->start(CLEANUP_INTERVAL_MS);
}

quint32 SecurityWatchdog::parseIpToInt(const QHostAddress &addr) const {
    bool ok;
    quint32 ip = addr.toIPv4Address(&ok);
    return ok ? ip : 0;
}

// ==================== 业务同步逻辑 ====================

void SecurityWatchdog::markSessionActive(const QHostAddress &ip, quint32 sessionId)
{
    if (sessionId == 0) return;
    QMutexLocker locker(&m_mutex);
    quint32 ipv4 = parseIpToInt(ip);

    if (ipv4 != 0) {
        m_ipStats[ipv4].activeSessions.insert(sessionId);
        m_ipStats[ipv4].lastActivityTime = QDateTime::currentMSecsSinceEpoch();
    } else {
        m_ipStatsFallback[ip.toString()].activeSessions.insert(sessionId);
        m_ipStatsFallback[ip.toString()].lastActivityTime = QDateTime::currentMSecsSinceEpoch();
    }
}

void SecurityWatchdog::markSessionInactive(const QHostAddress &ip, quint32 sessionId)
{
    QMutexLocker locker(&m_mutex);
    quint32 ipv4 = parseIpToInt(ip);
    if (ipv4 != 0) {
        if (m_ipStats.contains(ipv4)) m_ipStats[ipv4].activeSessions.remove(sessionId);
    } else {
        QString ipStr = ip.toString();
        if (m_ipStatsFallback.contains(ipStr)) m_ipStatsFallback[ipStr].activeSessions.remove(sessionId);
    }
}

// ==================== 核心检查逻辑 ====================

bool SecurityWatchdog::checkUdpPacket(const QHostAddress &sender, int packetSize, PacketType packetType, quint32 sessionId)
{
    quint32 ipv4 = parseIpToInt(sender);
    QMutexLocker locker(&m_mutex);
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 1. 获取 Stats 引用
    IpStats *stats;
    QString ipStrFallback;
    if (ipv4 != 0) {
        if (m_whitelistInt.contains(ipv4)) return true;
        if (m_blacklistInt.contains(ipv4)) return false;
        stats = &m_ipStats[ipv4];
    } else {
        ipStrFallback = sender.toString();
        if (m_ipStatsFallback.size() > MAX_IP_STATS_SIZE) return false;
        stats = &m_ipStatsFallback[ipStrFallback];
    }

    stats->lastActivityTime = now;

    // 2. 封禁检查
    if (isIpBanned(ipv4, ipStrFallback, *stats, now)) return false;

    // 3. 基础体积过滤
    if (packetSize > 1450) {
        triggerBan(ipv4, ipStrFallback, *stats, "超大 UDP 畸形包");
        return false;
    }

    // 4. 频率重置逻辑
    if (now - stats->lastUdpResetTime > 1000) {
        stats->udpPacketCount = 0;
        stats->lastUdpResetTime = now;
    }
    stats->udpPacketCount++;

    // 5. 利用 sessionId 进行身份判定
    // 检查这个包携带的 SessionId 是否是我们已经记录并验证过的
    bool isKnownSession = (sessionId != 0 && stats->activeSessions.contains(sessionId));

    // 6. 动态阈值计算
    int dynamicLimit = getDynamicUdpLimit(*stats);

    // 7. 差异化放行策略
    if (packetType == PacketType::C_S_HEARTBEAT ||
        packetType == PacketType::C_S_PING ||
        packetType == PacketType::C_S_ROOM_PING)
    {
        dynamicLimit *= 5; // 对高频小包放宽限制
    }

    // 8. 针对非法 Session 的惩罚
    // 如果这个 IP 下已经有玩家在线了，但当前包是一个未知的 SessionId，且不是注册包
    // 这极有可能是攻击者在同一个 NAT 网关下伪造非法包。
    if (!isKnownSession && packetType != PacketType::C_S_REGISTER && !stats->activeSessions.isEmpty()) {
        // 对于已有在线玩家的 IP，非法的 Session 只允许占用 20% 的总带宽配额
        if (stats->udpPacketCount > (dynamicLimit / 5)) {
            // 这种情况下只静默丢弃包，不轻易触发封禁（防止由于玩家重连 Session 变更导致的误杀）
            return false;
        }
    }

    // 9. 总量封禁检查
    if (stats->udpPacketCount > dynamicLimit) {
        triggerBan(ipv4, ipStrFallback, *stats,
                   QString("UDP 洪水 (Limit:%1 | Known:%2)")
                       .arg(dynamicLimit).arg(isKnownSession ? "Yes" : "No"));
        return false;
    }

    return true;
}

bool SecurityWatchdog::checkTcpConnection(const QHostAddress &sender)
{
    quint32 ipv4 = parseIpToInt(sender);
    QMutexLocker locker(&m_mutex);
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    IpStats *stats;
    QString ipStrFallback;

    if (ipv4 != 0) {
        if (m_whitelistInt.contains(ipv4)) return true;
        stats = &m_ipStats[ipv4];
    } else {
        ipStrFallback = sender.toString();
        stats = &m_ipStatsFallback[ipStrFallback];
    }

    stats->lastActivityTime = now;
    if (isIpBanned(ipv4, ipStrFallback, *stats, now)) return false;

    // TCP 频率重置 (60秒)
    if (now - stats->lastTcpResetTime > 60000) {
        stats->tcpConnCount = 0;
        stats->lastTcpResetTime = now;
    }
    stats->tcpConnCount++;

    int dynamicLimit = getDynamicTcpLimit(*stats);
    if (stats->tcpConnCount > dynamicLimit) {
        triggerBan(ipv4, ipStrFallback, *stats, "TCP 连接频繁");
        return false;
    }

    return true;
}

// ==================== 辅助私有逻辑 ====================

int SecurityWatchdog::getDynamicUdpLimit(const IpStats &stats) const {
    // 基础 150 + 每个在线玩家给 50 的额外带宽
    return BASE_UDP_PER_SEC + (stats.activeSessions.size() * EXTRA_UDP_PER_SESSION);
}

int SecurityWatchdog::getDynamicTcpLimit(const IpStats &stats) const {
    return MAX_TCP_PER_MIN_BASE + (stats.activeSessions.size() * EXTRA_TCP_PER_SESSION);
}

bool SecurityWatchdog::isIpBanned(quint32 ipInt, const QString& ipStr, IpStats &stats, qint64 now)
{
    if (!stats.isBanned) return false;
    if (now < stats.banExpireTime) return true;

    stats.isBanned = false;
    QString displayIp = ipInt != 0 ? QHostAddress(ipInt).toString() : ipStr;
    LOG_INFO(QString("🔓 IP %1 自动解封 (封禁期满)").arg(displayIp));
    return false;
}

void SecurityWatchdog::triggerBan(quint32 ipInt, const QString& ipStr, IpStats &stats, const QString &reason)
{
    if (stats.isBanned) return;

    stats.isBanned = true;
    stats.violationCount++;

    // 阶梯封禁: 1min, 2min, 4min, 8min, 16min, max 32min
    int duration = BAN_BASE_TIME_MS * (1 << qMin(stats.violationCount - 1, 5));
    stats.banExpireTime = QDateTime::currentMSecsSinceEpoch() + duration;

    QString displayIp = ipInt != 0 ? QHostAddress(ipInt).toString() : ipStr;
    LOG_WARNING(QString("🛡️ [安全拦截] 封禁 IP: %1 | 原因: %2 | 在线Session数: %3")
                    .arg(displayIp, reason).arg(stats.activeSessions.size()));
}

void SecurityWatchdog::cleanupStaleRecords()
{
    QMutexLocker locker(&m_mutex);
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    auto cleanupMap = [&](auto &map) {
        auto it = map.begin();
        while (it != map.end()) {
            // 只有同时满足以下条件才清理：
            // 1. 没有被封禁
            // 2. 超过 10 分钟没有数据往来
            // 3. 该 IP 下已经没有任何活跃的 Session
            if (!it.value().isBanned &&
                (now - it.value().lastActivityTime > RECORD_TIMEOUT_MS) &&
                it.value().activeSessions.isEmpty()) {
                it = map.erase(it);
            } else {
                ++it;
            }
        }
    };

    cleanupMap(m_ipStats);
    cleanupMap(m_ipStatsFallback);
}

void SecurityWatchdog::addWhitelist(const QString &ip) {
    QMutexLocker locker(&m_mutex);
    quint32 ipInt = parseIpToInt(QHostAddress(ip));
    if (ipInt != 0) m_whitelistInt.insert(ipInt);
}

void SecurityWatchdog::unban(const QString &ip) {
    QMutexLocker locker(&m_mutex);
    quint32 ipInt = parseIpToInt(QHostAddress(ip));
    if (ipInt != 0 && m_ipStats.contains(ipInt)) {
        m_ipStats[ipInt].isBanned = false;
        m_ipStats[ipInt].violationCount = 0;
    }
}

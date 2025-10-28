#include "stunclientmanager.h"
#include "stunclient.h"
#include "logger.h"
#include <QTimer>

// 定义静态成员变量
STUNClientManager* STUNClientManager::m_instance = nullptr;

STUNClientManager::STUNClientManager(QObject *parent)
    : QObject(parent)
    , m_stunClient(nullptr)
{
    m_stunClient = new STUNClient(this);
    connect(m_stunClient, &STUNClient::publicAddressDiscovered,
            this, &STUNClientManager::onPublicAddressDiscovered);
    connect(m_stunClient, &STUNClient::discoveryFailed,
            this, &STUNClientManager::onDiscoveryFailed);
}

STUNClientManager::~STUNClientManager()
{
    // 清理所有等待的会话
    for (const QString &sessionId : m_pendingSessions) {
        emit discoveryFailed(sessionId, "STUN client manager shutdown");
    }
    m_pendingSessions.clear();
}

STUNClientManager* STUNClientManager::instance()
{
    if (!m_instance) {
        m_instance = new STUNClientManager();
    }
    return m_instance;
}

void STUNClientManager::discoverPublicAddress(const QString &sessionId)
{
    // 检查是否已经在进行发现
    if (m_pendingSessions.contains(sessionId)) {
        LOG_DEBUG(QString("STUNClientManager: Session %1 already in discovery queue").arg(sessionId));
        return;
    }

    // 检查缓存 (缓存5分钟)
    auto it = m_cachedAddresses.find(sessionId);
    if (it != m_cachedAddresses.end()) {
        auto &cached = it.value();
        if (cached.second.secsTo(QDateTime::currentDateTime()) < 300) { // 5分钟缓存
            LOG_DEBUG(QString("STUNClientManager: Using cached address for session %1").arg(sessionId));
            emit publicAddressDiscovered(sessionId, cached.first.first, cached.first.second);
            return;
        } else {
            // 缓存过期，移除
            m_cachedAddresses.erase(it);
        }
    }

    // 添加到等待队列
    m_pendingSessions.insert(sessionId);
    LOG_DEBUG(QString("STUNClientManager: Added session %1 to discovery queue, queue size: %2")
                  .arg(sessionId).arg(m_pendingSessions.size()));

    // 如果没有当前进行的发现，立即开始
    if (m_currentSessionId.isEmpty()) {
        processNextDiscovery();
    }
}

void STUNClientManager::processNextDiscovery()
{
    if (m_pendingSessions.isEmpty()) {
        m_currentSessionId.clear();
        return;
    }

    // 获取下一个会话ID
    m_currentSessionId = *m_pendingSessions.begin();
    m_pendingSessions.remove(m_currentSessionId);

    LOG_DEBUG(QString("STUNClientManager: Starting discovery for session %1").arg(m_currentSessionId));
    m_stunClient->discoverPublicAddress();
}

bool STUNClientManager::isDiscoveryInProgress(const QString &sessionId) const
{
    return m_pendingSessions.contains(sessionId) || m_currentSessionId == sessionId;
}

void STUNClientManager::cancelDiscovery(const QString &sessionId)
{
    m_pendingSessions.remove(sessionId);

    if (m_currentSessionId == sessionId) {
        LOG_DEBUG(QString("STUNClientManager: Cancelling discovery for session %1").arg(sessionId));
        m_stunClient->cancelDiscovery();
        m_currentSessionId.clear();

        // 立即处理下一个发现请求
        QTimer::singleShot(0, this, &STUNClientManager::processNextDiscovery);
    }
}

void STUNClientManager::onPublicAddressDiscovered(const QHostAddress &address, quint16 port)
{
    if (m_currentSessionId.isEmpty()) return;

    // 缓存结果
    m_cachedAddresses.insert(m_currentSessionId,
                             qMakePair(qMakePair(address, port), QDateTime::currentDateTime()));

    LOG_INFO(QString("STUNClientManager: Discovery completed for session %1: %2:%3")
                 .arg(m_currentSessionId).arg(address.toString()).arg(port));

    // 通知当前会话
    emit publicAddressDiscovered(m_currentSessionId, address, port);

    // 处理下一个发现请求
    m_currentSessionId.clear();
    QTimer::singleShot(100, this, &STUNClientManager::processNextDiscovery); // 短暂延迟
}

void STUNClientManager::onDiscoveryFailed(const QString &error)
{
    if (m_currentSessionId.isEmpty()) return;

    LOG_ERROR(QString("STUNClientManager: Discovery failed for session %1: %2")
                  .arg(m_currentSessionId).arg(error));

    // 通知当前会话
    emit discoveryFailed(m_currentSessionId, error);

    // 处理下一个发现请求
    m_currentSessionId.clear();
    QTimer::singleShot(100, this, &STUNClientManager::processNextDiscovery); // 短暂延迟
}

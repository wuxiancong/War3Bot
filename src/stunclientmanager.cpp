#include "stunclientmanager.h"
#include "stunclient.h"
#include "logger.h"

STUNClientManager *STUNClientManager::m_instance = nullptr;

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
    if (m_stunClient) {
        m_stunClient->deleteLater();
    }
}

STUNClientManager *STUNClientManager::instance()
{
    if (!m_instance) {
        m_instance = new STUNClientManager();
    }
    return m_instance;
}

void STUNClientManager::discoverPublicAddress(const QString &sessionId)
{
    // 检查缓存
    if (m_cachedAddresses.contains(sessionId)) {
        auto address = m_cachedAddresses.value(sessionId);
        emit publicAddressDiscovered(sessionId, address.first, address.second);
        return;
    }

    // 如果已经有发现在进行，等待
    if (!m_pendingSessions.isEmpty()) {
        m_pendingSessions.insert(sessionId);
        return;
    }

    m_currentSessionId = sessionId;
    m_pendingSessions.insert(sessionId);
    m_stunClient->discoverPublicAddress();
}

bool STUNClientManager::isDiscoveryInProgress(const QString &sessionId) const
{
    return m_pendingSessions.contains(sessionId);
}

void STUNClientManager::cancelDiscovery(const QString &sessionId)
{
    m_pendingSessions.remove(sessionId);
    if (m_currentSessionId == sessionId) {
        m_stunClient->cancelDiscovery();
        m_currentSessionId.clear();
    }
}

void STUNClientManager::onPublicAddressDiscovered(const QHostAddress &address, quint16 port)
{
    // 缓存结果
    m_cachedAddresses.insert(m_currentSessionId, qMakePair(address, port));

    // 通知当前会话
    emit publicAddressDiscovered(m_currentSessionId, address, port);

    // 处理等待的会话
    m_pendingSessions.remove(m_currentSessionId);
    m_currentSessionId.clear();

    // 如果有等待的会话，继续处理下一个
    if (!m_pendingSessions.isEmpty()) {
        QString nextSessionId = *m_pendingSessions.begin();
        m_pendingSessions.remove(nextSessionId);
        discoverPublicAddress(nextSessionId);
    }
}

void STUNClientManager::onDiscoveryFailed(const QString &error)
{
    // 通知当前会话
    emit discoveryFailed(m_currentSessionId, error);

    // 处理等待的会话
    m_pendingSessions.remove(m_currentSessionId);
    m_currentSessionId.clear();

    // 如果有等待的会话，继续处理下一个
    if (!m_pendingSessions.isEmpty()) {
        QString nextSessionId = *m_pendingSessions.begin();
        m_pendingSessions.remove(nextSessionId);
        discoverPublicAddress(nextSessionId);
    }
}

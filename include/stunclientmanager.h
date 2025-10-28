#ifndef STUNCLIENTMANAGER_H
#define STUNCLIENTMANAGER_H

#include <QObject>
#include <QHostAddress>
#include <QHash>
#include <QSet>
#include <QDateTime>

class STUNClient;

class STUNClientManager : public QObject
{
    Q_OBJECT

public:
    static STUNClientManager *instance();
    void discoverPublicAddress(const QString &sessionId);
    bool isDiscoveryInProgress(const QString &sessionId) const;
    void cancelDiscovery(const QString &sessionId);
    void processNextDiscovery();

signals:
    void publicAddressDiscovered(const QString &sessionId, const QHostAddress &address, quint16 port);
    void discoveryFailed(const QString &sessionId, const QString &error);

private slots:
    void onPublicAddressDiscovered(const QHostAddress &address, quint16 port);
    void onDiscoveryFailed(const QString &error);

private:
    STUNClientManager(QObject *parent = nullptr);
    ~STUNClientManager();

    static STUNClientManager *m_instance;  // 声明静态成员

    STUNClient *m_stunClient;
    QHash<QString, QPair<QPair<QHostAddress, quint16>, QDateTime>> m_cachedAddresses;
    QSet<QString> m_pendingSessions;
    QString m_currentSessionId;
};

#endif // STUNCLIENTMANAGER_H

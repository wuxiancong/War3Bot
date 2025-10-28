#ifndef STUNCLIENTMANAGER_H
#define STUNCLIENTMANAGER_H

#include <QObject>
#include <QHostAddress>
#include <QHash>
#include <QSet>

class STUNClient;

class STUNClientManager : public QObject
{
    Q_OBJECT

public:
    static STUNClientManager *instance();
    void discoverPublicAddress(const QString &sessionId);
    bool isDiscoveryInProgress(const QString &sessionId) const;
    void cancelDiscovery(const QString &sessionId);

signals:
    void publicAddressDiscovered(const QString &sessionId, const QHostAddress &address, quint16 port);
    void discoveryFailed(const QString &sessionId, const QString &error);

private slots:
    void onPublicAddressDiscovered(const QHostAddress &address, quint16 port);
    void onDiscoveryFailed(const QString &error);

private:
    STUNClientManager(QObject *parent = nullptr);
    ~STUNClientManager();

    static STUNClientManager *m_instance;
    STUNClient *m_stunClient;
    QHash<QString, QPair<QHostAddress, quint16>> m_cachedAddresses;
    QSet<QString> m_pendingSessions;
    QString m_currentSessionId;
};

#endif // STUNCLIENTMANAGER_H

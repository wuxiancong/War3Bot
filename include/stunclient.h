#ifndef STUNCLIENT_H
#define STUNCLIENT_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QTimer>

class STUNClient : public QObject
{
    Q_OBJECT

public:
    explicit STUNClient(QObject *parent = nullptr);
    ~STUNClient();

    void discoverPublicAddress();
    QHostAddress getPublicAddress() const { return m_publicAddress; }
    quint16 getPublicPort() const { return m_publicPort; }
    bool isDiscoveryComplete() const { return m_discoveryComplete; }

signals:
    void publicAddressDiscovered(const QHostAddress &address, quint16 port);
    void discoveryFailed(const QString &error);

private slots:
    void onSocketReadyRead();
    void onDiscoveryTimeout();

private:
    QUdpSocket *m_udpSocket;
    QHostAddress m_publicAddress;
    quint16 m_publicPort;
    bool m_discoveryComplete;
    QTimer *m_discoveryTimer;
    QList<QHostAddress> m_stunServers;

    void sendBindingRequest(const QHostAddress &stunServer, quint16 port);
    bool parseBindingResponse(const QByteArray &data, QHostAddress &mappedAddress, quint16 &mappedPort);
};

#endif // STUNCLIENT_H

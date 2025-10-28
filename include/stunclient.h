#ifndef STUNCLIENT_H
#define STUNCLIENT_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QHostAddress>

class STUNClient : public QObject
{
    Q_OBJECT

public:
    explicit STUNClient(QObject *parent = nullptr);
    ~STUNClient();

    void discoverPublicAddress();
    void cancelDiscovery();

signals:
    void publicAddressDiscovered(const QHostAddress &address, quint16 port);
    void discoveryFailed(const QString &error);

private slots:
    void onSocketReadyRead();
    void onDiscoveryTimeout();

private:
    void sendBindingRequest(const QHostAddress &stunServer, quint16 port);
    bool parseBindingResponse(const QByteArray &data, QHostAddress &mappedAddress, quint16 &mappedPort);

    QUdpSocket *m_udpSocket;
    QHostAddress m_publicAddress;
    quint16 m_publicPort;
    bool m_discoveryComplete;
    QTimer *m_discoveryTimer;
    QVector<QHostAddress> m_stunServers;
    bool m_discoveryCancelled;
};

#endif // STUNCLIENT_H

#ifndef NETWORKDIAGNOSTIC_H
#define NETWORKDIAGNOSTIC_H

#include <QObject>

class NetworkDiagnostic : public QObject
{
    Q_OBJECT

public:
    static NetworkDiagnostic *instance();
    void checkNetworkConnectivity();
    bool isSTUNReachable();

signals:
    void networkStatusUpdate(const QString &status);

private:
    NetworkDiagnostic(QObject *parent = nullptr);

    // 增强诊断的私有方法
    void checkNetworkInterfaces();
    void testDNSResolution();
    void testSTUNConnectivity();
    void testSingleSTUNServer(const QString &host, quint16 port);
    void testUDPConnectivity(const QString &host, quint16 port);

    static NetworkDiagnostic *m_instance;  // 声明静态成员
};

#endif // NETWORKDIAGNOSTIC_H

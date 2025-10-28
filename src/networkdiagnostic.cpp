#include "networkdiagnostic.h"
#include "logger.h"
#include <QTcpSocket>
#include <QUdpSocket>
#include <QHostInfo>
#include <QTimer>
#include <QNetworkInterface>

// 定义静态成员变量
NetworkDiagnostic *NetworkDiagnostic::m_instance = nullptr;

NetworkDiagnostic::NetworkDiagnostic(QObject *parent)
    : QObject(parent)
{
}

NetworkDiagnostic *NetworkDiagnostic::instance()
{
    if (!m_instance) {
        m_instance = new NetworkDiagnostic();
    }
    return m_instance;
}

void NetworkDiagnostic::checkNetworkConnectivity()
{
    LOG_INFO("NetworkDiagnostic: Starting comprehensive network diagnostics...");

    // 1. 检查网络接口
    checkNetworkInterfaces();

    // 2. 测试DNS解析
    testDNSResolution();

    // 3. 测试STUN服务器连通性
    testSTUNConnectivity();
}

void NetworkDiagnostic::checkNetworkInterfaces()
{
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    LOG_INFO(QString("NetworkDiagnostic: Found %1 network interfaces").arg(interfaces.size()));

    for (const QNetworkInterface &interface : interfaces) {
        if (interface.flags().testFlag(QNetworkInterface::IsUp) &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack)) {

            LOG_INFO(QString("NetworkDiagnostic: Interface %1 is UP").arg(interface.humanReadableName()));

            QList<QNetworkAddressEntry> entries = interface.addressEntries();
            for (const QNetworkAddressEntry &entry : entries) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    LOG_INFO(QString("NetworkDiagnostic:   IPv4: %1, Netmask: %2")
                                 .arg(entry.ip().toString()).arg(entry.netmask().toString()));
                }
            }
        }
    }
}

void NetworkDiagnostic::testDNSResolution()
{
    LOG_INFO("NetworkDiagnostic: Testing DNS resolution...");

    QStringList testHosts = {
        "stun.l.google.com",
        "google.com",
        "github.com"
    };

    for (const QString &host : testHosts) {
        QHostInfo::lookupHost(host, this, [this, host](const QHostInfo &info) {
            if (info.error() == QHostInfo::NoError && !info.addresses().isEmpty()) {
                LOG_INFO(QString("NetworkDiagnostic: DNS resolution for %1: SUCCESS - %2")
                             .arg(host).arg(info.addresses().first().toString()));
                emit networkStatusUpdate(QString("DNS %1: OK").arg(host));
            } else {
                LOG_ERROR(QString("NetworkDiagnostic: DNS resolution for %1: FAILED - %2")
                              .arg(host).arg(info.errorString()));
                emit networkStatusUpdate(QString("DNS %1: Failed").arg(host));
            }
        });
    }
}

void NetworkDiagnostic::testSTUNConnectivity()
{
    LOG_INFO("NetworkDiagnostic: Testing STUN server connectivity...");

    // 测试多个STUN服务器
    QVector<QPair<QString, quint16>> stunServers = {
        {"stun.l.google.com", 19302},
        {"stun1.l.google.com", 19302},
        {"stun.qq.com", 3478},
        {"stun.voip.blackberry.com", 3478}
    };

    for (const auto &server : stunServers) {
        testSingleSTUNServer(server.first, server.second);
    }
}

void NetworkDiagnostic::testSingleSTUNServer(const QString &host, quint16 port)
{
    // 测试TCP连接
    QTcpSocket *tcpSocket = new QTcpSocket(this);
    connect(tcpSocket, &QTcpSocket::connected, this, [this, host, port, tcpSocket]() {
        LOG_INFO(QString("NetworkDiagnostic: TCP connection to %1:%2: SUCCESS").arg(host).arg(port));
        emit networkStatusUpdate(QString("TCP %1:%2: OK").arg(host).arg(port));
        tcpSocket->close();
        tcpSocket->deleteLater();
    });

    connect(tcpSocket, &QTcpSocket::errorOccurred, this, [this, host, port, tcpSocket](QAbstractSocket::SocketError error) {
        LOG_WARNING(QString("NetworkDiagnostic: TCP connection to %1:%2: FAILED - %3")
                        .arg(host).arg(port).arg(tcpSocket->errorString()));
        emit networkStatusUpdate(QString("TCP %1:%2: Failed").arg(host).arg(port));
        tcpSocket->deleteLater();

        // TCP失败后测试UDP
        testUDPConnectivity(host, port);
    });

    tcpSocket->connectToHost(host, port);
    if (!tcpSocket->waitForConnected(3000)) {
        LOG_WARNING(QString("NetworkDiagnostic: TCP connection to %1:%2: TIMEOUT").arg(host).arg(port));
        emit networkStatusUpdate(QString("TCP %1:%2: Timeout").arg(host).arg(port));
        tcpSocket->deleteLater();

        // TCP超时后测试UDP
        testUDPConnectivity(host, port);
    }
}

void NetworkDiagnostic::testUDPConnectivity(const QString &host, quint16 port)
{
    QUdpSocket *udpSocket = new QUdpSocket(this);

    // 明确指定bind参数避免歧义
    if (udpSocket->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        LOG_INFO(QString("NetworkDiagnostic: UDP socket bound successfully for %1:%2").arg(host).arg(port));

        // 解析主机名
        QHostInfo::lookupHost(host, this, [this, host, port, udpSocket](const QHostInfo &info) {
            if (info.error() == QHostInfo::NoError && !info.addresses().isEmpty()) {
                QHostAddress address = info.addresses().first();

                // 发送测试数据
                QByteArray testData = "UDP_TEST";
                qint64 sent = udpSocket->writeDatagram(testData, address, port);

                if (sent > 0) {
                    LOG_INFO(QString("NetworkDiagnostic: UDP data sent to %1:%2: SUCCESS").arg(host).arg(port));
                    emit networkStatusUpdate(QString("UDP %1:%2: OK").arg(host).arg(port));
                } else {
                    LOG_WARNING(QString("NetworkDiagnostic: UDP data send to %1:%2: FAILED - %3")
                                    .arg(host).arg(port).arg(udpSocket->errorString()));
                    emit networkStatusUpdate(QString("UDP %1:%2: Send failed").arg(host).arg(port));
                }
            } else {
                LOG_ERROR(QString("NetworkDiagnostic: Cannot resolve %1 for UDP test").arg(host));
                emit networkStatusUpdate(QString("UDP %1:%2: DNS failed").arg(host).arg(port));
            }

            udpSocket->close();
            udpSocket->deleteLater();
        });
    } else {
        LOG_ERROR(QString("NetworkDiagnostic: UDP bind failed for %1:%2: %3")
                      .arg(host).arg(port).arg(udpSocket->errorString()));
        emit networkStatusUpdate(QString("UDP %1:%2: Bind failed").arg(host).arg(port));
        udpSocket->deleteLater();
    }
}

bool NetworkDiagnostic::isSTUNReachable()
{
    // 简化的STUN可达性检查
    QTcpSocket socket;
    socket.connectToHost("stun.l.google.com", 19302);

    if (socket.waitForConnected(5000)) {
        LOG_INFO("NetworkDiagnostic: STUN server is reachable via TCP");
        socket.close();
        emit networkStatusUpdate("STUN server is reachable");
        return true;
    } else {
        LOG_WARNING(QString("NetworkDiagnostic: STUN server TCP connection failed: %1").arg(socket.errorString()));

        // 尝试UDP连通性测试
        QUdpSocket udpSocket;
        // 明确指定参数避免歧义
        if (udpSocket.bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress)) {
            LOG_INFO("NetworkDiagnostic: UDP socket bound successfully");
            emit networkStatusUpdate("UDP connectivity OK");
            udpSocket.close();
            return true;
        } else {
            LOG_ERROR(QString("NetworkDiagnostic: UDP bind failed: %1").arg(udpSocket.errorString()));
            emit networkStatusUpdate("UDP connectivity failed");
            return false;
        }
    }
}

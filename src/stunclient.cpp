#include "stunclient.h"
#include "logger.h"
#include <QNetworkDatagram>
#include <QRandomGenerator>
#include <QThread>
#include <QDataStream>

// 平台无关的字节序转换函数
namespace {
quint32 networkToHost32(quint32 value) {
    return ((value & 0x000000FF) << 24) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0xFF000000) >> 24);
}

quint16 networkToHost16(quint16 value) {
    return ((value & 0x00FF) << 8) |
           ((value & 0xFF00) >> 8);
}
}

STUNClient::STUNClient(QObject *parent)
    : QObject(parent)
    , m_udpSocket(nullptr)
    , m_publicPort(0)
    , m_discoveryComplete(false)
    , m_discoveryTimer(nullptr)
    , m_discoveryCancelled(false)
{
    m_udpSocket = new QUdpSocket(this);
    m_discoveryTimer = new QTimer(this);
    m_discoveryTimer->setSingleShot(true);
    connect(m_discoveryTimer, &QTimer::timeout, this, &STUNClient::onDiscoveryTimeout);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &STUNClient::onSocketReadyRead);

    // 更可靠的 STUN 服务器列表
    m_stunServers = {
        QHostAddress("stun.l.google.com"),
        QHostAddress("stun1.l.google.com"),
        QHostAddress("stun2.l.google.com"),
        QHostAddress("stun.qq.com"),
        QHostAddress("stun.voip.blackberry.com")
    };
}

STUNClient::~STUNClient()
{
    cancelDiscovery();
}

void STUNClient::discoverPublicAddress()
{
    m_discoveryComplete = false;
    m_discoveryCancelled = false;
    m_publicAddress = QHostAddress();
    m_publicPort = 0;

    // 确保socket处于未连接状态
    if (m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
        m_udpSocket->close();
        QThread::msleep(100); // 给socket关闭一点时间
    }

    LOG_INFO("STUNClient: Starting public address discovery");

    // 尝试绑定到所有可用接口
    for (int attempt = 0; attempt < 3; attempt++) {
        if (m_udpSocket->bind(QHostAddress::Any, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            LOG_INFO(QString("STUNClient: Bound to local port %1").arg(m_udpSocket->localPort()));

            // 设置socket选项以提高可靠性
            m_udpSocket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 8192);
            m_udpSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 8192);

            // 发送STUN请求
            sendSTUNRequests();
            return;
        }

        LOG_WARNING(QString("STUNClient: Bind attempt %1 failed: %2")
                        .arg(attempt + 1).arg(m_udpSocket->errorString()));
        QThread::msleep(200);
    }

    LOG_ERROR("STUNClient: Failed to bind UDP socket after 3 attempts");
    emit discoveryFailed("Bind failed: " + m_udpSocket->errorString());
}

void STUNClient::sendSTUNRequests()
{
    // 只尝试前3个STUN服务器，减少网络负载
    int maxServers = qMin(3, m_stunServers.size());
    int requestsSent = 0;

    for (int i = 0; i < maxServers; ++i) {
        if (sendBindingRequest(m_stunServers[i], 19302)) {
            requestsSent++;
        }
    }

    if (requestsSent == 0) {
        LOG_ERROR("STUNClient: Failed to send any STUN requests");
        emit discoveryFailed("No STUN requests could be sent");
        return;
    }

    LOG_INFO(QString("STUNClient: Sent %1 STUN requests").arg(requestsSent));
    m_discoveryTimer->start(10000); // 10秒超时
}

bool STUNClient::sendBindingRequest(const QHostAddress &stunServer, quint16 port)
{
    if (m_discoveryCancelled) return false;

    // 创建STUN绑定请求
    QByteArray request;
    QDataStream stream(&request, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    // STUN头部 (20字节)
    // 方法: Binding (0x0001)
    stream << quint16(0x0001); // Message Type: Binding Request

    // 消息长度: 0
    stream << quint16(0x0000);

    // Magic Cookie (0x2112A442)
    stream << quint32(0x2112A442);

    // Transaction ID (12字节随机数)
    QRandomGenerator *generator = QRandomGenerator::global();
    for (int i = 0; i < 12; ++i) {
        stream << quint8(generator->bounded(256));
    }

    qint64 sent = m_udpSocket->writeDatagram(request, stunServer, port);
    if (sent == -1) {
        LOG_WARNING(QString("STUNClient: Failed to send request to %1:%2: %3")
                        .arg(stunServer.toString()).arg(port).arg(m_udpSocket->errorString()));
        return false;
    } else {
        LOG_DEBUG(QString("STUNClient: Sent STUN request to %1:%2, size: %3")
                      .arg(stunServer.toString()).arg(port).arg(sent));
        return true;
    }
}

void STUNClient::onSocketReadyRead()
{
    if (m_discoveryCancelled || m_discoveryComplete) return;

    while (m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        if (!datagram.isValid()) {
            continue;
        }

        QByteArray data = datagram.data();
        QHostAddress senderAddr = datagram.senderAddress();
        quint16 senderPort = datagram.senderPort();

        LOG_DEBUG(QString("STUNClient: Received response from %1:%2, size: %3")
                      .arg(senderAddr.toString()).arg(senderPort).arg(data.size()));

        QHostAddress mappedAddress;
        quint16 mappedPort;

        if (parseBindingResponse(data, mappedAddress, mappedPort)) {
            m_discoveryTimer->stop();
            m_publicAddress = mappedAddress;
            m_publicPort = mappedPort;
            m_discoveryComplete = true;

            LOG_INFO(QString("STUNClient: Public address discovered: %1:%2")
                         .arg(m_publicAddress.toString()).arg(m_publicPort));

            // 立即关闭socket释放资源
            m_udpSocket->close();

            emit publicAddressDiscovered(m_publicAddress, m_publicPort);
            return;
        } else {
            LOG_DEBUG("STUNClient: Failed to parse binding response");
        }
    }
}

bool STUNClient::parseBindingResponse(const QByteArray &data, QHostAddress &mappedAddress, quint16 &mappedPort)
{
    if (data.size() < 20) {
        LOG_DEBUG(QString("STUNClient: Response too small: %1 bytes").arg(data.size()));
        return false;
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    // 读取消息头
    quint16 messageType;
    quint16 messageLength;
    quint32 magicCookie;

    stream >> messageType >> messageLength >> magicCookie;

    // 验证消息类型和Magic Cookie
    if (messageType != 0x0101) { // Binding Response
        LOG_DEBUG(QString("STUNClient: Not a binding response: 0x%1").arg(messageType, 4, 16, QLatin1Char('0')));
        return false;
    }

    if (magicCookie != 0x2112A442) {
        LOG_DEBUG(QString("STUNClient: Invalid magic cookie: 0x%1").arg(magicCookie, 8, 16, QLatin1Char('0')));
        return false;
    }

    // 跳过Transaction ID (12字节)
    stream.skipRawData(12);

    // 解析属性
    int remainingBytes = messageLength;
    while (remainingBytes >= 4) {
        quint16 attrType, attrLength;
        stream >> attrType >> attrLength;

        // 计算填充后的长度
        int paddedLength = attrLength;
        if (paddedLength % 4 != 0) {
            paddedLength += 4 - (paddedLength % 4);
        }

        if (remainingBytes < 4 + paddedLength) {
            break; // 属性长度超出数据范围
        }

        LOG_DEBUG(QString("STUNClient: Processing attribute type 0x%1, length %2")
                      .arg(attrType, 4, 16, QLatin1Char('0')).arg(attrLength));

        // XOR-MAPPED-ADDRESS 属性 (0x0020)
        if (attrType == 0x0020 && attrLength >= 8) {
            quint8 reserved, family;
            quint16 xPort;
            quint32 xAddress;

            stream >> reserved >> family >> xPort >> xAddress;

            if (family == 0x01) { // IPv4
                // 解码XOR映射的地址
                mappedPort = xPort ^ 0x2112; // magic cookie的高16位
                quint32 address = xAddress ^ 0x2112A442;

                mappedAddress = QHostAddress(networkToHost32(address));
                mappedPort = networkToHost16(mappedPort);

                LOG_INFO(QString("STUNClient: Found XOR-MAPPED-ADDRESS - IP: %1, Port: %2")
                             .arg(mappedAddress.toString()).arg(mappedPort));
                return true;
            }
        }
        // MAPPED-ADDRESS 属性 (0x0001) - 备用
        else if (attrType == 0x0001 && attrLength >= 8) {
            quint8 reserved, family;
            quint16 port;
            quint32 address;

            stream >> reserved >> family >> port >> address;

            if (family == 0x01) { // IPv4
                mappedPort = networkToHost16(port);
                mappedAddress = QHostAddress(networkToHost32(address));

                LOG_INFO(QString("STUNClient: Found MAPPED-ADDRESS - IP: %1, Port: %2")
                             .arg(mappedAddress.toString()).arg(mappedPort));
                return true;
            }
        } else {
            // 跳过未知属性
            stream.skipRawData(paddedLength);
        }

        remainingBytes -= 4 + paddedLength;
    }

    LOG_DEBUG("STUNClient: No valid mapped address attribute found");
    return false;
}

void STUNClient::onDiscoveryTimeout()
{
    if (!m_discoveryComplete && !m_discoveryCancelled) {
        LOG_ERROR("STUNClient: Discovery timeout - no STUN servers responded");

        // 关闭socket释放资源
        if (m_udpSocket) {
            m_udpSocket->close();
        }

        emit discoveryFailed("Discovery timeout - no STUN servers responded");
    }
}

void STUNClient::cancelDiscovery()
{
    m_discoveryCancelled = true;
    if (m_discoveryTimer) {
        m_discoveryTimer->stop();
    }
    if (m_udpSocket && m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
        m_udpSocket->close();
    }
}

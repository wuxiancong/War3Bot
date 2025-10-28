#include "stunclient.h"
#include "logger.h"
#include <QNetworkDatagram>
#include <QRandomGenerator>

// 平台无关的字节序转换函数
namespace {
quint32 networkToHost32(quint32 value) {
    // 手动字节序转换，避免依赖特定平台函数
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
{
    m_udpSocket = new QUdpSocket(this);
    m_discoveryTimer = new QTimer(this);
    m_discoveryTimer->setSingleShot(true);
    connect(m_discoveryTimer, &QTimer::timeout, this, &STUNClient::onDiscoveryTimeout);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &STUNClient::onSocketReadyRead);

    // 公共STUN服务器列表
    m_stunServers = {
        QHostAddress("stun.l.google.com"),
        QHostAddress("stun1.l.google.com"),
        QHostAddress("stun2.l.google.com"),
        QHostAddress("stun3.l.google.com"),
        QHostAddress("stun4.l.google.com")
    };
}

STUNClient::~STUNClient()
{
    if (m_discoveryTimer) {
        m_discoveryTimer->stop();
    }
}

void STUNClient::discoverPublicAddress()
{
    m_discoveryComplete = false;
    m_publicAddress = QHostAddress();
    m_publicPort = 0;

    // 使用最简单的绑定方式，避免歧义
    if (!m_udpSocket->bind()) {
        LOG_ERROR("STUNClient: Failed to bind UDP socket");
        emit discoveryFailed("Bind failed: " + m_udpSocket->errorString());
        return;
    }

    LOG_INFO(QString("STUNClient: Bound to local port %1").arg(m_udpSocket->localPort()));

    // 向所有STUN服务器发送请求
    for (const QHostAddress &server : m_stunServers) {
        sendBindingRequest(server, 19302); // STUN标准端口
    }

    m_discoveryTimer->start(10000); // 10秒超时
}

void STUNClient::sendBindingRequest(const QHostAddress &stunServer, quint16 port)
{
    // STUN绑定请求消息
    QByteArray request;

    // STUN头部 (20字节)
    // 方法: Binding (0x0001)
    request.append(char(0x00));
    request.append(char(0x01));

    // 消息长度: 0
    request.append(char(0x00));
    request.append(char(0x00));

    // Magic Cookie (0x2112A442)
    request.append(char(0x21));
    request.append(char(0x12));
    request.append(char(0xA4));
    request.append(char(0x42));

    // Transaction ID (12字节随机数)
    QRandomGenerator *generator = QRandomGenerator::global();
    for (int i = 0; i < 12; ++i) {
        request.append(char(generator->bounded(256)));
    }

    qint64 sent = m_udpSocket->writeDatagram(request, stunServer, port);
    if (sent == -1) {
        LOG_WARNING(QString("STUNClient: Failed to send request to %1: %2")
                        .arg(stunServer.toString()).arg(m_udpSocket->errorString()));
    } else {
        LOG_DEBUG(QString("STUNClient: Sent STUN request to %1:%2, size: %3")
                      .arg(stunServer.toString()).arg(port).arg(sent));
    }
}

void STUNClient::onSocketReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
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

    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data.constData());

    // 检查STUN消息头
    uint16_t messageType = (bytes[0] << 8) | bytes[1];
    if (messageType != 0x0101) { // Binding Response
        LOG_DEBUG(QString("STUNClient: Not a binding response: 0x%1").arg(messageType, 4, 16, QLatin1Char('0')));
        return false;
    }

    // 验证Magic Cookie
    uint32_t magicCookie = (bytes[4] << 24) | (bytes[5] << 16) | (bytes[6] << 8) | bytes[7];
    if (magicCookie != 0x2112A442) {
        LOG_DEBUG(QString("STUNClient: Invalid magic cookie: 0x%1").arg(magicCookie, 8, 16, QLatin1Char('0')));
        return false;
    }

    // 解析属性
    int pos = 20; // 跳过头部
    while (pos + 4 <= data.size()) {
        uint16_t attrType = (bytes[pos] << 8) | bytes[pos + 1];
        uint16_t attrLength = (bytes[pos + 2] << 8) | bytes[pos + 3];
        pos += 4;

        if (pos + attrLength > data.size()) {
            LOG_DEBUG("STUNClient: Attribute length exceeds data size");
            break;
        }

        LOG_DEBUG(QString("STUNClient: Processing attribute type 0x%1, length %2")
                      .arg(attrType, 4, 16, QLatin1Char('0')).arg(attrLength));

        // XOR-MAPPED-ADDRESS 属性 (0x0020)
        if (attrType == 0x0020 && attrLength >= 8) {
            uint8_t family = bytes[pos + 1];
            if (family == 0x01) { // IPv4
                // 端口号 (XOR with magic cookie的高16位)
                uint16_t xorPort = (bytes[pos + 2] << 8) | bytes[pos + 3];
                uint16_t magicHigh = 0x2112;
                mappedPort = xorPort ^ magicHigh;

                // IP地址 (XOR with magic cookie)
                uint32_t xorIP = (bytes[pos + 4] << 24) | (bytes[pos + 5] << 16) |
                                 (bytes[pos + 6] << 8) | bytes[pos + 7];
                uint32_t magic = 0x2112A442;
                uint32_t ip = xorIP ^ magic;

                // 使用自定义的字节序转换函数替代 ntohl
                mappedAddress = QHostAddress(networkToHost32(ip));

                LOG_DEBUG(QString("STUNClient: Found XOR-MAPPED-ADDRESS - IP: %1, Port: %2")
                              .arg(mappedAddress.toString()).arg(mappedPort));
                return true;
            } else {
                LOG_DEBUG(QString("STUNClient: Unsupported address family: %1").arg(family));
            }
        }
        // MAPPED-ADDRESS 属性 (0x0001) - 备用
        else if (attrType == 0x0001 && attrLength >= 8) {
            uint8_t family = bytes[pos + 1];
            if (family == 0x01) { // IPv4
                mappedPort = (bytes[pos + 2] << 8) | bytes[pos + 3];
                uint32_t ip = (bytes[pos + 4] << 24) | (bytes[pos + 5] << 16) |
                              (bytes[pos + 6] << 8) | bytes[pos + 7];

                // 使用自定义的字节序转换函数替代 ntohl
                mappedPort = networkToHost16(mappedPort);
                mappedAddress = QHostAddress(networkToHost32(ip));

                LOG_DEBUG(QString("STUNClient: Found MAPPED-ADDRESS - IP: %1, Port: %2")
                              .arg(mappedAddress.toString()).arg(mappedPort));
                return true;
            }
        }

        // 移动到下一个属性
        pos += attrLength;
        if (attrLength % 4 != 0) {
            pos += 4 - (attrLength % 4); // 填充对齐
        }
    }

    LOG_DEBUG("STUNClient: No valid mapped address attribute found");
    return false;
}

void STUNClient::onDiscoveryTimeout()
{
    if (!m_discoveryComplete) {
        LOG_ERROR("STUNClient: Discovery timeout");
        emit discoveryFailed("Discovery timeout - no STUN servers responded");
    }
}

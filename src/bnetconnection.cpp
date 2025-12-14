#include "bncsutil/checkrevision.h"
#include "bnetconnection.h"
#include "logger.h"
#include <QDir>
#include <QDataStream>
#include <QCoreApplication>
#include <QNetworkInterface>

BnetConnection::BnetConnection(QObject *parent)
    : QObject(parent)
    , m_nls(nullptr)
{
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &BnetConnection::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &BnetConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &BnetConnection::onDisconnected);

    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError){
        LOG_ERROR(QString("战网连接错误: %1").arg(m_socket->errorString()));
    });

    QString appDir = QCoreApplication::applicationDirPath();
    QDir dir(appDir);
    if (dir.cd("war3files")) {
        m_war3ExePath = dir.absoluteFilePath("War3.exe");
    } else {
        m_war3ExePath = "War3.exe";
        LOG_WARNING("找不到 war3files 目录，尝试直接读取当前目录下的 War3.exe");
    }
    LOG_INFO(QString("War3 文件路径设置为: %1").arg(m_war3ExePath));
}

BnetConnection::~BnetConnection()
{
    disconnectFromHost();
    if (m_nls) {
        delete m_nls;
        m_nls = nullptr;
    }
}

void BnetConnection::setCredentials(const QString &user, const QString &pass)
{
    m_user = user;
    m_pass = pass;
}

void BnetConnection::connectToHost(const QString &address, quint16 port)
{
    m_serverAddr = address;
    m_serverPort = port;
    LOG_INFO(QString("正在建立 TCP 连接至战网: %1:%2").arg(address).arg(port));
    m_socket->connectToHost(address, port);
}

void BnetConnection::onConnected()
{
    LOG_INFO("✅ TCP 链路已建立，发送协议握手字节...");

    char protocolByte = 1;
    m_socket->write(&protocolByte, 1);

    sendAuthInfo();
}

void BnetConnection::sendPacket(PacketID id, const QByteArray &payload)
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out << BNET_HEADER;
    out << (quint8)id;
    out << (quint16)(payload.size() + 4);

    if (!payload.isEmpty()) {
        packet.append(payload);
    }

    m_socket->write(packet);

    // === 打印发送内容的 HEX ===
    QString hexStr = packet.toHex().toUpper();
    for(int i = 2; i < hexStr.length(); i += 3) hexStr.insert(i, " ");
    LOG_INFO(QString("📤 发送包 ID: 0x%1 Len:%2 Data: %3")
                 .arg(QString::number(id, 16))
                 .arg(packet.size())
                 .arg(hexStr));
}

void BnetConnection::sendAuthInfo()
{
    // 获取本地IP
    QString localIpStr = getPrimaryIPv4();
    quint32 localIp = 0;

    if (!localIpStr.isEmpty()) {
        localIp = ipToUint32(localIpStr);
        LOG_INFO(QString("使用本地IP地址: %1 转换为: 0x%2").arg(localIpStr, QString::number(localIp, 16)));
    } else {
        LOG_INFO(QString("未找到合适的本地IP地址，使用0"));
    }

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out << (quint32)0;                   // Protocol ID
    out.writeRawData("68XI", 4);         // Platform (IX86)
    out.writeRawData("PX3W", 4);         // Product (W3XP)
    out << (quint32)26;                  // Version (0x1A)
    out.writeRawData("SUne", 4);         // Language (enUS)
    out << localIp;                      // Local IP
    out << (quint32)0xFFFFFE20;          // Timezone bias (-480 minutes)
    out << (quint32)2052;                // Locale ID (简体中文)
    out << (quint32)2052;                // Language ID
    out.writeRawData("CHN", 3);          // Country code
    out.writeRawData("\0", 1);
    out.writeRawData("China", 5);        // Country name
    out.writeRawData("\0", 1);

    sendPacket(SID_AUTH_INFO, payload);
}

void BnetConnection::createGameOnLadder(const QString &gameName, const QByteArray &mapStatString, quint16 udpPort)
{
    LOG_INFO(QString("🚀 请求创建房间: %1").arg(gameName));

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out << (quint32)0x11;   // Public Game
    out << (quint32)0;
    out << (quint16)0x1F;
    out << (quint32)0;
    out << (quint32)0;
    out.writeRawData(gameName.toUtf8().constData(), gameName.toUtf8().size());
    out << (quint8)0;
    out << (quint8)0;       // Password
    out.writeRawData(mapStatString.constData(), mapStatString.size());
    out << (quint8)0;
    out << (quint16)udpPort;

    sendPacket(SID_STARTADVEX3, payload);
}

void BnetConnection::onReadyRead()
{
    // === 调试代码：打印当前缓冲区所有数据 ===
    // 注意：peek 不会移除数据，只是看看
    if (m_socket->bytesAvailable() > 0) {
        QByteArray allData = m_socket->peek(m_socket->bytesAvailable());
        QString hexStr = allData.toHex().toUpper();
        // 每2个字符加空格，方便阅读
        for(int i = 2; i < hexStr.length(); i += 3) hexStr.insert(i, " ");

        LOG_INFO(QString("📥Socket 缓冲区有数据 (%1 字节): %2").arg(allData.size()).arg(hexStr));
    }
    // ======================================

    while (m_socket->bytesAvailable() > 0) {
        // 1. 检查头部长度 (FF + ID + LEN_L + LEN_H) = 4字节
        if (m_socket->bytesAvailable() < 4) {
            LOG_INFO("数据不足 4 字节，等待更多数据...");
            return;
        }

        QByteArray headerData = m_socket->peek(4);

        // 2. 检查协议头 0xFF
        if ((quint8)headerData[0] != BNET_HEADER) {
            LOG_WARNING(QString("⚠️ 协议头错误! 收到: 0x%1 (期望 0xFF) - 丢弃 1 字节")
                            .arg(QString::number((quint8)headerData[0], 16)));
            m_socket->read(1); // 丢弃错误字节，滑动窗口
            continue;
        }

        // 3. 解析长度
        quint16 length;
        QDataStream lenStream(headerData.mid(2, 2));
        lenStream.setByteOrder(QDataStream::LittleEndian);
        lenStream >> length;

        LOG_INFO(QString("解析包头: ID=0x%1 长度=%2 可用=%3")
                     .arg(QString::number((quint8)headerData[1], 16))
                     .arg(length)
                     .arg(m_socket->bytesAvailable()));

        // 4. 检查数据包是否完整
        if (m_socket->bytesAvailable() < length) {
            LOG_INFO(QString("包不完整 (需要 %1，只有 %2)，等待拼接...").arg(length).arg(m_socket->bytesAvailable()));
            return;
        }

        // 5. 读取完整的一个包
        QByteArray packetData = m_socket->read(length);
        quint8 packetIdVal = (quint8)packetData[1];

        LOG_INFO(QString("📦 完整读取包 ID: 0x%1，正在分发...").arg(QString::number(packetIdVal, 16)));

        // 6. 分发处理
        handlePacket((PacketID)packetIdVal, packetData.mid(4));
    }
}

void BnetConnection::handlePacket(PacketID id, const QByteArray &data)
{
    LOG_INFO(QString("📥 收到包 ID: 0x%1").arg(QString::number(id, 16)));
    switch (id) {
    case SID_NULL: // 0x00 (KeepAlive)
        LOG_INFO(QString("收到其他战网包: 0x%1").arg(QString::number(id, 16)));
        break;
    case SID_PING:
        sendPacket(SID_PING, data);
        break;

    case SID_LOGONRESPONSE: // 0x29
        LOG_ERROR("✅ 登录响应：收到登录响应回复包 (SID_LOGONRESPONSE)");
        break;
    case SID_LOGONRESPONSE2: // 0x3A (收到 Salt 和 ServerKey)
        handleLoginResponse(data);
        break;

    case SID_REQUIREDWORK: // 0x4C
        LOG_ERROR("❌ 登录失败：服务器要求更新版本 (SID_REQUIREDWORK)");
        break;

    case SID_AUTH_INFO:  // 0x50
    case SID_AUTH_CHECK: // 0x51
        // 如果包长度足够大（包含文件名等），就认为是 Challenge 包
        if (data.size() > 16) {
            LOG_INFO(QString("收到 Auth Challenge (ID=0x%1)，开始处理...").arg(QString::number(id, 16)));
            handleAuthCheck(data);
        } else {
            LOG_INFO("收到 Auth Info 回显，忽略...");
        }
        break;

    case SID_AUTH_ACCOUNTLOGON: // 0x53
        // 4 (Status) + 32 (Salt) + 32 (Key) = 68 字节
        if (data.size() >= 68) {
            LOG_INFO("收到有效的登录响应 (Salt/Key)，正在计算 Proof...");
            handleLoginResponse(data);
        } else {
            LOG_INFO(QString("忽略短包 (ID: 0x%1 Len: %2)，等待完整响应...").arg(QString::number(id, 16)).arg(data.size()));
        }
        break;
    case SID_AUTH_ACCOUNTLOGONPROOF: // 0x54
        if (data.size() >= 4 && data[0] == 0) {
            LOG_INFO("🎉 战网登录成功！");
            emit authenticated();
            // 登录成功后，求进入聊天界面
            sendPacket(SID_ENTERCHAT, QByteArray());
        } else {
            quint32 errCode = 0;
            if (data.size() >= 4) {
                QDataStream ds(data);
                ds.setByteOrder(QDataStream::LittleEndian);
                ds >> errCode;
            }
            LOG_ERROR(QString("❌ 登录失败: 错误码 0x%1").arg(QString::number(errCode, 16)));
        }
        break;

    case SID_STARTADVEX3:
        LOG_INFO("✅ 房间创建成功！");
        emit gameListRegistered();
        break;

    default:
        LOG_WARNING(QString("⚠️ 收到未处理的包 ID: 0x%1").arg(QString::number(id, 16)));
        break;
    }
}

void BnetConnection::sendLoginRequest()
{
    // 1. 初始化 NLS (SRP 协议处理模块)
    if (m_nls) delete m_nls;
    // 使用 std::string 确保转换安全
    m_nls = new NLS(m_user.toUtf8().toStdString(), m_pass.toUtf8().toStdString());

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // 2. 获取 32 字节 Client Public Key
    QByteArray clientKey(32, 0);
    m_nls->getPublicKey((char*)clientKey.data());

    // === 关键点 A：必须使用 writeRawData ===
    // 写入 32 字节的 Key (不带长度头)
    out.writeRawData(clientKey.data(), 32);

    // === 关键点 B：写入用户名 ===
    // 写入用户名字符串 + 结束符 \0
    QByteArray userBytes = m_user.toUtf8();
    out.writeRawData(userBytes.data(), userBytes.length());
    out << (quint8)0; // 字符串结束符

    // === 关键点 C：0x29 不需要 UserData ===
    // 相比 0x3A，这里不需要再写一个 0 了

    // 发送 SID_LOGONRESPONSE (0x29)
    sendPacket(SID_LOGONRESPONSE, payload);

    LOG_INFO(QString("正在发送登录请求 (SID_LOGONRESPONSE 0x29): %1").arg(m_user));
}

void BnetConnection::handleLoginResponse(const QByteArray &data)
{
    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 status;
    QByteArray salt(32, 0);
    QByteArray serverKey(32, 0);

    in >> status;
    in.readRawData(salt.data(), 32);
    in.readRawData(serverKey.data(), 32);

    if (status != 0) {
        LOG_ERROR("登录请求被拒绝，状态码: " + QString::number(status));
        return;
    }

    QByteArray proof(20, 0);
    // 计算 M1 (Client Proof)
    m_nls->getClientSessionKey((char*)proof.data(), (char*)salt.data(), (char*)serverKey.data());

    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out.writeRawData(proof.data(), 20);
    out.writeRawData(QByteArray(20, 0).data(), 20);// M2 (客户端无需发送M2，只需发送0)

    sendPacket(SID_AUTH_ACCOUNTLOGON, response);
}

void BnetConnection::handleAuthCheck(const QByteArray &data)
{
    LOG_INFO("🔍 解析 Auth Challenge 数据...");

    if (data.size() < 24) {
        LOG_ERROR("AuthCheck 数据包太短！");
        return;
    }

    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 logonType;
    quint32 serverToken;
    quint32 udpToken;
    quint64 mpqFileTime;

    in >> logonType >> serverToken >> udpToken >> mpqFileTime;

    int offset = 20;

    // 1. 提取 MPQ 文件名
    int strEnd = data.indexOf('\0', offset);
    if (strEnd == -1) return;
    QByteArray mpqFileName = data.mid(offset, strEnd - offset);

    // 2. 提取公式字符串
    offset = strEnd + 1;
    strEnd = data.indexOf('\0', offset);
    if (strEnd == -1) return;
    QByteArray formulaString = data.mid(offset, strEnd - offset);

    LOG_INFO(QString("服务端要求验证文件名: %1").arg(QString(mpqFileName)));

    int mpqNumber = extractMPQNumber(mpqFileName.constData());
    if (mpqNumber < 0) return;

    // 检查文件
    QFile f1(m_war3ExePath);
    if (!f1.open(QIODevice::ReadOnly)) return;
    f1.close();

    // --- 冲突点 A：这里定义了 exeInfo (类型 QFileInfo) ---
    QFileInfo exeInfo(m_war3ExePath);
    QString dirPath = exeInfo.absolutePath();
    QString gamePath = QDir(dirPath).filePath("Game.dll");
    QString stormPath = QDir(dirPath).filePath("Storm.dll");

    unsigned long checkSum = 0;

    checkRevisionFlat(
        formulaString.constData(),
        m_war3ExePath.toUtf8().constData(),
        stormPath.toUtf8().constData(),
        gamePath.toUtf8().constData(),
        mpqNumber,
        &checkSum
        );

    if (checkSum == 0) {
        LOG_ERROR("❌ 哈希计算失败 (结果为 0)");
        return;
    }

    LOG_INFO(QString("✅ 哈希计算成功! CheckSum=0x%1").arg(QString::number(checkSum, 16).toUpper()));

    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    quint32 exeVersion = 0x011a0001; // 1.26a 版本通常为 26，需与 AUTH_INFO 中的版本一致

    out << (quint32)serverToken;     // Client Token
    out << (quint32)exeVersion;      // Version
    out << (quint32)checkSum;        // Hash
    out << (quint32)1;               // Num Results (CD-Key 数量)
    out << (quint32)0;               // Spawn Key (0 = False)

    // === 构造 CD-Key 数据块 ===
    unsigned long productVal = 18; // TFT
    unsigned long publicVal = 0;
    out << (quint32)20;
    out << (quint32)productVal;
    out << (quint32)publicVal;
    out << (quint32)0;
    QByteArray hashData(20, 0);
    out.writeRawData(hashData.data(), 20);

    QByteArray exeInfoStr = "War3.exe 03/18/11 02:00:00 471040";
    out.writeRawData(exeInfoStr.data(), exeInfoStr.length() + 1);

    out.writeRawData(m_user.toUtf8().constData(), m_user.toUtf8().size() + 1);

    sendPacket(SID_AUTH_CHECK, response);

    LOG_INFO("已发送 AuthCheckResponse，正在发起登录请求 (0x3A)...");

    sendLoginRequest();
}

QString BnetConnection::getPrimaryIPv4()
{
    foreach(const QNetworkInterface &interface, QNetworkInterface::allInterfaces()) {
        // 过滤掉回环接口和非活动接口
        if (interface.flags() & QNetworkInterface::IsUp &&
            interface.flags() & QNetworkInterface::IsRunning &&
            !(interface.flags() & QNetworkInterface::IsLoopBack)) {
            foreach(const QNetworkAddressEntry &entry, interface.addressEntries()) {
                QHostAddress ip = entry.ip();
                if (ip.protocol() == QAbstractSocket::IPv4Protocol) {
                    // 排除常见的内部网段
                    QString ipStr = ip.toString();
                    if (!ipStr.startsWith("169.254.") &&  // 链路本地地址
                        !ipStr.startsWith("127.")) {      // 回环地址
                        return ipStr;
                    }
                }
            }
        }
    }
    return QString();
}

quint32 BnetConnection::ipToUint32(const QString &ipAddress)
{
    QHostAddress address(ipAddress);
    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
        return address.toIPv4Address();
    }
    return 0;
}

quint32 BnetConnection::ipToUint32(const QHostAddress &address)
{
    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
        return address.toIPv4Address();
    }
    return 0;
}

void BnetConnection::disconnectFromHost() { m_socket->disconnectFromHost(); }
bool BnetConnection::isConnected() const { return m_socket->state() == QAbstractSocket::ConnectedState; }
void BnetConnection::onDisconnected() { LOG_WARNING("🔌 战网连接断开"); }

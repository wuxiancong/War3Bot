#include "bncsutil/checkrevision.h"
#include "bnetconnection.h"
#include "logger.h"
#include <QDir>
#include <QDataStream>
#include <QCoreApplication>
#include <QNetworkInterface>
#include <QDateTime>
#include <QFileInfo>

// 辅助宏：循环左移
#define ROTL32(x, n) (((x) << ((n) & 31)) | ((x) >> (32 - ((n) & 31))))

BnetConnection::BnetConnection(QObject *parent)
    : QObject(parent)
    , m_loginProtocol(Protocol_Old_0x29)
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

    // 路径处理逻辑
    if (dir.cd("war3files")) {
        m_war3ExePath = dir.absoluteFilePath("War3.exe");
        m_gameDllPath = dir.absoluteFilePath("Game.dll");
        m_stormDllPath = dir.absoluteFilePath("Storm.dll");
    } else {
        LOG_WARNING("找不到 war3files 目录，尝试直接读取当前目录下的 War3.exe");
        dir.setPath(appDir);
        m_war3ExePath = dir.absoluteFilePath("War3.exe");
        m_gameDllPath = dir.absoluteFilePath("Game.dll");
        m_stormDllPath = dir.absoluteFilePath("Storm.dll");
    }

    LOG_INFO(QString("War3 路径: %1").arg(m_war3ExePath));
}

BnetConnection::~BnetConnection()
{
    disconnectFromHost();
    if (m_nls) {
        delete m_nls;
        m_nls = nullptr;
    }
}

void BnetConnection::setCredentials(const QString &user, const QString &pass, LoginProtocol protocol)
{
    m_user = user.trimmed();
    m_pass = pass.trimmed();
    m_loginProtocol = protocol;

    QString protoName;
    if (protocol == Protocol_Old_0x29) protoName = "Old (0x29)";
    else if (protocol == Protocol_Logon2_0x3A) protoName = "Logon2 (0x3A)";
    else protoName = "SRP (0x53)";

    LOG_INFO(QString("设置凭据: 用户[%1] 密码[%2] 协议[%3]").arg(m_user, m_pass, protoName));
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

    QString hexStr = packet.toHex().toUpper();
    for(int i = 2; i < hexStr.length(); i += 3) hexStr.insert(i, " ");
    LOG_INFO(QString("📤 发送包 ID: 0x%1 Len:%2 Data: %3")
                 .arg(QString::number(id, 16))
                 .arg(packet.size())
                 .arg(hexStr));
}

void BnetConnection::sendAuthInfo()
{
    QString localIpStr = getPrimaryIPv4();
    quint32 localIp = localIpStr.isEmpty() ? 0 : ipToUint32(localIpStr);

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out << (quint32)0;
    out.writeRawData("68XI", 4);
    out.writeRawData("PX3W", 4);
    out << (quint32)26;
    out.writeRawData("SUne", 4);
    out << localIp;
    out << (quint32)0xFFFFFE20;
    out << (quint32)2052;
    out << (quint32)2052;
    out.writeRawData("CHN", 3);
    out.writeRawData("\0", 1);
    out.writeRawData("China", 5);
    out.writeRawData("\0", 1);

    sendPacket(SID_AUTH_INFO, payload);
}

// === 核心哈希算法 (Broken SHA1, 返回 Big Endian) ===
QByteArray BnetConnection::calculateBrokenSHA1(const QByteArray &data)
{
    quint32 H[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };

    QByteArray processedData = data;
    int dataLen = processedData.size();
    int numBlocks = (dataLen + 63) / 64;
    if (numBlocks == 0) numBlocks = 1;

    for (int blockIdx = 0; blockIdx < numBlocks; blockIdx++) {
        quint32 W[80];
        memset(W, 0, sizeof(W));

        for (int i = 0; i < 16; i++) {
            int bytePos = blockIdx * 64 + i * 4;
            quint32 val = 0;
            for (int b = 0; b < 4; b++) {
                if (bytePos + b < dataLen) {
                    val |= ((quint32)(quint8)processedData[bytePos + b]) << (b * 8);
                }
            }
            W[i] = val;
        }

        for (int t = 16; t < 80; t++) {
            quint32 xorVal = W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16];
            W[t] = ROTL32(1, xorVal); // 暴雪特有的 1 bit 循环
        }

        quint32 a = H[0], b = H[1], c = H[2], d = H[3], e = H[4];

        for (int t = 0; t < 80; t++) {
            quint32 f, k;
            if (t < 20) { f = (b & c) | ((~b) & d); k = 0x5a827999; }
            else if (t < 40) { f = b ^ c ^ d; k = 0x6ed9eba1; }
            else if (t < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdc; }
            else { f = b ^ c ^ d; k = 0xca62c1d6; }
            quint32 temp = ROTL32(a, 5) + f + e + k + W[t];
            e = d; d = c; c = ROTL32(b, 30); b = a; a = temp;
        }
        H[0] += a; H[1] += b; H[2] += c; H[3] += d; H[4] += e;
    }

    QByteArray result;
    QDataStream ds(&result, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian); // 返回 Big Endian
    ds << H[0] << H[1] << H[2] << H[3] << H[4];
    return result;
}

// === 双重哈希计算 (适用于 0x29 和 0x3A) ===
QByteArray BnetConnection::calculateOldLogonProof(const QString &password, quint32 clientToken, quint32 serverToken)
{
    // 1. Broken SHA1 (Output: BE)
    QByteArray passBytes = password.toLower().toUtf8();
    QByteArray passHashBE = calculateBrokenSHA1(passBytes);

    // 2. 准备外层输入
    QByteArray buffer;
    QDataStream ds(&buffer, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds << clientToken;
    ds << serverToken;

    // 写入 PassHash (转为 Little Endian 以匹配内存)
    QDataStream dsReader(passHashBE);
    dsReader.setByteOrder(QDataStream::BigEndian);
    for(int i=0; i<5; i++) {
        quint32 val;
        dsReader >> val;
        ds << val;
    }

    // 3. 外层哈希：Broken SHA1 (Output: BE)
    QByteArray finalHashBE = calculateBrokenSHA1(buffer);

    // 4. 转为 Little Endian 发送
    QByteArray proofToSend;
    QDataStream dsFinal(&proofToSend, QIODevice::WriteOnly);
    dsFinal.setByteOrder(QDataStream::LittleEndian);
    QDataStream dsFinalReader(finalHashBE);
    dsFinalReader.setByteOrder(QDataStream::BigEndian);

    for(int i=0; i<5; i++) {
        quint32 val;
        dsFinalReader >> val;
        dsFinal << val;
    }

    return proofToSend;
}

void BnetConnection::onReadyRead()
{
    while (m_socket->bytesAvailable() > 0) {
        if (m_socket->bytesAvailable() < 4) return;

        QByteArray headerData = m_socket->peek(4);
        if ((quint8)headerData[0] != BNET_HEADER) {
            m_socket->read(1);
            continue;
        }

        quint16 length;
        QDataStream lenStream(headerData.mid(2, 2));
        lenStream.setByteOrder(QDataStream::LittleEndian);
        lenStream >> length;

        if (m_socket->bytesAvailable() < length) return;

        QByteArray packetData = m_socket->read(length);
        quint8 packetIdVal = (quint8)packetData[1];
        handlePacket((PacketID)packetIdVal, packetData.mid(4));
    }
}

void BnetConnection::handlePacket(PacketID id, const QByteArray &data)
{
    LOG_INFO(QString("📥 收到包 ID: 0x%1").arg(QString::number(id, 16)));

    switch (id) {
    case SID_PING:
        sendPacket(SID_PING, data);
        break;

    // === 0x29 登录响应 ===
    case SID_LOGONRESPONSE:
    {
        if (data.size() < 4) return;
        quint32 result;
        QDataStream ds(data);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds >> result;

        if (result == 1) {
            LOG_INFO("🎉 战网登录成功 (协议 0x29)！");
            emit authenticated();
            // 发送进入聊天，必须带空字符串
            QByteArray enterChatPayload; enterChatPayload.append('\0');
            sendPacket(SID_ENTERCHAT, enterChatPayload);
        } else {
            LOG_ERROR(QString("❌ 登录失败 (0x29): 错误码 0x%1").arg(QString::number(result, 16)));
        }
        break;
    }

    // === 0x3A 登录响应 (结构同 0x29) ===
    case SID_LOGONRESPONSE2:
    {
        if (data.size() < 4) return;
        quint32 result;
        QDataStream ds(data);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds >> result;

        if (result == 0) {
            LOG_INFO("🎉 战网登录成功 (协议 0x3A)！");
            emit authenticated();
            QByteArray enterChatPayload; enterChatPayload.append('\0');
            sendPacket(SID_ENTERCHAT, enterChatPayload);
        } else {
            LOG_ERROR(QString("❌ 登录失败 (0x3A): 错误码 0x%1").arg(QString::number(result, 16)));
        }
        break;
    }

    case SID_AUTH_INFO:
    case SID_AUTH_CHECK:
        if (data.size() > 16) handleAuthCheck(data);
        break;

    // === SRP 步骤 1 响应 ===
    case SID_AUTH_ACCOUNTLOGON:
        if (m_loginProtocol == Protocol_SRP_0x53) {
            handleSRPLoginResponse(data);
        }
        break;

    // === SRP 步骤 2 响应 (最终结果) ===
    case SID_AUTH_ACCOUNTLOGONPROOF:
    {
        if (data.size() < 4) return;
        quint32 status;
        QDataStream ds(data);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds >> status;

        if (status == 0 || status == 0x0E) { // 0x00=Success
            LOG_INFO("🎉 战网登录成功 (协议 SRP)！");
            emit authenticated();
            QByteArray enterChatPayload; enterChatPayload.append('\0');
            sendPacket(SID_ENTERCHAT, enterChatPayload);
        } else {
            LOG_ERROR(QString("❌ 登录失败 (SRP): 错误码 0x%1").arg(QString::number(status, 16)));
        }
        break;
    }

    case SID_STARTADVEX3:
        LOG_INFO("✅ 房间创建成功！");
        emit gameListRegistered();
        break;

    default:
        break;
    }
}

void BnetConnection::handleAuthCheck(const QByteArray &data)
{
    LOG_INFO("🔍 解析 Auth Challenge (0x51)...");

    if (data.size() < 24) return;

    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 udpToken;
    quint64 mpqFileTime;

    in >> m_logonType >> m_serverToken >> udpToken >> mpqFileTime;

    // === 生成 ClientToken ===
    m_clientToken = QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF;

    LOG_INFO(QString("AuthParams -> Type:%1 ServerToken:0x%2 ClientToken:0x%3")
                 .arg(QString::number(m_logonType), QString::number(m_serverToken, 16), QString::number(m_clientToken, 16)));

    // MPQ 文件名解析
    int offset = 20;
    int strEnd = data.indexOf('\0', offset);
    if (strEnd == -1) return;
    QByteArray mpqFileName = data.mid(offset, strEnd - offset);

    offset = strEnd + 1;
    strEnd = data.indexOf('\0', offset);
    if (strEnd == -1) return;
    QByteArray formulaString = data.mid(offset, strEnd - offset);

    int mpqNumber = extractMPQNumber(mpqFileName.constData());
    if (mpqNumber < 0) return;

    unsigned long checkSum = 0;
    if (QFile::exists(m_war3ExePath)) {
        checkRevisionFlat(
            formulaString.constData(),
            m_war3ExePath.toUtf8().constData(),
            m_stormDllPath.toUtf8().constData(),
            m_gameDllPath.toUtf8().constData(),
            mpqNumber,
            &checkSum
            );
    } else {
        LOG_ERROR("War3.exe 不存在，无法计算哈希");
        return;
    }

    LOG_INFO(QString("✅ 哈希: 0x%1").arg(QString::number(checkSum, 16).toUpper()));

    // === 构造 0x51 响应 ===
    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    quint32 exeVersion = 0x011a0001;

    out << (quint32)m_clientToken;
    out << (quint32)exeVersion;
    out << (quint32)checkSum;
    out << (quint32)1;
    out << (quint32)0;

    // CD-Key
    out << (quint32)20 << (quint32)18 << (quint32)0 << (quint32)0;
    out.writeRawData(QByteArray(20, 0).data(), 20);

    // ExeInfo
    QFileInfo fileInfo(m_war3ExePath);
    if (fileInfo.exists()) {
        const QString fileName = fileInfo.fileName();
        const QString fileSize = QString::number(fileInfo.size());
        const QString fileTime = fileInfo.lastModified().toString("MM/dd/yy HH:mm:ss");
        const QString exeInfoString = QString("%1 %2 %3").arg(fileName, fileTime, fileSize);
        out.writeRawData(exeInfoString.toUtf8().constData(), exeInfoString.length());
        out << (quint8)0;
    } else {
        out.writeRawData("War3.exe 03/18/11 02:00:00 471040\0", 38);
    }

    out.writeRawData(m_user.toUtf8().constData(), m_user.toUtf8().size());
    out << (quint8)0;

    sendPacket(SID_AUTH_CHECK, response);

    LOG_INFO("发起登录请求...");
    sendLoginRequest(m_loginProtocol);
}

void BnetConnection::sendLoginRequest(LoginProtocol protocol)
{
    if (protocol == Protocol_Old_0x29 || protocol == Protocol_Logon2_0x3A) {
        // === 旧版/中期协议 (Double Hash) ===
        LOG_INFO(QString("正在发送 DoubleHash 登录请求 (0x%1)...").arg(QString::number(protocol, 16)));

        QByteArray proof = calculateOldLogonProof(m_pass, m_clientToken, m_serverToken);

        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);

        out << (quint32)m_clientToken;
        out << (quint32)m_serverToken;
        out.writeRawData(proof.data(), 20);
        out.writeRawData(m_user.toUtf8().constData(), m_user.toUtf8().size());
        out << (quint8)0; // String terminator

        // 发送对应的包 ID
        sendPacket(protocol == Protocol_Old_0x29 ? SID_LOGONRESPONSE : SID_LOGONRESPONSE2, payload);
    }
    else if (protocol == Protocol_SRP_0x53) {
        // === SRP 协议 (0x53) ===
        LOG_INFO("正在发送 SRP 登录请求 (0x53)...");

        if (m_nls) delete m_nls;

        // 1. 计算密码的 Broken SHA1 (Big Endian)
        QByteArray passBytes = m_pass.toLower().toUtf8();
        QByteArray passHashBE = calculateBrokenSHA1(passBytes);

        LOG_INFO(QString("SRP Secret (BE): %1").arg(QString(passHashBE.toHex())));

        // 2. 构造 NLS 对象
        std::string usernameInput = m_user.toUpper().trimmed().toUtf8().toStdString();
        std::string passwordInput(passHashBE.constData(), passHashBE.size());

        m_nls = new NLS(usernameInput, passwordInput);

        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);

        QByteArray clientKey(32, 0);
        m_nls->getPublicKey((char*)clientKey.data());

        out.writeRawData(clientKey.data(), 32);
        out.writeRawData(m_user.trimmed().toUtf8().constData(), m_user.length());
        out << (quint8)0;

        sendPacket(SID_AUTH_ACCOUNTLOGON, payload);
    }
}

// === SRP 0x53 处理逻辑 ===
void BnetConnection::handleSRPLoginResponse(const QByteArray &data)
{
    if (data.size() < 68) {
        LOG_ERROR("SRP 响应数据不足");
        return;
    }

    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 status;
    QByteArray salt(32, 0);
    QByteArray serverKey(32, 0);

    in >> status;
    in.readRawData(salt.data(), 32);
    in.readRawData(serverKey.data(), 32);

    if (status != 0) {
        LOG_ERROR("SRP 步骤1 被拒绝，状态码: 0x" + QString::number(status, 16));
        return;
    }

    // 计算 M1 Proof
    QByteArray proof(20, 0);
    m_nls->getClientSessionKey((char*)proof.data(), (char*)salt.data(), (char*)serverKey.data());

    // 发送 0x54
    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out.writeRawData(proof.data(), 20);
    out.writeRawData(QByteArray(20, 0).data(), 20); // M2 占位符 (客户端不验证服务端Proof可填0)

    sendPacket(SID_AUTH_ACCOUNTLOGONPROOF, response);
}

void BnetConnection::createGameOnLadder(const QString &gameName, const QByteArray &mapStatString, quint16 udpPort) {
    LOG_INFO(QString("🚀 请求创建房间: %1").arg(gameName));
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out << (quint32)0x11 << (quint32)0 << (quint16)0x1F << (quint32)0 << (quint32)0;
    out.writeRawData(gameName.toUtf8().constData(), gameName.toUtf8().size());
    out << (quint8)0 << (quint8)0;
    out.writeRawData(mapStatString.constData(), mapStatString.size());
    out << (quint8)0 << (quint16)udpPort;
    sendPacket(SID_STARTADVEX3, payload);
}

QString BnetConnection::getPrimaryIPv4() {
    foreach(const QNetworkInterface &interface, QNetworkInterface::allInterfaces()) {
        if (interface.flags() & QNetworkInterface::IsUp && interface.flags() & QNetworkInterface::IsRunning && !(interface.flags() & QNetworkInterface::IsLoopBack)) {
            foreach(const QNetworkAddressEntry &entry, interface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.ip().toString().startsWith("169.254.") && !entry.ip().toString().startsWith("127."))
                    return entry.ip().toString();
            }
        }
    }
    return QString();
}

quint32 BnetConnection::ipToUint32(const QString &ipAddress) {
    return QHostAddress(ipAddress).toIPv4Address();
}
quint32 BnetConnection::ipToUint32(const QHostAddress &address) {
    return address.toIPv4Address();
}
void BnetConnection::disconnectFromHost() { m_socket->disconnectFromHost(); }
bool BnetConnection::isConnected() const { return m_socket->state() == QAbstractSocket::ConnectedState; }
void BnetConnection::onDisconnected() { LOG_WARNING("🔌 战网连接断开"); }

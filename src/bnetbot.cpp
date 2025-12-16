#include "bncsutil/checkrevision.h"
#include "BnetBot.h"
#include "bnethash.h"
#include "bnetsrp3.h"
#include "logger.h"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDataStream>
#include <QCoreApplication>
#include <QNetworkInterface>
#include <QCryptographicHash>

BnetBot::BnetBot(QObject *parent)
    : QObject(parent)
    , m_loginProtocol(Protocol_Old_0x29)
    , m_srp(nullptr)
{
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &BnetBot::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &BnetBot::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &BnetBot::onDisconnected);

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

BnetBot::~BnetBot()
{
    disconnectFromHost();
    if (m_srp) {
        delete m_srp;
        m_srp = nullptr;
    }
}

void BnetBot::setCredentials(const QString &user, const QString &pass, LoginProtocol protocol)
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

void BnetBot::connectToHost(const QString &address, quint16 port)
{
    m_serverAddr = address;
    m_serverPort = port;
    LOG_INFO(QString("正在建立 TCP 连接至战网: %1:%2").arg(address).arg(port));
    m_socket->connectToHost(address, port);
}

void BnetBot::onConnected()
{
    LOG_INFO("✅ TCP 链路已建立，发送协议握手字节...");
    char protocolByte = 1;
    m_socket->write(&protocolByte, 1);
    sendAuthInfo();
}

void BnetBot::sendPacket(PacketID id, const QByteArray &payload)
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

void BnetBot::sendAuthInfo()
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
QByteArray BnetBot::calculateBrokenSHA1(const QByteArray &data)
{
    t_hash hashOut;
    bnet_hash(&hashOut, data.size(), data.constData());

    QByteArray result;
    QDataStream ds(&result, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    for(int i = 0; i < 5; i++) {
        ds << hashOut[i];
    }
    return result;
}

// === 双重哈希计算 (适用于 0x29 和 0x3A) ===
QByteArray BnetBot::calculateOldLogonProof(const QString &password, quint32 clientToken, quint32 serverToken)
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

void BnetBot::onReadyRead()
{
    while (m_socket->bytesAvailable() > 0) {
        if (m_socket->bytesAvailable() < 4) return;

        QByteArray headerData = m_socket->peek(4);
        if ((quint8)headerData[0] != BNET_HEADER) {
            m_socket->read(1); // 丢弃无效字节
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

void BnetBot::handlePacket(PacketID id, const QByteArray &data)
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
            QByteArray enterChatPayload; enterChatPayload.append('\0');
            sendPacket(SID_ENTERCHAT, enterChatPayload);
        } else {
            LOG_ERROR(QString("❌ 登录失败 (0x29): 错误码 0x%1").arg(QString::number(result, 16)));
        }
        break;
    }

    // === 0x3A 登录响应 ===
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

void BnetBot::handleAuthCheck(const QByteArray &data)
{
    LOG_INFO("🔍 解析 Auth Challenge (0x51)...");

    if (data.size() < 24) return;

    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 udpToken;
    quint64 mpqFileTime;

    in >> m_logonType >> m_serverToken >> udpToken >> mpqFileTime;

    m_clientToken = QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF;

    LOG_INFO(QString("AuthParams -> Type:%1 ServerToken:0x%2 ClientToken:0x%3")
                 .arg(QString::number(m_logonType), QString::number(m_serverToken, 16), QString::number(m_clientToken, 16)));

    // ... (MPQ 和 checkRevision 逻辑保持不变) ...
    // 为节省篇幅，此处省略 MPQ 解析部分，请保持原有的 checkRevisionFlat 调用逻辑不变
    // 假设您原有的代码能正确计算 checkSum

    // 重新获取文件名以计算 hash (这里简化，请确保您原有的 checkRevision 代码被保留)
    int offset = 20;
    int strEnd = data.indexOf('\0', offset);
    QByteArray mpqFileName = data.mid(offset, strEnd - offset);
    offset = strEnd + 1;
    strEnd = data.indexOf('\0', offset);
    QByteArray formulaString = data.mid(offset, strEnd - offset);
    int mpqNumber = extractMPQNumber(mpqFileName.constData());

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
    // ...

    LOG_INFO(QString("✅ 哈希: 0x%1").arg(QString::number(checkSum, 16).toUpper()));

    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    quint32 exeVersion = 0x011a0001; // 1.26a

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

void BnetBot::sendLoginRequest(LoginProtocol protocol)
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
        out << (quint8)0;

        sendPacket(protocol == Protocol_Old_0x29 ? SID_LOGONRESPONSE : SID_LOGONRESPONSE2, payload);
    }
    else if (protocol == Protocol_SRP_0x53) {
        // ============================================================
        // [SRP Step 1] 客户端初始化 & 发送公钥 A
        // ============================================================
        LOG_INFO("正在发送 SRP 登录请求 (0x53)...");

        if (m_srp) delete m_srp;

        // 初始化 SRP 对象 (内部生成随机私钥 a)
        m_srp = new BnetSRP3(m_user, m_pass);

        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);

        // --------------------------------------------------------------------------
        // [SRP Step 1.1] 计算客户端公钥 A = g^a % N
        // --------------------------------------------------------------------------
        BigInt A = m_srp->getClientSessionPublicKey();

        // 记录日志
        LOG_INFO(QString("[SRP Step 1.1] 客户端生成公钥 (A): %1").arg(A.toHexString()));

        // --------------------------------------------------------------------------
        // [SRP Step 1.2] 转换为 32 字节的小端序字节流 (准备发送)
        // --------------------------------------------------------------------------
        QByteArray A_bytes = A.toByteArray(32, 1, false);
        LOG_INFO(QString("[SRP Step 1.2] 公钥 (A) [Raw Bytes]: %1").arg(QString(A_bytes.toHex())));

        out.writeRawData(A_bytes.constData(), 32);
        out.writeRawData(m_user.toLower().trimmed().toUtf8().constData(), m_user.length());
        out << (quint8)0;

        // --------------------------------------------------------------------------
        // [SRP Step 1.3] 发送包 SID_AUTH_ACCOUNTLOGON
        // --------------------------------------------------------------------------
        sendPacket(SID_AUTH_ACCOUNTLOGON, payload);
    }
}

// === SRP 0x53 响应处理 ===
void BnetBot::handleSRPLoginResponse(const QByteArray &data)
{
    // ============================================================
    // [SRP Step 3] 接收服务端公钥 B & 发送证明 M1
    // ============================================================
    if (data.size() < 68) {
        LOG_ERROR("[SRP Step 3] 响应数据不足 (SID_AUTH_ACCOUNTLOGON)");
        return;
    }

    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 status;
    QByteArray saltBytes(32, 0);
    QByteArray serverKeyBytes(32, 0); // Key B

    // 读取数据
    in >> status;
    in.readRawData(saltBytes.data(), 32);
    in.readRawData(serverKeyBytes.data(), 32);

    if (status != 0) {
        LOG_ERROR("[SRP Step 3.1] 被拒绝，状态码: 0x" + QString::number(status, 16));
        return;
    }

    // [SRP Step 3.1] 记录原始字节流
    LOG_INFO(QString("[SRP Step 3.1] 收到服务端 Salt (s) [Raw Bytes]: %1").arg(QString(saltBytes.toHex())));
    LOG_INFO(QString("[SRP Step 3.1] 收到服务端公钥 (B) [Raw Bytes]: %1").arg(QString(serverKeyBytes.toHex())));

    if (!m_srp) {
        LOG_ERROR("SRP 对象未初始化！");
        return;
    }

    // === 计算 Proof (M1) ===

    // --------------------------------------------------------------------------
    // [SRP Step 3.2] 设置 Salt
    // 服务端使用 blockSize=4 来加载 Salt，我们也必须用 4
    // --------------------------------------------------------------------------
    BigInt saltVal((const unsigned char*)saltBytes.constData(), 32, 4, false);
    LOG_INFO(QString("[SRP Step 3.2] Salt 转换为 BigInt: %1").arg(saltVal.toHexString()));
    m_srp->setSalt(saltVal);

    // --------------------------------------------------------------------------
    // [SRP Step 3.3] 转换服务端公钥 B
    // 使用 1 直接读取 LE 流即可还原正确的 B
    // 服务端发送的是 LE 流，客户端 bigInt(..., 4) 会导致错误的翻转。
    // --------------------------------------------------------------------------
    BigInt B_val((const unsigned char*)serverKeyBytes.constData(), 32, 1, false);
    LOG_INFO(QString("[SRP Step 3.3] B 转换为 BigInt:    %1").arg(B_val.toHexString()));

    // --------------------------------------------------------------------------
    // [SRP Step 3.4] 计算会话密钥 K = Hash(S)
    // --------------------------------------------------------------------------
    // 这一步内部会计算: x = H(s, H(P)), u = H(B), S = (B - g^x)^(a + ux)
    // 务必确保 bnetsrp3.cpp 中 getClientPrivateKey 的 x 构造使用了 blockSize=1
    BigInt K = m_srp->getHashedClientSecret(B_val);

    // 记录 K 以便调试 (K 对了，说明 x, u, S 都对了)
    LOG_INFO(QString("[SRP Step 3.4] 计算出的会话密钥 (K): %1").arg(K.toHexString()));

    // --------------------------------------------------------------------------
    // [SRP Step 3.x] 获取本地公钥 A
    // --------------------------------------------------------------------------
    // 获取 A (用于 Proof 计算)
    BigInt A = m_srp->getClientSessionPublicKey();
    LOG_INFO(QString("[SRP Step 3.x] 本地公钥 (A):       %1").arg(A.toHexString()));

    // --------------------------------------------------------------------------
    // [SRP Step 3.5] 计算客户端证明 M1 = H(I, H(U), s, A, B, K)
    // --------------------------------------------------------------------------
    BigInt M1 = m_srp->getClientPasswordProof(A, B_val, K);

    // 将 Proof 转换为 20 字节的数据
    QByteArray proofBytes = M1.toByteArray(20, 1, false);

    LOG_INFO(QString("[SRP Step 3.5] 计算出的 Proof (M1): %1").arg(QString(proofBytes.toHex())));

    // === 发送 SID_AUTH_ACCOUNTLOGONPROOF (0x54) ===
    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // [SRP Step 3.6] 发送 M1 给服务端进行验证
    out.writeRawData(proofBytes.constData(), 20);
    out.writeRawData(QByteArray(20, 0).data(), 20); // M2 verification space

    sendPacket(SID_AUTH_ACCOUNTLOGONPROOF, response);
}

void BnetBot::createGameOnLadder(const QString &gameName, const QByteArray &mapStatString, quint16 udpPort) {
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

QString BnetBot::getPrimaryIPv4() {
    // ... (保持不变)
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

quint32 BnetBot::ipToUint32(const QString &ipAddress) {
    return QHostAddress(ipAddress).toIPv4Address();
}
quint32 BnetBot::ipToUint32(const QHostAddress &address) {
    return address.toIPv4Address();
}
void BnetBot::disconnectFromHost() { m_socket->disconnectFromHost(); }
bool BnetBot::isConnected() const { return m_socket->state() == QAbstractSocket::ConnectedState; }
void BnetBot::onDisconnected() { LOG_WARNING("🔌 战网连接断开"); }

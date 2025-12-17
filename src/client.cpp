#include "bncsutil/checkrevision.h"
#include "bnethash.h"
#include "bnetsrp3.h"
#include "client.h"
#include "logger.h"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDataStream>
#include <QRandomGenerator>
#include <QCoreApplication>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QCryptographicHash>

Client::Client(QObject *parent)
    : QObject(parent)
    , m_loginProtocol(Protocol_Old_0x29)
    , m_srp(nullptr)
    , m_tcpSocket(nullptr)
    , m_udpSocket(nullptr)
{
    m_tcpSocket = new QTcpSocket(this);
    m_udpSocket = new QUdpSocket(this);

    connect(m_tcpSocket, &QTcpSocket::connected, this, &Client::onConnected);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &Client::onTcpReadyRead);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &Client::onUdpReadyRead);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &Client::onDisconnected);

    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError){
        LOG_ERROR(QString("战网连接错误: %1").arg(m_tcpSocket->errorString()));
    });

    if (m_udpSocket->bind(QHostAddress::AnyIPv4, 6112)) {
        LOG_INFO("✅ UDP 监听启动成功: 0.0.0.0:6112");
    } else {
        LOG_ERROR(QString("❌ UDP 6112 绑定失败: %1").arg(m_udpSocket->errorString()));
    }

    QString appDir = QCoreApplication::applicationDirPath();
    QDir dir(appDir);

    // 路径处理逻辑
    if (dir.cd("war3files")) {
        m_war3ExePath = dir.absoluteFilePath("War3.exe");
        m_gameDllPath = dir.absoluteFilePath("Game.dll");
        m_stormDllPath = dir.absoluteFilePath("Storm.dll");
        m_dota683dPath = dir.absoluteFilePath("maps/DotA v6.83d.w3x");
    } else {
        LOG_WARNING("找不到 war3files 目录，尝试直接读取当前目录下的 War3.exe");
        dir.setPath(appDir);
        m_war3ExePath = dir.absoluteFilePath("War3.exe");
        m_gameDllPath = dir.absoluteFilePath("Game.dll");
        m_stormDllPath = dir.absoluteFilePath("Storm.dll");
        m_dota683dPath = dir.absoluteFilePath("maps/DotA v6.83d.w3x");
    }

    LOG_INFO(QString("War3 路径: %1").arg(m_war3ExePath));
}

Client::~Client()
{
    disconnectFromHost();
    if (m_srp) {
        delete m_srp;
        m_srp = nullptr;
    }
    // 清理 TCP
    if (m_tcpSocket) {
        m_tcpSocket->close();
        delete m_tcpSocket;
    }
    // 清理 UDP
    if (m_udpSocket) {
        m_udpSocket->close();
        delete m_udpSocket;
    }
}

void Client::setCredentials(const QString &user, const QString &pass, LoginProtocol protocol)
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

void Client::connectToHost(const QString &address, quint16 port)
{
    m_serverAddr = address;
    m_serverPort = port;
    LOG_INFO(QString("正在建立 TCP 连接至战网: %1:%2").arg(address).arg(port));
    m_tcpSocket->connectToHost(address, port);
}

void Client::onConnected()
{
    LOG_INFO("✅ TCP 链路已建立，发送协议握手字节...");
    char protocolByte = 1;
    m_tcpSocket->write(&protocolByte, 1);
    sendAuthInfo();
}

void Client::sendPacket(PacketID id, const QByteArray &payload)
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

    m_tcpSocket->write(packet);

    QString hexStr = packet.toHex().toUpper();
    for(int i = 2; i < hexStr.length(); i += 3) hexStr.insert(i, " ");
    LOG_INFO(QString("📤 发送包 ID: 0x%1 Len:%2 Data: %3")
                 .arg(QString::number(id, 16))
                 .arg(packet.size())
                 .arg(hexStr));
}

void Client::sendAuthInfo()
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
QByteArray Client::calculateBrokenSHA1(const QByteArray &data)
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
QByteArray Client::calculateOldLogonProof(const QString &password, quint32 clientToken, quint32 serverToken)
{
    // 1. Broken SHA1 (Output: BE)
    QByteArray passBytes = password.toLatin1();
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

void Client::onTcpReadyRead()
{
    while (m_tcpSocket->bytesAvailable() > 0) {
        if (m_tcpSocket->bytesAvailable() < 4) return;

        QByteArray headerData = m_tcpSocket->peek(4);
        if ((quint8)headerData[0] != BNET_HEADER) {
            m_tcpSocket->read(1); // 丢弃无效字节
            continue;
        }

        quint16 length;
        QDataStream lenStream(headerData.mid(2, 2));
        lenStream.setByteOrder(QDataStream::LittleEndian);
        lenStream >> length;

        if (m_tcpSocket->bytesAvailable() < length) return;

        QByteArray packetData = m_tcpSocket->read(length);
        quint8 packetIdVal = (quint8)packetData[1];
        handlePacket((PacketID)packetIdVal, packetData.mid(4));
    }
}

void Client::onUdpReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        QByteArray data = datagram.data();
        LOG_INFO(QString("📨 [UDP] 收到 %1 字节来自 %2:%3")
                     .arg(data.size())
                     .arg(datagram.senderAddress().toString())
                     .arg(datagram.senderPort()));
    }
}

void Client::handlePacket(PacketID id, const QByteArray &data)
{
    LOG_INFO(QString("📥 收到包 ID: 0x%1").arg(QString::number(id, 16)));

    switch (id) {
    case SID_PING:
    {
        sendPacket(SID_PING, data);
        break;
    }
    case SID_ENTERCHAT:
    {
        LOG_INFO("✅ 已成功进入聊天环境 (Unique Name Received)");
        queryChannelList();
        break;
    }
    case SID_GETCHANNELLIST:
    {
        LOG_INFO("📦 收到频道列表包，正在解析...");

        m_channelList.clear();
        int offset = 0;

        // 遍历 Payload，提取所有以 \0 结尾的字符串
        while (offset < data.size()) {
            int strEnd = data.indexOf('\0', offset);
            if (strEnd == -1) break; // 防止数据不完整

            QByteArray rawStr = data.mid(offset, strEnd - offset);
            QString channelName = QString::fromUtf8(rawStr);

            // 列表通常以一个空字符串结尾，我们跳过空名
            if (!channelName.isEmpty()) {
                m_channelList.append(channelName);
            }

            offset = strEnd + 1; // 移动到下一个字符串开头
        }

        if (m_channelList.isEmpty()) {
            LOG_WARNING("⚠️ 服务器返回的频道列表为空！尝试加入默认频道 'Waiting Players'");
            joinChannel("Waiting Players");
        } else {
            LOG_INFO(QString("📋 获取到 %1 个频道: %2").arg(m_channelList.size()).arg(m_channelList.join(", ")));

            // 默认进入第一个频道
            QString targetChannel = m_channelList.first();
            LOG_INFO(QString("自动加入第一个频道: %1").arg(targetChannel));
            joinChannel(targetChannel);
        }
        break;
    }
    case SID_CHATEVENT:
    {
        if (data.size() < 24) return;

        QDataStream in(data);
        in.setByteOrder(QDataStream::LittleEndian);

        quint32 eventId;
        quint32 flags;
        quint32 ping;
        quint32 ipAddress;
        quint32 accountNum;
        quint32 regAuthority;

        // 1. 读取完整的头部 (24字节)
        in >> eventId >> flags >> ping >> ipAddress >> accountNum >> regAuthority;

        // 2. 解析两个字符串：Username 和 Text
        int currentOffset = 24;
        auto readString = [&](int &offset) -> QString {
            if (offset >= data.size()) return QString();
            int end = data.indexOf('\0', offset);
            if (end == -1) return QString(); // 数据包不完整

            QString s = QString::fromUtf8(data.mid(offset, end - offset));
            offset = end + 1; // 移动到 \0 之后
            return s;
        };

        QString username = readString(currentOffset);
        QString text = readString(currentOffset);

        // 3. 根据 EventID 处理逻辑
        switch (eventId) {
        case 0x01: // EID_SHOWUSER (进入频道时显示的已存在用户)
            LOG_INFO(QString("👤 [频道用户] %1 (Ping: %2)").arg(username).arg(ping));
            break;

        case 0x02: // EID_JOIN (有人加入)
            LOG_INFO(QString("➡️ %1 加入了频道").arg(username));
            break;

        case 0x03: // EID_LEAVE (有人离开)
            LOG_INFO(QString("⬅️ %1 离开了频道").arg(username));
            break;

        case 0x04: // EID_WHISPER (收到私聊)
            LOG_INFO(QString("📩 [%1] 悄悄对你说: %2").arg(username, text));
            // TODO: 在这里处理 Bot 指令，例如 "admin: !host dota"
            break;

        case 0x05: // EID_TALK (公共聊天)
            LOG_INFO(QString("💬 [%1]: %2").arg(username, text));
            break;

        case 0x06: // EID_BROADCAST (全服广播)
            LOG_INFO(QString("📢 [广播]: %1").arg(text));
            break;

        case 0x07: // EID_CHANNEL (自己加入了某频道)
            // 对于此事件，'text' 字段存储的是频道名称
            LOG_INFO(QString("🏠 已加入频道: [%1]").arg(text));
            // 创建房间
            createGameOnLadder("fast 1k~2k", "", 6112, GameType::GameType_UMS);
            break;

        case 0x09: // EID_USERFLAGS (用户权限/图标变更)
            LOG_INFO(QString("🔧 %1 更新了状态 (Flags: %2)").arg(username, QString::number(flags, 16)));
            break;

        case 0x0A: // EID_WHISPERSENT (自己发送的私聊确认)
            LOG_INFO(QString("📤 你对 [%1] 说: %2").arg(username, text));
            break;

        case 0x0D: // EID_CHANNELFULL
            LOG_WARNING("⚠️ 无法加入频道：频道已满");
            break;

        case 0x0E: // EID_CHANNELDOESNOTEXIST
            LOG_WARNING("⚠️ 无法加入频道：频道不存在");
            break;

        case 0x0F: // EID_CHANNELRESTRICTED
            LOG_WARNING("⚠️ 无法加入频道：权限受限 (需要更高权限)");
            break;

        case 0x12: // EID_INFO (系统信息)
            LOG_INFO(QString("ℹ️ [系统]: %1").arg(text));
            break;

        case 0x13: // EID_ERROR (错误信息)
            LOG_ERROR(QString("❌ [错误]: %1").arg(text));
            break;

        case 0x17: // EID_EMOTE (表情动作 /me)
            LOG_INFO(QString("✨ %1 %2").arg(username, text));
            break;

        default:
            LOG_INFO(QString("📦 未知聊天事件 ID: 0x%1 | User: %2 | Text: %3")
                         .arg(QString::number(eventId, 16), username, text));
            break;
        }
        break;
    }
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
            QByteArray enterChatPayload;
            enterChatPayload.append('\0');
            enterChatPayload.append('\0');
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
            QByteArray enterChatPayload;
            enterChatPayload.append('\0');
            enterChatPayload.append('\0');
            sendPacket(SID_ENTERCHAT, enterChatPayload);
        } else {
            LOG_ERROR(QString("❌ 登录失败 (0x3A): 错误码 0x%1").arg(QString::number(result, 16)));
        }
        break;
    }

    case SID_AUTH_INFO:
    case SID_AUTH_CHECK:
    {
        if (data.size() > 16) handleAuthCheck(data);
        break;
    }
    case SID_AUTH_ACCOUNTCREATE:
    {
        if (data.size() < 4) return;
        quint32 status;
        QDataStream ds(data);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds >> status;

        if (status == 0) {
            LOG_INFO("🎉 账号注册成功！");
            emit accountCreated();
            // 注册成功后，立即尝试登录
            LOG_INFO("🔄 正在自动登录新注册的账号...");
            sendLoginRequest(Protocol_SRP_0x53);
        }
        else if (status == 0x04) {
            LOG_WARNING("⚠️ 账号已存在 (Status: 0x04)");
            // 如果账号已存在，尝试直接登录
            LOG_INFO("🔄 尝试直接登录...");
            sendLoginRequest(Protocol_SRP_0x53);
        }
        else {
            // 其他错误码 (如包含非法字符等)
            LOG_ERROR(QString("❌ 注册失败: 错误码 0x%1").arg(QString::number(status, 16)));
        }
        break;
    }

    // === SRP 步骤 1 响应 ===
    case SID_AUTH_ACCOUNTLOGON:
    {
        if (m_loginProtocol == Protocol_SRP_0x53) {
            handleSRPLoginResponse(data);
        }
        break;
    }
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
            QByteArray enterChatPayload;
            enterChatPayload.append('\0');
            enterChatPayload.append('\0');
            sendPacket(SID_ENTERCHAT, enterChatPayload);
        } else {
            LOG_ERROR(QString("❌ 登录失败 (SRP): 错误码 0x%1").arg(QString::number(status, 16)));
        }
        break;
    }

    case SID_STARTADVEX3:
    {
        LOG_INFO("✅ 房间创建成功！");
        emit gameListRegistered();
        break;
    }
    default:
        break;
    }
}

void Client::handleAuthCheck(const QByteArray &data)
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

void Client::sendLoginRequest(LoginProtocol protocol)
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
        // [SRP 步骤 1] 客户端初始化 & 发送公钥 A
        // ============================================================
        LOG_INFO("正在发送 SRP 登录请求 (0x53)...");

        if (m_srp) delete m_srp;

        // 初始化 SRP 对象 (内部生成随机私钥 a)
        m_srp = new BnetSRP3(m_user, m_pass);

        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);

        // --------------------------------------------------------------------------
        // [SRP 步骤 1.1] 计算客户端公钥 A = g^a % N
        // --------------------------------------------------------------------------
        BigInt A = m_srp->getClientSessionPublicKey();

        // --------------------------------------------------------------------------
        // [SRP 步骤 1.2] 转换为 32 字节的小端序字节流 (准备发送)
        // --------------------------------------------------------------------------
        QByteArray A_bytes = A.toByteArray(32, 1, false);
        LOG_INFO(QString("[登录调试] 客户端发送 A (原始数据/Raw): %1").arg(QString(A_bytes.toHex())));

        out.writeRawData(A_bytes.constData(), 32);
        out.writeRawData(m_user.trimmed().toUtf8().constData(), m_user.length());
        out << (quint8)0;

        // --------------------------------------------------------------------------
        // [SRP 步骤 1.3] 发送包 SID_AUTH_ACCOUNTLOGON
        // --------------------------------------------------------------------------
        sendPacket(SID_AUTH_ACCOUNTLOGON, payload);
    }
}

// === SRP 0x53 响应处理 ===
void Client::handleSRPLoginResponse(const QByteArray &data)
{
    // ============================================================
    // [SRP 步骤 3] 接收服务端公钥 B & 发送证明 M1
    // ============================================================
    if (data.size() < 68) {
        LOG_ERROR("[SRP 步骤 3] 响应数据不足 (SID_AUTH_ACCOUNTLOGON)");
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
        if (status == 0x01) {
            // 0x01 = 账号不存在 (SERVER_LOGINREPLY_W3_MESSAGE_BADACCT)
            LOG_WARNING(QString("⚠️ 账号 %1 不存在，正在自动发起注册...").arg(m_user));

            // 触发注册流程 (0x52)
            createAccount();
        }
        else if (status == 0x05) {
            // 0x05 = 密码错误 (SERVER_LOGINREPLY_W3_MESSAGE_BADPASS)
            LOG_ERROR(QString("❌ 登录失败: 密码错误 (User: %1)").arg(m_user));
        }
        else {
            // 其他错误 (如封禁等)
            LOG_ERROR("[SRP 步骤 3.1] 被拒绝，状态码: 0x" + QString::number(status, 16));
        }
        return;
    }

    LOG_INFO(QString("[登录调试] 收到服务端 Salt (原始数据/Raw): %1").arg(QString(saltBytes.toHex())));
    LOG_INFO(QString("[登录调试] 收到服务端 B (原始数据/Raw):    %1").arg(QString(serverKeyBytes.toHex())));

    if (!m_srp) {
        LOG_ERROR("SRP 对象未初始化！");
        return;
    }

    // === 计算 Proof (M1) ===

    // --------------------------------------------------------------------------
    // [SRP 步骤 3.2] 设置 Salt
    // --------------------------------------------------------------------------
    BigInt saltVal((const unsigned char*)saltBytes.constData(), 32, 4, false);
    LOG_INFO(QString("[登录调试] Salt 转为 BigInt: %1").arg(saltVal.toHexString()));

    m_srp->setSalt(saltVal);

    // --------------------------------------------------------------------------
    // [SRP 步骤 3.3] 转换服务端公钥 B
    // --------------------------------------------------------------------------
    BigInt B_val((const unsigned char*)serverKeyBytes.constData(), 32, 1, false);
    LOG_INFO(QString("[登录调试] 服务端 B 转为 BigInt: %1").arg(B_val.toHexString()));

    // --------------------------------------------------------------------------
    // [SRP 步骤 3.4] 计算会话密钥 K = Hash(S)
    // --------------------------------------------------------------------------
    // 这一步内部会计算: x = H(s, H(P)), u = H(B), S = (B - g^x)^(a + ux)
    BigInt K = m_srp->getHashedClientSecret(B_val);
    LOG_INFO(QString("[登录调试] 计算出的会话密钥 K: %1").arg(K.toHexString()));

    // --------------------------------------------------------------------------
    // [SRP 步骤 3.x] 获取本地公钥 A
    // --------------------------------------------------------------------------
    BigInt A = m_srp->getClientSessionPublicKey();

    // --------------------------------------------------------------------------
    // [SRP 步骤 3.5] 计算客户端证明 M1 = H(I, H(U), s, A, B, K)
    // --------------------------------------------------------------------------
    BigInt M1 = m_srp->getClientPasswordProof(A, B_val, K);
    LOG_INFO(QString("[登录调试] 计算出的证明 M1:    %1").arg(M1.toHexString()));

    // 将 Proof 转换为 20 字节的数据
    QByteArray proofBytes = M1.toByteArray(20, 1, false);
    LOG_INFO(QString("[验证调试] 客户端发送 M1:      %1").arg(QString(proofBytes.toHex())));

    // === 发送 SID_AUTH_ACCOUNTLOGONPROOF (0x54) ===
    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // [SRP 步骤 3.6] 发送 M1 给服务端进行验证
    out.writeRawData(proofBytes.constData(), 20);
    out.writeRawData(QByteArray(20, 0).data(), 20); // M2 verification space

    sendPacket(SID_AUTH_ACCOUNTLOGONPROOF, response);
}

void Client::createGameOnLadder(const QString &gameName, const QString &password, quint16 udpPort, GameType gameType)
{
    if (m_udpSocket->localPort() != udpPort) {
        m_udpSocket->close();
        if (m_udpSocket->bind(QHostAddress::AnyIPv4, udpPort)) {
            LOG_INFO(QString("✅ UDP 端口切换至 %1 成功").arg(udpPort));
        } else {
            LOG_ERROR(QString("❌ UDP 端口切换至 %1 失败！房间将不可见。").arg(udpPort));
        }
    }

    LOG_INFO(QString("🚀 请求广播房间: [%1] (Port: %2)").arg(gameName).arg(udpPort));

    if (m_war3Map.load(m_dota683dPath)) {

        // 生成 StatString
        QByteArray mapStatString = m_war3Map.getEncodedStatString(m_user);

        if (mapStatString.isEmpty()) {
            LOG_ERROR("❌ 无法创建房间：MapStatString 生成为空！");
            return;
        }

        LOG_INFO(QString("🗺️ MapStatString 生成完毕 (Size: %1)").arg(mapStatString.size()));
        LOG_INFO(QString("🗺️ MapStatString Hex: %1").arg(QString(mapStatString.toHex().toUpper())));

        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);

        // 1. (UINT32) Game State
        quint32 state = 0x00000000;
        if (!password.isEmpty()) {
            state |= 0x01; // Private
        }
        out << state;

        // 2. (UINT32) Game Elapsed Time (创建时为 0)
        out << (quint32)0;

        // 3. (UINT16) Game Type (0x0A = UMS/DotA)
        out << (quint16)gameType;

        // 4. (UINT16) Sub Game Type (通常为 0x01)
        out << (quint16)0x01;

        // 5. (UINT32) Provider Version Constant (War3 = 0xFFFFFFFF)
        out << (quint32)0xFFFFFFFF;

        // 6. (UINT32) Ladder Type (0 = Non-Ladder/Custom)
        out << (quint32)0;

        // 7. (STRING) Game Name
        out.writeRawData(gameName.toUtf8().constData(), gameName.toUtf8().size());
        out << (quint8)0; // Null Terminator

        // 8. (STRING) Game Password
        out.writeRawData(password.toUtf8().constData(), password.toUtf8().size());
        out << (quint8)0; // Null Terminator

        // 9. (STRING) Game Statstring
        out.writeRawData(mapStatString.constData(), mapStatString.size());
        out << (quint8)0; // Null Terminator

        // 发送包 SID_STARTADVEX3 (0x1C)
        sendPacket(SID_STARTADVEX3, payload);
        LOG_INFO("📤 房间创建请求已发送，等待 UDP 握手...");
    } else {
        LOG_ERROR(QString("❌ 地图加载失败: %1").arg(m_dota683dPath));
    }
}

void Client::createAccount()
{
    LOG_INFO("📝 正在发起账号注册 (Legacy Plaintext Mode 0x52)...");

    if (m_user.isEmpty() || m_pass.isEmpty()) {
        LOG_ERROR("注册失败: 用户名或密码为空");
        return;
    }

    // ---------------------------------------------------------
    // 1. 准备 Salt (32字节)
    // ---------------------------------------------------------
    // 在发送明文密码模式下，服务端会忽略客户端发来的 Salt，并自己重新生成。
    // 但为了保持数据包格式正确，我们填充 32 字节的随机数。
    QByteArray s_bytes;
    s_bytes.resize(32);
    for (int i = 0; i < 32; ++i) {
        s_bytes[i] = (char)(QRandomGenerator::global()->generate() & 0xFF);
    }

    // ---------------------------------------------------------
    // 2. 准备 Verifier 字段 (32字节，存放明文密码)
    // ---------------------------------------------------------
    // 官方客户端行为：直接把密码字符串拷贝进去，剩余补 0。
    // 服务端检测到这是可打印字符后，会自动计算 SC Hash 和 SRP Verifier。
    QByteArray v_bytes;
    v_bytes.resize(32);
    v_bytes.fill(0); // 必须初始化为 0，这很重要！

    // 获取密码字节 (通常使用 Latin1 或 UTF8)
    QByteArray passRaw = m_pass.toLatin1(); // 官方客户端通常使用非 Unicode 编码发送密码

    // 截断密码以防溢出 (虽然密码很少超过 32 字符)
    int copyLen = qMin(passRaw.size(), 32);

    // 将密码复制到 buffer 头部
    memcpy(v_bytes.data(), passRaw.constData(), copyLen);

    LOG_INFO(QString("[Register] Salt (Random): %1").arg(QString(s_bytes.toHex())));
    LOG_INFO(QString("[Register] Verifier (Plaintext): %1 (Hex: %2)").arg(m_pass, QString(v_bytes.toHex())));

    // ---------------------------------------------------------
    // 3. 构造数据包
    // ---------------------------------------------------------
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out.writeRawData(s_bytes.constData(), 32); // 发送随机 Salt
    out.writeRawData(v_bytes.constData(), 32); // 发送明文密码
    out.writeRawData(m_user.toLower().trimmed().toLatin1().constData(), m_user.length());
    out << (quint8)0; // 字符串结束符

    // 4. 发送
    sendPacket(SID_AUTH_ACCOUNTCREATE, payload);
}

void Client::queryChannelList()
{
    LOG_INFO("📜 正在请求服务器频道列表...");
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out << (quint32)0;

    sendPacket(SID_GETCHANNELLIST, payload);
}

void Client::joinChannel(const QString &channelName)
{
    if (channelName.isEmpty()) return;

    LOG_INFO(QString("💬 请求加入频道: %1").arg(channelName));

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // Flags: 0x01 = First Join / Force Create
    out << (quint32)0x01;
    out.writeRawData(channelName.toUtf8().constData(), channelName.toUtf8().size());
    out << (quint8)0; // 字符串结尾

    sendPacket(SID_JOINCHANNEL, payload);
}

QString Client::getPrimaryIPv4() {
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

quint32 Client::ipToUint32(const QString &ipAddress) {
    return QHostAddress(ipAddress).toIPv4Address();
}
quint32 Client::ipToUint32(const QHostAddress &address) {
    return address.toIPv4Address();
}
void Client::disconnectFromHost() { m_tcpSocket->disconnectFromHost(); }
bool Client::isConnected() const { return m_tcpSocket->state() == QAbstractSocket::ConnectedState; }
void Client::onDisconnected() { LOG_WARNING("🔌 战网连接断开"); }

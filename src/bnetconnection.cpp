#include "bnetconnection.h"
#include "logger.h"
#include <QDataStream>

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

    m_war3ExePath = "Warcraft III.exe";
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
    LOG_DEBUG(QString("📤 发送战网包 ID: 0x%1").arg(QString::number(id, 16)));
}

void BnetConnection::sendAuthInfo()
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out << (quint32)0;                   // Protocol ID
    out.writeRawData("IX86", 4);         // Platform
    out.writeRawData("W3XP", 4);         // Product (TFT)
    out << (quint32)26;                  // Version
    out << (quint32)0;                   // Language
    out << (quint32)0;                   // Local IP
    out << (quint32)0;                   // Timezone
    out << (quint32)0x409;               // Locale ID
    out << (quint32)0x409;               // Lang ID
    out.writeRawData("USA", 3);          // Country
    out.writeRawData("\0", 1);
    out.writeRawData("United States", 13);
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
    while (m_socket->bytesAvailable() > 0) {
        QByteArray headerData = m_socket->peek(4);
        if (headerData.size() < 4) return;

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
    switch (id) {
    case SID_PING:
        sendPacket(SID_PING, data);
        break;

    case SID_AUTH_CHECK:
        handleAuthCheck(data);
        break;

    case SID_AUTH_INFO:
        LOG_INFO("✅ 版本验证通过 (AuthInfo)，准备发送账号密码...");
        // 注意：有些服务器在 AuthInfo 后直接 Login，有些需要等 AuthCheck
        // 这里假设服务器会先发 AuthCheck。如果服务器不发 Check 直接登录，
        // 你可以在这里调用 sendLoginRequest()。
        // 标准流程通常是：AuthInfo -> Server sends AuthCheck -> Client sends AuthCheckResponse -> Login
        break;

    case SID_AUTH_ACCOUNTLOGON:
        handleLoginResponse(data);
        break;

    case SID_AUTH_ACCOUNTLOGONPROOF:
        if (data.size() >= 4 && data[0] == 0) {
            LOG_INFO("🎉 战网登录成功！");
            emit authenticated();
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
        break;
    }
}

void BnetConnection::sendLoginRequest()
{
    if (m_nls) {
        delete m_nls;
    }
    // 使用真实类，不再是伪代码
    m_nls = new NLS(m_user.toStdString(), m_pass.toStdString());

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    QByteArray clientKey(32, 0);
    // 这里不再报错，因为 m_nls 已经是 NLS* 类型
    m_nls->getPublicKey((char*)clientKey.data());

    out << (quint32)clientKey.size();
    out.writeRawData(clientKey.data(), clientKey.size());
    out.writeRawData(m_user.toUtf8().constData(), m_user.toUtf8().size());
    out << (quint8)0;

    sendPacket(SID_LOGONRESPONSE2, payload); // 0x3A
    LOG_INFO(QString("正在尝试登录账号: %1").arg(m_user));
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

    // 4. [C API] 计算 M1 (Proof)
    // 参数顺序：nls对象, ServerKey(B), Salt, OutputBuffer
    nls_get_M1((nls_t*)m_nls, serverKey.data(), salt.data(), proof.data());

    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out.writeRawData(proof.data(), 20);
    out.writeRawData(QByteArray(20, 0).data(), 20); // M2 (Client sends empty/zeros usually)

    sendPacket(SID_AUTH_ACCOUNTLOGONPROOF, response);
}

void BnetConnection::handleAuthCheck(const QByteArray &data)
{
    LOG_INFO("🔍 收到 AuthCheck，正在计算版本哈希...");

    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 clientToken;
    quint32 udpToken;
    quint32 mpqFileTime;
    quint32 mpqFileNameLen;
    in >> clientToken >> udpToken >> mpqFileTime >> mpqFileNameLen;

    QByteArray mpqFileName = data.mid(16, mpqFileNameLen);
    // formulaString 紧跟在文件名之后
    QByteArray formulaString = data.mid(16 + mpqFileNameLen + 1);

    unsigned long exeVersion = 0;
    unsigned long exeHash = 0;

    // === 关键修复：参数类型匹配 ===
    // 1. 使用 toStdString().c_str() 获取 const char* (但注意要在表达式内使用)
    // 2. 使用 formulaString.constData() 替代 .data()，确保它是 const char*
    // 3. 确保你的 Warcraft III.exe 等文件确实存在于 m_war3ExePath 目录下，否则返回 0

    std::string exePathStr = m_war3ExePath.toStdString();

    int res = checkRevisionFlat(
        exePathStr.c_str(),           // value1: exe path
        "Storm.dll",                  // value2
        "Game.dll",                   // value3
        formulaString.constData(),    // value4: formula (const char*)
        exeVersion,                  // result1
        &exeHash                      // result2
        );

    if (res != 0 && res != 1) {
        LOG_WARNING(QString("checkRevisionFlat 可能失败或文件未找到，返回值: %1").arg(res));
    } else {
        LOG_INFO(QString("哈希计算成功: Ver=%1 Hash=%2").arg(exeVersion).arg(exeHash));
    }

    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    out << (quint32)exeVersion;
    out << (quint32)exeHash;
    out << (quint32)0; // Result info
    out.writeRawData("war3", 4);

    sendPacket(SID_AUTH_CHECK, response);

    // AuthCheck 发送后，通常紧接着就是登录请求
    sendLoginRequest();
}

void BnetConnection::disconnectFromHost() { m_socket->disconnectFromHost(); }
bool BnetConnection::isConnected() const { return m_socket->state() == QAbstractSocket::ConnectedState; }
void BnetConnection::onDisconnected() { LOG_WARNING("🔌 战网连接断开"); }

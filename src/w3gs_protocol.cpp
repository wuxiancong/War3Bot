#include "w3gs_protocol.h"
#include <QDataStream>
#include <QDebug>
#include <winsock2.h>

W3GSProtocol::W3GSProtocol(QObject *parent)
    : QObject(parent)
    , m_protocolVersion(0xF7)
{
}

bool W3GSProtocol::parsePacket(const QByteArray &data, W3GSHeader &header)
{
    if (static_cast<size_t>(data.size()) < sizeof(W3GSHeader)) {
        return false;
    }

    const unsigned char* rawData = reinterpret_cast<const unsigned char*>(data.constData());
    header.protocol = rawData[0];
    header.size = static_cast<uint16_t>(rawData[1]) | (static_cast<uint16_t>(rawData[2]) << 8);
    header.type = static_cast<uint16_t>(rawData[3]) | (static_cast<uint16_t>(rawData[4]) << 8);
    header.unknown = rawData[5];

    return true;
}

QByteArray W3GSProtocol::buildPacket(uint16_t type, const QByteArray &payload)
{
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << static_cast<uint8_t>(m_protocolVersion);
    stream << static_cast<uint16_t>(payload.size() + 5);
    stream << type;
    stream << static_cast<uint8_t>(0);

    packet.append(payload);
    return packet;
}

void W3GSProtocol::writeString(QDataStream &stream, const QString &str)
{
    QByteArray utf8 = str.toUtf8();
    stream << static_cast<uint8_t>(utf8.size());
    stream.writeRawData(utf8.constData(), utf8.size());
}

QString W3GSProtocol::readString(QDataStream &stream)
{
    uint8_t length;
    stream >> length;

    QByteArray data;
    data.resize(length);
    stream.readRawData(data.data(), length);

    return QString::fromUtf8(data);
}

bool W3GSProtocol::parseSlotInfoJoin(const QByteArray &data, PlayerInfo &player)
{
    if (data.size() < 10) return false;

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    uint8_t playerId;
    stream >> playerId;
    player.playerId = playerId;

    player.name = readString(stream);

    uint32_t internalIP;
    stream >> internalIP;
    player.address = QHostAddress(ntohl(internalIP));

    stream >> player.port;

    return stream.status() == QDataStream::Ok;
}

bool W3GSProtocol::parseChatToHost(const QByteArray &data, uint8_t &fromPlayer, uint8_t &toPlayer, QString &message)
{
    if (data.size() < 3) return false;

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream >> fromPlayer;
    stream >> toPlayer;
    message = readString(stream);

    return stream.status() == QDataStream::Ok;
}

bool W3GSProtocol::parseIncomingAction(const QByteArray &data, QByteArray &actions)
{
    if (data.size() < 5) return false;

    actions = data.mid(5);
    return true;
}

bool W3GSProtocol::parseMapCheck(const QByteArray &data, QByteArray &mapData)
{
    if (data.size() < 4) return false;

    mapData = data;
    return true;
}

QByteArray W3GSProtocol::createPingPacket()
{
    return buildPacket(C_TO_S_PING_FROM_HOST, QByteArray());
}

QByteArray W3GSProtocol::createPongPacket()
{
    return buildPacket(S_TO_C_PONG_TO_HOST, QByteArray());
}

QByteArray W3GSProtocol::createRejectPacket(uint32_t reason)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << reason;

    return buildPacket(S_TO_C_REJECT, payload);
}

QByteArray W3GSProtocol::createSlotInfoPacket(const QVector<PlayerInfo> &players)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << static_cast<uint8_t>(players.size());

    for (const PlayerInfo &player : players) {
        stream << player.playerId;
        writeString(stream, player.name);
        stream << static_cast<uint32_t>(htonl(player.address.toIPv4Address()));
        stream << player.port;
        stream << player.externalIP;
        stream << player.externalPort;
    }

    return buildPacket(S_TO_C_SLOT_INFO, payload);
}

QByteArray W3GSProtocol::createChatFromHostPacket(uint8_t fromPlayer, uint8_t toPlayer, const QString &message)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << fromPlayer;
    stream << toPlayer;
    writeString(stream, message);

    return buildPacket(S_TO_C_CHAT_FROM_HOST, payload);
}

QByteArray W3GSProtocol::createPlayerLeftPacket(uint8_t playerId, uint32_t reason)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << playerId;
    stream << reason;

    return buildPacket(S_TO_C_PLAYER_LEFT, payload);
}

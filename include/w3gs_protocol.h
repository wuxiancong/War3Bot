#ifndef W3GS_PROTOCOL_H
#define W3GS_PROTOCOL_H

#include <QObject>
#include <QByteArray>
#include <QMap>
#include <QHostAddress>
#include <QVector>

struct W3GSHeader {
    uint8_t protocol;
    uint16_t size;
    uint16_t type;
    uint8_t unknown;
};

struct PlayerInfo {
    uint8_t playerId;
    QString name;
    QHostAddress address;
    uint16_t port;
    uint32_t externalIP;
    uint16_t externalPort;

    PlayerInfo() : playerId(0), port(0), externalIP(0), externalPort(0) {}
    PlayerInfo(uint8_t id, const QString &playerName, const QHostAddress &addr, quint16 playerPort)
        : playerId(id), name(playerName), address(addr), port(playerPort), externalIP(0), externalPort(0) {}
};

class W3GSProtocol : public QObject {
    Q_OBJECT
public:
    explicit W3GSProtocol(QObject *parent = nullptr);

    // 数据包解析
    bool parsePacket(const QByteArray &data, W3GSHeader &header);
    QByteArray buildPacket(uint16_t type, const QByteArray &payload);

    // 数据包类型 (根据 bnetdocs.org 文档)
    enum PacketType {
        // 客户端到服务器数据包
        C_TO_S_PING_FROM_HOST        = 0x01,
        C_TO_S_SLOT_INFOJOIN         = 0x04,
        C_TO_S_REJECT_JOIN           = 0x05,
        C_TO_S_PLAYER_LOADED         = 0x06,
        C_TO_S_LOADING_DONE          = 0x07,
        C_TO_S_COUNTDOWN_END         = 0x09,
        C_TO_S_INCOMING_ACTION       = 0x0A,
        C_TO_S_CHAT_TO_HOST          = 0x0F,
        C_TO_S_DROP_ORDER            = 0x10,
        C_TO_S_LEAVE_GAME            = 0x11,
        C_TO_S_GAME_OVER             = 0x12,
        C_TO_S_MAP_CHECK             = 0x1A,
        C_TO_S_START_DOWNLOAD        = 0x1F,
        C_TO_S_MAP_SIZE              = 0x21,
        C_TO_S_MAP_PART              = 0x22,
        C_TO_S_MAP_PART_OK           = 0x24,
        C_TO_S_MAP_PART_ERROR        = 0x25,

        // 服务器到客户端数据包
        S_TO_C_PONG_TO_HOST          = 0x02,
        S_TO_C_REJECT                = 0x03,
        S_TO_C_SLOT_INFO             = 0x08,
        S_TO_C_OUTGOING_ACTION       = 0x0B,
        S_TO_C_OUTGOING_KEEP_ALIVE   = 0x0C,
        S_TO_C_CHAT_FROM_HOST        = 0x0E,
        S_TO_C_SEARCH_GAME           = 0x13,
        S_TO_C_GAME_CREATED          = 0x14,
        S_TO_C_REQ_JOIN              = 0x15,
        S_TO_C_PLAYER_EXTRA          = 0x16,
        S_TO_C_PLAYER_EXTRA_2        = 0x17,
        S_TO_C_PLAYER_LEFT           = 0x18,
        S_TO_C_PLAYER_LOADED         = 0x19,
        S_TO_C_PLAYER_LOADING_DONE   = 0x1B,
        S_TO_C_PLAYER_SLOT           = 0x1C,
        S_TO_C_PLAYER_SLOT_2         = 0x1D,
        S_TO_C_PLAYER_INFO           = 0x1E,
        S_TO_C_MAP_CHECK             = 0x20,
        S_TO_C_MAP_SIZE              = 0x23,
        S_TO_C_MAP_DATA              = 0x26,
        S_TO_C_UNKNOWN_0x27          = 0x27,
        S_TO_C_UNKNOWN_0x28          = 0x28
    };

    // 数据包构建方法
    QByteArray createPingPacket();
    QByteArray createPongPacket();
    QByteArray createRejectPacket(uint32_t reason);
    QByteArray createSlotInfoPacket(const QVector<PlayerInfo> &players);
    QByteArray createChatFromHostPacket(uint8_t fromPlayer, uint8_t toPlayer, const QString &message);
    QByteArray createPlayerLeftPacket(uint8_t playerId, uint32_t reason);

    // 数据包解析方法
    bool parseSlotInfoJoin(const QByteArray &data, PlayerInfo &player);
    bool parseChatToHost(const QByteArray &data, uint8_t &fromPlayer, uint8_t &toPlayer, QString &message);
    bool parseIncomingAction(const QByteArray &data, QByteArray &actions);
    bool parseMapCheck(const QByteArray &data, QByteArray &mapData);

private:
    uint8_t m_protocolVersion;

    // 工具方法
    void writeString(QDataStream &stream, const QString &str);
    QString readString(QDataStream &stream);
};

#endif // W3GS_PROTOCOL_H

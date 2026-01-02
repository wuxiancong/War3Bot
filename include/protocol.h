#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QtGlobal>

// ==================== 协议常量 ====================
const quint16 PROTOCOL_MAGIC        = 0xF801;   // 魔数
const quint8  PROTOCOL_VERSION      = 0x01;     // 版本号

// ==================== 指令集 ====================
enum class PacketType : quint8 {
    C_S_HEARTBEAT                   = 0x01,
    C_S_REGISTER                    = 0x02,
    S_C_REGISTER                    = 0x03,
    C_S_UNREGISTER                  = 0x04,
    S_C_UNREGISTER                  = 0x05,
    C_S_GAMEDATA                    = 0x06,
    S_C_GAMEDATA                    = 0x07,
    C_S_COMMAND                     = 0x08,
    S_C_COMMAND                     = 0x09,
    C_S_CHECKMAPCRC                 = 0x0A,
    S_C_CHECKMAPCRC                 = 0x0B,
    C_S_PING                        = 0x0C,
    S_C_PONG                        = 0x0D,
    ERROR                           = 0x0E
};

#pragma pack(push, 1)

struct PacketHeader {
    quint16 magic;
    quint8  version;
    quint8  command;
    quint32 sessionId;
    quint32 seq;
    quint16 payloadLen;
    quint16 checksum;
};

struct CSRegisterPacket {
    char clientId[40];
    char username[32];
    char localIp[16];
    char publicIp[16];
    quint16 localPort;
    quint16 publicPort;
    quint8 natType;
};

struct SCRegisterPacket {
    quint32 sessionId;
    quint8 status;
};

struct SCPongPacket {
    quint8 status;
};

struct CSCommandPacket {
    char username[32];
    char command[16];
    char text[200];
};

struct SCCommandPacket {
    char clientId[40];
    char username[32];
    char command[16];
};

struct CSCheckMapCRCPacket {
    char crcHex[10];
};

struct SCCheckMapCRCPacket {
    char crcHex[10];
    quint8 exists;
};

#pragma pack(pop)

#endif // PROTOCOL_H

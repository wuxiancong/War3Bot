#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QtGlobal>

// ==================== 协议常量 ====================
const quint16 PROTOCOL_MAGIC        = 0xF801;   // 魔数
const quint8  PROTOCOL_VERSION      = 0x01;     // 版本号

// ==================== 指令集 ====================
enum PacketType : quint8 {
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
    S_C_ERROR                       = 0x0E,
    S_C_MESSAGE                     = 0x0F,
    S_C_UPLOADRESULT                = 0x10
};

// ==================== 错误码定义 ====================

enum UploadErrorCode : quint8 {
    UPLOAD_ERR_UNKNOWN              = 0,
    UPLOAD_OK                       = 1,
    UPLOAD_ERR_MAGIC                = 2,
    UPLOAD_ERR_TOKEN                = 3,
    UPLOAD_ERR_FILENAME             = 4,
    UPLOAD_ERR_SIZE                 = 5,
    UPLOAD_ERR_IO                   = 6,
    UPLOAD_ERR_OVERFLOW             = 7
};

// ==================== 消息码定义 ====================

enum ErrorCode : quint8 {
    ERR_OK                          = 0,
    ERR_UNKNOWN                     = 1,
    ERR_PARAM_ERROR                 = 2,
    ERR_ALREADY_IN_GAME             = 3,
    ERR_COOLDOWN                    = 4,
    ERR_PERMISSION_DENIED           = 5,
    ERR_MAP_NOT_SUPPORTED           = 6,
    ERR_NAME_TOO_LONG               = 7,
    ERR_GAME_NAME_EXISTS            = 8,
    ERR_NO_BOTS_AVAILABLE           = 9,
    ERR_CREATE_FAILED               = 10
};

enum MessageCode : quint8 {
    MSG_HOST_JOINED_GAME            = 0
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

struct SCMessagePacket {
    quint8 type;
    quint8 code;
    quint64 data;
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
    char clientId[40];
    char username[32];
    char command[16];
    char text[200];
};

struct SCCommandPacket {
    char clientId[40];
    char username[32];
    char command[16];
    char text[200];
};

struct CSCheckMapCRCPacket {
    char crcHex[10];
};

struct SCCheckMapCRCPacket {
    char crcHex[10];
    quint8 exists;
};

struct SCUploadResultPacket {
    char crcHex[10];
    char fileName[64];
    quint8 status;
    quint8 reason;
};

#pragma pack(pop)

#endif // PROTOCOL_H

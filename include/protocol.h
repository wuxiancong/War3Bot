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
    S_C_UPLOADRESULT                = 0x10,
    S_C_PING_LIST                   = 0x11,
    S_C_READY_LIST                  = 0x12,
    C_S_PREJOINROOM                 = 0x13,
    S_C_PREJOINROOM                 = 0x14,
    C_S_ROOM_PING                   = 0x15,
    S_C_ROOM_PONG                   = 0x16
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
    ERR_NOT_ENOUGH_PLAYERS          = 10,
    ERR_WAIT_HOST_TIMEOUT           = 11,
    ERR_CREATE_ERROR                = 12,
    ERR_TASK_TIMEOUT                = 13,
    ERR_CREATE_INTERRUPTED          = 14,
    ERR_TASK_INTERRUPTED            = 15,
    ERR_GAME_NOT_FOUND              = 16,
    ERR_GAME_FULL                   = 17,
    ERR_GAME_STARTED                = 18,
    ERR_DUPLICATE_NAME              = 19,
    ERR_BANNED_USER                 = 20
};

enum MessageCode : quint8 {
    MSG_HOST_JOINED_GAME            = 0,
    MSG_HOST_CREATED_GAME           = 1,
    MSG_CHECK_MAP_CRC               = 2,
    MSG_HOST_LEAVE_GAME             = 3,
    MSG_PLAYER_LEAVE_GAME           = 4,
    MSG_HOST_UNHOST_GAME            = 5,
    MSG_ROOM_HOST_CHANGE            = 6,
    MSG_REJECT_REJOIN               = 7,
    MSG_GAME_STATE_CHANGE           = 8
};

enum GameState : quint8 {
    GAME_STATE_IDLE                 = 0,
    GAME_STATE_INROOM               = 1,
    GAME_STATE_LOADING              = 2,
    GAME_STATE_INGAME               = 3,
    GAME_STATE_FINISHED             = 4
};

enum TcpConnType {
    Tcp_Unknown                     = 0,
    Tcp_Upload                      = 1,
    Tcp_Custom                      = 2,
    Tcp_W3GS                        = 3
};

#pragma pack(push, 1)

struct PacketHeader {
    quint16 magic;
    quint8  version;
    quint8  command;
    quint32 sessionId;
    quint64 seq;
    quint16 payloadLen;
    quint16 checksum;
    char    signature[16];
};

struct SCMessagePacket {
    quint8 type;
    quint8 code;
    quint64 data;
};

struct CSRegisterPacket {
    char clientId[40];
    char hardwareId[65];
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

struct CSPreJoinRoomPacket {
    char userName[32];
    char roomName[32];
    char hostName[32];
    char clientId[64];
};

struct SCPreJoinRoomPacket {
    char userName[32];
    char hostName[32];
    quint8 status;
    quint8 errorCode;
};

struct CSRoomPingPacket {
    quint64 clientSendTime;
    char    targetHostName[32];
    char    targetClientId[64];
};

struct SCRoomPongPacket {
    quint64 clientSendTime;
    quint8  currentPlayers;
    quint8  maxPlayers;
    char    targetHostName[32];
    char    targetClientId[64];
};
#pragma pack(pop)

#endif // PROTOCOL_H

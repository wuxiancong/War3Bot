#include "war3map.h"
#include "logger.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QtEndian>
#include <QFileInfo>
#include <QCryptographicHash>

#include <zlib.h>
#include "StormLib.h"

QString War3Map::s_priorityCrcDir = "";

// =========================================================
// 辅助函数
// =========================================================
static QByteArray toBytes(quint32 val) {
    QByteArray b; b.resize(4); qToLittleEndian(val, (uchar*)b.data()); return b;
}
static QByteArray toBytes16(quint16 val) {
    QByteArray b; b.resize(2); qToLittleEndian(val, (uchar*)b.data()); return b;
}

// 循环左移 (ROL)
static inline quint32 rotateLeft(quint32 value, int shift) {
    return (value << shift) | (value >> (32 - shift));
}

// 以 Little-Endian 模式将 uint32 写入 SHA1
void sha1UpdateInt32(QCryptographicHash &sha1, quint32 value) {
    quint32 le = qToLittleEndian(value);
    sha1.addData((const char*)&le, 4);
}

// =========================================================
// War3Map 类实现
// =========================================================

War3Map::War3Map() :
    m_valid(false),
    m_numPlayers(0),
    m_numTeams(0),
    m_mapSpeed(MAPSPEED_FAST),
    m_mapVisibility(MAPVIS_DEFAULT),
    m_mapObservers(MAPOBS_NONE),
    m_mapFlags(MAPFLAG_TEAMSTOGETHER | MAPFLAG_FIXEDTEAMS)
{
}

War3Map::~War3Map() {}

quint32 War3Map::getMapSize() const {
    if (m_mapSize.size() < 4) return 0;
    return qFromLittleEndian<quint32>(m_mapSize.constData());
}

quint32 War3Map::getMapInfo() const {
    if (m_mapInfo.size() < 4) return 0;
    return qFromLittleEndian<quint32>(m_mapInfo.constData());
}

quint32 War3Map::getMapSHA1() const {
    if (m_mapSHA1Bytes.size() < 4) return 0;
    return qFromLittleEndian<quint32>(m_mapSHA1Bytes.constData());
}

quint32 War3Map::getMapCRC() const {
    if (m_mapCRC.size() < 4) return 0;
    return qFromLittleEndian<quint32>(m_mapCRC.constData());
}

QString War3Map::getMapName() const {
    if (m_mapPath.isEmpty()) return QString();
    return QFileInfo(m_mapPath).fileName();
}

// 仅获取游戏参数标志位 (用于 StatString)
QByteArray War3Map::getMapGameFlags()
{
    quint32 GameFlags = 0;

    // 1. Speed (Mask 0x03)
    if (m_mapSpeed == MAPSPEED_SLOW)        GameFlags = 0x00000000;
    else if (m_mapSpeed == MAPSPEED_NORMAL) GameFlags = 0x00000001;
    else                                    GameFlags = 0x00000002;

    // 2. Visibility (Mask 0x0F00)
    if (m_mapVisibility == MAPVIS_HIDETERRAIN)       GameFlags |= 0x00000100;
    else if (m_mapVisibility == MAPVIS_EXPLORED)     GameFlags |= 0x00000200;
    else if (m_mapVisibility == MAPVIS_ALWAYSVISIBLE)GameFlags |= 0x00000400;
    else                                             GameFlags |= 0x00000800;

    // 3. Observers (Mask 0x40003000)
    if (m_mapObservers == MAPOBS_ONDEFEAT)      GameFlags |= 0x00002000;
    else if (m_mapObservers == MAPOBS_ALLOWED)  GameFlags |= 0x00003000;
    else if (m_mapObservers == MAPOBS_REFEREES) GameFlags |= 0x40000000;

    // 4. Teams/Units/Hero/Race (Mask 0x07064000)
    if (m_mapFlags & MAPFLAG_TEAMSTOGETHER) GameFlags |= 0x00004000;
    if (m_mapFlags & MAPFLAG_FIXEDTEAMS)    GameFlags |= 0x00060000;
    if (m_mapFlags & MAPFLAG_UNITSHARE)     GameFlags |= 0x01000000;
    if (m_mapFlags & MAPFLAG_RANDOMHERO)    GameFlags |= 0x02000000;
    if (m_mapFlags & MAPFLAG_RANDOMRACES)   GameFlags |= 0x04000000;

    return toBytes(GameFlags);
}

// 核心加载函数
bool War3Map::load(const QString &mapPath)
{
    m_valid = false;
    m_mapPath = mapPath;

    LOG_INFO(QString("[War3Map] 开始加载地图: %1").arg(mapPath));

    // -------------------------------------------------------
    // 1. 基础文件检查与 MPQ 打开
    // -------------------------------------------------------
    QFile file(mapPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("[War3Map] ❌ 无法打开地图文件: %1").arg(mapPath));
        return false;
    }

    // 读取所有数据以计算 CRC
    m_mapRawData = file.readAll();
    file.close();

    // 1. 设置地图大小
    m_mapSize = toBytes((quint32)m_mapRawData.size());

    // 2. 计算 CRC32 并赋值给 m_mapInfo (解决 NETERROR 关键点)
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef*)m_mapRawData.constData(), m_mapRawData.size());

    // 赋值!
    m_mapInfo = toBytes((quint32)crc);

    LOG_INFO(QString("[War3Map] MapInfo (CRC32): 0x%1").arg(QString::number(crc, 16).toUpper()));

    // 打开 MPQ
    HANDLE hMpq = NULL;
    QString nativePath = QDir::toNativeSeparators(mapPath);

#ifdef UNICODE
    const wchar_t *pathStr = (const wchar_t*)nativePath.utf16();
#else
    const char *pathStr = nativePath.toLocal8Bit().constData();
#endif

    if (!SFileOpenArchive(pathStr, 0, MPQ_OPEN_READ_ONLY, &hMpq)) {
        LOG_ERROR(QString("[War3Map] ❌ 无法打开 MPQ: %1").arg(nativePath));
        return false;
    }

    // -------------------------------------------------------
    // 2. 定义读取辅助 Lambda
    // -------------------------------------------------------

    // 从本地 war3 目录读取环境文件
    auto readLocalScript = [&](const QString &fileName) -> QByteArray {
        if (!s_priorityCrcDir.isEmpty()) {
            QFile f(s_priorityCrcDir + "/" + fileName);
            if (f.exists() && f.open(QIODevice::ReadOnly)) return f.readAll();
        }
        QFile fDefault("war3files/" + fileName);
        if (fDefault.open(QIODevice::ReadOnly)) return fDefault.readAll();
        return QByteArray();
    };

    // 从 MPQ 读取文件
    auto readMpqFile = [&](const QString &fileName) -> QByteArray {
        HANDLE hFile = NULL;
        QByteArray buffer;
        if (SFileOpenFileEx(hMpq, fileName.toLocal8Bit().constData(), 0, &hFile)) {
            DWORD s = SFileGetFileSize(hFile, NULL);
            if (s > 0 && s != 0xFFFFFFFF) {
                buffer.resize(s);
                DWORD read = 0;
                SFileReadFile(hFile, buffer.data(), s, &read, NULL);
            }
            SFileCloseFile(hFile);
        }
        return buffer;
    };

    // -------------------------------------------------------
    // 3. 准备核心数据 (Script & Env)
    // -------------------------------------------------------

    // 读取环境脚本
    QByteArray dataCommon = readLocalScript("common.j");
    QByteArray dataBlizzard = readLocalScript("blizzard.j");

    if (dataCommon.isEmpty() || dataBlizzard.isEmpty()) {
        LOG_ERROR("[War3Map] ❌ 严重错误: 缺少 common.j 或 blizzard.j，校验无法进行！");
        SFileCloseArchive(hMpq);
        return false;
    }

    // 读取地图脚本 (支持 war3map.j / scripts\war3map.j / war3map.lua)
    QByteArray dataMapScript = readMpqFile("war3map.j");
    if (dataMapScript.isEmpty()) dataMapScript = readMpqFile("scripts\\war3map.j");
    if (dataMapScript.isEmpty()) dataMapScript = readMpqFile("war3map.lua");

    if (dataMapScript.isEmpty()) {
        LOG_ERROR("[War3Map] ❌ 严重错误: 无法在地图中找到脚本文件");
        SFileCloseArchive(hMpq);
        return false;
    }

    // -------------------------------------------------------
    // 4. 初始化校验算法 (Legacy CRC & New SHA1)
    // -------------------------------------------------------

    // === A. 初始化 SHA-1 (1.26a 核心逻辑) ===
    QCryptographicHash sha1Ctx(QCryptographicHash::Sha1);

    sha1Ctx.addData(dataCommon);          // 1. common.j
    sha1Ctx.addData(dataBlizzard);        // 2. blizzard.j
    sha1UpdateInt32(sha1Ctx, 0x03F1379E); // 3. Salt (0x03F1379E)
    sha1Ctx.addData(dataMapScript);       // 4. war3map.j

    // === B. 初始化 Legacy CRC (XORO 算法, 兼容旧平台) ===
    quint32 crcVal = 0;
    quint32 hCommon = calcBlizzardHash(dataCommon);
    quint32 hBlizz = calcBlizzardHash(dataBlizzard);
    quint32 hScript = calcBlizzardHash(dataMapScript);

    crcVal = hBlizz ^ hCommon;      // Xor
    crcVal = rotateLeft(crcVal, 3); // Rol 1
    crcVal = crcVal ^ 0x03F1379E;   // Salt
    crcVal = rotateLeft(crcVal, 3); // Rol 2
    crcVal = hScript ^ crcVal;      // Mix Map
    crcVal = rotateLeft(crcVal, 3); // Rol 3

    // -------------------------------------------------------
    // 5. 统一遍历组件 (同时更新两个算法)
    // -------------------------------------------------------
    const char *componentFiles[] = {
        "war3map.w3e", "war3map.wpm", "war3map.doo", "war3map.w3u",
        "war3map.w3b", "war3map.w3d", "war3map.w3a", "war3map.w3q"
    };

    for (const char *compName : componentFiles) {
        // 读取组件数据
        QByteArray compData = readMpqFile(compName);

        // 如果文件存在，同时加入两个算法的计算
        if (!compData.isEmpty()) {
            // A. Update SHA-1
            sha1Ctx.addData(compData);

            // B. Update Legacy CRC
            quint32 hComp = calcBlizzardHash(compData);
            crcVal = crcVal ^ hComp;
            crcVal = rotateLeft(crcVal, 3);

            LOG_INFO(QString("   + [Checksum] 组件已加入: %1 (Size: %2)").arg(compName).arg(compData.size()));
        }
    }

    // -------------------------------------------------------
    // 6. 结算与保存结果
    // -------------------------------------------------------

    // 保存 SHA-1 (StatString 真正用到的 20 字节)
    m_mapSHA1Bytes = sha1Ctx.result();

    // 保存 CRC (兼容字段)
    m_mapCRC = toBytes(crcVal);

    LOG_INFO(QString("[War3Map] ✅ CRC  Checksum: %1").arg(QString(m_mapCRC.toHex().toUpper())));
    LOG_INFO(QString("[War3Map] ✅ SHA1 Checksum: %1").arg(QString(m_mapSHA1Bytes.toHex().toUpper())));

    // -------------------------------------------------------
    // 7. 解析 war3map.w3i (获取地图信息)
    // -------------------------------------------------------
    QByteArray w3iData = readMpqFile("war3map.w3i");
    if (!w3iData.isEmpty()) {
        QDataStream in(w3iData);
        in.setByteOrder(QDataStream::LittleEndian);

        quint32 fileFormat;
        in >> fileFormat;

        if (fileFormat == 18 || fileFormat == 25) {
            in.skipRawData(4); // saves
            in.skipRawData(4); // editor ver

            // 跳过变长字符串
            auto skipStr = [&]() {
                char c;
                do { in >> (quint8&)c; } while(c != 0 && !in.atEnd());
            };
            skipStr(); skipStr(); skipStr(); skipStr();

            in.skipRawData(32); // camera bounds
            in.skipRawData(16); // camera complements

            quint32 rawW, rawH, rawFlags;
            in >> rawW >> rawH >> rawFlags;

            m_mapWidth = toBytes16((quint16)rawW);
            m_mapHeight = toBytes16((quint16)rawH);
            m_mapOptions = rawFlags;
        }
    } else {
        LOG_WARNING("[War3Map] ⚠️ 无法读取 war3map.w3i，将使用默认参数");
    }

    // -------------------------------------------------------
    // 8. 清理与完成
    // -------------------------------------------------------
    SFileCloseArchive(hMpq);
    m_valid = true;
    return true;
}

QByteArray War3Map::decodeStatString(const QByteArray &encoded)
{
    QByteArray decoded;
    int i = 0;
    while (i < encoded.size()) {
        unsigned char mask = (unsigned char)encoded[i++];
        for (int j = 0; j < 7 && i < encoded.size(); ++j) {
            if ((mask & (1 << (j + 1))) == 0) {
                decoded.append(encoded[i] - 1);
            } else {
                decoded.append(encoded[i]);
            }
            i++;
        }
    }
    return decoded;
}

QByteArray War3Map::encodeStatString(const QByteArray &data) {
    QByteArray result;
    unsigned char mask = 1;
    QByteArray chunk;
    for (int i = 0; i < data.size(); ++i) {
        unsigned char c = (unsigned char)data[i];
        if (c % 2 == 0) chunk.append((char)(c + 1));
        else { chunk.append((char)c); mask |= 1 << ((i % 7) + 1); }
        if ((i % 7) == 6 || i == data.size() - 1) {
            result.append((char)mask); result.append(chunk); chunk.clear(); mask = 1;
        }
    }
    return result;
}

QByteArray War3Map::getEncodedStatString(const QString &hostName, const QString &netPathOverride)
{
    if (!m_valid) return QByteArray();

    QByteArray rawData;
    QDataStream out(&rawData, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // 1. Map Flags
    QByteArray gameFlagsBytes = getMapGameFlags();
    quint32 gameFlagsInt;
    QDataStream ds(gameFlagsBytes);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds >> gameFlagsInt;

    quint32 finalFlags = 0;
    finalFlags |= gameFlagsInt;

    out << finalFlags << (quint8)0;
    out.writeRawData(m_mapWidth.constData(), 2);
    out.writeRawData(m_mapHeight.constData(), 2);
    out.writeRawData(m_mapCRC.constData(), 4);

    // 2. Map Path
    QString finalPath = netPathOverride.isEmpty() ?
                            "Maps\\Download\\" + QFileInfo(m_mapPath).fileName() : netPathOverride;
    finalPath = finalPath.replace("/", "\\");

    out.writeRawData(finalPath.toLocal8Bit().constData(), finalPath.toLocal8Bit().size());
    out << (quint8)0; // Map Path Terminator

    // 3. Host Name
    out.writeRawData(hostName.toUtf8().constData(), hostName.toUtf8().size());
    out << (quint8)0; // Host Name Terminator

    // 4. Map Unknown
    // 根据协议：(UINT8) Map unknown (possibly a STRING with just the null terminator)
    out << (quint8)0;

    // 5. Map SHA1
    out.writeRawData(m_mapSHA1Bytes.constData(), 20);

    QByteArray encoded = encodeStatString(rawData);
    analyzeStatString("War3Map生成结果", encoded);
    return encoded;
}

void War3Map::analyzeStatString(const QString &label, const QByteArray &encodedData)
{
    QByteArray decoded = War3Map::decodeStatString(encodedData);
    QDataStream in(decoded);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 flags;
    quint16 w, h;
    quint32 crc;
    quint8 padding;

    in >> flags;
    in >> padding;
    in >> w >> h >> crc;

    // 读取路径字符串
    QByteArray pathBytes;
    char c;
    while (!in.atEnd()) {
        in.readRawData(&c, 1);
        if (c == 0) break;
        pathBytes.append(c);
    }

    LOG_INFO("========================================");
    LOG_INFO(QString("📊 StatString分析 [%1]").arg(label));
    LOG_INFO(QString("   Encoded Hex: %1").arg(QString(encodedData.toHex().toUpper())));
    LOG_INFO(QString("   Decoded Hex: %1").arg(QString(decoded.toHex().toUpper())));
    LOG_INFO("----------------------------------------");
    LOG_INFO(QString("   [Flags]    : 0x%1").arg(QString::number(flags, 16).toUpper()));

    bool hideMap = (flags & 0x00010000);
    bool fastSpeed = ((flags & 0x0000000F) == 0x02);
    bool obs = (flags & 0x00060000);

    LOG_INFO(QString("      -> 隐藏地图(0x10000): %1").arg(hideMap ? "⚠️ 是" : "✅ 否"));
    LOG_INFO(QString("      -> 游戏速度(Low=2)  : %1").arg(fastSpeed ? "✅ 快速" : "❌ 非法/慢速"));
    LOG_INFO(QString("      -> 观察者(0x60000)  : %1").arg(obs ? "✅ 开启" : "❓ 未知"));

    LOG_INFO(QString("   [Width]    : %1").arg(w));
    LOG_INFO(QString("   [Height]   : %1").arg(h));
    LOG_INFO(QString("   [CRC]      : 0x%1").arg(QString::number(crc, 16).toUpper()));
    LOG_INFO(QString("   [Path]     : %1").arg(QString(pathBytes)));
    LOG_INFO("========================================");
}

void War3Map::setPriorityCrcDirectory(const QString &dirPath)
{
    s_priorityCrcDir = dirPath;
    LOG_INFO(QString("[War3Map] 设置计算CRC的文件搜索路径: %1").arg(dirPath));
}

// =========================================================
// 核心算法：暴雪自定义哈希 (Blizzard Hash)
// 汇编入口: Game.dll + 39E5C0
// =========================================================
quint32 War3Map::calcBlizzardHash(const QByteArray &data) {
    quint32 hash = 0;
    const char *ptr = data.constData();
    int length = data.size();

    // 1. 处理 4 字节块 (DWORD)
    // 汇编: game.dll + 39E5C3 | shr esi,2 (Count of DWORDs)
    while (length >= 4) {
        // 读取 4 字节 (强制转换为 quint32, 依赖 CPU 小端序)
        // 汇编: game.dll + 39E5D0 | mov edi,dword ptr ds:[ecx]
        quint32 chunk = *reinterpret_cast<const quint32*>(ptr);

        // XOR
        // 汇编: game.dll + 39E5D2 | xor edi,eax
        hash = hash ^ chunk;

        // ROL 3
        // 汇编: game.dll + 39E5D7 | rol edi,3
        hash = rotateLeft(hash, 3);

        ptr += 4;
        length -= 4;
    }

    // 2. 处理剩余字节
    // 汇编: game.dll + 39E5E8 (循环处理剩余字节)
    while (length > 0) {
        quint8 byteVal = (quint8)*ptr;

        // XOR
        // 汇编: game.dll + 39E5EB | xor esi,eax
        hash = hash ^ byteVal;

        // ROL 3
        // 汇编: game.dll + 39E5F0 | rol esi,3
        hash = rotateLeft(hash, 3);

        ptr++;
        length--;
    }

    return hash;
}

// ---------------------------------------------------------
// 标准 IEEE 802.3 CRC-32 (魔兽争霸3 使用的版本)
// ---------------------------------------------------------
quint32 War3Map::calcCrc32(const QByteArray &data)
{
    static const quint32 crc_table[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
        0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
        0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
        0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
        0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
        0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
        0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
        0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
        0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
        0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
        0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
        0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };

    quint32 crc = 0xFFFFFFFF;
    const quint8 *p = reinterpret_cast<const quint8 *>(data.constData());
    int size = data.size();

    for (int i = 0; i < size; ++i) {
        crc = (crc >> 8) ^ crc_table[(crc ^ p[i]) & 0xFF];
    }

    return crc ^ 0xFFFFFFFF;
}

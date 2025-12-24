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

    QFile file(mapPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("[War3Map] ❌ 无法打开地图文件: %1").arg(mapPath));
        return false;
    }

    // 1. 读取地图基础信息 (Size & CRC32 & SHA1)
    QByteArray mapRawData = file.readAll();
    file.close();

    quint32 mapSizeInt = mapRawData.size();
    m_mapSize = toBytes(mapSizeInt);

    // 标准 Zip CRC32 (用于 MPQ 完整性)
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef*)mapRawData.constData(), mapRawData.size());
    m_mapInfo = toBytes((quint32)crc);

    // 计算 SHA1 (Map Data + Magic String)
    // 这是标准的 Map SHA1 计算方式，替代原本基于 combinedData 的逻辑
    QCryptographicHash sha1(QCryptographicHash::Sha1);
    sha1.addData(mapRawData);
    sha1.addData("\x9E\x37\xF1\x03", 4); // War3 Map SHA1 Magic
    m_mapSHA1 = sha1.result();

    // 2. 打开 MPQ 档案
    HANDLE hMpq = NULL;
    QString nativePath = QDir::toNativeSeparators(mapPath);
    DWORD flags = MPQ_OPEN_READ_ONLY;

#ifdef UNICODE
    const wchar_t *pathStr = (const wchar_t*)nativePath.utf16();
#else
    const char *pathStr = nativePath.toLocal8Bit().constData();
#endif

    if (!SFileOpenArchive(pathStr, 0, flags, &hMpq)) {
        LOG_ERROR(QString("[War3Map] ❌ 无法打开 MPQ: %1").arg(nativePath));
        return false;
    }

    // -------------------------------------------------------------
    // ★★★ 地图一致性校验 (Map Checksum) - Stage 1 & 2 ★★★
    // -------------------------------------------------------------

    // Helper: 从本地磁盘读取环境脚本 (common.j / blizzard.j)
    auto readLocalScript = [&](const QString &fileName) -> QByteArray {
        // A. 优先目录
        if (!s_priorityCrcDir.isEmpty()) {
            QFile f(s_priorityCrcDir + "/" + fileName);
            if (f.exists() && f.open(QIODevice::ReadOnly)) return f.readAll();
        }
        // B. 默认目录
        QFile fDefault("war3files/" + fileName);
        if (fDefault.open(QIODevice::ReadOnly)) return fDefault.readAll();
        return QByteArray();
    };

    // Helper: 从 MPQ 读取文件
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

    // --- Step 1: 准备环境数据 ---
    QByteArray dataCommon = readLocalScript("common.j");
    QByteArray dataBlizzard = readLocalScript("blizzard.j");

    // 尝试读取 war3map.j，如果是 Lua 地图则尝试 war3map.lua
    QByteArray dataMapScript = readMpqFile("war3map.j");
    if (dataMapScript.isEmpty()) dataMapScript = readMpqFile("scripts\\war3map.j");
    if (dataMapScript.isEmpty()) dataMapScript = readMpqFile("war3map.lua");

    if (dataCommon.isEmpty()) {
        LOG_ERROR("[War3Map] ❌ 严重错误: 无法读取 common.j (路径: war3files/common.j)");
    }
    if (dataBlizzard.isEmpty()) {
        LOG_ERROR("[War3Map] ❌ 严重错误: 无法读取 blizzard.j (路径: war3files/blizzard.j)");
    }
    if (dataMapScript.isEmpty()) {
        LOG_ERROR("[War3Map] ❌ 严重错误: 无法在地图中找到脚本文件");
    }

    quint32 hCommon = calcBlizzardHash(dataCommon);
    quint32 hBlizzard = calcBlizzardHash(dataBlizzard);
    quint32 hMapScript = calcBlizzardHash(dataMapScript);

    LOG_INFO(QString("🔹 Common.j Hash:   %1 (Size: %2)").arg(QString::number(hCommon, 16).toUpper()).arg(dataCommon.size()));
    LOG_INFO(QString("🔹 Blizzard.j Hash: %1 (Size: %2)").arg(QString::number(hBlizzard, 16).toUpper()).arg(dataBlizzard.size()));
    LOG_INFO(QString("🔹 War3Map.j Hash:  %1 (Size: %2)").arg(QString::number(hMapScript, 16).toUpper()).arg(dataMapScript.size()));

    // --- Step 2: 脚本环境混合 (Stage 1) ---
    // 公式: ROL(ROL(Bliz ^ Com, 3) ^ Magic, 3) ^ MapScript
    quint32 val = 0;

    val = hBlizzard ^ hCommon;      // Xor
    val = rotateLeft(val, 3);       // Rol 1
    val = val ^ 0x03F1379E;         // Salt
    val = rotateLeft(val, 3);       // Rol 2
    val = hMapScript ^ val;         // Mix Map
    val = rotateLeft(val, 3);       // Rol 3

    LOG_INFO(QString("[War3Map] Stage 1 Checksum: %1").arg(QString::number(val, 16).toUpper()));

    // --- Step 3: 组件校验 (Stage 2) ---
    // 严格顺序: w3e, wpm, doo, w3u, w3b, w3d, w3a, w3q
    const char *componentFiles[] = {
        "war3map.w3e", "war3map.wpm", "war3map.doo", "war3map.w3u",
        "war3map.w3b", "war3map.w3d", "war3map.w3a", "war3map.w3q"
    };

    for (const char *compName : componentFiles) {
        QByteArray compData = readMpqFile(compName);

        // 文件不存在或为空则跳过 (不混入0)
        if (compData.isEmpty()) continue;

        quint32 hComp = calcBlizzardHash(compData);

        // Mix Component
        val = val ^ hComp;
        val = rotateLeft(val, 3);

        LOG_INFO(QString("   + Mixed Component: %1 (Hash: %2)").arg(compName, QString::number(hComp, 16).toUpper()));
    }

    // 保存最终 CRC
    m_mapCRC = toBytes(val);

    LOG_INFO(QString("[War3Map] Final Checksum: %1").arg(QString(m_mapCRC.toHex().toUpper())));
    LOG_INFO(QString("[War3Map] Map SHA1:       %1").arg(QString(m_mapSHA1.toHex().toUpper())));


    // 3. 解析 war3map.w3i (获取地图信息)
    QByteArray w3iData = readMpqFile("war3map.w3i");
    if (!w3iData.isEmpty()) {
        QDataStream in(w3iData);
        in.setByteOrder(QDataStream::LittleEndian);

        quint32 fileFormat;
        in >> fileFormat;

        // 支持版本 18 (RoC) 和 25 (TFT)
        if (fileFormat == 18 || fileFormat == 25) {
            in.skipRawData(4); // saves
            in.skipRawData(4); // editor ver

            // 跳过地图名称/作者/描述/建议玩家 (4个字符串)
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

            LOG_INFO(QString("[War3Map] w3i 解析成功. Size: %1x%2 Flags: 0x%3")
                         .arg(rawW).arg(rawH).arg(QString::number(m_mapOptions, 16).toUpper()));
        }
    } else {
        LOG_WARNING("[War3Map] ⚠️ 无法读取 war3map.w3i (可能是受保护的地图)");
    }

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
    out.writeRawData(m_mapSHA1.constData(), 20);

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

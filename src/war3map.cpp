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

QMutex War3Map::s_cacheMutex;
QString War3Map::s_priorityCrcDir = "";
QMap<QString, std::shared_ptr<War3MapSharedData>> War3Map::s_cache;

// =========================================================
// 辅助函数
// =========================================================
static QByteArray toBytes(quint32 val) {
    QByteArray b; b.resize(4); qToLittleEndian(val, (uchar*)b.data()); return b;
}
static QByteArray toBytes16(quint16 val) {
    QByteArray b; b.resize(2); qToLittleEndian(val, (uchar*)b.data()); return b;
}

// 32位循环左移
static inline quint32 rotateLeft(quint32 value, int shift) {
    return (value << shift) | (value >> (32 - shift));
}

// Little-Endian 写入 SHA1
void sha1UpdateInt32(QCryptographicHash &sha1, quint32 value) {
    quint32 le = qToLittleEndian(value);
    sha1.addData((const char*)&le, 4);
}

// =========================================================
// War3Map 类实现
// =========================================================

War3Map::War3Map() :
    m_mapSpeed(MAPSPEED_FAST),
    m_mapVisibility(MAPVIS_DEFAULT),
    m_mapObservers(MAPOBS_NONE),
    m_mapFlags(MAPFLAG_TEAMSTOGETHER | MAPFLAG_FIXEDTEAMS)
{
    m_sharedData = nullptr;
}

War3Map::~War3Map() {}

bool War3Map::isValid() const {
    return m_sharedData && m_sharedData->valid;
}

QList<W3iForce> War3Map::getForces() const {
    return m_sharedData ? m_sharedData->w3iForces : QList<W3iForce>();
}

QList<W3iPlayer> War3Map::getPlayers() const {
    return m_sharedData ? m_sharedData->w3iPlayers : QList<W3iPlayer>();
}

QString War3Map::getMapPath() const {
    return m_sharedData ? m_sharedData->mapPath : QString();
}

QByteArray War3Map::getMapRawData() const {
    return m_sharedData ? m_sharedData->mapRawData : QByteArray();
}

quint32 War3Map::getMapSize() const {
    if (!isValid() || m_sharedData->mapSize.size() < 4) return 0;
    return qFromLittleEndian<quint32>(m_sharedData->mapSize.constData());
}

quint32 War3Map::getMapInfo() const {
    if (!isValid() || m_sharedData->mapInfo.size() < 4) return 0;
    return qFromLittleEndian<quint32>(m_sharedData->mapInfo.constData());
}

quint32 War3Map::getMapSHA1() const {
    if (!isValid() || m_sharedData->mapSHA1Bytes.size() < 4) return 0;
    return qFromLittleEndian<quint32>(m_sharedData->mapSHA1Bytes.constData());
}

quint32 War3Map::getMapCRC() const {
    if (!isValid() || m_sharedData->mapCRC.size() < 4) return 0;
    return qFromLittleEndian<quint32>(m_sharedData->mapCRC.constData());
}

QByteArray War3Map::getMapSHA1Bytes() const {
    return m_sharedData ? m_sharedData->mapSHA1Bytes : QByteArray();
}

QByteArray War3Map::getMapWidth() const {
    return m_sharedData ? m_sharedData->mapWidth : QByteArray();
}

QByteArray War3Map::getMapHeight() const {
    return m_sharedData ? m_sharedData->mapHeight : QByteArray();
}

QString War3Map::getMapName() const {
    if (!isValid()) return QString();
    return QFileInfo(m_sharedData->mapPath).fileName();
}

// 获取游戏标志位 (StatString用)
QByteArray War3Map::getMapGameFlags()
{
    quint32 GameFlags = 0;

    // 1. Speed
    if (m_mapSpeed == MAPSPEED_SLOW)        GameFlags = 0x00000000;
    else if (m_mapSpeed == MAPSPEED_NORMAL) GameFlags = 0x00000001;
    else                                    GameFlags = 0x00000002;

    // 2. Visibility
    if (m_mapVisibility == MAPVIS_HIDETERRAIN)       GameFlags |= 0x00000100;
    else if (m_mapVisibility == MAPVIS_EXPLORED)     GameFlags |= 0x00000200;
    else if (m_mapVisibility == MAPVIS_ALWAYSVISIBLE)GameFlags |= 0x00000400;
    else                                             GameFlags |= 0x00000800;

    // 3. Observers
    if (m_mapObservers == MAPOBS_ONDEFEAT)      GameFlags |= 0x00002000;
    else if (m_mapObservers == MAPOBS_ALLOWED)  GameFlags |= 0x00003000;
    else if (m_mapObservers == MAPOBS_REFEREES) GameFlags |= 0x40000000;

    // 4. Teams/Units/Hero/Race
    if (m_mapFlags & MAPFLAG_TEAMSTOGETHER) GameFlags |= 0x00004000;
    if (m_mapFlags & MAPFLAG_FIXEDTEAMS)    GameFlags |= 0x00060000;
    if (m_mapFlags & MAPFLAG_UNITSHARE)     GameFlags |= 0x01000000;
    if (m_mapFlags & MAPFLAG_RANDOMHERO)    GameFlags |= 0x02000000;
    if (m_mapFlags & MAPFLAG_RANDOMRACES)   GameFlags |= 0x04000000;

    return toBytes(GameFlags);
}

// 暴雪哈希算法
quint32 War3Map::calcBlizzardHash(const QByteArray &data) {
    quint32 hash = 0;
    const char *ptr = data.constData();
    int length = data.size();

    // 4字节块处理
    while (length >= 4) {
        quint32 chunk = qFromLittleEndian<quint32>((const uchar*)ptr);
        hash = hash ^ chunk;
        hash = rotateLeft(hash, 3);
        ptr += 4;
        length -= 4;
    }

    // 剩余字节处理
    while (length > 0) {
        quint8 byteVal = (quint8)*ptr;
        hash = hash ^ byteVal;
        hash = rotateLeft(hash, 3);
        ptr++;
        length--;
    }

    return hash;
}

// 核心加载函数
bool War3Map::load(const QString &mapPath)
{
    QString cleanPath = QFileInfo(mapPath).absoluteFilePath();
    QString fileName = QFileInfo(mapPath).fileName();

    LOG_INFO(QString("🗺️ [地图加载] 请求: %1").arg(fileName));

    // 1. 缓存检查
    {
        QMutexLocker locker(&s_cacheMutex);
        if (s_cache.contains(cleanPath)) {
            m_sharedData = s_cache[cleanPath];
            if (m_sharedData && m_sharedData->valid) {
                LOG_INFO(QString("   └─ ⚡ [缓存命中] Ref: %1").arg(m_sharedData.use_count()));
                return true;
            }
            s_cache.remove(cleanPath);
        }
    }

    auto newData = std::make_shared<War3MapSharedData>();
    newData->mapPath = cleanPath;
    newData->valid = false;

    // 2. 读取物理文件
    QFile file(mapPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("   └─ ❌ [错误] 无法打开文件: %1").arg(mapPath));
        return false;
    }
    newData->mapRawData = file.readAll();
    file.close();

    // 3. 计算基础校验信息 (Size, CRC32)
    newData->mapSize = toBytes((quint32)newData->mapRawData.size());
    uLong zCrc = crc32(0L, Z_NULL, 0);
    zCrc = crc32(zCrc, (const Bytef*)newData->mapRawData.constData(), newData->mapRawData.size());
    newData->mapInfo = toBytes((quint32)zCrc);

    LOG_INFO(QString("   ├─ 📂 文件读取: %1 bytes (CRC: %2)").arg(newData->mapRawData.size()).arg(QString::number(zCrc, 16).toUpper()));

    // 4. MPQ 操作
    HANDLE hMpq = NULL;
    QString nativePath = QDir::toNativeSeparators(mapPath);
#ifdef UNICODE
    const wchar_t *pathStr = (const wchar_t*)nativePath.utf16();
#else
    const char *pathStr = nativePath.toLocal8Bit().constData();
#endif

    if (!SFileOpenArchive(pathStr, 0, MPQ_OPEN_READ_ONLY, &hMpq)) {
        LOG_ERROR("   └─ ❌ [错误] MPQ 归档损坏");
        return false;
    }

    auto readLocalScript = [&](const QString &name) -> QByteArray {
        if (!s_priorityCrcDir.isEmpty()) {
            QFile f(s_priorityCrcDir + "/" + name);
            if (f.exists() && f.open(QIODevice::ReadOnly)) return f.readAll();
        }
        // 请确保 war3files 目录下是 1.26a 版本的文件
        QFile fDefault("war3files/" + name);
        if (fDefault.open(QIODevice::ReadOnly)) return fDefault.readAll();
        return QByteArray();
    };

    auto readMpqFile = [&](const QString &name) -> QByteArray {
        HANDLE hFile = NULL;
        QByteArray buf;
        if (SFileOpenFileEx(hMpq, name.toLocal8Bit().constData(), 0, &hFile)) {
            DWORD s = SFileGetFileSize(hFile, NULL);
            if (s > 0 && s != 0xFFFFFFFF) {
                buf.resize(s);
                DWORD r = 0;
                SFileReadFile(hFile, buf.data(), s, &r, NULL);
            }
            SFileCloseFile(hFile);
        }
        return buf;
    };

    // 5. GameCRC 计算 (Map Check)
    QByteArray dataCommon = readLocalScript("common.j");
    QByteArray dataBlizzard = readLocalScript("blizzard.j");
    QByteArray dataMapScript = readMpqFile("war3map.j");
    if (dataMapScript.isEmpty()) dataMapScript = readMpqFile("scripts\\war3map.j");

    if (dataCommon.isEmpty() || dataBlizzard.isEmpty() || dataMapScript.isEmpty()) {
        LOG_ERROR("   └─ ❌ [错误] 核心脚本缺失，无法计算 Hash");
        SFileCloseArchive(hMpq);
        return false;
    }

    // 串行 CRC 计算逻辑
    QCryptographicHash sha1Ctx(QCryptographicHash::Sha1);
    quint32 crcVal = 0;

    sha1Ctx.addData(dataCommon);
    crcVal = rotateLeft(crcVal ^ calcBlizzardHash(dataCommon), 3);

    sha1Ctx.addData(dataBlizzard);
    crcVal = rotateLeft(crcVal ^ calcBlizzardHash(dataBlizzard), 3);

    sha1UpdateInt32(sha1Ctx, 0x03F1379E);
    crcVal = rotateLeft(crcVal ^ 0x03F1379E, 3);

    sha1Ctx.addData(dataMapScript);
    crcVal = rotateLeft(crcVal ^ calcBlizzardHash(dataMapScript), 3);

    const char *componentFiles[] = {
        "war3map.w3e", "war3map.wpm", "war3map.doo", "war3map.w3u",
        "war3map.w3b", "war3map.w3d", "war3map.w3a", "war3map.w3q"
    };

    for (const char *compName : componentFiles) {
        QByteArray compData = readMpqFile(compName);
        if (!compData.isEmpty()) {
            sha1Ctx.addData(compData);
            crcVal = rotateLeft(crcVal ^ calcBlizzardHash(compData), 3);
        }
    }

    newData->mapSHA1Bytes = sha1Ctx.result();
    newData->mapCRC = toBytes(crcVal);
    LOG_INFO(QString("   ├─ 🔐 校验计算: MapCRC=0x%1").arg(QString::number(crcVal, 16).toUpper()));

    // 6. w3i 元数据解析 (关键修复：偏移量对齐)
    LOG_INFO("   ├─ ℹ️ 元数据解析 (w3i):");
    QByteArray w3iData = readMpqFile("war3map.w3i");

    if (!w3iData.isEmpty()) {
        QDataStream in(w3iData);
        in.setByteOrder(QDataStream::LittleEndian);
        in.setFloatingPointPrecision(QDataStream::SinglePrecision);

        auto readString = [&]() -> QString {
            QByteArray buf; char c;
            while (!in.atEnd()) {
                in.readRawData(&c, 1);
                if (c == 0) break;
                buf.append(c);
            }
            return QString::fromUtf8(buf);
        };

        // Header
        quint32 fileFormat; in >> fileFormat;
        quint32 saveCount;  in >> saveCount;
        quint32 editorVer;  in >> editorVer;

        LOG_INFO(QString("   │  ├─ 📄 格式版本: %1").arg(fileFormat));

        readString(); // Name
        readString(); // Author
        readString(); // Desc
        readString(); // Recommended

        in.skipRawData(32); // Camera Bounds
        in.skipRawData(16); // Camera Complements

        quint32 rawW, rawH, rawFlags;
        in >> rawW >> rawH >> rawFlags;
        quint8 tileset; in >> tileset;

        newData->mapWidth = toBytes16((quint16)rawW);
        newData->mapHeight = toBytes16((quint16)rawH);
        newData->mapOptions = rawFlags;

        LOG_INFO(QString("   │  ├─ 📏 尺寸: %1x%2").arg(rawW).arg(rawH));

        // --- Loading Screen (偏移量修复核心) ---
        quint32 bgNum; in >> bgNum; // Campaign Background Number

        if (fileFormat >= 25) {
            readString(); // LoadingScreenModel
        }

        readString(); // LoadingScreenText
        readString(); // LoadingScreenTitle
        readString(); // LoadingScreenSubtitle

        // [重要] Version 18+ 都有这个 LoadingScreenIndex
        // DotA 等 v25 地图如果这里漏读，后面所有数据都会错位 4 字节
        if (fileFormat >= 18) {
            quint32 lsIndex;
            in >> lsIndex;
        }

        // [重要] Version 25+ 才有 GameDataSet
        if (fileFormat >= 25) {
            quint32 gameDataSet; in >> gameDataSet;
            readString(); // Prologue Path
            readString(); // Prologue Text
            readString(); // Prologue Title
            readString(); // Prologue Subtitle
        }

        // --- Fog / Weather (v25+) ---
        if (fileFormat >= 25) {
            in.skipRawData(4); // Fog Type
            in.skipRawData(4); // Fog Start Z
            in.skipRawData(4); // Fog End Z
            in.skipRawData(4); // Fog Density
            in.skipRawData(4); // Fog Color
            in.skipRawData(4); // Weather ID
            readString();      // Sound Env
            in.skipRawData(1); // Light Env
            in.skipRawData(4); // Water Tint
        }

        // --- Script Mode (v31+) ---
        if (fileFormat >= 31) {
            in.skipRawData(4);
        }

        // --- Players ---
        quint32 numPlayers;
        in >> numPlayers;

        // 安全检查：防止偏移量依然错误导致读出天量数字
        if (numPlayers > 24) {
            LOG_ERROR(QString("   │  ⚠️ 玩家数量异常(%1)，可能 w3i 解析偏移错误").arg(numPlayers));
            numPlayers = 0;
        }
        newData->numPlayers = (quint8)numPlayers;
        LOG_INFO(QString("   │  ├─ 👥 玩家数量: %1").arg(numPlayers));

        for (quint32 i = 0; i < numPlayers; ++i) {
            W3iPlayer player;
            in >> player.id;
            in >> player.type;
            in >> player.race;
            in >> player.fix;
            player.name = readString();
            in >> player.startX >> player.startY;
            in >> player.allyLow >> player.allyHigh;

            newData->w3iPlayers.append(player);
            // 调试日志：检查是否解析正确 (P1 通常是 Red, ID=0)
            if (i < 4) {
                LOG_INFO(QString("   │  │  Slot %1: ID=%2 Type=%3 Race=%4").arg(i).arg(player.id).arg(player.type).arg(player.race));
            }
        }

        // --- Forces (Teams) ---
        quint32 numForces; in >> numForces;
        if (numForces > 12) numForces = 0;
        LOG_INFO(QString("   │  └─ 🚩 队伍数量: %1").arg(numForces));

        for (quint32 i = 0; i < numForces; ++i) {
            W3iForce force;
            in >> force.flags;
            in >> force.playerMasks;
            force.name = readString();
            newData->w3iForces.append(force);

            LOG_INFO(QString("   │     Team %1: Mask 0x%2").arg(i).arg(QString::number(force.playerMasks, 16).toUpper()));
        }

    } else {
        LOG_ERROR("   │  └─ ⚠️ w3i 文件缺失或无法读取");
    }

    SFileCloseArchive(hMpq);
    newData->valid = true;

    {
        QMutexLocker locker(&s_cacheMutex);
        s_cache.insert(cleanPath, newData);
        m_sharedData = newData;
    }

    LOG_INFO("   └─ ✅ [完成] 加载结束");
    return true;
}

void War3Map::clearUnusedCache()
{
    QMutexLocker locker(&s_cacheMutex);
    auto it = s_cache.begin();
    while (it != s_cache.end()) {
        if (it.value().use_count() <= 1) {
            it = s_cache.erase(it);
        } else {
            ++it;
        }
    }
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
    if (!m_sharedData->valid) return QByteArray();

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
    out.writeRawData(m_sharedData->mapWidth.constData(), 2);
    out.writeRawData(m_sharedData->mapHeight.constData(), 2);
    out.writeRawData(m_sharedData->mapCRC.constData(), 4);

    // 2. Map Path
    QString finalPath = netPathOverride.isEmpty() ?
                            "Maps\\Download\\" + QFileInfo(m_sharedData->mapPath).fileName() : netPathOverride;
    finalPath = finalPath.replace("/", "\\");

    out.writeRawData(finalPath.toLocal8Bit().constData(), finalPath.toLocal8Bit().size());
    out << (quint8)0;

    // 3. Host Name
    out.writeRawData(hostName.toUtf8().constData(), hostName.toUtf8().size());
    out << (quint8)0;

    // 4. Map Unknown
    out << (quint8)0;

    // 5. Map SHA1
    out.writeRawData(m_sharedData->mapSHA1Bytes.constData(), 20);

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

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
    // 1. 路径处理
    QString cleanPath = QFileInfo(mapPath).absoluteFilePath();
    QString fileName = QFileInfo(mapPath).fileName();

    LOG_INFO(QString("🗺️ [地图加载] 请求: %1").arg(fileName));

    // 2. 缓存检查
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

    // 3. 读取物理文件
    QFile file(mapPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("   └─ ❌ [错误] 无法打开文件: %1").arg(mapPath));
        return false;
    }
    newData->mapRawData = file.readAll();
    file.close();

    // 4. 计算基础校验信息 (Size, CRC32)
    newData->mapSize = toBytes((quint32)newData->mapRawData.size());
    uLong zCrc = crc32(0L, Z_NULL, 0);
    zCrc = crc32(zCrc, (const Bytef*)newData->mapRawData.constData(), newData->mapRawData.size());
    newData->mapInfo = toBytes((quint32)zCrc);

    LOG_INFO(QString("   ├─ 📂 文件读取: %1 bytes (CRC: %2)").arg(newData->mapRawData.size()).arg(QString::number(zCrc, 16).toUpper()));

    // 5. MPQ 操作
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

    // 辅助读取函数
    auto readLocalScript = [&](const QString &name) -> QByteArray {
        if (!s_priorityCrcDir.isEmpty()) {
            QFile f(s_priorityCrcDir + "/" + name);
            if (f.exists() && f.open(QIODevice::ReadOnly)) return f.readAll();
        }
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

    // 🔐 哈希计算
    QByteArray dataCommon = readLocalScript("common.j");
    QByteArray dataBlizzard = readLocalScript("blizzard.j");
    QByteArray dataMapScript = readMpqFile("war3map.j");
    if (dataMapScript.isEmpty()) dataMapScript = readMpqFile("scripts\\war3map.j");

    if (dataCommon.isEmpty() || dataBlizzard.isEmpty() || dataMapScript.isEmpty()) {
        LOG_ERROR("   └─ ❌ [错误] 核心脚本缺失，无法计算 Hash");
        SFileCloseArchive(hMpq);
        return false;
    }

    QCryptographicHash sha1Ctx(QCryptographicHash::Sha1);

    // 基于 war3 1.26a 反汇编计算 (game.dll + 3B1A20)

    // Step 1: 计算基础 Hash
    quint32 hCommon = calcBlizzardHash(dataCommon);
    quint32 hBlizz = calcBlizzardHash(dataBlizzard);
    quint32 hScript = calcBlizzardHash(dataMapScript);

    // Step 2: 汇编 game.dll + 3B1AEB: xor ebx, ebp (Blizz ^ Common)
    quint32 crcVal = hBlizz ^ hCommon;

    // Step 3: 汇编 game.dll + 3B1AED: rol ebx, 3
    crcVal = rotateLeft(crcVal, 3);

    // Step 4: 汇编 game.dll + 3B1AF0: xor ebx, 3F1379E (Magic)
    crcVal = crcVal ^ 0x03F1379E;

    // Step 5: 汇编 game.dll + 3B1AF6: rol ebx, 3
    crcVal = rotateLeft(crcVal, 3);

    // Step 6: 汇编 game.dll + 3B1B27: xor edx, ebx (Script ^ Intermediate)
    crcVal = hScript ^ crcVal;

    // Step 7: 汇编 game.dll + 3B1B2B: rol edx, 3
    crcVal = rotateLeft(crcVal, 3);

    // SHA1 填充
    sha1Ctx.addData(dataCommon);
    sha1Ctx.addData(dataBlizzard);
    sha1UpdateInt32(sha1Ctx, 0x03F1379E);
    sha1Ctx.addData(dataMapScript);

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

    // ℹ️ 元数据解析
    LOG_INFO("   ├─ ℹ️ 元数据解析 (w3i):");
    QByteArray w3iData = readMpqFile("war3map.w3i");

    if (!w3iData.isEmpty()) {
        QDataStream in(w3iData);
        in.setByteOrder(QDataStream::LittleEndian);
        in.setFloatingPointPrecision(QDataStream::SinglePrecision);

        auto readString = [&]() -> QString {
            QByteArray buf; char c;
            while (!in.atEnd()) {
                in >> (quint8&)c;
                if (c == 0) break;
                buf.append(c);
            }
            return QString::fromUtf8(buf);
        };

        // 1. 基础头部
        quint32 fileFormat; in >> fileFormat; // 18 or 25
        quint32 saveCount;  in >> saveCount;
        quint32 editorVer;  in >> editorVer;

        LOG_INFO(QString("   │  ├─ 📄 格式版本: %1").arg(fileFormat));

        readString(); // Map Name
        readString(); // Author
        readString(); // Desc
        readString(); // Players Rec

        // 2. 摄像机与尺寸
        in.skipRawData(8 * 4); // Camera Bounds
        in.skipRawData(4 * 4); // Camera Bounds Complements

        quint32 rawW, rawH, rawFlags;
        in >> rawW >> rawH >> rawFlags;
        quint8 tileset; in >> tileset;

        newData->mapWidth = toBytes16((quint16)rawW);
        newData->mapHeight = toBytes16((quint16)rawH);
        newData->mapOptions = rawFlags;

        LOG_INFO(QString("   │  ├─ 📏 尺寸: %1x%2 (Flags: 0x%3)").arg(rawW).arg(rawH).arg(QString::number(rawFlags, 16).toUpper()));

        // 3. 加载屏幕设置
        quint32 bgNum; in >> bgNum;

        // [Version 25+] 自定义加载屏幕模型
        if (fileFormat >= 25) {
            readString(); // Custom Model Path
        }

        readString(); // Text
        readString(); // Title
        readString(); // Subtitle

        // 4. 游戏数据设置
        if (fileFormat >= 25) {
            quint32 gameDataSet; in >> gameDataSet;
            readString(); // Prologue Path
            readString(); // Prologue Text
            readString(); // Prologue Title
            readString(); // Prologue Sub
        } else if (fileFormat == 18) {
            quint32 loadingScreenID_v18;
            in >> loadingScreenID_v18; // Loading Screen Number for v18
        }

        // 5. [Version 25+] 地形雾、天气、环境
        if (fileFormat >= 25) {
            in.skipRawData(4); // Fog Type
            in.skipRawData(4); // Fog Start Z
            in.skipRawData(4); // Fog End Z
            in.skipRawData(4); // Fog Density
            in.skipRawData(4); // Fog Color (RBGA)

            in.skipRawData(4); // Global Weather ID
            readString();      // Sound Env
            in.skipRawData(1); // Light Env Tileset
            in.skipRawData(4); // Water Tinting (RGBA)
        }

        // 6. [Version 31+] 脚本语言
        if (fileFormat >= 31) {
            in.skipRawData(4);
        }

        // 7. 玩家数据解析
        quint32 numPlayers;
        in >> numPlayers;

        if (numPlayers > 32) numPlayers = 0;
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

            LOG_INFO(QString("   │  │  P%1 Type:%2 Race:%3")
                         .arg(player.id).arg(player.type).arg(player.race));
        }

        // 8. 队伍数据 (Forces)
        quint32 numForces; in >> numForces;
        LOG_INFO(QString("   │  └─ 🚩 队伍数量: %1").arg(numForces));

        for (quint32 i = 0; i < numForces; ++i) {
            W3iForce force;
            in >> force.flags;
            in >> force.playerMasks;
            force.name = readString();
            newData->w3iForces.append(force);

            LOG_INFO(QString("   │     Force %1: Mask 0x%2").arg(i).arg(QString::number(force.playerMasks, 16).toUpper()));
        }

    } else {
        LOG_INFO("   │  └─ ⚠️ w3i 缺失");
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
            LOG_INFO(QString("[War3Map] 🧹 清理闲置地图缓存: %1").arg(it.key()));
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
        if (c % 2 == 0) {
            chunk.append((char)(c + 1));
        } else {
            chunk.append((char)c);
            mask |= (1 << ((i % 7) + 1));
        }
        if ((i % 7) == 6 || i == data.size() - 1) {
            result.append((char)mask);
            result.append(chunk);
            chunk.clear();
            mask = 1;
        }
    }
    return result;
}

QByteArray War3Map::getEncodedStatString(const QString &hostName, const QString &netPathOverride)
{
    if (!isValid()) {
        LOG_ERROR("❌ [StatString] 生成失败: 地图数据无效");
        return QByteArray();
    }

    QByteArray rawData;
    QDataStream out(&rawData, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    LOG_INFO("🌳 [StatString] 开始构建原始数据包...");

    // 1. Map Flags
    QByteArray gameFlagsBytes = getMapGameFlags();
    quint32 gameFlagsInt;
    QDataStream ds(gameFlagsBytes);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds >> gameFlagsInt;

    // 打印 Flags 详情
    LOG_INFO(QString("   ├─ 🚩 GameFlags (Int): 0x%1").arg(QString::number(gameFlagsInt, 16).toUpper().rightJustified(8, '0')));

    // 写入 Flags (4字节) + 0x00 分隔符
    out << gameFlagsInt << (quint8)0;

    // 2. Map Size & CRC
    quint16 width = 0, height = 0;
    quint32 crc = 0;
    if (m_sharedData->mapWidth.size() >= 2) width = qFromLittleEndian<quint16>(m_sharedData->mapWidth.constData());
    if (m_sharedData->mapHeight.size() >= 2) height = qFromLittleEndian<quint16>(m_sharedData->mapHeight.constData());
    if (m_sharedData->mapCRC.size() >= 4) crc = qFromLittleEndian<quint32>(m_sharedData->mapCRC.constData());

    out.writeRawData(m_sharedData->mapWidth.constData(), 2);
    out.writeRawData(m_sharedData->mapHeight.constData(), 2);
    out.writeRawData(m_sharedData->mapCRC.constData(), 4);

    LOG_INFO(QString("   ├─ 📏 Size: %1 x %2").arg(width).arg(height));
    LOG_INFO(QString("   ├─ 🔐 CRC: 0x%1").arg(QString::number(crc, 16).toUpper().rightJustified(8, '0')));

    // 3. Map Path
    QString finalPath = netPathOverride.isEmpty() ?
                            "Maps\\Download\\" + QFileInfo(m_sharedData->mapPath).fileName() : netPathOverride;
    finalPath = finalPath.replace("/", "\\");

    // 确保路径转为 Local8Bit (War3 不支持 UTF-8 路径)
    QByteArray pathBytes = finalPath.toLocal8Bit();
    out.writeRawData(pathBytes.constData(), pathBytes.size());
    out << (quint8)0; // Path Terminator

    LOG_INFO(QString("   ├─ 📂 Path: %1 (Hex: %2)").arg(finalPath, QString(pathBytes.toHex())));

    // 4. Host Name
    QByteArray hostBytes = hostName.toUtf8();
    out.writeRawData(hostBytes.constData(), hostBytes.size());
    out << (quint8)0; // Host Name Terminator

    LOG_INFO(QString("   ├─ 👤 Host: %1").arg(hostName));

    // 5. Separator (Map Unknown)
    out << (quint8)0;

    // 6. Map SHA1
    QByteArray sha1 = m_sharedData->mapSHA1Bytes;
    if (sha1.size() != 20) {
        LOG_WARNING("⚠️ SHA1 长度异常，进行补零");
        sha1.resize(20);
    }
    out.writeRawData(sha1.constData(), 20);

    LOG_INFO(QString("   └─ 🔑 SHA1: %1").arg(QString(sha1.toHex().toUpper())));

    QByteArray encoded = encodeStatString(rawData);

    analyzeStatString("自检验证", encoded);

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

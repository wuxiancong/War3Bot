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
    // 1. 规范化路径 (作为 Cache Key)
    QString cleanPath = QFileInfo(mapPath).absoluteFilePath();
    QString fileName = QFileInfo(mapPath).fileName();

    // --- [日志根节点] ---
    LOG_INFO(QString("🗺️ [地图加载] 请求: %1").arg(fileName));

    // 2. 检查缓存
    {
        QMutexLocker locker(&s_cacheMutex);
        if (s_cache.contains(cleanPath)) {
            // 命中缓存！
            m_sharedData = s_cache[cleanPath];

            // 简单校验一下缓存是否有效
            if (m_sharedData && m_sharedData->valid) {
                LOG_INFO(QString("   └─ ⚡ [缓存命中] 内存复用成功 (Ref: %1)")
                             .arg(m_sharedData.use_count()));
                return true;
            }
            // 如果缓存无效，移除并重新加载
            s_cache.remove(cleanPath);
        }
    }

    // 3. 缓存未命中，开始从磁盘加载
    LOG_INFO("   ├─ 💾 [缓存未命中] 启动磁盘加载流程...");

    // 创建新的共享数据对象
    auto newData = std::make_shared<War3MapSharedData>();
    newData->mapPath = cleanPath;
    newData->valid = false;

    // --- 文件读取 ---
    QFile file(mapPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("   └─ ❌ [错误] 无法打开文件: %1").arg(mapPath));
        return false;
    }

    newData->mapRawData = file.readAll(); // [大内存分配]
    file.close();

    // --- 计算基础信息 ---
    newData->mapSize = toBytes((quint32)newData->mapRawData.size());

    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef*)newData->mapRawData.constData(), newData->mapRawData.size());
    newData->mapInfo = toBytes((quint32)crc);

    LOG_INFO(QString("   ├─ 📂 文件读取: %1 bytes").arg(newData->mapRawData.size()));
    LOG_INFO(QString("   │  └─ 🏷️ MapInfo CRC32: 0x%1").arg(QString::number(crc, 16).toUpper()));

    // --- MPQ 操作 ---
    HANDLE hMpq = NULL;
    QString nativePath = QDir::toNativeSeparators(mapPath);
#ifdef UNICODE
    const wchar_t *pathStr = (const wchar_t*)nativePath.utf16();
#else
    const char *pathStr = nativePath.toLocal8Bit().constData();
#endif

    if (!SFileOpenArchive(pathStr, 0, MPQ_OPEN_READ_ONLY, &hMpq)) {
        LOG_ERROR("   └─ ❌ [错误] MPQ 归档损坏或无法读取");
        return false;
    }

    // 定义读取 Lambda (保持不变)
    auto readLocalScript = [&](const QString &fileName) -> QByteArray {
        if (!s_priorityCrcDir.isEmpty()) {
            QFile f(s_priorityCrcDir + "/" + fileName);
            if (f.exists() && f.open(QIODevice::ReadOnly)) return f.readAll();
        }
        QFile fDefault("war3files/" + fileName);
        if (fDefault.open(QIODevice::ReadOnly)) return fDefault.readAll();
        return QByteArray();
    };

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

    // --- 核心脚本校验 ---
    QByteArray dataCommon = readLocalScript("common.j");
    QByteArray dataBlizzard = readLocalScript("blizzard.j");

    QByteArray dataMapScript = readMpqFile("war3map.j");
    QString scriptName = "war3map.j";
    if (dataMapScript.isEmpty()) { dataMapScript = readMpqFile("scripts\\war3map.j"); scriptName = "scripts\\war3map.j"; }
    if (dataMapScript.isEmpty()) { dataMapScript = readMpqFile("war3map.lua"); scriptName = "war3map.lua"; }

    if (dataCommon.isEmpty() || dataBlizzard.isEmpty() || dataMapScript.isEmpty()) {
        LOG_ERROR("   └─ ❌ [错误] 核心脚本缺失 (common/blizzard/war3map.j)");
        SFileCloseArchive(hMpq);
        return false;
    }

    LOG_INFO("   ├─ 📜 核心脚本校验:");
    LOG_INFO(QString("   │  ├─ common.j:   %1 bytes").arg(dataCommon.size()));
    LOG_INFO(QString("   │  ├─ blizzard.j: %1 bytes").arg(dataBlizzard.size()));
    LOG_INFO(QString("   │  └─ map_script: %1 bytes (%2)").arg(dataMapScript.size()).arg(scriptName));

    // --- Hash 计算初始化 ---
    QCryptographicHash sha1Ctx(QCryptographicHash::Sha1);
    sha1Ctx.addData(dataCommon);
    sha1Ctx.addData(dataBlizzard);
    sha1UpdateInt32(sha1Ctx, 0x03F1379E);
    sha1Ctx.addData(dataMapScript);

    quint32 crcVal = 0;
    quint32 hCommon = calcBlizzardHash(dataCommon);
    quint32 hBlizz = calcBlizzardHash(dataBlizzard);
    quint32 hScript = calcBlizzardHash(dataMapScript);
    crcVal = rotateLeft(hBlizz ^ hCommon, 3) ^ 0x03F1379E;
    crcVal = rotateLeft(crcVal, 3);
    crcVal = rotateLeft(hScript ^ crcVal, 3);

    const char *componentFiles[] = {
        "war3map.w3e", "war3map.wpm", "war3map.doo", "war3map.w3u",
        "war3map.w3b", "war3map.w3d", "war3map.w3a", "war3map.w3q"
    };

    QStringList foundComponents;

    for (const char *compName : componentFiles) {
        QByteArray compData = readMpqFile(compName);
        if (!compData.isEmpty()) {
            sha1Ctx.addData(compData);
            crcVal = rotateLeft(crcVal ^ calcBlizzardHash(compData), 3);
            foundComponents << compName;
        }
    }

    if (!foundComponents.isEmpty()) {
        LOG_INFO(QString("   ├─ 🧩 包含组件: %1").arg(foundComponents.join(", ")));
    }

    newData->mapSHA1Bytes = sha1Ctx.result();
    newData->mapCRC = toBytes(crcVal);

    // --- 元数据 w3i 解析 ---
    LOG_INFO("   ├─ ℹ️ 元数据解析 (w3i):");
    QByteArray w3iData = readMpqFile("war3map.w3i");
    if (!w3iData.isEmpty()) {
        QDataStream in(w3iData);
        in.setByteOrder(QDataStream::LittleEndian);
        in.setFloatingPointPrecision(QDataStream::SinglePrecision); // W3I 使用 float

        // 辅助函数：读取 C 风格字符串 (以 \0 结尾)
        auto readString = [&]() -> QString {
            QByteArray buffer;
            char c;
            while (!in.atEnd()) {
                in >> (quint8&)c;
                if (c == 0) break;
                buffer.append(c);
            }
            return QString::fromUtf8(buffer);
        };

        // 1. 文件头
        quint32 fileFormat; in >> fileFormat; // 版本号: 18, 25, 28, 31
        quint32 saveCount; in >> saveCount;
        quint32 editorVer; in >> editorVer;

        // 2. 地图信息字符串 (必须读取以移动指针)
        /* QString mapName =    */ readString();
        /* QString mapAuthor =  */ readString();
        /* QString mapDesc =    */ readString();
        /* QString recPlayers = */ readString();

        // 3. 摄像机边界 (8个float)
        in.skipRawData(8 * 4);
        // 摄像机边界补充 (4个int)
        in.skipRawData(4 * 4);

        // 4. 地图尺寸与标志
        quint32 rawW, rawH, rawFlags;
        in >> rawW >> rawH >> rawFlags;
        quint8 tileset; in >> tileset;

        newData->mapWidth       = toBytes16((quint16)rawW);
        newData->mapHeight      = toBytes16((quint16)rawH);
        newData->mapOptions     = rawFlags;

        LOG_INFO(QString("   │  ├─ 📏 尺寸: %1 x %2").arg(rawW).arg(rawH));
        LOG_INFO(QString("   │  └─ 🏳️ 标志: 0x%1").arg(QString::number(rawFlags, 16).toUpper()));

        // 5. 加载屏幕信息
        quint32 loadingScreenID;in >> loadingScreenID;
        /* path =               */ readString();
        /* text =               */ readString();
        /* title =              */ readString();
        /* sub =                */ readString();

        // 6. 游戏数据设置
        quint32 gameDataSet;    in >> gameDataSet;
        /* prologuePath =       */ readString();
        /* prologueText =       */ readString();
        /* prologueTitle =      */ readString();
        /* prologueSub =        */ readString();

        if (fileFormat >= 25) {
            in.skipRawData(4); // Fog Type (int)
            in.skipRawData(4); // Fog Start Z (float)
            in.skipRawData(4); // Fog End Z (float)
            in.skipRawData(4); // Fog Density (float)
            in.skipRawData(4); // Fog Color (int)

            in.skipRawData(4); // Global Weather ID (int)
            readString();      // Sound Environment (String)
            in.skipRawData(1); // Light Environment Tileset (char)
            in.skipRawData(4); // Water Tinting Color (struct)
        }

        if (fileFormat >= 31) {
            in.skipRawData(4); // Script Language (int)
        }

        // 7. 玩家数据解析 (Player Data)
        quint32 numPlayers;     in >> numPlayers;

        if (numPlayers > 32) {
            LOG_ERROR(QString("   │  └─ ❌ [严重错误] 玩家数量异常: %1 (解析偏移导致) - 强制重置为 0").arg(numPlayers));
            numPlayers = 0;
        }

        newData->numPlayers = (quint8)numPlayers;
        LOG_INFO(QString("   │  ├─ 👥 预设玩家: %1 人").arg(numPlayers));

        for (quint32 i = 0; i < numPlayers; ++i) {
            W3iPlayer player;
            in >> player.id;        // 4 bytes
            in >> player.type;      // 4 bytes (1=Human, 2=Comp)
            in >> player.race;      // 4 bytes (1=Hum, 2=Orc, 3=UD, 4=NE)
            in >> player.fix;       // 4 bytes (Fixed Start Position)
            player.name = readString(); // Player Name
            in >> player.startX >> player.startY >> player.startZ; // 3 * 4 bytes (float)

            // Skip: Unknown(4) + Unknown(4)
            in.skipRawData(4 + 4);

            newData->w3iPlayers.append(player);

            QString typeStr = (player.type == 1) ? "Human" : (player.type == 2 ? "Comp" : "Other");
            LOG_DEBUG(QString("      - P%1 [%2] Race:%3 Name:%4")
                          .arg(player.id).arg(typeStr).arg(player.race).arg(player.name));
        }

        // 8. 队伍数据解析 (Force Data)
        quint32 numForces; in >> numForces;

        if (numForces > 32) {
            LOG_ERROR(QString("   │  └─ ❌ [严重错误] 队伍数量异常: %1 (解析偏移导致) - 强制重置为 0").arg(numForces));
            numForces = 0;
        }

        LOG_INFO(QString("   │  └─ 🚩 预设队伍: %1 队").arg(numForces));

        for (quint32 i = 0; i < numForces; ++i) {
            W3iForce force;
            in >> force.flags;
            in >> force.playerMasks;
            force.name = readString();

            newData->w3iForces.append(force);

            LOG_DEBUG(QString("      - Force %1: Mask 0x%2").arg(i).arg(QString::number(force.playerMasks, 16)));
        }

    } else {
        LOG_INFO("   │  └─ ⚠️ w3i 文件缺失，使用默认值");
    }

    SFileCloseArchive(hMpq);

    // 标记为有效
    newData->valid = true;

    // --- 最终结果输出 ---
    LOG_INFO("   ├─ 🔐 最终校验值:");
    LOG_INFO(QString("   │  ├─ XORO CRC: 0x%1").arg(QString(newData->mapCRC.toHex().toUpper())));
    LOG_INFO(QString("   │  └─ SHA1:     %1").arg(QString(newData->mapSHA1Bytes.toHex().toUpper())));

    // 4. 存入缓存并赋值给当前实例
    {
        QMutexLocker locker(&s_cacheMutex);
        s_cache.insert(cleanPath, newData);
        m_sharedData = newData;
    }

    LOG_INFO("   └─ ✅ [完成] 数据已存入全局缓存");
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
    out << (quint8)0; // Map Path Terminator

    // 3. Host Name
    out.writeRawData(hostName.toUtf8().constData(), hostName.toUtf8().size());
    out << (quint8)0; // Host Name Terminator

    // 4. Map Unknown
    // 根据协议：(UINT8) Map unknown (possibly a STRING with just the null terminator)
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

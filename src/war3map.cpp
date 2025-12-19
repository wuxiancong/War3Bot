#include "war3map.h"
#include "logger.h"
#include <QDir>
#include <QFile>
#include <QtEndian>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDebug>

#include <zlib.h>
#include "StormLib.h"

QString War3Map::s_priorityCrcDir = "";

static QByteArray toBytes(quint32 val) {
    QByteArray b; b.resize(4); qToLittleEndian(val, (uchar*)b.data()); return b;
}
static QByteArray toBytes16(quint16 val) {
    QByteArray b; b.resize(2); qToLittleEndian(val, (uchar*)b.data()); return b;
}

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

    // 1. 计算 Map Size
    quint32 mapSizeInt = file.size();
    m_mapSize = toBytes(mapSizeInt);
    QByteArray mapData = file.readAll();
    file.close();

    // 2. 计算 Map Info (Real CRC32)
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef*)mapData.constData(), mapData.size());
    m_mapInfo = toBytes((quint32)crc);

    // 3. 打开 MPQ 档案
    HANDLE hMpq = NULL;
    QString nativePath = QDir::toNativeSeparators(mapPath);

    DWORD flags = MPQ_OPEN_READ_ONLY;

#ifdef UNICODE
    const wchar_t *pathStr = (const wchar_t*)nativePath.utf16();
#else
    const char *pathStr = nativePath.toLocal8Bit().constData();
#endif

    if (!SFileOpenArchive(pathStr, 0, flags, &hMpq)) {
#ifdef Q_OS_WIN
        DWORD err = GetLastError();
        LOG_ERROR(QString("[War3Map] ❌ StormLib 无法打开 MPQ (Err: %1): %2").arg(err).arg(nativePath));
#else
        LOG_ERROR(QString("[War3Map] ❌ StormLib 无法打开 MPQ: %1").arg(nativePath));
#endif
        return false;
    }

    // 4. 计算 Map CRC (XORO) & SHA1
    QCryptographicHash sha1(QCryptographicHash::Sha1);
    quint32 xoroVal = 0;

    auto processFile = [&](const QByteArray &data) {
        xoroVal = xoroVal ^ xorRotateLeft((unsigned char*)data.constData(), data.size());
        sha1.addData(data);
    };

    auto tryReadFile = [&](const QString &fileName) -> bool {
        // 1. 优先尝试热门 CRC 目录
        if (!s_priorityCrcDir.isEmpty()) {
            QString priorityPath = s_priorityCrcDir + "/" + fileName;
            QFile f(priorityPath);
            if (f.exists() && f.open(QIODevice::ReadOnly)) {
                processFile(f.readAll());
                f.close();
                LOG_INFO(QString("[War3Map] ✅ 使用热门CRC文件: %1").arg(priorityPath));
                return true;
            }
        }

        // 2. 回退到默认目录
        QString defaultPath = "war3files/" + fileName;
        QFile fDefault(defaultPath);
        if (fDefault.open(QIODevice::ReadOnly)) {
            processFile(fDefault.readAll());
            fDefault.close();
            LOG_INFO(QString("[War3Map] 使用默认文件: %1").arg(defaultPath));
            return true;
        }

        return false;
    };

    // 读取 common.j
    if (!tryReadFile("common.j")) {
        LOG_WARNING("[War3Map] ⚠️ 警告：找不到 common.j，CRC计算将必定错误！");
    }

    // 读取 blizzard.j
    if (!tryReadFile("blizzard.j")) {
        LOG_WARNING("[War3Map] ⚠️ 警告：找不到 blizzard.j");
    }

    // 魔数处理
    xoroVal = ROTL(xoroVal, 3);
    xoroVal = ROTL(xoroVal ^ 0x03F1379E, 3);
    sha1.addData("\x9E\x37\xF1\x03", 4);

    // 寻找地图内的脚本文件
    const char *scriptFiles[] = { "war3map.j", "scripts\\war3map.j", "war3map.w3e", "war3map.wpm", "war3map.doo", "war3map.w3u", "war3map.w3b", "war3map.w3d", "war3map.w3a", "war3map.w3q" };

    int scriptsFound = 0;
    for (const char *filename : scriptFiles) {
        HANDLE hFile = NULL;
        if (SFileOpenFileEx(hMpq, filename, 0, &hFile)) {
            DWORD fileSize = SFileGetFileSize(hFile, NULL);
            if (fileSize > 0 && fileSize != 0xFFFFFFFF) {
                QByteArray buffer;
                buffer.resize(fileSize);
                DWORD bytesRead = 0;
                SFileReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL);

                xoroVal = ROTL(xoroVal ^ xorRotateLeft((unsigned char*)buffer.constData(), bytesRead), 3);
                sha1.addData(buffer);
                scriptsFound++;
            }
            SFileCloseFile(hFile);
        }
    }

    m_mapCRC = toBytes(xoroVal);
    m_mapSHA1 = sha1.result();

    LOG_INFO(QString("[War3Map] 地图校验完毕. 脚本文件数: %1").arg(scriptsFound));
    LOG_INFO(QString("[War3Map] Map CRC (XORO): %1").arg(QString(m_mapCRC.toHex().toUpper())));
    LOG_INFO(QString("[War3Map] Map SHA1:       %1").arg(QString(m_mapSHA1.toHex().toUpper())));

    // 5. 解析 war3map.w3i
    HANDLE hW3i = NULL;
    if (SFileOpenFileEx(hMpq, "war3map.w3i", 0, &hW3i)) {
        DWORD fileSize = SFileGetFileSize(hW3i, NULL);
        if (fileSize > 0) {
            QByteArray w3iData;
            w3iData.resize(fileSize);
            DWORD read = 0;
            SFileReadFile(hW3i, w3iData.data(), fileSize, &read, NULL);

            QDataStream in(w3iData);
            in.setByteOrder(QDataStream::LittleEndian);

            quint32 fileFormat;
            in >> fileFormat;

            if (fileFormat == 18 || fileFormat == 25) {
                in.skipRawData(4); // saves
                in.skipRawData(4); // editor ver
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
        }
        SFileCloseFile(hW3i);
    } else {
        LOG_WARNING("[War3Map] ⚠️ 无法读取 war3map.w3i (可能是受保护的地图)");
    }

    SFileCloseArchive(hMpq);
    m_valid = true;
    return true;
}

quint32 War3Map::xorRotateLeft(unsigned char *data, quint32 length) {
    quint32 i = 0, val = 0;
    if (length > 3) {
        while (i < length - 3) {
            val = ROTL(val ^ ((quint32)data[i] + ((quint32)data[i+1]<<8) + ((quint32)data[i+2]<<16) + ((quint32)data[i+3]<<24)), 3);
            i += 4;
        }
    }
    while (i < length) { val = ROTL(val ^ data[i], 3); ++i; }
    return val;
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

    QString finalPath = netPathOverride.isEmpty() ?
                            "Maps\\Download\\" + QFileInfo(m_mapPath).fileName() : netPathOverride;
    finalPath = finalPath.replace("/", "\\");

    out.writeRawData(finalPath.toLocal8Bit().constData(), finalPath.toLocal8Bit().size());
    out << (quint8)0;
    out.writeRawData(hostName.toUtf8().constData(), hostName.toUtf8().size());
    out << (quint8)0;
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

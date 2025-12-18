#include "war3map.h"
#include "logger.h"
#include <QDir>
#include <QFile>
#include <QtEndian>
#include <QFileInfo>
#include <QCryptographicHash>

#include <zlib.h>
#include "StormLib.h"

War3Map::War3Map() : m_valid(false), m_numPlayers(0), m_numTeams(0)
{
}

War3Map::~War3Map()
{
}

static QByteArray toBytes(quint32 val) {
    QByteArray b;
    b.resize(4);
    qToLittleEndian(val, (uchar*)b.data());
    return b;
}

static QByteArray toBytes16(quint16 val) {
    QByteArray b;
    b.resize(2);
    qToLittleEndian(val, (uchar*)b.data());
    return b;
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

    // ★★★ 这里的 flags 是关键 ★★★
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

    // 读取外部 common.j
    QFile commonJ("war3files/common.j");
    if (commonJ.open(QIODevice::ReadOnly)) {
        processFile(commonJ.readAll());
        commonJ.close();
    } else {
        LOG_WARNING("[War3Map] ⚠️ 警告：找不到 war3files/common.j，CRC计算将必定错误！");
    }

    // 读取外部 blizzard.j
    QFile blizzardJ("war3files/blizzard.j");
    if (blizzardJ.open(QIODevice::ReadOnly)) {
        processFile(blizzardJ.readAll());
        blizzardJ.close();
    } else {
        LOG_WARNING("[War3Map] ⚠️ 警告：找不到 war3files/blizzard.j");
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

quint32 War3Map::xorRotateLeft(unsigned char *data, quint32 length)
{
    quint32 i = 0;
    quint32 val = 0;
    if (length > 3) {
        while (i < length - 3) {
            val = ROTL(val ^ ( (quint32)data[i] + ((quint32)data[i+1] << 8) +
                              ((quint32)data[i+2] << 16) + ((quint32)data[i+3] << 24) ), 3);
            i += 4;
        }
    }
    while (i < length) {
        val = ROTL(val ^ data[i], 3);
        ++i;
    }
    return val;
}

QByteArray War3Map::encodeStatString(const QByteArray &data)
{
    QByteArray result;
    unsigned char mask = 1;
    QByteArray chunk;

    for (int i = 0; i < data.size(); ++i) {
        unsigned char c = (unsigned char)data[i];

        if (c % 2 == 0) {
            chunk.append((char)(c + 1));
        } else {
            chunk.append((char)c);
            mask |= 1 << ((i % 7) + 1);
        }

        if ((i % 7) == 6 || i == data.size() - 1) {
            // 1. 先写入 Mask
            result.append((char)mask);

            // 2. 再写入数据块
            result.append(chunk);

            // 重置
            chunk.clear();
            mask = 1;
        }
    }

    return result;
}

QByteArray War3Map::getEncodedStatString(const QString &hostName, const QString &netPathOverride)
{
    if (!m_valid) {
        LOG_ERROR("[War3Map] ❌ 尝试获取 StatString 但地图未有效加载");
        return QByteArray();
    }

    QByteArray rawData;
    QDataStream out(&rawData, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    // 1. 取出 w3i 中的 flags
    quint32 finalFlags = m_mapOptions & 0xFFFF0000;

    // 2. 注入房间设置
    finalFlags = m_mapOptions | 0x00000002;
    finalFlags |= 0x00004000;
    finalFlags |= 0x00000002;

    out << finalFlags  << (quint8)0;
    out.writeRawData(m_mapWidth.constData(), 2);
    out.writeRawData(m_mapHeight.constData(), 2);
    out.writeRawData(m_mapCRC.constData(), 4);

    QString finalPath;
    if (!netPathOverride.isEmpty()) {
        finalPath = netPathOverride;
    } else {
        QFileInfo fi(m_mapPath);
        finalPath = "Maps\\Download\\" + fi.fileName();
    }

    finalPath = QDir::toNativeSeparators(finalPath);

    out.writeRawData(finalPath.toLocal8Bit().constData(), finalPath.toLocal8Bit().size());
    out << (quint8)0;

    out.writeRawData(hostName.toUtf8().constData(), hostName.toUtf8().size());
    out << (quint8)0;

    out.writeRawData(m_mapSHA1.constData(), 20);

    // ================= [调试日志：编码前 RawData] =================
    QString rawHex, rawAscii;
    for (char c : qAsConst(rawData)) {
        rawHex.append(QString("%1 ").arg((quint8)c, 2, 16, QChar('0')).toUpper());
        rawAscii.append((c >= 32 && c <= 126) ? c : '.');
    }
    LOG_INFO(QString("[War3Map] RawData (Size %1):").arg(rawData.size()));
    LOG_INFO(QString("   HEX: %1").arg(rawHex));
    LOG_INFO(QString("   ASC: %1").arg(rawAscii));
    // ============================================================

    QByteArray result = encodeStatString(rawData);

    // ================= [调试日志：编码后 EncodedData] =================
    QString encHex, encAscii;
    for (char c : qAsConst(result)) {
        encHex.append(QString("%1 ").arg((quint8)c, 2, 16, QChar('0')).toUpper());
        encAscii.append((c >= 32 && c <= 126) ? c : '.');
    }
    LOG_INFO(QString("[War3Map] EncodedData (Size %1):").arg(result.size()));
    LOG_INFO(QString("   HEX: %1").arg(encHex));
    LOG_INFO(QString("   ASC: %1").arg(encAscii));
    // ==============================================================

    return result;
}

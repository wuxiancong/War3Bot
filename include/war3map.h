#ifndef WAR3MAP_H
#define WAR3MAP_H

#include <QVector>
#include <QString>
#include <QByteArray>
#include <QDataStream>

// 游戏类型枚举
enum W3GameFlag {
    W3FLAG_TEAMSTOGETHER = 0x00004000,
    W3FLAG_FIXEDTEAMS    = 0x00060000,
    W3FLAG_UNITSHARE     = 0x01000000,
    W3FLAG_RANDOMHERO    = 0x02000000,
    W3FLAG_RANDOMRACES   = 0x04000000
};

class War3Map
{
public:
    explicit War3Map();
    ~War3Map();

    /**
     * @brief 加载并解析地图
     * @param mapFilePath .w3x/.w3m 文件的绝对路径
     * @return 成功返回 true
     */
    bool load(const QString &mapFilePath);

    // === Getters ===
    bool isValid() const { return m_valid; }
    QString getMapPath() const { return m_mapPath; }

    // 用于构建 StatString 的原始数据
    QByteArray getMapSize() const { return m_mapSize; }     // 4 bytes
    QByteArray getMapInfo() const { return m_mapInfo; }     // 4 bytes (Real CRC32)
    QByteArray getMapCRC() const { return m_mapCRC; }       // 4 bytes (Xoro CRC)
    QByteArray getMapSHA1() const { return m_mapSHA1; }     // 20 bytes

    // 地图基本信息
    quint32 getMapOptions() const { return m_mapOptions; }
    QByteArray getMapWidth() const { return m_mapWidth; }   // 2 bytes
    QByteArray getMapHeight() const { return m_mapHeight; } // 2 bytes
    int getNumPlayers() const { return m_numPlayers; }
    int getNumTeams() const { return m_numTeams; }

    /**
     * @brief 生成最终用于发送的编码后 StatString
     * @param hostName 主机名 (创建者名字)
     */
    QByteArray getEncodedStatString(const QString &hostName, const QString &netPathOverride = "");
private:
    bool m_valid;
    QString m_mapPath;

    // 核心校验数据
    QByteArray m_mapSize;
    QByteArray m_mapInfo;
    QByteArray m_mapCRC;
    QByteArray m_mapSHA1;

    // 地图参数
    quint32 m_mapOptions;
    QByteArray m_mapWidth;
    QByteArray m_mapHeight;
    quint32 m_numPlayers;
    quint32 m_numTeams;

    // 计算标准的 CRC32
    quint32 calculateCRC32(const QByteArray &data);

    // StatString 编码
    QByteArray encodeStatString(const QByteArray &data);

    // Xoro 旋转算法 (用于 MapCRC)
    quint32 xorRotateLeft(unsigned char *data, quint32 length);
    inline quint32 ROTL(quint32 x, int n) { return (x << n) | (x >> (32 - n)); }
};

#endif // WAR3MAP_H

#ifndef WAR3MAP_H
#define WAR3MAP_H

#include <QString>
#include <QByteArray>
#include <QDataStream>

// =========================================================
// 枚举定义
// =========================================================

// 游戏速度
enum W3MapSpeed {
    MAPSPEED_SLOW           = 1,
    MAPSPEED_NORMAL         = 2,
    MAPSPEED_FAST           = 3
};

// 战争迷雾/可见度
enum W3MapVisibility {
    MAPVIS_HIDETERRAIN      = 1,
    MAPVIS_EXPLORED         = 2,
    MAPVIS_ALWAYSVISIBLE    = 3,
    MAPVIS_DEFAULT          = 4
};

// 观察者设置
enum W3MapObservers {
    MAPOBS_NONE             = 1,
    MAPOBS_ONDEFEAT         = 2,
    MAPOBS_ALLOWED          = 3,
    MAPOBS_REFEREES         = 4
};

// 游戏标志位
enum W3MapFlag {
    MAPFLAG_TEAMSTOGETHER   = 1,
    MAPFLAG_FIXEDTEAMS      = 2,
    MAPFLAG_UNITSHARE       = 4,
    MAPFLAG_RANDOMHERO      = 8,
    MAPFLAG_RANDOMRACES     = 16
};

// 地图选项 (StatString 解析用)
#define MAPOPT_HIDEMINIMAP              (1 << 0)
#define MAPOPT_MELEE                    (1 << 2)
#define MAPOPT_FIXEDPLAYERSETTINGS      (1 << 5)
#define MAPOPT_CUSTOMFORCES             (1 << 6)

// =========================================================
// War3Map 类定义
// =========================================================
class War3Map
{
public:
    explicit War3Map();
    ~War3Map();

    // 加载并解析地图 (核心入口)
    bool load(const QString &mapFilePath);

    // === Setters (游戏设置) ===
    void setMapVisibility(W3MapVisibility vis)  { m_mapVisibility = vis; }
    void setMapObservers(W3MapObservers obs)    { m_mapObservers = obs; }
    void setMapSpeed(W3MapSpeed speed)          { m_mapSpeed = speed; }
    void setMapFlags(quint32 flags)             { m_mapFlags = flags; }

    // === Getters (获取信息) ===
    bool isValid() const                        { return m_valid; }
    QString getMapPath() const                  { return m_mapPath; }
    QByteArray getMapWidth() const              { return m_mapWidth; }
    QByteArray getMapHeight() const             { return m_mapHeight; }
    QByteArray getMapSHA1Bytes() const          { return m_mapSHA1Bytes; }

    // === Getters (协议专用) ===
    quint32 getMapCRC() const;
    quint32 getMapSHA1() const;
    quint32 getMapSize() const;
    quint32 getMapInfo() const;
    QString getMapName() const;

    // 获取构建 StatString 所需的游戏标志位
    QByteArray getMapGameFlags();

    // === 工具函数 (Static) ===
    // 暴雪自定义哈希 (原 computeXoroCRC，纯净版)
    static quint32 calcBlizzardHash(const QByteArray &data);

    // StatString 编码/解码
    static QByteArray encodeStatString(const QByteArray &data);
    static QByteArray decodeStatString(const QByteArray &encoded);

    // === 业务逻辑 ===
    // 分析 StatString 并打印日志
    void analyzeStatString(const QString &label, const QByteArray &encodedData);

    // 生成用于局域网广播的 StatString
    QByteArray getEncodedStatString(const QString &hostName, const QString &netPathOverride = "");

    // 设置 CRC 计算时优先查找的脚本目录
    static void setPriorityCrcDirectory(const QString &dirPath);

private:
    bool m_valid;
    QString m_mapPath;

    // --- 核心校验数据 ---
    QByteArray m_mapSize;
    QByteArray m_mapInfo;               // 地图信息 CRC
    QByteArray m_mapCRC;                // 游戏逻辑 CRC
    QByteArray m_mapSHA1Bytes;          // 地图文件 SHA1

    // --- 地图信息 (w3i) ---
    quint32 m_mapOptions;
    QByteArray m_mapWidth;
    QByteArray m_mapHeight;
    int m_numPlayers;
    int m_numTeams;

    // --- 游戏房间设置 ---
    W3MapSpeed      m_mapSpeed;
    W3MapVisibility m_mapVisibility;
    W3MapObservers  m_mapObservers;
    quint32         m_mapFlags;

    // 静态配置
    static QString s_priorityCrcDir;
};

#endif // WAR3MAP_H

#ifndef WAR3MAP_H
#define WAR3MAP_H

#include <QString>
#include <QByteArray>
#include <QDataStream>

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

// 地图选项
#define MAPOPT_HIDEMINIMAP              (1 << 0)
#define MAPOPT_MELEE                    (1 << 2)
#define MAPOPT_FIXEDPLAYERSETTINGS      (1 << 5)
#define MAPOPT_CUSTOMFORCES             (1 << 6)

class War3Map
{
public:
    explicit War3Map();
    ~War3Map();

    bool load(const QString &mapFilePath);

    // === Setters  ===
    void setMapVisibility(W3MapVisibility vis) { m_mapVisibility = vis; }
    void setMapObservers(W3MapObservers obs) { m_mapObservers = obs; }
    void setMapSpeed(W3MapSpeed speed) { m_mapSpeed = speed; }
    void setMapFlags(quint32 flags) { m_mapFlags = flags; }

    // === Getters ===
    QByteArray getMapGameFlags();
    bool isValid() const { return m_valid; }
    QString getMapPath() const { return m_mapPath; }
    QByteArray getMapCRC() const { return m_mapCRC; }
    QByteArray getMapSHA1() const { return m_mapSHA1; }
    QByteArray getMapWidth() const { return m_mapWidth; }
    QByteArray getMapHeight() const { return m_mapHeight; }

    quint32 computeXoroCRC(const QByteArray &data);
    QByteArray encodeStatString(const QByteArray &data);
    QByteArray decodeStatString(const QByteArray &encoded);
    void analyzeStatString(const QString &label, const QByteArray &encodedData);
    QByteArray getEncodedStatString(const QString &hostName, const QString &netPathOverride = "");

    // === Static ===
    static void setPriorityCrcDirectory(const QString &dirPath);

private:
    bool m_valid;
    QString m_mapPath;

    // 基础数据
    QByteArray m_mapSize;
    QByteArray m_mapInfo;
    QByteArray m_mapCRC;
    QByteArray m_mapSHA1;

    // === 地图信息 ===
    quint32 m_mapOptions;
    QByteArray m_mapWidth;
    QByteArray m_mapHeight;
    int m_numPlayers;
    int m_numTeams;

    // === 游戏设置 ===
    W3MapSpeed      m_mapSpeed;
    W3MapVisibility m_mapVisibility;
    W3MapObservers  m_mapObservers;
    quint32         m_mapFlags;

    static QString s_priorityCrcDir;
};

#endif // WAR3MAP_H

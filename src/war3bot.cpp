#include "logger.h"
#include "war3bot.h"

War3Bot::War3Bot(QObject *parent)
    : QObject(parent)
    , m_p2pServer(nullptr)
    , m_forcePortReuse(false)
    , m_bnetConnection(nullptr)
{
    m_bnetConnection = new BnetConnection(this);
    connect(m_bnetConnection, &BnetConnection::authenticated, this, &War3Bot::onBnetAuthenticated);
    connect(m_bnetConnection, &BnetConnection::gameListRegistered, this, &War3Bot::onGameListRegistered);
}

War3Bot::~War3Bot()
{
    stopServer();
    if (m_bnetConnection) {
        m_bnetConnection->deleteLater();
    }
}

bool War3Bot::startServer(quint16 port, const QString &configFile)
{
    if (m_p2pServer && m_p2pServer->isRunning()) {
        LOG_WARNING("服务器已在运行中");
        return true;
    }

    if (!m_p2pServer) {
        m_p2pServer = new P2PServer(this);

        // 设置强制端口重用
        if (m_forcePortReuse) {
            // 这里可以传递强制重用标志给 P2PServer
            // 或者 P2PServer 会自动处理
        }

        // 连接信号
        connect(m_p2pServer, &P2PServer::peerRegistered,
                this, &War3Bot::onPeerRegistered);
        connect(m_p2pServer, &P2PServer::peerRemoved,
                this, &War3Bot::onPeerRemoved);
        connect(m_p2pServer, &P2PServer::punchRequested,
                this, &War3Bot::onPunchRequested);
    }

    bool success = m_p2pServer->startServer(port, configFile);
    if (success) {
        LOG_INFO("War3Bot 服务器启动成功");
        // 1. 检查配置文件路径
        QString configPath = configFile;
        if (configPath.isEmpty()) {
            configPath = "config/war3bot.ini"; // 默认路径
        }

        if (QFile::exists(configPath)) {
            QSettings settings(configPath, QSettings::IniFormat);

            // 读取 [bnet] 节点下的数据
            QString bnetHost = settings.value("bnet/server", "139.155.155.166").toString();
            int bnetPort     = settings.value("bnet/port", 6112).toInt();
            QString bnetUser = settings.value("bnet/username", "").toString();
            QString bnetPass = settings.value("bnet/password", "").toString();

            if (bnetUser.isEmpty() || bnetPass.isEmpty()) {
                LOG_WARNING("INI 配置文件中未找到 [bnet] 用户名或密码，跳过自动连接。");
            } else {
                LOG_INFO(QString("读取配置成功，准备连接战网: %1 (用户: %2)").arg(bnetHost, bnetUser));
                // 调用更新后的连接函数
                connectToBattleNet(bnetHost, bnetPort, bnetUser, bnetPass);
            }
        } else {
            LOG_WARNING(QString("找不到配置文件: %1，无法读取战网信息").arg(configPath));
        }
    } else {
        LOG_ERROR("启动 War3Bot 服务器失败");
    }

    return success;
}

void War3Bot::connectToBattleNet(const QString &hostname, quint16 port, const QString &user, const QString &pass)
{
    if (!m_bnetConnection) return;

    LOG_INFO(QString("正在配置战网账号信息: 用户[%1]").arg(user));

    m_bnetConnection->setCredentials(user, pass);
    m_bnetConnection->connectToHost(hostname, port);
}

void War3Bot::stopServer()
{
    if (m_p2pServer) {
        m_p2pServer->stopServer();
        LOG_INFO("War3Bot 服务器已停止");
    }
}

bool War3Bot::isRunning() const
{
    return m_p2pServer && m_p2pServer->isRunning();
}

void War3Bot::onPeerRegistered(const QString &peerId, const QString &clientUuid, int size)
{
    LOG_INFO(QString("新对等节点已连接 - peerID: %1, 客户端ID: %2 共计 %3 个客户端").arg(peerId, clientUuid).arg(size));
}

void War3Bot::onPeerRemoved(const QString &peerId)
{
    LOG_INFO(QString("对等节点已断开连接 - peerID: %1").arg(peerId));
}

void War3Bot::onPunchRequested(const QString &sourcePeerId, const QString &targetPeerId)
{
    LOG_INFO(QString("打洞请求已处理 - %1 -> %2").arg(sourcePeerId, targetPeerId));
}

void War3Bot::onBnetAuthenticated()
{
    LOG_INFO("战网登录成功");
    // 示例：自动创建一个 DotA 房间
    // m_bnetConnection->createGameOnLadder("DotA v6.83d -ap", QByteArray(), 6112);
}

void War3Bot::onGameListRegistered()
{
    LOG_INFO("房间创建成功");
}

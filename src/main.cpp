#include "logger.h"
#include "war3bot.h"
#include "botmanager.h"

#include <QDir>
#include <QTimer>
#include <QThread>
#include <QProcess>
#include <QSettings>
#include <QUdpSocket>
#include <QTextCodec>
#include <QCoreApplication>
#include <QCommandLineParser>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

// 改进的端口检查函数
bool isPortInUse(quint16 port) {
    QUdpSocket testSocket;
    // 尝试绑定到端口
    bool bound = testSocket.bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress);
    if (bound) {
        testSocket.close();
        return false; // 端口可用
    }
    return true; // 端口被占用
}

// 改进的进程杀死函数
bool killProcessOnPort(quint16 port) {
    LOG_INFO(QString("正在尝试释放端口 %1").arg(port));

#ifdef Q_OS_WIN
    // Windows 方法
    QProcess process;
    process.start("netstat", QStringList() << "-ano" << "-p" << "udp");

    if (!process.waitForFinished(5000)) {
        return false;
    }

    QString output = process.readAllStandardOutput();

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QStringList lines = output.split('\n', QString::SkipEmptyParts);
#else
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
#endif

    for (const QString &line : qAsConst(lines)) {
        if (line.contains(QString(":%1").arg(port)) && line.contains("UDP")) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
            QStringList parts = line.split(' ', QString::SkipEmptyParts);
#else
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
#endif
            if (parts.size() >= 5) {
                QString pidStr = parts.last();
                bool ok;
                int pid = pidStr.toInt(&ok);
                if (ok && pid > 0) {
                    LOG_WARNING(QString("正在终止占用端口 %2 的进程 %1").arg(pid).arg(port));

                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                    if (hProcess != NULL) {
                        TerminateProcess(hProcess, 0);
                        CloseHandle(hProcess);
                        QThread::msleep(1000); // 等待进程结束
                        return true;
                    }
                }
            }
        }
    }
#else
    // Linux 方法
    QProcess process;
    process.start("sh", QStringList() << "-c"
                                      << QString("lsof -i udp:%1 -t 2>/dev/null").arg(port));

    if (!process.waitForFinished(3000)) {
        return false;
    }

    QString output = process.readAllStandardOutput().trimmed();
    if (!output.isEmpty()) {
        QStringList pids = output.split('\n', Qt::SkipEmptyParts);
        for (const QString &pidStr : pids) {
            bool ok;
            int pid = pidStr.toInt(&ok);
            if (ok && pid > 0) {
                LOG_WARNING(QString("正在终止占用端口 %2 的进程 %1").arg(pid).arg(port));
                QProcess::execute("kill", QStringList() << "-9" << QString::number(pid));
            }
        }
        QThread::msleep(1000); // 等待进程结束
        return true;
    }
#endif

    return false;
}

// 强制释放端口的函数
bool forceFreePort(quint16 port) {
    LOG_INFO(QString("正在强制释放端口 %1").arg(port));
    if (killProcessOnPort(port)) {
        QThread::msleep(2000); // 等待更长时间
        return !isPortInUse(port);
    }
    return true; // 让 War3Bot 自己处理
}

int main(int argc, char *argv[]) {
    // 设置编码为 UTF-8
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    QTextCodec::setCodecForLocale(codec);

    QCoreApplication app(argc, argv);

    QCoreApplication::setApplicationName("War3Bot");
    QCoreApplication::setApplicationVersion("3.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("魔兽争霸 III P2P 连接机器人");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption botCountOption({"b", "bot-count"}, "启动的机器人数量", "count", "0");
    parser.addOption(botCountOption);

    QCommandLineOption portOption({"p", "port"}, "监听端口 (默认: 0 [自动分配随机])", "port", "0");
    parser.addOption(portOption);

    QCommandLineOption logLevelOption({"l", "log-level"}, "日志级别 (debug, info, warning, error, critical)", "level", "info");
    parser.addOption(logLevelOption);

    QCommandLineOption configOption({"c", "config"}, "配置文件路径", "config", "war3bot.ini");
    parser.addOption(configOption);

    QCommandLineOption killOption({"k", "kill-existing"}, "终止占用端口的现有进程");
    parser.addOption(killOption);

    QCommandLineOption forceOption({"f", "force"}, "强制端口重用");
    parser.addOption(forceOption);

    parser.process(app);

    // === 先加载配置文件来设置日志 ===
    QString configFile = parser.value(configOption);

    // 检查配置文件是否存在，如果不存在则使用默认值
    QFileInfo configFileInfo(configFile);
    if (!configFileInfo.exists()) {
        // 尝试在可执行文件目录查找
        QString exeDir = QCoreApplication::applicationDirPath();
        QString alternativeConfig = exeDir + "/config/" + configFile;
        if (QFileInfo::exists(alternativeConfig)) {
            configFile = alternativeConfig;
        } else {
            // 如果都不存在，创建默认配置文件
            QString defaultConfigPath = exeDir + "/config/war3bot.ini";
            QFile defaultConfig(defaultConfigPath);
            if (defaultConfig.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&defaultConfig);

                // [server] 节点
                out << "[server]\n";
                out << "broadcast_port=6112\n";
                out << "enable_broadcast=false\n";
                out << "peer_timeout=300000\n";
                out << "cleanup_interval=60000\n";
                out << "broadcast_interval=30000\n";

                // [log] 节点
                out << "\n[log]\n";
                out << "level=info\n";
                out << "enable_console=true\n";
                out << "log_file=/var/log/war3bot/war3bot.log\n";
                out << "max_size=10485760\n";
                out << "backup_count=5\n";

                // [bnet] 战网配置节点
                out << "\n[bnet]\n";
                out << "server=139.155.155.166\n";
                out << "port=6112\n";
                out << "username=bot\n";
                out << "password=wxc123\n";

                defaultConfig.close();
                configFile = defaultConfigPath;
                LOG_INFO(QString("已创建默认配置文件: %1").arg(configFile));
            }
        }
    }

    QSettings configSettings(configFile, QSettings::IniFormat);

    // 从配置文件获取日志设置
    QString configLogLevel = configSettings.value("log/level", "info").toString().toLower();
    bool enableConsole = configSettings.value("log/enable_console", true).toBool();
    QString logFilePath = configSettings.value("log/log_file", "/var/log/war3bot/war3bot.log").toString();
    qint64 maxLogSize = configSettings.value("log/max_size", 10 * 1024 * 1024).toLongLong();
    int backupCount = configSettings.value("log/backup_count", 5).toInt();

    // 确保日志目录存在
    QFileInfo logFileInfo(logFilePath);
    QDir logDir = logFileInfo.dir();
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    // 初始化日志系统
    Logger::instance()->setLogLevel(Logger::logLevelFromString(configLogLevel));
    Logger::instance()->enableConsoleOutput(enableConsole);
    Logger::instance()->setLogFile(logFilePath);
    Logger::instance()->setMaxFileSize(maxLogSize);
    Logger::instance()->setBackupCount(backupCount);

    // 命令行参数覆盖配置文件设置
    QString logLevel = parser.value(logLevelOption).toLower();
    if (parser.isSet(logLevelOption)) {
        Logger::instance()->setLogLevel(Logger::logLevelFromString(logLevel));
    }

    quint16 port = parser.value(portOption).toUShort();
    bool killExisting = parser.isSet(killOption);
    bool forceReuse = parser.isSet(forceOption);

    LOG_INFO("=== War3Bot P2P 服务器正在启动 ===");
    LOG_INFO(QString("版本: %1").arg(app.applicationVersion()));
    LOG_INFO(QString("端口: %1").arg(port));
    LOG_INFO(QString("配置文件: %1").arg(configFile));
    LOG_INFO(QString("日志级别: %1").arg(Logger::instance()->logLevelToString()));
    LOG_INFO(QString("日志文件: %1").arg(logFilePath));

    // 检查端口是否被占用
    if (port != 0) {
        bool portInUse = isPortInUse(port);
        if (portInUse) {
            LOG_WARNING(QString("端口 %1 已被占用").arg(port));
            if (killExisting) {
                LOG_INFO("正在尝试终止占用端口的现有进程...");
                if (forceFreePort(port)) {
                    LOG_INFO("端口现在应该已释放，正在重试...");
                    portInUse = isPortInUse(port);
                }
            }

            if (portInUse && !forceReuse) {
                LOG_INFO("正在尝试其他端口...");
                bool foundPort = false;
                for (quint16 altPort = port + 1; altPort <= port + 20; altPort++) {
                    if (!isPortInUse(altPort)) {
                        port = altPort;
                        foundPort = true;
                        LOG_INFO(QString("使用备用端口: %1").arg(port));
                        break;
                    }
                }
                if (!foundPort) {
                    LOG_CRITICAL("未找到可用端口");
                    return -1;
                }
            }
        }
    }

    War3Bot war3bot;
    if (!war3bot.startServer(port, configFile)) {
        LOG_CRITICAL("启动 War3Bot 服务器失败");
        return -1;
    }

    LOG_INFO("War3Bot 服务器正在运行。按 Ctrl+C 停止。");
    LOG_INFO("=== 服务器启动完成，开始监听 ===");

    // 2. 启动 BotManager (战网机器人客户端)
    int botCount = parser.value(botCountOption).toInt();

    BotManager botManager;
    if (botCount > 0) {
        LOG_INFO(QString("正在启动 %1 个战网机器人...").arg(botCount));

        // 初始化机器人 (从 configFile 读取 username/password)
        botManager.initializeBots(botCount, configFile);

        // 开始连接
        botManager.startAll();
    } else {
        LOG_INFO("未指定机器人数量，仅运行 P2P 服务器模式。");
    }

    // 添加定时状态报告
    QTimer *statusTimer = new QTimer(&app);
    QObject::connect(statusTimer, &QTimer::timeout, &app, [&war3bot, &botManager, startTime = QDateTime::currentDateTime()]() {
        qint64 uptimeSeconds = startTime.secsTo(QDateTime::currentDateTime());
        qint64 days = uptimeSeconds / (24 * 3600);
        qint64 hours = (uptimeSeconds % (24 * 3600)) / 3600;
        qint64 minutes = (uptimeSeconds % 3600) / 60;
        qint64 seconds = uptimeSeconds % 60;

        QString uptimeStr;
        if (days > 0) uptimeStr = QString("运行 %1天%2小时%3分钟%4秒").arg(days).arg(hours).arg(minutes).arg(seconds);
        else if (hours > 0) uptimeStr = QString("运行 %1小时%2分钟%3秒").arg(hours).arg(minutes).arg(seconds);
        else if (minutes > 0) uptimeStr = QString("运行 %1分钟%2秒").arg(minutes).arg(seconds);
        else uptimeStr = QString("运行 %1秒").arg(seconds);

        // 统计机器人状态
        int connectedBots = 0;
        int waitingBots = 0;
        const auto& bots = botManager.getAllBots();
        for(const auto* b : bots) {
            if (b->state == BotState::Idle) connectedBots++;
            if (b->state == BotState::Waiting) waitingBots++;
        }

        QString botStatus = "";
        if (!bots.isEmpty()) {
            botStatus = QString(" | Bots: %1/%2 在线 (%3 房间中)").arg(connectedBots).arg(bots.size()).arg(waitingBots);
        }

        LOG_INFO(QString("🔄 服务器状态 - %1 - 运行中: %2").arg(uptimeStr, war3bot.isRunning() ? "是" : "否"));
    });
    statusTimer->start(30000); // 每30秒报告一次

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &war3bot, [&war3bot]() {
        LOG_INFO("正在关闭 War3Bot 服务器...");
        war3bot.stopServer();
    });

    int result = app.exec();
    Logger::destroyInstance();
    return result;
}

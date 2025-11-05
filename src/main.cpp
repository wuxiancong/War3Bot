#include "logger.h"
#include "war3bot.h"
#include <QDir>
#include <QTimer>
#include <QThread>
#include <QProcess>
#include <QSettings>
#include <QUdpSocket>
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

// 修复 Qt 版本兼容性问题
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QStringList lines = output.split('\n', QString::SkipEmptyParts);
#else
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
#endif

    for (const QString &line : qAsConst(lines)) {
        if (line.contains(QString(":%1").arg(port)) && line.contains("UDP")) {
// 提取 PID
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

    // 方法1: 尝试杀死占用进程
    if (killProcessOnPort(port)) {
        QThread::msleep(2000); // 等待更长时间
        return !isPortInUse(port);
    }

    // 方法2: 使用 SO_REUSEADDR 强制绑定
    return true; // 让 War3Bot 自己处理
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("War3Bot");
    QCoreApplication::setApplicationVersion("3.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("魔兽争霸 III P2P 连接机器人");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(
        {"p", "port"},
        "监听端口 (默认: 6112)",
        "port",
        "6112"
        );
    parser.addOption(portOption);

    QCommandLineOption logLevelOption(
        {"l", "log-level"},
        "日志级别 (debug, info, warning, error, critical)",
        "level",
        "info"
        );
    parser.addOption(logLevelOption);

    QCommandLineOption configOption(
        {"c", "config"},
        "配置文件路径",
        "config",
        "war3bot.ini"
        );
    parser.addOption(configOption);

    QCommandLineOption killOption(
        {"k", "kill-existing"},
        "终止占用端口的现有进程"
        );
    parser.addOption(killOption);

    QCommandLineOption forceOption(
        {"f", "force"},
        "强制端口重用"
        );
    parser.addOption(forceOption);

    parser.process(app);

    // === 先加载配置文件来设置日志 ===
    QString configFile = parser.value(configOption);

    // 检查配置文件是否存在，如果不存在则使用默认值
    QFileInfo configFileInfo(configFile);
    if (!configFileInfo.exists()) {
        // 尝试在可执行文件目录查找
        QString exeDir = QCoreApplication::applicationDirPath();
        QString alternativeConfig = exeDir + "/" + configFile;
        if (QFileInfo::exists(alternativeConfig)) {
            configFile = alternativeConfig;
        } else {
            // 如果都不存在，创建默认配置文件
            QString defaultConfigPath = exeDir + "/war3bot.ini";
            QFile defaultConfig(defaultConfigPath);
            if (defaultConfig.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&defaultConfig);
                out << "[server]\n";
                out << "broadcast_port=6112\n";
                out << "enable_broadcast=false\n";
                out << "peer_timeout=300000\n";
                out << "cleanup_interval=60000\n";
                out << "broadcast_interval=30000\n";
                out << "\n[log]\n";
                out << "level=info\n";
                out << "enable_console=true\n";
                out << "log_file=/var/log/war3bot/war3bot.log\n";
                out << "max_size=10485760\n";
                out << "backup_count=5\n";
                defaultConfig.close();
                configFile = defaultConfigPath;
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

    // 初始化日志系统（先使用配置文件的设置）
    Logger::instance()->setLogLevel(Logger::logLevelFromString(configLogLevel));
    Logger::instance()->enableConsoleOutput(enableConsole);
    Logger::instance()->setLogFile(logFilePath);
    Logger::instance()->setMaxFileSize(maxLogSize);
    Logger::instance()->setBackupCount(backupCount);

    // 命令行参数覆盖配置文件设置
    QString logLevel = parser.value(logLevelOption).toLower();
    if (parser.isSet(logLevelOption)) {
        // 如果命令行指定了日志级别，则覆盖配置文件
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
    LOG_INFO(QString("控制台输出: %1").arg(enableConsole ? "启用" : "禁用"));
    LOG_INFO(QString("最大日志大小: %1 MB").arg(maxLogSize / (1024 * 1024)));
    LOG_INFO(QString("备份数量: %1").arg(backupCount));

    // 检查端口是否被占用
    bool portInUse = isPortInUse(port);

    if (portInUse) {
        LOG_WARNING(QString("端口 %1 已被占用").arg(port));

        if (killExisting) {
            LOG_INFO("正在尝试终止占用端口的现有进程...");
            if (forceFreePort(port)) {
                LOG_INFO("端口现在应该已释放，正在重试...");
                // 重新检查端口
                portInUse = isPortInUse(port);
            }
        }

        if (portInUse && !forceReuse) {
            // 尝试使用其他端口
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

    War3Bot bot;

    // 设置强制端口重用选项
    if (forceReuse) {
        bot.setForcePortReuse(true);
    }

    if (!bot.startServer(port, configFile)) {
        LOG_CRITICAL("启动 War3Bot 服务器失败");
        return -1;
    }

    LOG_INFO("War3Bot 服务器正在运行。按 Ctrl+C 停止。");
    // 在启动服务器后添加网络测试
    LOG_INFO("=== 服务器启动完成，开始监听 ===");
    // 添加定时状态报告
    QTimer *statusTimer = new QTimer(&app);
    QObject::connect(statusTimer, &QTimer::timeout, &app, [&bot, startTime = QDateTime::currentDateTime()]() {
        qint64 uptimeSeconds = startTime.secsTo(QDateTime::currentDateTime());
        qint64 days = uptimeSeconds / (24 * 3600);
        qint64 hours = (uptimeSeconds % (24 * 3600)) / 3600;
        qint64 minutes = (uptimeSeconds % 3600) / 60;
        qint64 seconds = uptimeSeconds % 60;

        QString uptimeStr;
        if (days > 0) {
            uptimeStr = QString("运行 %1天%2小时%3分钟%4").arg(days).arg(hours).arg(minutes).arg(seconds);
        } else if (hours > 0) {
            uptimeStr = QString("运行 %1小时%2分钟").arg(hours).arg(minutes).arg(seconds);
        } else if (minutes > 0) {
            uptimeStr = QString("运行 %1分钟%2秒").arg(minutes).arg(seconds);
        } else {
            uptimeStr = QString("运行 %1秒").arg(seconds);
        }

        LOG_INFO(QString("🔄 服务器状态 - %1 - 运行中: %2")
                     .arg(uptimeStr, bot.isRunning() ? "是" : "否"));
    });
    statusTimer->start(30000); // 每30秒报告一次

    // 设置退出信号处理
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &bot, [&bot]() {
        LOG_INFO("正在关闭 War3Bot 服务器...");
        bot.stopServer();
    });

    int result = app.exec();

    // 清理日志系统
    Logger::destroyInstance();

    return result;
}

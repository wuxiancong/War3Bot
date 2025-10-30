#include "war3bot.h"
#include "logger.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <QDir>
#include <QSettings>  // 添加这个头文件
#include <QUdpSocket> // 添加这个头文件

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

// 改进的端口检查函数
bool isPortInUse(quint16 port) {
    QUdpSocket testSocket;

    // 尝试绑定到端口
    bool bound = testSocket.bind(QHostAddress::Any, port, QUdpSocket::ShareAddress);

    if (bound) {
        testSocket.close();
        return false; // 端口可用
    }

    return true; // 端口被占用
}

// 改进的进程杀死函数
bool killProcessOnPort(quint16 port) {
    LOG_INFO(QString("Attempting to free port %1").arg(port));

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

    for (const QString &line : lines) {
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
                    LOG_WARNING(QString("Killing process %1 occupying port %2").arg(pid).arg(port));

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
                LOG_WARNING(QString("Killing process %1 occupying port %2").arg(pid).arg(port));
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
    LOG_INFO(QString("Force freeing port %1").arg(port));

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
    QCoreApplication::setApplicationVersion("2.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Warcraft III P2P Connection Bot");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(
        {"p", "port"},
        "Listen port (default: 6112)",
        "port",
        "6112"
        );
    parser.addOption(portOption);

    QCommandLineOption logLevelOption(
        {"l", "log-level"},
        "Log level (debug, info, warning, error, critical)",
        "level",
        "info"
        );
    parser.addOption(logLevelOption);

    QCommandLineOption configOption(
        {"c", "config"},
        "Config file path",
        "config",
        "war3bot.ini"
        );
    parser.addOption(configOption);

    QCommandLineOption killOption(
        {"k", "kill-existing"},
        "Kill existing processes using the port"
        );
    parser.addOption(killOption);

    QCommandLineOption forceOption(
        {"f", "force"},
        "Force port reuse"
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
                out << "log_file=logs/war3bot.log\n";
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
    QString logFilePath = configSettings.value("log/log_file", "logs/war3bot.log").toString();
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

    LOG_INFO("=== War3Bot P2P Server Starting ===");
    LOG_INFO(QString("Version: %1").arg(app.applicationVersion()));
    LOG_INFO(QString("Port: %1").arg(port));
    LOG_INFO(QString("Config: %1").arg(configFile));
    LOG_INFO(QString("Log Level: %1").arg(Logger::instance()->logLevelToString()));
    LOG_INFO(QString("Log File: %1").arg(logFilePath));
    LOG_INFO(QString("Console Output: %1").arg(enableConsole ? "enabled" : "disabled"));
    LOG_INFO(QString("Max Log Size: %1 MB").arg(maxLogSize / (1024 * 1024)));
    LOG_INFO(QString("Backup Count: %1").arg(backupCount));

    // 检查端口是否被占用
    bool portInUse = isPortInUse(port);

    if (portInUse) {
        LOG_WARNING(QString("Port %1 is already in use").arg(port));

        if (killExisting) {
            LOG_INFO("Attempting to kill existing process on port...");
            if (forceFreePort(port)) {
                LOG_INFO("Port should be free now, retrying...");
                // 重新检查端口
                portInUse = isPortInUse(port);
            }
        }

        if (portInUse && !forceReuse) {
            // 尝试使用其他端口
            LOG_INFO("Trying alternative ports...");
            bool foundPort = false;
            for (quint16 altPort = port + 1; altPort <= port + 20; altPort++) {
                if (!isPortInUse(altPort)) {
                    port = altPort;
                    foundPort = true;
                    LOG_INFO(QString("Using alternative port: %1").arg(port));
                    break;
                }
            }
            if (!foundPort) {
                LOG_CRITICAL("No available ports found");
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
        LOG_CRITICAL("Failed to start War3Bot server");
        return -1;
    }

    LOG_INFO("War3Bot server is running. Press Ctrl+C to stop.");

    // 设置退出信号处理
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &bot, [&bot]() {
        LOG_INFO("Shutting down War3Bot server...");
        bot.stopServer();
    });

    int result = app.exec();

    // 清理日志系统
    Logger::destroyInstance();

    return result;
}

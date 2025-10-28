#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include "war3bot.h"
#include "logger.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("War3Bot");
    QCoreApplication::setApplicationVersion("1.0");

    // 初始化日志系统
    Logger::instance()->setLogLevel(Logger::INFO);
    Logger::instance()->enableConsoleOutput(true);

    // 确保日志目录存在
    QDir logDir("/var/log/war3bot");
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }
    Logger::instance()->setLogFile("/var/log/war3bot/war3bot.log");

    QCommandLineParser parser;
    parser.setApplicationDescription("Warcraft III P2P Connection Bot");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(
        {"p", "port"},
        "Listen port (default: 6113)",
        "port",
        "6113"
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

    parser.process(app);

    // 设置日志级别
    QString logLevel = parser.value(logLevelOption).toLower();
    if (logLevel == "debug") Logger::instance()->setLogLevel(Logger::DEBUG);
    else if (logLevel == "warning") Logger::instance()->setLogLevel(Logger::WARNING);
    else if (logLevel == "error") Logger::instance()->setLogLevel(Logger::ERROR);
    else if (logLevel == "critical") Logger::instance()->setLogLevel(Logger::CRITICAL);

    quint16 port = parser.value(portOption).toUShort();
    QString configFile = parser.value(configOption);

    LOG_INFO("=== War3Bot P2P Server Starting ===");
    LOG_INFO(QString("Version: %1").arg(app.applicationVersion()));
    LOG_INFO(QString("Port: %1").arg(port));
    LOG_INFO(QString("Config: %1").arg(configFile));
    LOG_INFO(QString("Log Level: %1").arg(logLevel));

    War3Bot bot;
    if (!bot.startServer(port, configFile)) {
        LOG_CRITICAL("Failed to start War3Bot server");
        return -1;
    }

    LOG_INFO("War3Bot server is running. Press Ctrl+C to stop.");

    // 设置退出信号处理
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&bot]() {
        LOG_INFO("Shutting down War3Bot server...");
        bot.stopServer();
    });

    int result = app.exec();

    // 清理日志系统
    Logger::destroy();

    return result;
}

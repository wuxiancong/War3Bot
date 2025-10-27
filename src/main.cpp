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
    parser.setApplicationDescription("Warcraft III Game Session Proxy");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(
        {"p", "port"},
        "Listen port (default: 6113)",  // 修改默认端口说明
        "port",
        "6113"  // 修改默认端口为 6113
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

    War3Bot bot;
    if (!bot.startServer(port)) {
        LOG_CRITICAL("Failed to start War3Bot server");
        return -1;
    }

    LOG_INFO(QString("War3Bot is running on port %1... Press Ctrl+C to exit.").arg(port));

    int result = app.exec();

    // 清理日志系统
    Logger::destroy();

    return result;
}

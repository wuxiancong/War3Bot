#include "logger.h"
#include "client.h"
#include "war3bot.h"
#include "command.h"
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
#include <QRegularExpression>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

// === 端口检查函数 ===
bool isPortInUse(quint16 port) {
    QUdpSocket testSocket;
    bool bound = testSocket.bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress);
    if (bound) {
        testSocket.close();
        return false; // 端口可用
    }
    return true; // 端口被占用
}

// === 强制释放端口函数 ===
bool killProcessOnPort(quint16 port) {
    LOG_INFO(QString("正在尝试释放端口 %1").arg(port));

#ifdef Q_OS_WIN
    // Windows 方法
    QProcess process;
    process.start("cmd", QStringList() << "/c" << QString("for /f \"tokens=5\" %a in ('netstat -aon ^| findstr :%1') do taskkill /f /pid %a").arg(port));
    process.waitForFinished(5000);
    return true;
#else
    // Linux 方法
    QProcess process;
    // 尝试 fuser (更强力)
    process.start("sh", QStringList() << "-c" << QString("fuser -k -9 %1/udp; fuser -k -9 %1/tcp").arg(port));
    if (!process.waitForFinished(3000)) {
        // 尝试 lsof (备用)
        process.start("sh", QStringList() << "-c" << QString("lsof -t -i:%1 | xargs kill -9").arg(port));
        process.waitForFinished(3000);
    }
    QThread::msleep(500); // 等待系统回收
    return true;
#endif
}

bool forceFreePort(quint16 port) {
    LOG_INFO(QString("正在强制释放端口 %1").arg(port));
    if (killProcessOnPort(port)) {
        QThread::msleep(2000);
        return !isPortInUse(port);
    }
    return true;
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

    QCommandLineOption portOption({"p", "port"}, "监听端口 (默认: 6116)", "port", "6116");
    parser.addOption(portOption);

    QCommandLineOption logLevelOption({"l", "log-level"}, "日志级别", "level", "info");
    parser.addOption(logLevelOption);

    QCommandLineOption configOption({"c", "config"}, "配置文件路径", "config", "war3bot.ini");
    parser.addOption(configOption);

    QCommandLineOption killOption({"k", "kill-existing"}, "终止占用端口的现有进程");
    parser.addOption(killOption);

    QCommandLineOption forceOption({"f", "force"}, "强制端口重用");
    parser.addOption(forceOption);

    parser.process(app);

    // === 1. 加载配置与日志初始化 ===
    QString configFile = parser.value(configOption);
    QFileInfo configFileInfo(configFile);

    // 如果配置文件不存在，尝试查找或创建默认配置
    if (!configFileInfo.exists()) {
        QString exeDir = QCoreApplication::applicationDirPath();
        QString alternativeConfig = exeDir + "/config/" + configFile;
        if (QFileInfo::exists(alternativeConfig)) {
            configFile = alternativeConfig;
        } else {
            QString defaultConfigPath = exeDir + "/config/war3bot.ini";
            QFile defaultConfig(defaultConfigPath);
            if (defaultConfig.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&defaultConfig);
                out << "[server]\nbroadcast_port=6112\nenable_broadcast=false\npeer_timeout=300000\ncleanup_interval=60000\nbroadcast_interval=30000\n";
                out << "\n[log]\nlevel=info\nenable_console=true\nlog_file=/var/log/war3bot/war3bot.log\nmax_size=10485760\nbackup_count=5\n";
                out << "\n[bnet]\nserver=139.155.155.166\nport=6112\nusername=bot\npassword=wxc123\n";
                defaultConfig.close();
                configFile = defaultConfigPath;
                LOG_INFO(QString("已创建默认配置文件: %1").arg(configFile));
            }
        }
    }

    QSettings configSettings(configFile, QSettings::IniFormat);

    // 初始化日志
    QString configLogLevel = configSettings.value("log/level", "info").toString().toLower();
    bool enableConsole = configSettings.value("log/enable_console", true).toBool();
    QString logFilePath = configSettings.value("log/log_file", "/var/log/war3bot/war3bot.log").toString();
    qint64 maxLogSize = configSettings.value("log/max_size", 10 * 1024 * 1024).toLongLong();
    int backupCount = configSettings.value("log/backup_count", 5).toInt();

    QFileInfo logFileInfo(logFilePath);
    if (!logFileInfo.dir().exists()) logFileInfo.dir().mkpath(".");

    Logger::instance()->setLogLevel(Logger::logLevelFromString(configLogLevel));
    Logger::instance()->enableConsoleOutput(enableConsole);
    Logger::instance()->setLogFile(logFilePath);
    Logger::instance()->setMaxFileSize(maxLogSize);
    Logger::instance()->setBackupCount(backupCount);

    if (parser.isSet(logLevelOption)) {
        Logger::instance()->setLogLevel(Logger::logLevelFromString(parser.value(logLevelOption).toLower()));
    }

    // === 2. 端口检查与清理 ===
    quint16 port = parser.value(portOption).toUShort();
    if (port == 0) port = 6116;
    int broadcastPort = configSettings.value("server/broadcast_port", 6112).toInt();

    // 启动前强制清理端口
    killProcessOnPort(port);
    killProcessOnPort(broadcastPort);

    bool killExisting = parser.isSet(killOption);
    bool forceReuse = parser.isSet(forceOption);

    LOG_INFO("=== War3Bot P2P 服务器正在启动 ===");
    LOG_INFO(QString("版本: %1").arg(app.applicationVersion()));
    LOG_INFO(QString("端口: %1").arg(port));
    LOG_INFO(QString("配置文件: %1").arg(configFile));

    if (port != 0) {
        bool portInUse = isPortInUse(port);
        if (portInUse) {
            LOG_WARNING(QString("端口 %1 已被占用").arg(port));
            if (killExisting && forceFreePort(port)) {
                LOG_INFO("端口已释放，正在重试...");
                portInUse = isPortInUse(port);
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

    // === 3. 启动核心服务 ===
    War3Bot war3bot;
    if (!war3bot.startServer(port, configFile)) {
        LOG_CRITICAL("启动 War3Bot 服务器失败");
        return -1;
    }

    LOG_INFO("War3Bot 服务器正在运行。按 Ctrl+C 停止。");
    LOG_INFO("=== 服务器启动完成，开始监听 ===");

    // === 4. 控制台命令处理 ===
    Command command;
    QObject::connect(&command, &Command::inputReceived, &app, [&](QString cmd){
        QStringList parts;
        QRegularExpression regex("(\"[^\"]*\"|[^\\s\"]+)");
        QRegularExpressionMatchIterator i = regex.globalMatch(cmd);
        while (i.hasNext()) {
            QString arg = i.next().captured(0);
            if (arg.startsWith('"') && arg.endsWith('"') && arg.length() >= 2) {
                arg = arg.mid(1, arg.length() - 2);
            }
            parts.append(arg);
        }

        if (parts.isEmpty()) return;
        QString action = parts[0].toLower();

        // 获取真实的 BotManager (从 War3Bot 实例中获取)
        BotManager* activeBotManager = war3bot.getBotManager();
        if (!activeBotManager) {
            LOG_ERROR("无法获取 BotManager 实例");
            return;
        }

        // 读取配置以判断模式
        QSettings settings(configFile, QSettings::IniFormat);
        QString configUser = settings.value("bnet/username", "").toString();
        bool isBotMode = (configUser == "bot");

        // ---------------------------------------------------------
        // 命令: connect <地址> <端口> <用户名> <密码>
        // ---------------------------------------------------------
        if (action == "connect") {
            QString server = (parts.size() > 1) ? parts[1] : "";
            int p          = (parts.size() > 2) ? parts[2].toInt() : 0;
            QString user   = (parts.size() > 3) ? parts[3] : "";
            QString pass   = (parts.size() > 4) ? parts[4] : "";
            war3bot.connectToBattleNet(server, p, user, pass);
        }
        // ---------------------------------------------------------
        // 命令: create <游戏名> [密码] [指定Bot账号]
        // ---------------------------------------------------------
        else if (action == "create") {
            if (parts.size() < 2) {
                LOG_WARNING("命令格式错误。用法: create <游戏名> [密码] [Bot账号] [Bot密码]");
                return;
            }
            QString gameName = parts[1];
            QString gamePass = (parts.size() > 2) ? parts[2] : "";
            QString targetUser = (parts.size() > 3) ? parts[3] : "";
            QString targetUserPass = (parts.size() > 4) ? parts[4] : "";

            if (isBotMode) {
                const auto &bots = activeBotManager->getAllBots(); // 使用正确的 Manager
                bool foundBot = false;

                for (auto *bot : bots) {
                    if (!bot || !bot->client) continue;

                    // 场景 A-1: 指定了机器人
                    if (!targetUser.isEmpty()) {
                        if (bot->username.compare(targetUser, Qt::CaseInsensitive) == 0) {
                            if (bot->client->isConnected()) {
                                LOG_INFO(QString("🤖 [Bot-%1] 指定调用 %2 创建游戏...").arg(bot->id).arg(bot->username));
                                bot->client->createGame(gameName, gamePass, ProviderVersion::Provider_TFT_New, ComboGameType::Game_TFT_Custom, SubGameType::SubType_Internet, LadderType::Ladder_None);
                                bot->state = BotState::Creating;
                                foundBot = true;
                            } else {
                                LOG_WARNING(QString("❌ 找到机器人 '%1' 但未连接战网").arg(targetUser));
                                foundBot = true; // 标记找到了，虽然没成功
                            }
                            break;
                        }
                    }
                    // 场景 A-2: 自动寻找空闲机器人
                    else {
                        if (bot->client->isConnected() && bot->state == BotState::Idle) {
                            LOG_INFO(QString("🤖 [Bot-%1] 状态空闲，已被选中创建游戏: %2").arg(bot->id).arg(gameName));
                            bot->client->createGame(gameName, gamePass, ProviderVersion::Provider_TFT_New, ComboGameType::Game_TFT_Custom, SubGameType::SubType_Internet, LadderType::Ladder_None);
                            bot->state = BotState::Creating;
                            foundBot = true;
                            break;
                        }
                    }
                }

                if (!foundBot) {
                    if (!targetUser.isEmpty()) LOG_WARNING(QString("❌ 未找到名为 '%1' 的机器人").arg(targetUser));
                    else LOG_WARNING(QString("❌ 创建失败: 当前没有空闲 (Idle) 的机器人 (总数: %1)").arg(bots.size()));
                }
            } else {
                // 单用户模式
                war3bot.createGame(gameName, gamePass, targetUser, targetUserPass);
            }
        }
        // ---------------------------------------------------------
        // 命令: cancel [Bot账号]
        // ---------------------------------------------------------
        else if (action == "cancel") {
            QString targetUser = (parts.size() > 1) ? parts[1] : "";

            if (isBotMode) {
                const auto &bots = activeBotManager->getAllBots();
                int count = 0;

                if (targetUser.isEmpty()) LOG_INFO("❌ 正在 [销毁] 所有机器人的房间...");
                else LOG_INFO(QString("❌ 正在销毁机器人 [%1] 的房间...").arg(targetUser));

                for (auto *bot : bots) {
                    if (bot && bot->client && bot->client->isConnected()) {
                        bool match = targetUser.isEmpty() || (bot->username.compare(targetUser, Qt::CaseInsensitive) == 0);
                        if (match) {
                            bot->client->cancelGame();
                            bot->state = BotState::Idle; // ★ 关键：重置为空闲
                            count++;
                            LOG_INFO(QString("✅ Bot-%1 (%2) 房间已销毁，状态重置为 Idle").arg(bot->id).arg(bot->username));
                        }
                    }
                }
                if (count == 0) LOG_WARNING("未找到匹配的机器人。");
            } else {
                war3bot.cancelGame();
            }
        }
        // ---------------------------------------------------------
        // 命令: stop [Bot账号] (停止广播)
        // ---------------------------------------------------------
        else if (action == "stop") {
            QString targetUser = (parts.size() > 1) ? parts[1] : "";

            if (isBotMode) {
                const auto &bots = activeBotManager->getAllBots();
                int count = 0;

                if (targetUser.isEmpty()) LOG_INFO("🛑 正在停止所有机器人的广播...");
                else LOG_INFO(QString("🛑 正在停止机器人 [%1] 的广播...").arg(targetUser));

                for (auto *bot : bots) {
                    if (bot && bot->client && bot->client->isConnected()) {
                        bool match = targetUser.isEmpty() || (bot->username.compare(targetUser, Qt::CaseInsensitive) == 0);
                        if (match) {
                            bot->client->stopAdv();
                            // 注意：stop 不重置状态
                            count++;
                            LOG_INFO(QString("✅ Bot-%1 (%2) 已停止广播").arg(bot->id).arg(bot->username));
                        }
                    }
                }
                if (count == 0) LOG_WARNING("未找到匹配的机器人。");
            } else {
                war3bot.stopAdv();
            }
        }
        else {
            LOG_INFO("未知命令。可用命令: connect, create, cancel, stop");
        }
    });

    // 启动监听
    command.start();

    // === 5. 定时状态报告 ===
    QTimer *statusTimer = new QTimer(&app);
    QObject::connect(statusTimer, &QTimer::timeout, &app, [&war3bot, startTime = QDateTime::currentDateTime()]() {
        qint64 uptimeSeconds = startTime.secsTo(QDateTime::currentDateTime());
        // 简单计算时间...
        QString uptimeStr = QString("运行 %1秒").arg(uptimeSeconds);

        // 获取真实状态
        BotManager* bm = war3bot.getBotManager();
        int online = 0, idle = 0;
        if (bm) {
            const auto& bots = bm->getAllBots();
            for(auto* b : bots) {
                if (b->client && b->client->isConnected()) online++;
                if (b->state == BotState::Idle) idle++;
            }
        }

        LOG_INFO(QString("🔄 服务器状态 - %1 - 在线Bot: %2 (空闲: %3)").arg(uptimeStr).arg(online).arg(idle));
    });
    statusTimer->start(30000);

    // === 6. 退出清理 ===
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &war3bot, [&war3bot]() {
        LOG_INFO("正在关闭 War3Bot 服务器...");
        war3bot.stopServer();
    });

    int result = app.exec();
    Logger::destroyInstance();
    return result;
}

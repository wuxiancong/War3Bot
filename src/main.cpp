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
#include <QLocalServer>
#include <QLocalSocket>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QRegularExpression>
#include <QFileSystemWatcher>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

// === 定义IPC名称 ===
const QString IPC_SERVER_NAME = "war3bot_ipc";

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

int runConsoleClient(const QString &logFile) {
    // 1. 尝试连接后台服务
    QLocalSocket socket;
    socket.connectToServer(IPC_SERVER_NAME);
    if (!socket.waitForConnected(1000)) {
        printf("❌ 无法连接到 War3Bot 后台服务。\n");
        printf("请确保服务已启动 (sudo systemctl start war3bot)\n");
        printf("错误信息: %s\n", qPrintable(socket.errorString()));
        return -1;
    }
    printf("✅ 已连接到 War3Bot 服务。您可以输入命令，日志将实时显示。\n");
    printf("👉 输入 'quit' 或按 Ctrl+C 退出控制台 (不会停止后台服务)\n");
    printf("----------------------------------------------------------\n");

    // 2. 启动输入监听线程
    Command cmdThread(nullptr); // Client 指针传 nullptr，因为只用来读 stdin

    QObject::connect(&cmdThread, &Command::inputReceived, [&](QString cmd) {
        if (cmd == "quit" || cmd == "exit") {
            QCoreApplication::quit();
            return;
        }
        // 发送命令到后台
        socket.write(cmd.toUtf8());
        socket.flush();
    });
    cmdThread.start();

    // 3. 实时读取日志文件
    QFile file(logFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        file.seek(file.size()); // 跳到文件末尾，不打印历史日志

        QTimer *logTimer = new QTimer();
        QObject::connect(logTimer, &QTimer::timeout, [&file]() {
            QByteArray newLines = file.readAll();
            if (!newLines.isEmpty()) {
                printf("%s", newLines.constData());
                fflush(stdout);
            }
        });
        logTimer->start(200); // 每200毫秒检查一次新日志
    } else {
        printf("⚠️ 警告: 无法打开日志文件进行监控: %s\n", qPrintable(logFile));
    }

    return QCoreApplication::exec();
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

    QCommandLineOption execOption({"x", "exec"}, "发送命令到正在运行的后台服务", "command");
    parser.addOption(execOption);

    QCommandLineOption attachOption({"a", "attach"}, "附着到运行中的服务 (查看日志并发送命令)");
    parser.addOption(attachOption);

    parser.process(app);

    if (parser.isSet(execOption)) {
        QString cmdToSend = parser.value(execOption);
        if (cmdToSend.isEmpty()) {
            fprintf(stderr, "错误: 命令不能为空\n");
            return 1;
        }

        QLocalSocket socket;
        socket.connectToServer(IPC_SERVER_NAME);

        if (socket.waitForConnected(1000)) {
            // 发送命令
            QByteArray data = cmdToSend.toUtf8();
            socket.write(data);
            socket.waitForBytesWritten(1000);
            socket.disconnectFromServer();
            printf("✅ 命令已发送: %s\n", qPrintable(cmdToSend));
            return 0; // 发送成功，退出进程
        } else {
            fprintf(stderr, "❌ 连接失败: 无法连接到后台服务 (%s)\n", qPrintable(socket.errorString()));
            fprintf(stderr, "请确认 sudo systemctl status war3bot 是否正在运行。\n");
            return 1;
        }
    }

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

    if (parser.isSet(attachOption)) {
        return runConsoleClient(logFilePath);
    }

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

    auto processCommand = [&](QString cmd) {
        LOG_INFO(QString("📥 收到指令: %1").arg(cmd));
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
        BotManager *botManager = war3bot.getBotManager();
        if (!botManager) {
            LOG_ERROR("无法获取 BotManager 实例");
            return;
        }

        // 读取配置以判断模式
        QSettings settings(configFile, QSettings::IniFormat);
        QString configUser = settings.value("bnet/username", "").toString();
        bool isBotMode = (configUser == "bot");

        // ---------------------------------------------------------
        // 命令: connect <用户名> <密码> <地址> <端口>
        // ---------------------------------------------------------
        if (action == "connect") {
            QString user   = (parts.size() > 1) ? parts[1] : "";
            QString pass   = (parts.size() > 2) ? parts[2] : "";
            QString server = (parts.size() > 3) ? parts[3] : "";
            int port       = (parts.size() > 4) ? parts[4].toInt() : 0;

            // 如果是 Bot 模式 (多机器人)
            if (isBotMode) {
                const auto &bots = botManager->getAllBots();
                bool foundBot = false;

                // 场景 A: 批量启动
                if (user.isEmpty()) {
                    LOG_INFO("🤖 收到批量启动指令，正在启动所有机器人...");
                    // startAll 内部已经包含了状态检查和错峰逻辑 (前提是你修改了 BotManager)
                    botManager->startAll();
                    return;
                }

                // 场景 B: 指定机器人启动
                for (auto *bot : bots) {
                    if (!bot || !bot->client) continue;
                    if (bot->username.compare(user, Qt::CaseInsensitive) == 0) {
                        foundBot = true;

                        // 检查 1: 防止重复连接
                        if (bot->client->isConnected()) {
                            LOG_WARNING(QString("⚠️ 机器人 %1 已经在线 (状态: %2)，请先执行 disconnect/stop 断开").arg(user).arg((int)bot->state));
                            break;
                        }

                        LOG_INFO(QString("🤖 [Bot-%1] 正在连接: %2").arg(bot->id).arg(bot->username));

                        // 更新密码 (如果命令行提供了)
                        if (!pass.isEmpty()) {
                            bot->password = pass;
                        }

                        QString targetServer = server.isEmpty() ? "127.0.0.1" : server;
                        int targetPort = (port == 0) ? 6112 : port;

                        // 重新设置凭据 (防止之前被修改)
                        bot->client->setCredentials(bot->username, bot->password, Protocol_SRP_0x53);

                        // 发起连接
                        bot->client->connectToHost(targetServer, targetPort);

                        // 让 Client 的信号去更新 state，不要在这里手动 set state，除非是为了 UI 立即反馈
                        // bot->state = BotState::Unregistered;
                        break;
                    }
                }

                if (!foundBot) {
                    LOG_WARNING(QString("❌ 未找到名为 '%1' 的机器人。请检查 config.ini 中的前缀或数量。").arg(user));
                }
            }
        }
        // ---------------------------------------------------------
        // 命令: create <游戏名称> [用户账号] [用户密码] [游戏密码]
        // ---------------------------------------------------------
        else if (action == "create") {
            if (parts.size() < 2) {
                LOG_WARNING("命令格式错误。用法: create <游戏名称> [用户账号] [用户密码] [游戏密码]");
                return;
            }
            QString gameName = parts[1];
            QString targetUser = (parts.size() > 2) ? parts[2] : "";
            QString targetUserPass = (parts.size() > 3) ? parts[3] : "";
            QString gameEnterRoomPass = (parts.size() > 4) ? parts[4] : "";
            if (isBotMode) {
                const auto &bots = botManager->getAllBots(); // 使用正确的 Manager
                bool foundBot = false;

                for (auto *bot : bots) {
                    if (!bot || !bot->client) continue;

                    // 场景 A-1: 指定了机器人
                    if (!targetUser.isEmpty()) {
                        if (bot->username.compare(targetUser, Qt::CaseInsensitive) == 0) {
                            if (bot->client->isConnected()) {
                                LOG_INFO(QString("🤖 [Bot-%1] 指定调用 %2 创建游戏...").arg(bot->id).arg(bot->username));
                                bot->client->createGame(gameName, gameEnterRoomPass, Provider_TFT_New, Game_TFT_Custom, SubType_Internet, Ladder_None, From_Server);
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
                            bot->client->createGame(gameName, gameEnterRoomPass, Provider_TFT_New, Game_TFT_Custom, SubType_Internet, Ladder_None, From_Server);
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
                war3bot.createGame(gameName, gameEnterRoomPass, targetUser, targetUserPass);
            }
        }
        // ---------------------------------------------------------
        // 命令: cancel [Bot账号]
        // ---------------------------------------------------------
        else if (action == "cancel") {
            QString targetUser = (parts.size() > 1) ? parts[1] : "";

            if (isBotMode) {
                const auto &bots = botManager->getAllBots();
                int count = 0;

                if (targetUser.isEmpty()) LOG_INFO("❌ 正在 [销毁] 所有机器人的房间...");
                else LOG_INFO(QString("❌ 正在销毁机器人 [%1] 的房间...").arg(targetUser));

                for (auto *bot : bots) {
                    if (bot && bot->client && bot->client->isConnected()) {
                        bool match = targetUser.isEmpty() || (bot->username.compare(targetUser, Qt::CaseInsensitive) == 0);
                        if (match) {
                            bot->client->cancelGame();
                            bot->state = BotState::Idle;
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
                const auto &bots = botManager->getAllBots();
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
    };

    // === 4. 控制台命令处理 ===
    Command *command = nullptr;
    if (enableConsole) {
        command = new Command(nullptr, &app); // 使用堆分配，避免 main 函数栈溢出风险
        QObject::connect(command, &Command::inputReceived, &app, processCommand);
        command->start();
        LOG_INFO("✅ 控制台命令监听已启动");
    }

    // === 5. 启动 IPC 本地服务器 ===
    QLocalServer ipcServer;
    if (ipcServer.listen(IPC_SERVER_NAME)) {
        // 设置权限，确保 sudo 运行的用户或者同组用户能访问
        // Linux 下建议设置为 User/Group 可读写
#ifndef Q_OS_WIN
        QFile ipcFile(QDir::tempPath() + "/" + IPC_SERVER_NAME);
        ipcFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
#endif
        LOG_INFO(QString("✅ IPC 命令服务已启动，监听: %1").arg(ipcServer.fullServerName()));

        QObject::connect(&ipcServer, &QLocalServer::newConnection, &app, [&]() {
            QLocalSocket *clientConnection = ipcServer.nextPendingConnection();
            QObject::connect(clientConnection, &QLocalSocket::readyRead, [clientConnection, processCommand]() {
                QByteArray data = clientConnection->readAll();
                QString cmd = QString::fromUtf8(data).trimmed();
                if (!cmd.isEmpty()) {
                    processCommand(cmd);
                }
            });
            QObject::connect(clientConnection, &QLocalSocket::disconnected, clientConnection, &QLocalSocket::deleteLater);
        });
    } else {
        LOG_ERROR(QString("❌ IPC 服务启动失败: %1").arg(ipcServer.errorString()));
    }

    // === 6. 定时状态报告 ===
    QTimer *statusTimer = new QTimer(&app);
    QObject::connect(statusTimer, &QTimer::timeout, &app, [startTime = QDateTime::currentDateTime(), &war3bot]() {

        // 1. 计算运行时间 (Uptime)
        qint64 totalSeconds = startTime.secsTo(QDateTime::currentDateTime());
        qint64 days = totalSeconds / 86400;
        qint64 hours = (totalSeconds % 86400) / 3600;
        qint64 minutes = (totalSeconds % 3600) / 60;
        qint64 seconds = totalSeconds % 60;

        QString uptimeStr;
        if (days > 0) uptimeStr += QString("%1天 ").arg(days);
        if (hours > 0 || days > 0) uptimeStr += QString("%1时 ").arg(hours);
        uptimeStr += QString("%1分 %2秒").arg(minutes).arg(seconds);

        // 2. 获取机器人状态
        BotManager *botManager = war3bot.getBotManager();
        int online = 0;
        int idle = 0;
        int creating = 0;
        int inLobby = 0;
        int waiting = 0;
        int total = 0;

        if (botManager) {
            const auto &bots = botManager->getAllBots();
            total = bots.size();
            for(auto* b : bots) {
                if (b && b->client && b->client->isConnected()) {
                    online++;
                    // 细分状态统计
                    switch (b->state) {
                    case BotState::Idle: idle++; break;
                    case BotState::Creating: creating++; break;
                    case BotState::InLobby: inLobby++; break;
                    case BotState::Waiting: waiting++; break;
                    default: break;
                    }
                }
            }
        }

        // 3. 获取在线玩家状态
        NetManager *netManager = war3bot.getNetManager();
        int playerOnline = 0;
        QString playerDetails = "";

        if (netManager) {
            QList<RegisterInfo> players = netManager->getOnlinePlayers();
            playerOnline = players.size();

            if (playerOnline > 0) {
                std::sort(players.begin(), players.end(), [](const RegisterInfo& a, const RegisterInfo& b){
                    return a.firstSeen < b.firstSeen;
                });

                qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                QStringList detailsList;

                // 只取前 3 名
                int countToPrint = qMin(players.size(), 3);

                for (int i = 0; i < countToPrint; ++i) {
                    const RegisterInfo &p = players[i];

                    qint64 durationMs = nowMs - p.firstSeen;
                    qint64 durationSec = durationMs / 1000;

                    // 格式化时长
                    QString timeStr;
                    if (durationSec >= 86400) timeStr += QString("%1d").arg(durationSec / 86400);
                    if (durationSec >= 3600)  timeStr += QString("%1h").arg((durationSec % 86400) / 3600);
                    timeStr += QString("%1m").arg((durationSec % 3600) / 60);
                    if (timeStr.isEmpty()) timeStr = QString("%1s").arg(durationSec);

                    detailsList << QString("%1(%2)").arg(p.username, timeStr);
                }

                playerDetails = " -> [Top3: " + detailsList.join(", ");
                if (playerOnline > 3) {
                    playerDetails += QString(", ...等%1人").arg(playerOnline - 3);
                }
                playerDetails += "]";
            }
        }

        // 4. 打印详细日志
        LOG_INFO(QString("🔄 [服务器状态] 运行: %1 | Bot: %2/%3 (空闲:%4, 正在创建:%5, 大厅等待:%6, 房间等待:%7) | 玩家: %8%9")
                     .arg(uptimeStr)
                     .arg(online)           // %2
                     .arg(total)            // %3
                     .arg(idle)             // %4
                     .arg(creating)         // %5
                     .arg(inLobby)          // %6
                     .arg(waiting)          // %7
                     .arg(playerOnline)     // %8
                     .arg(playerDetails));  // %9
    });

    // 设置间隔为 30 秒 (30000 毫秒)
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

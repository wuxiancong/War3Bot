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
const QString IPC_SERVER_NAME = "/tmp/war3bot.ipc";

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
    QCoreApplication::setApplicationVersion("3.1");

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

    QCommandLineOption silentOption(QStringList() << "s" << "silent", "静默模式：禁用所有日志输出（不占用日志内存）");
    parser.addOption(silentOption);

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

    // === 1. 加载配置与日志初始化 (修改版) ===
    QString configFile = parser.value(configOption);
    bool configFound = false;

    // A. 如果用户在命令行指定了绝对路径，直接检查
    QFileInfo fileInfo(configFile);
    if (fileInfo.isAbsolute() && fileInfo.exists()) {
        configFound = true;
        LOG_INFO(QString("使用指定配置文件: %1").arg(configFile));
    }
    // B. 否则，在标准搜索路径中查找
    else {
        QStringList searchPaths;

        // 1. 命令行参数 (如果是相对路径，结合当前工作目录)
        searchPaths << configFile;
        searchPaths << "config/" + configFile;

        // 2. 开发/便携模式 (可执行文件所在目录)
        QString exeDir = QCoreApplication::applicationDirPath();
        searchPaths << exeDir + "/config/" + configFile;
        searchPaths << exeDir + "/" + configFile;

        // 3. Linux 系统标准安装路径
#ifdef Q_OS_LINUX
        searchPaths << "/etc/War3Bot/config/war3bot.ini";
        searchPaths << "/etc/War3Bot/war3bot.ini";
#endif

        // 遍历查找
        for (const QString &path : qAsConst(searchPaths)) {
            if (QFileInfo::exists(path)) {
                configFile = path;
                configFound = true;
                LOG_INFO(QString("✅ 找到配置文件: %1").arg(QDir::toNativeSeparators(configFile)));
                break;
            }
        }
    }

    // C. 如果到处都找不到，尝试在开发环境下生成默认配置
    if (!configFound) {
        LOG_WARNING("⚠️ 未找到配置文件，生成默认配置...");
        QString writePath = "config/war3bot.ini";
        QFileInfo writeInfo(writePath);
        QDir dir = writeInfo.absoluteDir();
        if (!dir.exists()) dir.mkpath(".");

        QFile defaultConfig(writePath);
        if (defaultConfig.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&defaultConfig);
            out << "[server]\nbroadcast_port=6112\nenable_broadcast=false\npeer_timeout=300000\ncleanup_interval=60000\nbroadcast_interval=30000\n";
            out << "\n[log]\nlevel=info\nenable_console=true\nlog_file=/var/log/War3Bot/war3bot.log\nmax_size=10485760\nbackup_count=5\n";
            out << "\n[bnet]\nserver=127.0.0.1\nport=6112\npassword=wxc123\n";
            out << "\n[bots]\nlist_number=1\ninit_count=10\nauto_generate=false\ndisplay_name=CC.Dota.XXX\n";
            out << "\n[mysql]\nhost=127.0.0.1\nport=3306\nuser=pvpgn\npass=Wxc@2409154\n";
            defaultConfig.close();
            configFile = writePath;
            LOG_INFO(QString("✅ 已创建默认配置文件: %1").arg(configFile));
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

    if (parser.isSet(silentOption)) {
        Logger::instance()->setDisabled(true);
        printf("🔇 静默模式已启用，日志系统已关闭。\n");
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
        QString configServer = settings.value("bnet/server", "").toString();
        ushort configPort = settings.value("bnet/port", 6112).toUInt();

        if (action == "log") {
            QString subOption = (parts.size() > 1) ? parts[1].toLower() : "";

            if (subOption == "off" || subOption == "0") {
                // 关闭控制台输出 (但文件日志依然会记录)
                Logger::instance()->enableConsoleOutput(false);
                // 这句话只能在日志文件里看到
                LOG_INFO("控制台日志已暂停 (输入 'log on' 恢复)");
                printf("🔇 控制台日志已暂停。现在可以清爽地输入命令了。\n");
            }
            else if (subOption == "on" || subOption == "1") {
                Logger::instance()->enableConsoleOutput(true);
                printf("🔊 控制台日志已恢复。\n");
                LOG_INFO("控制台日志已恢复");
            }
            else {
                printf("用法: log [on|off]\n");
            }
            return; // 处理完直接返回
        }
        // ---------------------------------------------------------
        // 命令: connect [用户名] [密码] [地址] [端口]
        // ---------------------------------------------------------
        else if (action == "connect") {
            QString user   = (parts.size() > 1) ? parts[1] : "";
            QString pass   = (parts.size() > 2) ? parts[2] : "";
            QString server = (parts.size() > 3) ? parts[3] : configServer;
            ushort port    = (parts.size() > 4) ? parts[4].toInt() : configPort;

            // 1. 如果没指定用户，全部上线
            if (user.isEmpty()) {
                LOG_INFO("🤖 执行批量上线 (加载配置数量的机器人)...");
                botManager->startAllBots();
            }
            // 2. 指定用户上线
            else {
                const auto &bots = botManager->getAllBots();
                bool found = false;
                for (auto *bot : bots) {
                    if (!bot || !bot->client) continue;
                    // 不区分大小写比较
                    if (bot->username.compare(user, Qt::CaseInsensitive) == 0) {
                        found = true;
                        if (bot->client->isConnected()) {
                            LOG_WARNING(QString("⚠️ 机器人 %1 已经在线 (状态: %2)，请先执行 stop 断开").arg(user).arg((int)bot->state));
                            break;
                        }

                        if (!pass.isEmpty()) {
                            bot->password = pass;
                        }

                        LOG_INFO(QString("🤖 [Bot-%1] 正在连接: %2 -> %3:%4")
                                     .arg(bot->id).arg(bot->username).arg(server, port));

                        QString targetServer = server.isEmpty() ? "127.0.0.1" : server;
                        ushort targetPort = (port == 0) ? 6112 : port;

                        bot->client->setCredentials(bot->username, bot->password, Protocol_SRP_0x53);
                        bot->client->connectToHost(targetServer, targetPort);
                        break;
                    }
                }
                if (!found) {
                    LOG_WARNING(QString("❌ 未找到指定的机器人: %1 (请检查是否已通过 startAllBots 加载)").arg(user));
                }
            }
        }
        // ---------------------------------------------------------
        // 命令: create <游戏模式> <游戏名称> [虚拟房主名]
        // ---------------------------------------------------------
        else if (action == "create") {
            if (parts.size() < 2) {
                LOG_WARNING("用法: create <游戏名称> <游戏名称> [虚拟房主名]");
                return;
            }
            QString gameMode = parts[1];
            QString gameName = parts[2];
            QString hostName = (parts.size() > 2) ? parts[3] : "Admin";
            QString consoleUuid = "CONSOLE_" + QString::number(QDateTime::currentMSecsSinceEpoch());

            // createGame 内部会自动处理寻找空闲Bot或扩容
            bool scheduled = botManager->createGame(hostName, gameName, gameMode, From_Client, consoleUuid);

            if (scheduled) {
                LOG_INFO(QString("✅ 任务已提交: 房主[%1] 房间[%2]").arg(hostName, gameName));
            } else {
                LOG_WARNING("❌ 创建请求被拒绝: 资源耗尽或冷却中");
            }
        }
        // ---------------------------------------------------------
        // 命令: cancel [Bot账号]
        // ---------------------------------------------------------
        else if (action == "cancel") {
            QString targetUser = (parts.size() > 1) ? parts[1] : "";
            const auto &bots = botManager->getAllBots();
            int count = 0;

            // 批量/单个 处理逻辑合并
            for (auto *bot : bots) {
                if (!bot || !bot->client) continue;

                // 筛选条件：不为空闲 && 不为断开 && (目标为空 OR 名字匹配)
                bool isTarget = targetUser.isEmpty() || (bot->username.compare(targetUser, Qt::CaseInsensitive) == 0);

                if (isTarget && bot->state != BotState::Idle && bot->state != BotState::Disconnected) {
                    botManager->removeGame(bot);
                    count++;
                    LOG_INFO(QString("✅ 已重置: Bot-%1 (%2) 的房间").arg(bot->id).arg(bot->username));

                    // 如果指定了单个目标，处理完直接退出循环
                    if (!targetUser.isEmpty()) break;
                }
            }

            if (count == 0) {
                LOG_WARNING(targetUser.isEmpty() ? "没有需要重置的房间。" : QString("未找到正在游戏的机器人: %1").arg(targetUser));
            } else {
                if (targetUser.isEmpty()) LOG_INFO(QString("已批量重置 %1 个房间。").arg(count));
            }
        }
        // ---------------------------------------------------------
        // 命令: stop [Bot账号] (停止广播)
        // ---------------------------------------------------------
        else if (action == "stop") {
            QString targetUser = (parts.size() > 1) ? parts[1] : "";
            const auto &bots = botManager->getAllBots();
            int count = 0;

            if (targetUser.isEmpty()) {
                LOG_INFO("🛑 正在停止 [所有] 机器人的广播...");
                for (auto *bot : bots) {
                    if (bot && bot->client) {
                        bot->client->stopAdv();
                        count++;
                    }
                }
                LOG_INFO(QString("已停止 %1 个机器人的广播").arg(count));
            } else {
                bool found = false;
                for (auto *bot : bots) {
                    if (bot && bot->client && bot->client->isConnected()) {
                        if (bot->username.compare(targetUser, Qt::CaseInsensitive) == 0) {
                            bot->client->stopAdv();
                            LOG_INFO(QString("✅ 已停止 Bot-%1 (%2) 的广播").arg(bot->id).arg(bot->username));
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) LOG_WARNING(QString("未找到指定的在线机器人: %1").arg(targetUser));
            }
        }
        else {
            LOG_INFO("未知命令。可用命令: connect, create, cancel, stop");
        }
    };

    // === 4. 控制台命令处理 ===
    Command *command = nullptr;
    if (enableConsole) {
        command = new Command(&app);
        QObject::connect(command, &Command::inputReceived, &app, processCommand);
        command->start();
        LOG_INFO("✅ 控制台命令监听已启动");
    }

    // === 5. 启动 IPC 本地服务器 ===
    QLocalServer *ipcServer = new QLocalServer(&app);
    QLocalServer::removeServer(IPC_SERVER_NAME);

    if (ipcServer->listen(IPC_SERVER_NAME)) {
        LOG_INFO(QString("✅ IPC 命令服务已启动，监听: %1").arg(ipcServer->fullServerName()));

#ifndef Q_OS_WIN
        // 在 Linux 上，QLocalServer 默认路径通常是 /tmp/<servername>
        QString socketPath = QDir::tempPath() + "/" + IPC_SERVER_NAME;
        QFile ipcFile(socketPath);

        if (ipcFile.exists()) {
            bool permOk = ipcFile.setPermissions(
                QFile::ReadOwner | QFile::WriteOwner |
                QFile::ReadGroup | QFile::WriteGroup |
                QFile::ReadOther | QFile::WriteOther
                );
            if (permOk) {
                LOG_INFO("   └─ 🔐 权限设置成功: 允许所有用户写入 (0666)");
            } else {
                LOG_WARNING("   └─ ⚠️ 权限设置失败，可能导致其他用户无法发送指令");
            }
        }
#endif

        QObject::connect(ipcServer, &QLocalServer::newConnection, &app, [ipcServer, processCommand]() {
            QLocalSocket *clientConnection = ipcServer->nextPendingConnection();
            if (!clientConnection) return;
            QObject::connect(clientConnection, &QLocalSocket::readyRead, [clientConnection, processCommand]() {
                while (clientConnection->canReadLine()) {
                    QByteArray data = clientConnection->readLine();
                    QString cmd = QString::fromUtf8(data).trimmed();
                    if (!cmd.isEmpty()) {
                        processCommand(cmd);
                    }
                }
                if (clientConnection->bytesAvailable() > 0) {
                    QByteArray data = clientConnection->readAll();
                    QString cmd = QString::fromUtf8(data).trimmed();
                    if (!cmd.isEmpty()) processCommand(cmd);
                }
            });
            QObject::connect(clientConnection, &QLocalSocket::disconnected, clientConnection, &QLocalSocket::deleteLater);
        });

    } else {
        LOG_ERROR(QString("❌ IPC 服务启动失败: %1").arg(ipcServer->errorString()));
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
        int reserved = 0;
        int waiting = 0;
        int starting = 0;
        int ingame = 0;
        int finishing = 0;
        int total = 0;

        if (botManager) {
            const auto &bots = botManager->getAllBots();
            total = bots.size();
            for(auto *bot : bots) {
                if (bot && bot->client &&bot->client->isConnected()) {
                    online++;
                    // 细分状态统计
                    switch (bot->state) {
                    case BotState::Idle: idle++; break;
                    case BotState::Creating: creating++; break;
                    case BotState::Reserved: reserved++; break;
                    case BotState::Waiting: waiting++; break;
                    case BotState::Starting: starting++; break;
                    case BotState::InGame: ingame++; break;
                    case BotState::Finishing: finishing++; break;
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
        LOG_INFO(QString("🔄 [服务器状态] 运行: %1 | Bot: %2/%3 (空闲:%4, 正在创建:%5, 房间预留:%6, 房间等待:%7, 正在开始:%8, 游戏中:%9, 正在结算:%10) | 玩家: %11%12")
                     .arg(uptimeStr)
                     .arg(online)           // %2
                     .arg(total)            // %3
                     .arg(idle)             // %4
                     .arg(creating)         // %5
                     .arg(reserved)         // %6
                     .arg(waiting)          // %7
                     .arg(starting)         // %8
                     .arg(ingame)           // %9
                     .arg(finishing)        // %10
                     .arg(playerOnline)     // %11
                     .arg(playerDetails));  // %12
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

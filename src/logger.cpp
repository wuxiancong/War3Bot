#include "logger.h"
#include <QDir>
#include <QDebug>
#include <QTextCodec>

#ifdef Q_OS_WIN
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

namespace Color {
const QString RESET   = "\033[0m";
const QString BLACK   = "\033[30m";
const QString RED     = "\033[31m";
const QString GREEN   = "\033[32m";
const QString YELLOW  = "\033[33m";
const QString BLUE    = "\033[34m";
const QString MAGENTA = "\033[35m";
const QString CYAN    = "\033[36m";
const QString WHITE   = "\033[37m";
const QString BOLD    = "\033[1m";
}

Logger *Logger::m_instance = nullptr;

Logger *Logger::instance()
{
    static QMutex mutex;
    if (!m_instance) {
        QMutexLocker locker(&mutex);
        if (!m_instance) {
            m_instance = new Logger();
        }
    }
    return m_instance;
}

void Logger::destroyInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}

Logger::Logger(QObject *parent)
    : QObject(parent)
    , m_logFile(nullptr)
    , m_stream(nullptr)
    , m_logLevel(LOG_INFO)
    , m_consoleOutput(true)
    , m_maxFileSize(10 * 1024 * 1024)   // 默认10MB
    , m_backupCount(5)                  // 默认5个备份
    , m_disabled(false)
{
#ifdef Q_OS_WIN
    // 1. 设置编码
    SetConsoleOutputCP(65001);

    // 2. 启用 Windows 10+ 的 ANSI 转义序列支持
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif

    // 设置全局编码
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
}

Logger::~Logger()
{
    if (m_stream) {
        m_stream->flush();
        delete m_stream;
        m_stream = nullptr;
    }
    if (m_logFile) {
        m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }
}

void Logger::setMaxFileSize(qint64 maxSize)
{
    QMutexLocker locker(&m_mutex);
    m_maxFileSize = maxSize;
}

void Logger::setBackupCount(int count)
{
    QMutexLocker locker(&m_mutex);
    m_backupCount = count;
}

void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

void Logger::setLogFile(const QString &filename)
{
    QMutexLocker locker(&m_mutex);

    // 1. 清理现有资源
    if (m_stream) {
        m_stream->flush();
        delete m_stream;
        m_stream = nullptr;
    }
    if (m_logFile) {
        m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }

    // 2. 更新路径信息
    m_logFileName = filename;
    QFileInfo fileInfo(filename);
    m_logFileBaseName = fileInfo.completeBaseName();
    m_logFileDir = fileInfo.absolutePath();

    // 3. 确保目录存在
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
        consoleOutput("FATAL: 无法创建日志目录: " + dir.absolutePath(), true);
        return;
    }

    // 4. 检查现有文件是否已经超过大小限制
    if (fileInfo.exists() && fileInfo.size() >= m_maxFileSize) {
        performLogRotation();
    }

    // 5. 打开文件
    m_logFile = new QFile(filename);
    if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_stream = new QTextStream(m_logFile);
        m_stream->setCodec("UTF-8");
        m_stream->setGenerateByteOrderMark(true); // 保持 UTF-8 BOM

        // 6. 写入本次启动的记录起始标识
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        *m_stream << QString("[%1] [INFO] === 日志服务启动 (Size: %2 bytes) ===\n")
                         .arg(timestamp).arg(m_logFile->size());
        m_stream->flush();

        consoleOutput("日志文件已设置: " + filename);
    } else {
        consoleOutput("无法打开日志文件: " + filename + " 原因: " + m_logFile->errorString(), true);
        delete m_logFile;
        m_logFile = nullptr;
    }
}

void Logger::enableConsoleOutput(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_consoleOutput = enable;
}

bool Logger::rotateLogFileIfNeeded()
{
    if (m_logFileName.isEmpty() || !m_logFile) {
        return false;
    }

    // 检查当前文件大小
    if (m_logFile->size() < m_maxFileSize) {
        return false;
    }

    // 关闭当前的文件流
    if (m_stream) {
        m_stream->flush();
        delete m_stream;
        m_stream = nullptr;
    }
    if (m_logFile) {
        m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }

    // 执行日志轮转
    return performLogRotation();
}

bool Logger::performLogRotation()
{
    QDir logDir(m_logFileDir);
    if (!logDir.exists()) {
        return false;
    }

    QString logExtension = ".log";

    // 删除最旧的备份文件
    QString oldestBackup = m_logFileBaseName + "_" + QString::number(m_backupCount) + logExtension;
    QFile::remove(logDir.filePath(oldestBackup));

    // 重命名现有的备份文件
    for (int i = m_backupCount - 1; i >= 1; i--) {
        QString oldName = m_logFileBaseName + "_" + QString::number(i) + logExtension;
        QString newName = m_logFileBaseName + "_" + QString::number(i + 1) + logExtension;
        QFile::rename(logDir.filePath(oldName), logDir.filePath(newName));
    }

    // 将当前日志文件重命名为第一个备份
    QString firstBackup = m_logFileBaseName + "_1" + logExtension;
    if (QFile::exists(m_logFileName)) {
        QFile::rename(m_logFileName, logDir.filePath(firstBackup));
    }

    // 重新打开日志文件
    m_logFile = new QFile(m_logFileName);
    if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_stream = new QTextStream(m_logFile);
        m_stream->setCodec("UTF-8");
        m_stream->setGenerateByteOrderMark(true);

        // 写入轮转提示信息
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        QString rotateMessage = QString("[%1] [INFO] 日志文件已轮转，新文件开始").arg(timestamp);
        *m_stream << rotateMessage << "\n";
        m_stream->flush();

        consoleOutput("日志文件轮转完成");
        return true;
    } else {
        consoleOutput("轮转后无法重新打开日志文件: " + m_logFileName, true);
        delete m_logFile;
        m_logFile = nullptr;
        return false;
    }
}

void Logger::consoleOutput(const QString &message, bool isError)
{
    if (!m_consoleOutput) return;

    QString colorCode = isError ? Color::RED : Color::RESET;
    QString coloredMessage = colorCode + message + Color::RESET;

    QTextStream stream(isError ? stderr : stdout);
    stream.setCodec("UTF-8");
    stream << coloredMessage << "\n";
    stream.flush();
}

void Logger::consoleOutput(LogLevel level, const QString &message)
{
    if (!m_consoleOutput) return;

    QString colorCode;
    bool toStderr = false;

    switch (level) {
    case LOG_DEBUG:
        colorCode = Color::CYAN;                    // 青色
        break;
    case LOG_INFO:
        colorCode = Color::GREEN;                   // 绿色
        break;
    case LOG_WARNING:
        colorCode = Color::YELLOW;                  // 黄色
        break;
    case LOG_ERROR:
        colorCode = Color::RED;                     // 红色
        toStderr = true;
        break;
    case LOG_CRITICAL:
        colorCode = Color::MAGENTA + Color::BOLD;   // 紫红色加粗
        toStderr = true;
        break;
    default:
        colorCode = Color::RESET;
        break;
    }

    // 拼接颜色代码 + 消息 + 重置代码
    QString coloredMessage = colorCode + message + Color::RESET;

    QTextStream stream(toStderr ? stderr : stdout);
    stream.setCodec("UTF-8");
    stream << coloredMessage << "\n";
    stream.flush();
}

void Logger::debug(const QString &message)
{
    log(LOG_DEBUG, message);
}

void Logger::info(const QString &message)
{
    log(LOG_INFO, message);
}

void Logger::warning(const QString &message)
{
    log(LOG_WARNING, message);
}

void Logger::error(const QString &message)
{
    log(LOG_ERROR, message);
}

void Logger::critical(const QString &message)
{
    log(LOG_CRITICAL, message);
}

void Logger::log(LogLevel level, const QString &message, int depth)
{
    // 1. 快速过滤
    if (level < m_logLevel || m_disabled) return;

    QMutexLocker locker(&m_mutex);

    // 2. 检查并处理日志轮转
    rotateLogFileIfNeeded();

    // 如果没有设置日志文件且没有控制台输出，直接返回
    if (!m_stream && !m_consoleOutput) return;

    // 3. 格式化消息内容
    QString indent = QString(depth * 4, ' ');
    QString treeMessage = (depth > 0 ? "└─ " : "") + message;

    QString levelStr;
    switch (level) {
    case LOG_DEBUG:    levelStr = "DEBUG"; break;
    case LOG_INFO:     levelStr = "INFO"; break;
    case LOG_WARNING:  levelStr = "WARNING"; break;
    case LOG_ERROR:    levelStr = "ERROR"; break;
    case LOG_CRITICAL: levelStr = "CRITICAL"; break;
    default:           levelStr = "INFO"; break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logMessage = QString("[%1] [%2] %3%4").arg(timestamp, levelStr, indent, treeMessage);

    // 4. 输出到控制台
    if (m_consoleOutput) {
        consoleOutput(level, logMessage);
    }

    // 5. 输出到文件
    if (m_stream && m_logFile && m_logFile->isOpen()) {
        *m_stream << logMessage << "\n";
        m_stream->flush();

        if (m_stream->status() != QTextStream::Ok) {
            consoleOutput(LOG_CRITICAL, "日志写入失败，磁盘空间可能已满或文件被锁定。");
        }
    }
}

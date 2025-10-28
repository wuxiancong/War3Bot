#include "logger.h"
#include <QDir>
#include <QDebug>
#include <iostream>

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

void Logger::destroy()
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
    , m_logLevel(INFO)
    , m_consoleOutput(true)
{
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

void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

void Logger::setLogFile(const QString &filename)
{
    QMutexLocker locker(&m_mutex);

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

    m_logFileName = filename;
    
    // 确保目录存在
    QFileInfo fileInfo(filename);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    m_logFile = new QFile(filename);
    if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_stream = new QTextStream(m_logFile);
        m_stream->setCodec("UTF-8");
    } else {
        std::cerr << "Cannot open log file: " << filename.toStdString() << std::endl;
        delete m_logFile;
        m_logFile = nullptr;
    }
}

void Logger::enableConsoleOutput(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_consoleOutput = enable;
}

void Logger::debug(const QString &message)
{
    log(DEBUG, message);
}

void Logger::info(const QString &message)
{
    log(INFO, message);
}

void Logger::warning(const QString &message)
{
    log(WARNING, message);
}

void Logger::error(const QString &message)
{
    log(ERROR, message);
}

void Logger::critical(const QString &message)
{
    log(CRITICAL, message);
}

void Logger::log(LogLevel level, const QString &message)
{
    if (level < m_logLevel) return;

    QMutexLocker locker(&m_mutex);

    QString levelStr;
    switch (level) {
    case DEBUG: levelStr = "DEBUG"; break;
    case INFO: levelStr = "INFO"; break;
    case WARNING: levelStr = "WARNING"; break;
    case ERROR: levelStr = "ERROR"; break;
    case CRITICAL: levelStr = "CRITICAL"; break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logMessage = QString("[%1] [%2] %3")
                             .arg(timestamp,levelStr,message);

    // 输出到控制台
    if (m_consoleOutput) {
        if (level >= ERROR) {
            std::cerr << logMessage.toStdString() << std::endl;
        } else {
            std::cout << logMessage.toStdString() << std::endl;
        }
    }

    // 输出到文件
    if (m_stream) {
        *m_stream << logMessage << "\n";
        m_stream->flush();
    } else if (!m_logFileName.isEmpty()) {
        // 如果文件打开失败，尝试重新打开
        m_logFile = new QFile(m_logFileName);
        if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            m_stream = new QTextStream(m_logFile);
            m_stream->setCodec("UTF-8");
            *m_stream << logMessage << "\n";
            m_stream->flush();
        } else {
            delete m_logFile;
            m_logFile = nullptr;
        }
    }
}

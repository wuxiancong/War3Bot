#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDateTime>

class Logger : public QObject
{
    Q_OBJECT

public:
    enum LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3,
        CRITICAL = 4
    };

    static Logger* instance();
    static void destroy();

    void setLogLevel(LogLevel level);
    void setLogFile(const QString &filename);
    void enableConsoleOutput(bool enable);

    void debug(const QString &message);
    void info(const QString &message);
    void warning(const QString &message);
    void error(const QString &message);
    void critical(const QString &message);

private:
    Logger(QObject *parent = nullptr);
    ~Logger();

    void log(LogLevel level, const QString &message);

    static Logger *m_instance;
    QFile *m_logFile;
    QTextStream *m_stream;
    QMutex m_mutex;
    LogLevel m_logLevel;
    bool m_consoleOutput;
    QString m_logFileName;
};

// 宏定义便于使用
#define LOG_DEBUG(message) Logger::instance()->debug(message)
#define LOG_INFO(message) Logger::instance()->info(message)
#define LOG_WARNING(message) Logger::instance()->warning(message)
#define LOG_ERROR(message) Logger::instance()->error(message)
#define LOG_CRITICAL(message) Logger::instance()->critical(message)

#endif // LOGGER_H

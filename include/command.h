#ifndef COMMAND_H
#define COMMAND_H

#include <QThread>
#include <QObject>

class Command : public QThread
{
    Q_OBJECT

public:
    explicit Command(QObject *parent = nullptr);
    ~Command() override;

    // 解析聊天命令
    void process(quint8 hostPid, const QString &command);

protected:
    void run() override;

signals:
    void inputReceived(QString command);
};

#endif // COMMAND_H

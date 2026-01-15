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

protected:
    void run() override;

signals:
    void inputReceived(QString command);
};

#endif // COMMAND_H

#ifndef COMMAND_H
#define COMMAND_H

#include <QThread>
#include <QString>

class Command : public QThread
{
    Q_OBJECT
public:
    explicit Command(QObject *parent = nullptr);
    ~Command() override;

protected:
    void run() override;

signals:
    void inputReceived(QString cmd);
};

#endif // COMMAND_H

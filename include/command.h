#ifndef COMMAND_H
#define COMMAND_H

#include <QThread>
#include <QObject>

// 前向声明
class Client;

class Command : public QThread
{
    Q_OBJECT

public:
    explicit Command(Client *client = nullptr, QObject *parent = nullptr);
    ~Command() override;

    // 解析聊天命令
    void process(quint8 hostPid, const QString &command);

protected:
    void run() override;

signals:
    void inputReceived(QString command);

private:
    // 具体指令实现
    void cmdHold(quint8 hostPid, QString target, quint8 s);        // 占用位置
    void cmdKick(quint8 hostPid, QString target);                  // 踢人
    void cmdSwap(quint8 s1, quint8 s2);                            // 交换位置
    void cmdClose(quint8 s);                                       // 关闭位置
    void cmdOpen(quint8 s);                                        // 打开位置
    void cmdAbort();                                               // 取消倒计时/解散
    void cmdCheck();                                               // 强制检查地图
    void cmdStart();                                               // 开始游戏

private:
    Client* m_client;
};

#endif // COMMAND_H

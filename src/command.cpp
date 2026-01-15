#include "command.h"

#include <QTimer>
#include <QDebug>

Command::Command(QObject *parent)
    : QThread(parent)
{
}

Command::~Command()
{

}

void Command::run() {
    QTextStream qin(stdin);

    while (!isInterruptionRequested()) {
        // 阻塞读取一行
        QString line = qin.readLine();

        // 如果读取到空并且 stdin 状态异常 (通常发生在后台服务模式)，则退出线程
        if (line.isNull()) {
            msleep(1000); // 防止死循环空转
            if (qin.status() != QTextStream::Ok) {
                break; // stdin 已关闭或失效，停止监听
            }
            continue;
        }

        // 只有非空行才发送信号
        if (!line.trimmed().isEmpty()) {
            emit inputReceived(line);
        }
    }
}

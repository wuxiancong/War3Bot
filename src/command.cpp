#include "command.h"
#include <iostream>
#include <string>

Command::Command(QObject *parent)
    : QThread(parent)
{
}

Command::~Command()
{

}

void Command::run()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        QString cmd = QString::fromStdString(line).trimmed();
        if (!cmd.isEmpty()) {
            emit inputReceived(cmd);
        }
    }
}

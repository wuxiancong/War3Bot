#include "command.h"
#include <iostream>
#include <string>

#include <QTimer>

#include "client.h"
#include "logger.h"

Command::Command(Client *client, QObject *parent)
    : QThread(parent), m_client(client)
{
    Q_ASSERT(m_client); // 确保 Client 不为空
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

void Command::process(quint8 hostPid, const QString &command)
{
    QStringList args = command.split(' ', Qt::SkipEmptyParts);
    if (args.isEmpty()) return;

    QString cmd = args[0].toLower();
    LOG_INFO(QString("🏠 房主 [%1] 执行指令: %2").arg(hostPid).arg(command));

    // --- 1. /kick <name/slot> ---
    if (cmd == "/kick" && args.size() > 1) {
        cmdKick(hostPid, args[1]);
    }
    // --- 2. /start [force] ---
    else if (cmd == "/start") {
        cmdStart();
    }
    // --- 3. /swap <slot1> <slot2> ---
    else if (cmd == "/swap" && args.size() > 2) {
        cmdSwap(args[1].toInt(), args[2].toInt());
    }
    // --- 4. /abort ---
    else if (cmd == "/abort") {
        cmdAbort();
    }
    // --- 5. /open <slot> ---
    else if (cmd == "/open" && args.size() > 1) {
        cmdOpen(args[1].toInt());
    }
    // --- 6. /close <slot> ---
    else if (cmd == "/close" && args.size() > 1) {
        cmdClose(args[1].toInt());
    }
    // --- 7. /hold <name> [slot] ---
    else if (cmd == "/hold" && args.size() > 1) {
        QString targetName = args[1];
        quint8 slotId = 0; // 0 表示自动寻找
        if (args.size() > 2) {
            slotId = args[2].toInt();
        }
        cmdHold(hostPid, targetName, slotId);
    }
}

// === 具体实现 ===

void Command::cmdKick(quint8 hostPid, QString target)
{
    quint8 targetPid = 0;

    // 1. 按槽位查找
    bool ok;
    quint8 slotIdx = target.toInt(&ok);
    quint8 maxSize = m_client->m_slots.size();
    if (ok && slotIdx > 0 && slotIdx <= maxSize) {
        targetPid = m_client->m_slots[slotIdx - 1].pid;
    }
    // 2. 按名字查找
    else {
        for (auto it = m_client->m_players.begin(); it != m_client->m_players.end(); ++it) {
            if (it.value().name.contains(target, Qt::CaseInsensitive)) {
                targetPid = it.key();
                break;
            }
        }
    }

    if (targetPid == 0 || targetPid == 1 || targetPid == hostPid) return;

    LOG_INFO(QString("👢 踢出 PID: %1").arg(targetPid));

    if (m_client->m_players.contains(targetPid)) {
        m_client->m_players[targetPid].socket->disconnectFromHost();
    }
}

void Command::cmdOpen(quint8 s)
{
    // 输入通常是 1-12，转换为索引 0-11
    qint8 idx = s - 1;
    auto &slotsData = m_client->m_slots;

    // 边界检查 & 保护 Bot 槽位 (假设 Bot 在 Slot 0 或 11，PID为1)
    if (idx < 0 || idx >= slotsData.size()) return;
    if (slotsData[idx].pid == 1) {
        LOG_WARNING("⚠️ 无法操作主机/Bot所在的槽位！");
        return;
    }

    // 如果该位置有玩家，先踢出
    if (slotsData[idx].slotStatus == 2 && slotsData[idx].pid != 0) {
        // 利用现有的 cmdKick 逻辑传 Slot ID 字符串
        cmdKick(1, QString::number(s));
    }

    // 修改槽位状态
    slotsData[idx].pid = 0;              // 清空 PID
    slotsData[idx].downloadStatus = 255; // 重置下载状态
    slotsData[idx].slotStatus = 0;       // 0 = cmdOpen
    slotsData[idx].computer = 0;         // 移除电脑标志

    m_client->broadcastSlotInfo();
    LOG_INFO(QString("🔓 槽位 %1 已开放").arg(s));
}

void Command::cmdClose(quint8 s)
{
    qint8 idx = s - 1;
    auto &slotsData = m_client->m_slots;

    if (idx < 0 || idx >= slotsData.size()) return;
    if (slotsData[idx].pid == 1) {
        LOG_WARNING("⚠️ 无法关闭主机/Bot所在的槽位！");
        return;
    }

    // 如果该位置有玩家，先踢出
    if (slotsData[idx].slotStatus == 2 && slotsData[idx].pid != 0) {
        cmdKick(1, QString::number(s));
    }

    // 修改槽位状态
    slotsData[idx].pid = 0;
    slotsData[idx].downloadStatus = 255;
    slotsData[idx].slotStatus = 1;       // 1 = Closed
    slotsData[idx].computer = 0;

    m_client->broadcastSlotInfo();
    LOG_INFO(QString("🔒 槽位 %1 已关闭").arg(s));
}

void Command::cmdHold(quint8 hostPid, QString target, quint8 s)
{
    qint8 idx = -1;
    auto &slotsData = m_client->m_slots;

    if (s > 0) {
        // 指定了槽位
        idx = s - 1;
        if (idx < 0 || idx >= slotsData.size() || slotsData[idx].pid == 1) {
            LOG_WARNING("⚠️ cmdHold 目标槽位无效");
            return;
        }
    } else {
        for (quint8 i = 0; i < slotsData.size(); ++i) {
            if (slotsData[i].pid == 1) continue; // 跳过 Bot
            if (slotsData[i].slotStatus != 2) {  // 0 或 1 都可以 cmdHold
                idx = i;
                break;
            }
        }

        // 如果都满了，但这只是个简单实现，就不踢随机路人了
        if (idx == -1) {
            LOG_WARNING("⚠️ 没有空闲槽位进行 cmdHold");
            return;
        }
    }

    if (slotsData[idx].slotStatus == 2 && slotsData[idx].pid != 0) {
        if (m_client->m_players.contains(slotsData[idx].pid)) {
            if (m_client->m_players[slotsData[idx].pid].name != target) {
                cmdKick(hostPid, QString::number(idx + 1));
            } else {
                LOG_INFO(QString("✅ 玩家 %1 已经在槽位 %2 上了").arg(target).arg(idx + 1));
                return;
            }
        }
    }

    // 设置为开放
    slotsData[idx].pid = 0;
    slotsData[idx].downloadStatus = NotStarted;
    slotsData[idx].slotStatus = Open;
    slotsData[idx].computer = Human;

    m_client->broadcastSlotInfo();

    // 发送一条聊天消息告知
    QString msgZH = QString("槽位 %1 已为 [%2] 预留").arg(idx + 1).arg(target);
    QString msgEN = QString("Slot %1 reserved for [%2]").arg(idx + 1).arg(target);
    MultiLangMsg chat;
    chat.add("CN", msgZH).add("EN", msgEN);
    m_client->broadcastChatMessage(chat);

    LOG_INFO(QString("🛡️ 槽位 %1 已为 [%2] 预留 (cmdOpen)").arg(idx + 1).arg(target));
}

void Command::cmdSwap(quint8 s1, quint8 s2)
{
    qint8 idx1 = s1 - 1;
    qint8 idx2 = s2 - 1;
    auto &slotsData = m_client->m_slots;

    if (idx1 < 0 || idx1 >= slotsData.size() || idx2 < 0 || idx2 >= slotsData.size()) return;
    if (slotsData[idx1].pid == 1 || slotsData[idx2].pid == 1) return;

    std::swap(slotsData[idx1], slotsData[idx2]);
    std::swap(slotsData[idx1].team, slotsData[idx2].team);
    std::swap(slotsData[idx1].color, slotsData[idx2].color);

    m_client->broadcastSlotInfo();
    LOG_INFO(QString("🔄 交换槽位 %1 <-> %2").arg(s1).arg(s2));
}

void Command::cmdStart()
{
    LOG_INFO("🚀 开始游戏倒计时...");

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out << (quint8)0xF7 << (quint8)0x0A << (quint16)0;

    QDataStream lenStream(&packet, QIODevice::ReadWrite);
    lenStream.setByteOrder(QDataStream::LittleEndian);
    lenStream.skipRawData(2);
    lenStream << (quint16)packet.size();

    m_client->broadcastPacket(packet, 0);

    // 使用 Client 的定时器或 SingleShot
    QTimer::singleShot(5000, m_client, [this](){
        LOG_INFO("🏁 倒计时结束");
        m_client->m_gameStarted = true;
        m_client->stopAdv();
        // 发送 0x0B ...
        // 同样调用 m_client->broadcastPacket(...)
    });
}

void Command::cmdAbort()
{
    // 取消倒计时等逻辑
}

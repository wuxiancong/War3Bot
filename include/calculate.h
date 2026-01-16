#ifndef CALCULATE_H
#define CALCULATE_H

#include <QByteArray>
#include <QtGlobal>

/**
 * @brief 计算 War3 协议标准的 CRC16 校验值 (XMODEM/CCITT 变体)
 * @note  可能用于 UDP 层的校验或某些特定的旧版协议包。
 *        注意：这**不是**用于游戏同步包 (0x0C) 的算法。
 * @param data 需要计算的数据
 * @return 计算出的 16 位校验和
 */
quint16 calculateStandardCRC16(const QByteArray &data);

/**
 * @brief 计算标准 CRC32 并截取低 16 位
 * @note  **核心修复**：专用于 W3GS_INCOMING_ACTION (0x0C) 游戏同步包。
 *        War3 和 Ghost++ 在处理 0x0C 包时，实际上是计算完整的 CRC32 (IEEE 802.3)，
 *        然后强制转换为 16 位 (截取低 2 字节) 放入包头。
 *        如果错误地使用了 calculateStandardCRC16，会导致客户端校验失败，
 *        表现为进游戏瞬间跳出结算画面 (Desync)。
 * @param data 需要计算的数据 (通常是 Action Block)
 * @return (quint16)(CRC32 & 0xFFFF)
 */
quint16 calculateCRC32Lower16(const QByteArray &data);

#endif // CALCULATE_H

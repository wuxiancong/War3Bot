#ifndef CALCULATE_H
#define CALCULATE_H

#include <QByteArray>
#include <QtGlobal>

/**
 * @brief 计算 War3 协议标准的 CRC16 校验值
 * @param data 需要计算的数据
 * @return 计算出的 16 位校验和
 */
quint16 calculateCRC16(const QByteArray &data);
quint16 calculateGhostCRC(const QByteArray &data)

#endif // CALCULATE_H

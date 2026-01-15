#include "calculate.h"

quint16 calculateCRC16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;

    // 获取数据长度
    int len = data.size();

    // 获取无符号指针
    const unsigned char *p = reinterpret_cast<const unsigned char*>(data.constData());

    for (int i = 0; i < len; i++) {
        // 取出字节 (再次确保是无符号)
        unsigned char byte = p[i];

        // 如果这里 byte 是负数，位运算结果会完全不同！
        unsigned char x = (unsigned char)((crc >> 8) ^ byte);
        x ^= x >> 4;
        crc = (quint16)((crc << 8) ^ (quint16)(x << 12) ^ (quint16)(x << 5) ^ (quint16)x);
    }

    return crc;
}

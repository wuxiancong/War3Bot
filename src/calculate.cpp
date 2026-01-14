#include "calculate.h"

quint16 calculateCRC16(const QByteArray &data)
{
    unsigned char *p = (unsigned char *)data.data();
    int len = data.size();
    quint16 crc = 0xFFFF;

    for (int i = 0; i < len; i++) {
        unsigned char byte = p[i];

        // 注意这里的位运算逻辑，和标准 CRC16-CCITT 有区别
        unsigned char x = (unsigned char)((crc >> 8) ^ byte);
        x ^= x >> 4;
        crc = (quint16)((crc << 8) ^ (quint16)(x << 12) ^ (quint16)(x << 5) ^ (quint16)x);
    }

    return crc;
}

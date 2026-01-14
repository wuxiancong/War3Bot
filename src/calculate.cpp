#include "calculate.h"

quint16 calculateCRC16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    const char *p = data.constData();
    int len = data.size();

    for (int i = 0; i < len; i++) {
        unsigned char byte = (unsigned char)p[i];

        unsigned char x = (crc >> 8) ^ byte;
        x ^= x >> 4;
        crc = (crc << 8) ^ (quint16)(x << 12) ^ (quint16)(x << 5) ^ (quint16)x;
    }
    return crc;
}

#include "calculate.h"

#include "zlib.h"

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

quint16 calculateGhostCRC(const QByteArray &actionBlock) {
    if (actionBlock.isEmpty()) return 0;
    uLong zCrc = crc32(0L, Z_NULL, 0);
    zCrc = crc32(zCrc, reinterpret_cast<const Bytef*>(actionBlock.constData()), actionBlock.size());
    quint32 fullCrc32 = static_cast<quint32>(zCrc);
    return (quint16)((fullCrc32 >> 16) ^ (fullCrc32 & 0xFFFF));
}

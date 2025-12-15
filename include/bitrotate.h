#ifndef BITROTATE_H
#define BITROTATE_H

#include <QtGlobal>

/**
 * @brief 安全的位循环左移函数 (Template Implementation)
 *
 * 逻辑解释：
 * (x << n) | (x >> (width - n))
 *
 * 利用 mask = width - 1 和 2's complement 特性：
 * ((-n) & mask) 等价于 (width - n) % width
 * 这可以安全地处理 n=0 或 n>=width 的情况，避免未定义行为。
 */
template <typename T>
static inline T RotateLeft(T x, int n)
{
    // 获取类型 T 的位宽 (例如 32 或 16)
    const int width = sizeof(T) * 8;
    const int mask = width - 1;

    // 强制转换为 unsigned 避免符号位扩展问题
    return (x << (n & mask)) | (x >> ((-n) & mask));
}

#ifndef ROTL32
#define ROTL32(x, n) RotateLeft<quint32>((quint32)(x), (int)(n))
#endif

#ifndef ROTL16
#define ROTL16(x, n) RotateLeft<quint16>((quint16)(x), (int)(n))
#endif

#endif // BITROTATE_H

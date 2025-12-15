/*
 * -------------------------------------------------------------------------
 * 文件名: bigint.h
 * 说明:
 *      任意大小无符号整数运算类 (BigInt) 的 Qt 移植版。
 *      该类用于实现 SRP (Secure Remote Password) 协议所需的数学运算，
 *      特别是大数的加减乘除和模幂运算。
 *
 *      原作者: Olaf Freyer
 * -------------------------------------------------------------------------
 */

#ifndef BIGINT_H
#define BIGINT_H

#include <QVector>
#include <QByteArray>
#include <QString>

class BigInt
{
public:
    // 默认构造函数，初始化为 0
    BigInt();

    // 从 64 位整数构造
    explicit BigInt(quint64 input);

    /**
     * @brief 核心构造函数：从字节序列构造大数
     * @param bytes 输入的字节数组
     * @param blockSize 块大小 (用于特殊的字节交换逻辑，默认为1表示不交换)
     * @param bigEndian 输入数据是否为大端序 (Big Endian)
     */
    BigInt(const QByteArray &bytes, int blockSize = 1, bool bigEndian = true);
    BigInt(const unsigned char *input, int input_size, int blockSize = 1, bool bigEndian = true);

    // 拷贝构造与赋值 (使用编译器生成的默认实现，QVector 支持隐式共享)
    BigInt(const BigInt& other) = default;
    BigInt& operator=(const BigInt& other) = default;
    ~BigInt() = default;

    // === 比较运算符 ===
    bool operator== (const BigInt& right) const;
    bool operator!= (const BigInt& right) const;
    bool operator<  (const BigInt& right) const;
    bool operator>  (const BigInt& right) const;
    bool operator<= (const BigInt& right) const;
    bool operator>= (const BigInt& right) const;

    // === 算术运算符 ===
    BigInt operator+ (const BigInt& right) const;
    BigInt operator- (const BigInt& right) const; // 注意：若结果为负，则返回 0 (仅支持无符号)
    BigInt operator* (const BigInt& right) const;
    BigInt operator/ (const BigInt& right) const;
    BigInt operator% (const BigInt& right) const;
    BigInt operator<< (int bytesToShift) const;   // 按字节左移 (即左移 bytesToShift * 8 位)

    // === 工具函数 ===

    // 转为十六进制字符串 (小写)
    QString toHexString() const;

    // 生成指定字节长度的随机大数
    static BigInt random(int size);

    /**
     * @brief 模幂运算 (Modular Exponentiation)
     * 计算 (this ^ exp) % mod
     * 这是 SRP 协议中最核心的加密计算函数。
     */
    BigInt powm(const BigInt& exp, const BigInt& mod) const;

    /**
     * @brief 导出为字节数组
     * @param minByteCount 最小输出长度 (不足补0)
     * @param blockSize 块交换大小
     * @param bigEndian 是否输出为大端序
     */
    QByteArray toByteArray(int minByteCount = 0, int blockSize = 1, bool bigEndian = true) const;

    // 导出数据到原始指针 (为了兼容旧代码接口)
    void getData(unsigned char *out, int byteCount, int blockSize = 1, bool bigEndian = true) const;

private:
    // 32位掩码
    static const quint64 BASE_MASK = 0xFFFFFFFF;

    // 内部数据存储：使用 32 位无符号整数的分段数组
    // 存储顺序为 Little Endian (index 0 存储最低位)
    QVector<quint32> m_data;

    // 移除高位多余的 0 段
    void trim();

    // 安全获取指定索引的段值，越界返回 0
    static quint32 getVal(const QVector<quint32>& vec, int index);
};

#endif // BIGINT_H

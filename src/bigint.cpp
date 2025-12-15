/*
 * -------------------------------------------------------------------------
 * 文件名: bigint.cpp
 * 说明:
 *      BigInt 类的具体实现。
 *      包含了大数的基本算术运算算法（加减乘除、模幂）。
 *      除法实现采用了基于 Knuth Algorithm D 的估算逻辑。
 * -------------------------------------------------------------------------
 */

#include "bigint.h"
#include <algorithm>
#include <QtMath>
#include <QRandomGenerator>

// 辅助函数：安全读取 Vector，越界则视为 0
quint32 BigInt::getVal(const QVector<quint32> &vec, int index)
{
    if (index < 0 || index >= vec.size()) return 0;
    return vec[index];
}

// 辅助函数：移除内部数组末尾（高位）的 0，保持数组紧凑
// 至少保留一个元素（即使是 0）
void BigInt::trim()
{
    while (m_data.size() > 1 && m_data.last() == 0) {
        m_data.removeLast();
    }
}

// 默认构造：值为 0
BigInt::BigInt()
{
    m_data.append(0);
}

// 从 quint64 构造
BigInt::BigInt(quint64 input)
{
    // 低 32 位
    m_data.append((quint32)(input & BASE_MASK));
    // 高 32 位
    m_data.append((quint32)((input >> 32) & BASE_MASK));
    trim();
}

// 便捷构造函数：直接接受 QByteArray
BigInt::BigInt(const QByteArray &bytes, int blockSize, bool bigEndian)
    : BigInt((const unsigned char*)bytes.constData(), bytes.size(), blockSize, bigEndian)
{
}

/**
 * @brief 核心构造函数
 * 处理输入的字节流，支持大小端转换和块内交换，最终转换为内部的 Little Endian 32位整数数组。
 */
BigInt::BigInt(const unsigned char *input, int input_size, int blockSize, bool bigEndian)
{
    if (input_size <= 0) {
        m_data.append(0);
        return;
    }

    // 1. 预处理数据：处理块交换 (Block Swap) 和小端序输入
    QByteArray processedData;
    if (!bigEndian && blockSize > 1) {
        // 如果是小端输入且需要块交换，手动调整字节顺序
        processedData.resize(input_size);
        for (int i = 0; i < input_size; i += blockSize) {
            int currentBlock = qMin(blockSize, input_size - i);
            for (int j = 0; j < currentBlock; j++) {
                processedData[i + j] = input[i + currentBlock - 1 - j];
            }
        }
    } else {
        // 大端序或无需块交换，直接拷贝
        processedData = QByteArray::fromRawData((const char*)input, input_size);
    }

    const quint8 *src = (const quint8*)processedData.constData();

    // 2. 计算需要的 segment 数量 (向上取整)
    int segCount = (input_size + 3) / 4;
    m_data.resize(segCount);
    m_data.fill(0);

    // 3. 将字节流打包进 32位整数数组
    if (bigEndian) {
        // 大端序输入：输入流的末尾是数值的低位
        for (int i = 0; i < input_size; i++) {
            int byteIndexFromEnd = input_size - 1 - i; // 倒序读取
            quint32 val = src[byteIndexFromEnd];

            int segIdx = i / 4;        // 对应的内部 segment 索引
            int shift = (i % 4) * 8;   // 对应的段内位移
            m_data[segIdx] |= (val << shift);
        }
    } else {
        // 小端序输入：输入流的开头是数值的低位
        for (int i = 0; i < input_size; i++) {
            quint32 val = src[i];
            int segIdx = i / 4;
            int shift = (i % 4) * 8;
            m_data[segIdx] |= (val << shift);
        }
    }
    trim();
}

// === 比较运算符实现 ===
bool BigInt::operator== (const BigInt &right) const { return m_data == right.m_data; }
bool BigInt::operator!= (const BigInt &right) const { return m_data != right.m_data; }

bool BigInt::operator< (const BigInt &right) const
{
    // 先比较数组长度
    if (m_data.size() != right.m_data.size())
        return m_data.size() < right.m_data.size();

    // 长度相同，从高位向低位逐个比较
    for (int i = m_data.size() - 1; i >= 0; i--) {
        if (m_data[i] != right.m_data[i])
            return m_data[i] < right.m_data[i];
    }
    return false;
}

bool BigInt::operator> (const BigInt &right) const { return right < *this; }
bool BigInt::operator<= (const BigInt &right) const { return !(*this > right); }
bool BigInt::operator>= (const BigInt &right) const { return !(*this < right); }

// === 加法运算 ===
BigInt BigInt::operator+ (const BigInt &right) const
{
    BigInt result;
    int maxLen = qMax(m_data.size(), right.m_data.size());
    result.m_data.resize(maxLen + 1); // 多留一位给进位

    quint64 carry = 0;
    for (int i = 0; i < maxLen; i++) {
        quint64 lVal = getVal(m_data, i);
        quint64 rVal = getVal(right.m_data, i);

        quint64 sum = lVal + rVal + carry;
        result.m_data[i] = (quint32)(sum & BASE_MASK);
        carry = sum >> 32; // 计算新的进位
    }
    result.m_data[maxLen] = (quint32)carry;
    result.trim();
    return result;
}

// === 减法运算 (仅支持结果非负) ===
BigInt BigInt::operator- (const BigInt &right) const
{
    if (*this < right) return BigInt((quint64)0); // 不支持负数，返回0

    BigInt result;
    result.m_data.resize(m_data.size());

    quint64 carry = 0; // 这里充当借位 (borrow)
    for (int i = 0; i < m_data.size(); i++) {
        quint64 lVal = m_data[i];
        quint64 rVal = getVal(right.m_data, i);

        quint64 diff;
        if (lVal < rVal + carry) {
            // 需要借位
            diff = (lVal + 0x100000000ULL) - (rVal + carry);
            carry = 1;
        } else {
            diff = lVal - (rVal + carry);
            carry = 0;
        }
        result.m_data[i] = (quint32)diff;
    }
    result.trim();
    return result;
}

// === 乘法运算 ===
BigInt BigInt::operator *(const BigInt &right) const
{
    if (*this == BigInt((quint64)0) || right == BigInt((quint64)0))
        return BigInt((quint64)0);

    BigInt result;
    // 结果的最大可能长度是两者长度之和
    result.m_data.resize(m_data.size() + right.m_data.size());
    result.m_data.fill(0);

    // 标准乘法算法 (Schoolbook Multiplication)
    for (int i = 0; i < m_data.size(); i++) {
        quint64 carry = 0;
        for (int j = 0; j < right.m_data.size(); j++) {
            // 当前位 = left[i] * right[j] + 原有值 + 进位
            quint64 prod = (quint64)m_data[i] * right.m_data[j] + result.m_data[i + j] + carry;
            result.m_data[i + j] = (quint32)(prod & BASE_MASK);
            carry = prod >> 32;
        }
        // 处理剩余进位
        if (carry) {
            int currIndex = i + right.m_data.size();
            while (carry > 0 && currIndex < result.m_data.size()) {
                quint64 sum = (quint64)result.m_data[currIndex] + carry;
                result.m_data[currIndex] = (quint32)(sum & BASE_MASK);
                carry = sum >> 32;
                currIndex++;
            }
            if (carry > 0) {
                result.m_data.append((quint32)carry);
            }
        }
    }
    result.trim();
    return result;
}

// === 除法运算 ===
BigInt BigInt::operator/ (const BigInt &right) const
{
    if (right == BigInt((quint64)0)) return BigInt((quint64)0); // 除零保护
    if (*this < right) return BigInt((quint64)0);

    BigInt quotient;
    quotient.m_data.resize(m_data.size() - right.m_data.size() + 1);
    quotient.m_data.fill(0);

    BigInt remainder;
    remainder.m_data.resize(right.m_data.size() + 1);
    remainder.m_data.fill(0);

    // 初始化余数的高位部分
    for (int j = 0; j < right.m_data.size(); j++) {
        remainder.m_data[j] = getVal(m_data, m_data.size() - right.m_data.size() + j);
    }

    // 从高位向低位遍历，逐位估算商 (Educated Guessing)
    for (int i = m_data.size(); i >= right.m_data.size(); i--) {
        // 提取余数的高 64 位用于估算
        quint64 n = (quint64)getVal(remainder.m_data, right.m_data.size()) << 32;
        n |= getVal(remainder.m_data, right.m_data.size() - 1);

        // 提取除数的高 32 位
        quint64 d = getVal(right.m_data, right.m_data.size() - 1);

        // 估算商 qest
        quint64 qest = (d == 0) ? 0 : (n / d);

        // 验证并调整 qest
        // 计算 p = qest * right，检查是否逼近 remainder
        BigInt p = BigInt(qest) * right;
        while (true) {
            if (p == remainder) {
                break; // 精确匹配
            }
            if (p < remainder) {
                if ((remainder - p) > right) {
                    // qest 估小了
                    qest++;
                    p = p + right;
                } else {
                    break; // 误差在合理范围内
                }
            } else {
                // qest 估大了
                qest--;
                p = p - right;
            }
        }

        // 保存当前位的商
        if (i - right.m_data.size() < quotient.m_data.size())
            quotient.m_data[i - right.m_data.size()] = (quint32)qest;

        // 更新余数
        remainder = remainder - p;

        // 如果还有下一位，将下一位数据移入余数低位
        if (i > right.m_data.size()) {
            remainder.m_data.prepend(0); // 腾出低位
            remainder.m_data[0] = getVal(m_data, i - right.m_data.size() - 1);
            remainder.trim();
        }
    }
    quotient.trim();
    return quotient;
}

// === 取模运算 ===
// 逻辑与除法基本一致，只是最后返回 remainder
BigInt BigInt::operator% (const BigInt &right) const
{
    if (right == BigInt((quint64)0)) return BigInt((quint64)0);
    if (*this < right) return *this;

    BigInt remainder;
    remainder.m_data.resize(right.m_data.size() + 1);
    remainder.m_data.fill(0);

    for (int j = 0; j < right.m_data.size(); j++) {
        remainder.m_data[j] = getVal(m_data, m_data.size() - right.m_data.size() + j);
    }

    for (int i = m_data.size(); i >= right.m_data.size(); i--) {
        quint64 n = (quint64)getVal(remainder.m_data, right.m_data.size()) << 32;
        n |= getVal(remainder.m_data, right.m_data.size() - 1);
        quint64 d = getVal(right.m_data, right.m_data.size() - 1);
        quint64 qest = (d == 0) ? 0 : (n / d);

        BigInt p = BigInt(qest) * right;
        while (true) {
            if (p == remainder) break;
            if (p < remainder) {
                if ((remainder - p) > right) {
                    qest++;
                    p = p + right;
                } else break;
            } else {
                qest--;
                p = p - right;
            }
        }
        remainder = remainder - p;

        if (i > right.m_data.size()) {
            remainder.m_data.prepend(0);
            remainder.m_data[0] = getVal(m_data, i - right.m_data.size() - 1);
            remainder.trim();
        }
    }
    remainder.trim();
    return remainder;
}

// === 按字节左移 ===
// 注意：参数是字节数 (bytes)，不是位数 (bits)
BigInt BigInt::operator<< (int bytesToShift) const
{
    if (bytesToShift <= 0) return *this;
    // 目前仅实现并需要按 segment (4字节) 对齐的移位
    if (bytesToShift % 4 != 0) return BigInt((quint64)0);

    int segShift = bytesToShift / 4;
    BigInt result;
    result.m_data.resize(m_data.size() + segShift);
    result.m_data.fill(0);

    // 整体移动数据段
    for (int i = 0; i < m_data.size(); i++) {
        result.m_data[i + segShift] = m_data[i];
    }
    return result;
}

// === 生成随机大数 ===
BigInt BigInt::random(int size)
{
    BigInt result;
    int segCount = (size + 3) / 4;
    result.m_data.resize(segCount);

    // 使用 Qt 的随机数生成器
    for (int i = 0; i < segCount; i++) {
        result.m_data[i] = QRandomGenerator::global()->generate();
    }
    return result;
}

// === 模幂运算 (this^exp % mod) ===
// 使用递归的“平方求幂”算法 (Exponentiation by Squaring)
BigInt BigInt::powm(const BigInt &exp, const BigInt &mod) const
{
    if (exp == BigInt((quint64)0)) return BigInt((quint64)1);
    if (exp == BigInt((quint64)1)) return (*this) % mod;
    if (exp == BigInt((quint64)2)) return (*this * *this) % mod;

    if (exp.m_data[0] % 2 == 0) {
        // 指数是偶数：a^2n = (a^n)^2
        BigInt half = exp / BigInt((quint64)2);
        BigInt halfPow = this->powm(half, mod);
        return (halfPow * halfPow) % mod;
    } else {
        // 指数是奇数：a^(2n+1) = a * (a^n)^2
        BigInt half = exp / BigInt((quint64)2);
        BigInt halfPow = this->powm(half, mod);
        return ((halfPow * halfPow) % mod * (*this)) % mod;
    }
}

// === 导出数据到缓冲区 ===
void BigInt::getData(unsigned char *out, int byteCount, int blockSize, bool bigEndian) const
{
    QByteArray bytes = toByteArray(byteCount, blockSize, bigEndian);
    memcpy(out, bytes.constData(), qMin(byteCount, bytes.size()));
}

// === 序列化为字节数组 ===
QByteArray BigInt::toByteArray(int minByteCount, int blockSize, bool bigEndian) const
{
    QByteArray result;
    // 将内部的 segment 拆解为字节流 (Little Endian)
    for (int i = 0; i < m_data.size(); i++) {
        quint32 val = m_data[i];
        result.append((char)(val & 0xFF));
        result.append((char)((val >> 8) & 0xFF));
        result.append((char)((val >> 16) & 0xFF));
        result.append((char)((val >> 24) & 0xFF));
    }

    // 处理末尾多余的 0
    while (result.size() > minByteCount && result.size() > 0 && result.at(result.size()-1) == 0) {
        result.chop(1);
    }
    // 补齐到最小请求长度
    while (result.size() < minByteCount) {
        result.append((char)0);
    }

    if (bigEndian) {
        // 大端序：翻转整个字节数组
        std::reverse(result.begin(), result.end());
    } else {
        // 小端序：如果需要块内交换
        if (blockSize > 1) {
            for (int i = 0; i < result.size(); i += blockSize) {
                int currentBlock = qMin(blockSize, result.size() - i);
                int left = i;
                int right = i + currentBlock - 1;
                while (left < right) {
                    char temp = result[left];
                    result[left] = result[right];
                    result[right] = temp;
                    left++;
                    right--;
                }
            }
        }
    }
    return result;
}

// === 转为十六进制字符串 ===
QString BigInt::toHexString() const
{
    // 强制转为大端序字节流，然后转 Hex
    QByteArray bytes = toByteArray(0, 1, true);
    return bytes.toHex();
}

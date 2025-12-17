/*
 * -------------------------------------------------------------------------
 * 文件名: bigint.cpp
 * 说明:
 *      BigInt 类的具体实现。
 *      修复了除法逻辑缺陷，实现了基于位运算的模幂优化。
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

// 判断是否为奇数
bool BigInt::isOdd() const
{
    if (m_data.isEmpty()) return false;
    return (m_data[0] & 1);
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
 */
BigInt::BigInt(const unsigned char *input, int input_size, int blockSize, bool bigEndian)
{
    if (input_size <= 0) {
        m_data.append(0);
        return;
    }

    QByteArray processedData;
    if (!bigEndian && blockSize > 1) {
        processedData.resize(input_size);
        for (int i = 0; i < input_size; i += blockSize) {
            int currentBlock = qMin(blockSize, input_size - i);
            for (int j = 0; j < currentBlock; j++) {
                processedData[i + j] = input[i + currentBlock - 1 - j];
            }
        }
    } else {
        processedData = QByteArray::fromRawData((const char*)input, input_size);
    }

    const quint8 *src = (const quint8*)processedData.constData();

    int segCount = (input_size + 3) / 4;
    m_data.resize(segCount);
    m_data.fill(0);

    if (bigEndian) {
        for (int i = 0; i < input_size; i++) {
            int byteIndexFromEnd = input_size - 1 - i;
            quint32 val = src[byteIndexFromEnd];
            int segIdx = i / 4;
            int shift = (i % 4) * 8;
            m_data[segIdx] |= (val << shift);
        }
    } else {
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
    if (m_data.size() != right.m_data.size())
        return m_data.size() < right.m_data.size();
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
    result.m_data.resize(maxLen + 1);

    quint64 carry = 0;
    for (int i = 0; i < maxLen; i++) {
        quint64 lVal = getVal(m_data, i);
        quint64 rVal = getVal(right.m_data, i);
        quint64 sum = lVal + rVal + carry;
        result.m_data[i] = (quint32)(sum & BASE_MASK);
        carry = sum >> 32;
    }
    result.m_data[maxLen] = (quint32)carry;
    result.trim();
    return result;
}

// === 减法运算 (仅支持结果非负) ===
BigInt BigInt::operator- (const BigInt &right) const
{
    if (*this < right) return BigInt((quint64)0);

    BigInt result;
    result.m_data.resize(m_data.size());

    quint64 carry = 0;
    for (int i = 0; i < m_data.size(); i++) {
        quint64 lVal = m_data[i];
        quint64 rVal = getVal(right.m_data, i);
        quint64 diff;
        if (lVal < rVal + carry) {
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
    result.m_data.resize(m_data.size() + right.m_data.size());
    result.m_data.fill(0);

    for (int i = 0; i < m_data.size(); i++) {
        quint64 carry = 0;
        for (int j = 0; j < right.m_data.size(); j++) {
            quint64 prod = (quint64)m_data[i] * right.m_data[j] + result.m_data[i + j] + carry;
            result.m_data[i + j] = (quint32)(prod & BASE_MASK);
            carry = prod >> 32;
        }
        if (carry) {
            int currIndex = i + right.m_data.size();
            while (carry > 0 && currIndex < result.m_data.size()) {
                quint64 sum = (quint64)result.m_data[currIndex] + carry;
                result.m_data[currIndex] = (quint32)(sum & BASE_MASK);
                carry = sum >> 32;
                currIndex++;
            }
            if (carry > 0) result.m_data.append((quint32)carry);
        }
    }
    result.trim();
    return result;
}

// === 除法运算 [FIXED] ===
// 修复了旧版逻辑中 prepend 导致的截断问题，使用完整余数减法
BigInt BigInt::operator/ (const BigInt &right) const
{
    if (right == BigInt((quint64)0)) return BigInt((quint64)0);
    if (*this < right) return BigInt((quint64)0);

    BigInt quotient;
    quotient.m_data.resize(m_data.size() - right.m_data.size() + 1);
    quotient.m_data.fill(0);

    BigInt remainder = *this;

    // 从高位向低位遍历，逐位估算商
    for (int i = m_data.size() - right.m_data.size(); i >= 0; i--) {
        // 1. 估算商 qest
        // 取余数的高 64 位 (相对于当前对齐位置 i)
        quint64 n = (quint64)getVal(remainder.m_data, i + right.m_data.size()) << 32;
        n |= getVal(remainder.m_data, i + right.m_data.size() - 1);

        // 取除数的高 32 位
        quint64 d = getVal(right.m_data, right.m_data.size() - 1);

        quint64 qest = (d == 0) ? 0 : (n / d);
        if (qest > 0xFFFFFFFF) qest = 0xFFFFFFFF; // 溢出保护

        // 2. 验证并调整
        // 计算 p = qest * right * (base ^ i)
        // 使用 operator<< (按字节移位) 来处理 segment 对齐
        BigInt p_raw = BigInt(qest) * right;
        BigInt p = p_raw << (i * 4); // i 个 segment = i * 4 字节

        // 检查 p 是否过大 (估算偏大)
        while (p > remainder) {
            qest--;
            p = p - (right << (i * 4));
        }

        // 3. 执行减法与存储商
        quotient.m_data[i] = (quint32)qest;
        remainder = remainder - p;
    }

    quotient.trim();
    return quotient;
}

// === 取模运算 [FIXED] ===
// 逻辑与修复后的除法一致
BigInt BigInt::operator% (const BigInt &right) const
{
    if (right == BigInt((quint64)0)) return BigInt((quint64)0);
    if (*this < right) return *this;

    BigInt remainder = *this;

    for (int i = m_data.size() - right.m_data.size(); i >= 0; i--) {
        quint64 n = (quint64)getVal(remainder.m_data, i + right.m_data.size()) << 32;
        n |= getVal(remainder.m_data, i + right.m_data.size() - 1);
        quint64 d = getVal(right.m_data, right.m_data.size() - 1);

        quint64 qest = (d == 0) ? 0 : (n / d);
        if (qest > 0xFFFFFFFF) qest = 0xFFFFFFFF;

        BigInt p_raw = BigInt(qest) * right;
        BigInt p = p_raw << (i * 4);

        while (p > remainder) {
            qest--;
            p = p - (right << (i * 4));
        }
        remainder = remainder - p;
    }
    remainder.trim();
    return remainder;
}

// === 按字节左移 (对齐 segment) ===
BigInt BigInt::operator<< (int bytesToShift) const
{
    if (bytesToShift <= 0) return *this;
    // 强制按 4 字节对齐，因为 operator/ 依赖此逻辑
    if (bytesToShift % 4 != 0) return BigInt((quint64)0);

    int segShift = bytesToShift / 4;
    BigInt result;
    result.m_data.resize(m_data.size() + segShift);
    result.m_data.fill(0);

    for (int i = 0; i < m_data.size(); i++) {
        result.m_data[i + segShift] = m_data[i];
    }
    return result;
}

// === [新增] 按位右移 (优化 powm) ===
BigInt BigInt::operator>> (int bitsToShift) const
{
    if (bitsToShift <= 0) return *this;
    if (m_data.isEmpty()) return *this;

    BigInt result;
    result.m_data.resize(m_data.size());

    int wordShift = bitsToShift / 32;
    int bitShift = bitsToShift % 32;
    quint32 carryMask = (1 << bitShift) - 1;

    // 1. 先处理整段移动
    if (wordShift > 0) {
        if (wordShift >= m_data.size()) return BigInt((quint64)0);
        for (int i = 0; i < m_data.size() - wordShift; i++) {
            result.m_data[i] = m_data[i + wordShift];
        }
        result.m_data.resize(m_data.size() - wordShift);
    } else {
        result.m_data = m_data;
    }

    // 2. 再处理段内位移
    if (bitShift > 0) {
        quint32 carry = 0;
        for (int i = result.m_data.size() - 1; i >= 0; i--) {
            quint32 current = result.m_data[i];
            // 下一个高位段移下来的部分
            quint32 nextCarry = current & carryMask;

            result.m_data[i] = (current >> bitShift) | (carry << (32 - bitShift));
            carry = nextCarry;
        }
    }

    result.trim();
    return result;
}

// === 生成随机大数 ===
BigInt BigInt::random(int size)
{
    BigInt result;
    int segCount = (size + 3) / 4;
    result.m_data.resize(segCount);
    for (int i = 0; i < segCount; i++) {
        result.m_data[i] = QRandomGenerator::global()->generate();
    }
    return result;
}

// === 模幂运算 [FIXED] ===
// 优化：使用位移和奇偶判断代替除法，避免调用 operator/
BigInt BigInt::powm(const BigInt &exp, const BigInt &mod) const
{
    if (exp == BigInt((quint64)0)) return BigInt((quint64)1);
    if (exp == BigInt((quint64)1)) return (*this) % mod;

    if (!exp.isOdd()) {
        // 偶数: exp >> 1
        BigInt half = exp >> 1;
        BigInt halfPow = this->powm(half, mod);
        return (halfPow * halfPow) % mod;
    } else {
        // 奇数: exp >> 1
        BigInt half = exp >> 1;
        BigInt halfPow = this->powm(half, mod);
        // (halfPow^2 % mod) * this % mod
        BigInt tmp = (halfPow * halfPow) % mod;
        return (tmp * (*this)) % mod;
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
    for (int i = 0; i < m_data.size(); i++) {
        quint32 val = m_data[i];
        result.append((char)(val & 0xFF));
        result.append((char)((val >> 8) & 0xFF));
        result.append((char)((val >> 16) & 0xFF));
        result.append((char)((val >> 24) & 0xFF));
    }

    while (result.size() > minByteCount && result.size() > 0 && result.at(result.size()-1) == 0) {
        result.chop(1);
    }
    while (result.size() < minByteCount) {
        result.append((char)0);
    }

    if (bigEndian) {
        std::reverse(result.begin(), result.end());
    } else {
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
    QByteArray bytes = toByteArray(0, 1, true);
    return bytes.toHex();
}

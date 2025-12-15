/*
 * -------------------------------------------------------------------------
 * 文件名: bnethash.cpp
 * 说明:
 *      战网哈希算法的具体实现。
 *      核心逻辑包括 hash_init, do_hash (混淆), hash_set_16 (块填充)。
 * -------------------------------------------------------------------------
 */

#include <cstring>
#include <cstdio>
#include <QtEndian>
#include "bnethash.h"
#include "bitrotate.h"

// 哈希变体枚举
typedef enum {
    do_blizzard_hash,   // 暴雪修改版 (Broken SHA1)
    do_sha1_hash        // 标准 SHA1
} t_hash_variant;

/**
 * @brief 初始化哈希常量 (魔数)
 * 这些是 SHA1 算法定义的标准初始值。
 */
static void hash_init(t_hash *hash)
{
    (*hash)[0] = 0x67452301;
    (*hash)[1] = 0xefcdab89;
    (*hash)[2] = 0x98badcfe;
    (*hash)[3] = 0x10325476;
    (*hash)[4] = 0xc3d2e1f0;
}

/**
 * @brief 执行核心哈希混淆运算 (80轮)
 * @param hash 当前的哈希状态 (输入/输出)
 * @param tmp  输入的消息块 (64字节扩展为80个32位字)
 * @param hash_variant 算法变体选择
 */
static void do_hash(t_hash *hash, quint32 *tmp, t_hash_variant hash_variant)
{
    uint i;
    quint32 a, b, c, d, e, g;

    // 1. 消息扩展 (Message Schedule)
    // 将 16 个 32位字扩展为 80 个
    for (i = 0; i < 64; i++) {
        quint32 xor_val = tmp[i] ^ tmp[i + 8] ^ tmp[i + 2] ^ tmp[i + 13];

        // 标准 SHA1: ROTL32(val, 1)
        // 暴雪 Hash: ROTL32(1, val)
        // PvPGN 的实现利用宏定义兼容了这种差异
        // 暴雪算法这里的行为是仅取 1 bit 参与运算或是移位逻辑不同
        if (hash_variant == do_blizzard_hash)
            tmp[i + 16] = ROTL32(1, xor_val);
        else
            tmp[i + 16] = ROTL32(xor_val, 1);
    }

    // 初始化工作变量
    a = (*hash)[0];
    b = (*hash)[1];
    c = (*hash)[2];
    d = (*hash)[3];
    e = (*hash)[4];

    // 2. 主循环 (80 轮运算)

    // Round 1 (0-19)
    for (i = 0; i < 20 * 1; i++) {
        g = tmp[i] + ROTL32(a, 5) + e + ((b & c) | (~b & d)) + 0x5a827999;
        e = d; d = c; c = ROTL32(b, 30); b = a; a = g;
    }
    // Round 2 (20-39)
    for (; i < 20 * 2; i++) {
        g = (d ^ c ^ b) + e + ROTL32(g, 5) + tmp[i] + 0x6ed9eba1;
        e = d; d = c; c = ROTL32(b, 30); b = a; a = g;
    }
    // Round 3 (40-59)
    for (; i < 20 * 3; i++) {
        g = tmp[i] + ROTL32(g, 5) + e + ((c & b) | (d & c) | (d & b)) - 0x70e44324;
        e = d; d = c; c = ROTL32(b, 30); b = a; a = g;
    }
    // Round 4 (60-79)
    for (; i < 20 * 4; i++) {
        g = (d ^ c ^ b) + e + ROTL32(g, 5) + tmp[i] - 0x359d3e2a;
        e = d; d = c; c = ROTL32(b, 30); b = a; a = g;
    }

    // 3. 累加结果
    (*hash)[0] += g;
    (*hash)[1] += b;
    (*hash)[2] += c;
    (*hash)[3] += d;
    (*hash)[4] += e;
}

/**
 * @brief 将输入字节流填充到 16 个 32位整数块中
 * 处理字节序转换 (Little Endian -> Big Endian for SHA1) 和填充位。
 */
static void hash_set_16(quint32 *dst, const unsigned char *src, uint count, t_hash_variant hash_variant)
{
    uint i;
    uint pos;

    for (pos = 0, i = 0; i < 16; i++) {
        dst[i] = 0;

        // 暴雪 Hash 直接按字节填充 (Little Endian)
        // SHA1 需要转换为 Big Endian，并在末尾添加 0x80

        // Byte 0
        if (hash_variant == do_blizzard_hash) {
            if (pos < count) dst[i] |= ((quint32)src[pos]);
        } else {
            if (pos < count) dst[i] |= ((quint32)src[pos]) << 24;
            else if (pos == count) dst[i] |= 0x80000000;
        }
        pos++;

        // Byte 1
        if (hash_variant == do_blizzard_hash) {
            if (pos < count) dst[i] |= ((quint32)src[pos]) << 8;
        } else {
            if (pos < count) dst[i] |= ((quint32)src[pos]) << 16;
            else if (pos == count) dst[i] |= 0x800000;
        }
        pos++;

        // Byte 2
        if (hash_variant == do_blizzard_hash) {
            if (pos < count) dst[i] |= ((quint32)src[pos]) << 16;
        } else {
            if (pos < count) dst[i] |= ((quint32)src[pos]) << 8;
            else if (pos == count) dst[i] |= 0x8000;
        }
        pos++;

        // Byte 3
        if (hash_variant == do_blizzard_hash) {
            if (pos < count) dst[i] |= ((quint32)src[pos]) << 24;
        } else {
            if (pos < count) dst[i] |= ((quint32)src[pos]);
            else if (pos == count) dst[i] |= 0x80;
        }
        pos++;
    }
}

/**
 * @brief 设置 SHA1 的长度填充块
 * SHA1 算法要求在最后 64 位由原始数据的比特长度填充 (Big Endian)。
 */
static void hash_set_length(quint32 *dst, uint size)
{
    quint32 size_high = 0;
    quint32 size_low = 0;

    // 计算比特长度 (size * 8)
    for (uint counter = 0; counter < size; counter++) {
        size_low += 8;
        if (size_low == 0) size_high++;
    }

    // 填充到最后的两个 32位 整数 (大端序)
    dst[14] |= ((size_high >> 24) & 0xff) << 24;
    dst[14] |= ((size_high >> 16) & 0xff) << 16;
    dst[14] |= ((size_high >> 8) & 0xff) << 8;
    dst[14] |= ((size_high) & 0xff);

    dst[15] |= ((size_low >> 24) & 0xff) << 24;
    dst[15] |= ((size_low >> 16) & 0xff) << 16;
    dst[15] |= ((size_low >> 8) & 0xff) << 8;
    dst[15] |= ((size_low) & 0xff);
}

/**
 * @brief 计算暴雪自定义哈希 (Broken SHA1)
 * @param hashout 输出缓冲区
 * @param size 输入数据大小
 * @param datain 输入数据指针
 */
void bnet_hash(t_hash *hashout, uint size, const void *datain)
{
    quint32 tmp[64 + 16];
    const unsigned char* data = (const unsigned char*)datain;
    uint inc;

    if (!hashout) return;

    hash_init(hashout);

    while (size > 0) {
        if (size > 64) inc = 64;
        else inc = size;

        hash_set_16(tmp, data, inc, do_blizzard_hash);
        do_hash(hashout, tmp, do_blizzard_hash);

        data += inc;
        size -= inc;
    }
}

/**
 * @brief 计算标准 SHA1 哈希
 * @param hashout 输出缓冲区
 * @param size 输入数据大小
 * @param datain 输入数据指针
 */
void sha1_hash(t_hash *hashout, uint size, const void *datain)
{
    quint32 tmp[64 + 16];
    const unsigned char* data = (const unsigned char*)datain;
    uint inc;
    uint orgSize = size;

    if (!hashout) return;

    hash_init(hashout);

    while (size > 0) {
        if (size >= 64) inc = 64;
        else inc = size;

        if (size >= 64) {
            // 完整块
            hash_set_16(tmp, data, inc, do_sha1_hash);
            do_hash(hashout, tmp, do_sha1_hash);
        } else if (size > 55) {
            // 最后一块大于 55 字节，无法放下长度填充，需要两个块
            hash_set_16(tmp, data, inc, do_sha1_hash);
            do_hash(hashout, tmp, do_sha1_hash);
            // 填充块 (全0 + 长度)
            hash_set_16(tmp, data, 0, do_blizzard_hash); // 利用 blizz 模式填充 0
            hash_set_length(tmp, orgSize);
            do_hash(hashout, tmp, do_sha1_hash);
        } else {
            // 最后一块足以放下长度填充
            hash_set_16(tmp, data, inc, do_sha1_hash);
            hash_set_length(tmp, orgSize);
            do_hash(hashout, tmp, do_sha1_hash);
        }

        data += inc;
        size -= inc;
    }
    // 处理空数据的情况
    if (orgSize == 0) {
        hash_set_16(tmp, data, 0, do_sha1_hash);
        hash_set_length(tmp, 0);
        do_hash(hashout, tmp, do_sha1_hash);
    }
}

/**
 * @brief 计算小端序修正的 SHA1 (PvPGN 特有)
 * 先计算标准 SHA1，然后将结果的 5 个整数转换为大端序存储。
 *
 * 原理解释:
 * PvPGN 在某些内部计算(如私钥生成)时，会将 SHA1 结果(内存中的字节)视为 32位整数，
 * 并强制进行 ntohl (Network to Host) 转换。此函数模拟了该行为。
 */
void little_endian_sha1_hash(t_hash *hashout, uint size, const void *datain)
{
    sha1_hash(hashout, size, datain);

    // 将结果从 Host Order 转换为 Big Endian (模拟 PvPGN 的 bn_int_nset)
    for (int i = 0; i < 5; i++) {
        (*hashout)[i] = qToBigEndian((*hashout)[i]);
    }
}

// === 辅助函数实现 ===

bool hash_eq(const t_hash h1, const t_hash h2)
{
    for (int i = 0; i < 5; i++)
        if (h1[i] != h2[i]) return false;
    return true;
}

QString hash_get_str(const t_hash hash)
{
    if (!hash) return QString();
    QString res;
    for (int i = 0; i < 5; i++) {
        char buf[9];
        // 将每个 32位整数格式化为 8位 十六进制
        std::sprintf(buf, "%08x", hash[i]);
        res.append(buf);
    }
    return res;
}

QString little_endian_hash_get_str(const t_hash hash)
{
    t_hash be_hash;
    // 先转换为大端序，再转字符串
    for (int i = 0; i < 5; i++) {
        be_hash[i] = qToBigEndian(hash[i]);
    }
    return hash_get_str(be_hash);
}

bool hash_set_str(t_hash *hash, const QString &str)
{
    if (!hash || str.length() != 40) return false;

    QByteArray ba = str.toLatin1();
    const char* s = ba.constData();

    // 每 8 个字符解析为一个 32位 整数
    for (int i = 0; i < 5; i++) {
        if (std::sscanf(&s[i * 8], "%8x", &(*hash)[i]) != 1)
            return false;
    }
    return true;
}

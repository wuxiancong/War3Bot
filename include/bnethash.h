/*
 * -------------------------------------------------------------------------
 * 文件名: bnethash.h
 * 说明:
 *      实现了战网 (Battle.net) 专用的哈希算法。
 *      包含标准的 SHA1 实现、暴雪修改版的 SHA1 (用于旧版登录和游戏创建)，
 *      以及 PvPGN 特有的字节序处理逻辑。
 *
 * 原作者: Descolada, Ross Combs
 * -------------------------------------------------------------------------
 */

#ifndef BNETHASH_H
#define BNETHASH_H

#include <QtGlobal>
#include <QByteArray>
#include <QString>

// 定义哈希状态类型 (5个32位无符号整数，共160位)
typedef quint32 t_hash[5];

/**
 * @brief 暴雪自定义哈希 (也称为 Broken SHA1)
 * 用于旧版登录协议 (0x29) 的密码哈希、游戏创建时的 Checksum 计算等。
 * 它与标准 SHA1 的区别在于数据扩充时的循环左移逻辑不同。
 */
void bnet_hash(t_hash *hashout, uint size, const void *datain);

/**
 * @brief 标准 SHA1 哈希算法
 * 用于 SRP 协议中的大部分计算。
 */
void sha1_hash(t_hash *hashout, uint size, const void *datain);

/**
 * @brief 特殊的小端序 SHA1
 * 先计算标准 SHA1，然后将结果的 5 个整数强制转换为大端序存储 (模拟网络字节序)。
 * 这是 PvPGN 服务端 SRP 实现中的一个特殊处理，必须匹配才能登录成功。
 */
void little_endian_sha1_hash(t_hash *hashout, uint size, const void *datain);

// === 辅助工具函数 ===

// 比较两个哈希值是否相等
bool hash_eq(const t_hash h1, const t_hash h2);

// 从十六进制字符串设置哈希值
bool hash_set_str(t_hash *hash, const QString &str);

// 将哈希值转换为十六进制字符串 (小写)
QString hash_get_str(const t_hash hash);

// 获取经过字节翻转的十六进制字符串
QString little_endian_hash_get_str(const t_hash hash);

#endif // BNETHASH_H

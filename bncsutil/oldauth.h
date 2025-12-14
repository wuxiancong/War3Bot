#ifndef BNCSUTIL_OLDAUTH_H
#define BNCSUTIL_OLDAUTH_H
#include <bncsutil/mutil.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * 使用给定的服务器令牌和客户端令牌对密码进行双重哈希处理
 *
 * @param password 原始密码字符串
 * @param clientToken 客户端令牌（32位无符号整数）
 * @param serverToken 服务器令牌（32位无符号整数）
 * @param outBuffer 输出缓冲区（至少20字节，用于存放哈希结果）
 *
 * 注意：此函数用于战网旧版身份验证协议中的密码验证
 */
MEXP(void) doubleHashPassword(const char* password, uint32_t clientToken,
                   uint32_t serverToken, char* outBuffer);

/**
 * 对密码进行单次哈希处理（用于账户创建和密码更改）
 *
 * @param password 原始密码字符串
 * @param outBuffer 输出缓冲区（至少20字节，用于存放哈希结果）
 *
 * 注意：此函数用于账户创建和密码修改操作，生成存储在数据库中的密码哈希
 */
MEXP(void) hashPassword(const char* password, char* outBuffer);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* BNCSUTIL_OLDAUTH_H */

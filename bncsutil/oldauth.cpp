/**
 * @file oldauth.cpp
 * @brief 旧版战网认证哈希实现 (Old Battle.net Authentication)
 *
 * 作用 (Purpose):
 * 此文件实现了 Battle.net 早期使用的“旧式”哈希算法 (Old Auth)。
 * 在 NLS (SRP) 普及之前，或者在某些特定的操作（如创建账号、修改密码）中，
 * 战网使用这种基于 SHA-1 的哈希方式。
 *
 * 主要功能:
 * 1. doubleHashPassword: 结合客户端 Token 和服务器 Token 对密码进行双重哈希。
 *    用于旧版本客户端登录验证。
 * 2. hashPassword: 对密码进行单次哈希。
 *    用于账号创建 (Account Create) 和修改密码 (Password Change) 请求。
 */

#include <bncsutil/mutil.h> // for MEXP()
#include <bncsutil/bsha1.h>
#include <bncsutil/oldauth.h>
#include <cstring> // for std::strlen
#include <cstdint> // for uint32_t

/**
 * 使用给定的服务器令牌 (Server Token) 和客户端令牌 (Client Token)
 * 对给定的密码进行双重哈希。
 *
 * outBuffer 必须至少 20 字节长。
 *
 * 逻辑:
 * 1. 计算密码的哈希 -> 放入缓冲区的偏移量 8 处
 * 2. 在头部填充 ClientToken (4字节) 和 ServerToken (4字节)
 * 3. 对整个 28 字节的块进行哈希
 */
MEXP(void) doubleHashPassword(const char *password, uint32_t clientToken,
                   uint32_t serverToken, char *outBuffer) {
    char intermediate[28];
    uint32_t *lp;

    // 计算密码哈希，写入 intermediate + 8 的位置
    calcHashBuf(password, std::strlen(password), intermediate + 8);

    // 强制转换为 uint32 指针以写入 Token
    lp = (uint32_t*) intermediate; // 注意：这里利用了 stack 内存地址，&intermediate 实际上就是 intermediate 数组首地址
    lp[0] = clientToken;
    lp[1] = serverToken;

    // 对整体进行哈希
    calcHashBuf(intermediate, 28, outBuffer);

#if DEBUG
    bncsutil_debug_message_a("doubleHashPassword(\"%s\", 0x%08X, 0x%08X) =",
                             password, clientToken, serverToken);
    bncsutil_debug_dump(outBuffer, 20);
#endif
}

/**
 * 对密码进行单次哈希，用于账号创建和修改密码。
 *
 * outBuffer 必须至少 20 字节长。
 */
MEXP(void) hashPassword(const char *password, char *outBuffer) {
    calcHashBuf(password, std::strlen(password), outBuffer);

#if DEBUG
    bncsutil_debug_message_a("hashPassword(\"%s\") =", password);
    bncsutil_debug_dump(outBuffer, 20);
#endif
}

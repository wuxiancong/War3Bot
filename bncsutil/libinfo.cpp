/**
 * @file libinfo.cpp
 * @brief BNCSUtil 库信息与版本管理接口
 *
 * 作用:
 * 此文件提供了获取 bncsutil 库当前版本信息的函数。
 * 它允许外部程序在运行时查询链接的 bncsutil 库的版本号，
 * 既可以获取原始的数字版本号，也可以获取格式化后的字符串版本号 (Major.Minor.Revision)。
 * 这对于调试 DLL 版本冲突或确保依赖库兼容性非常有用。
 */

#include <bncsutil/mutil.h>
#include <bncsutil/libinfo.h>
#include <cstdio> // for std::sprintf

/**
 * 获取库的原始版本号数值。
 *
 * @return 版本号整数 (例如 10203 代表 1.2.3)
 */
MEXP(unsigned long) bncsutil_getVersion() {
    return BNCSUTIL_VERSION;
}

/**
 * 获取格式化的版本字符串 "Major.Minor.Revision"。
 *
 * @param outBuf 输出缓冲区 (建议至少 16 字节)
 * @return 写入的字符数，如果失败返回 0
 */
MEXP(int) bncsutil_getVersionString(char *outBuf) {
    unsigned long major, minor, rev, ver;
    int printed;

    ver = BNCSUTIL_VERSION;

    // 提取主版本号 (Major) - 万位
    major = (unsigned long) (BNCSUTIL_VERSION / 10000);
    if (major > 99) return 0; // 这里的 < 0 检查对于 unsigned 来说是多余的，但为了保持原逻辑保留

    // 提取次版本号 (Minor) - 百位
    ver -= (major * 10000);
    minor = (unsigned long) (ver / 100);
    if (minor > 99) return 0;

    // 提取修订号 (Revision) - 个位
    ver -= (minor * 100);
    rev = ver;
    if (rev > 99) return 0;

    // 格式化字符串
    printed = std::sprintf(outBuf, "%lu.%lu.%lu", major, minor, rev);

    if (printed < 0) return 0;

    // 确保截断/安全 (虽然 sprintf 已经加了 \0，但这行是原逻辑)
    // 注意：这里硬编码索引 8 假设版本号不超过 x.x.x (最短 5 字符)
    // 但如果版本是 99.99.99 (8 字符)，这里 outBuf[8] 正好是结尾
    outBuf[8] = '\0';

    return printed;
}

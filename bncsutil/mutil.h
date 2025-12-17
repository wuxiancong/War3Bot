/**
 * @file mutil.h
 * @brief BNCSUtil 的基础工具定义
 * 包含跨平台类型定义、字节序转换、位操作宏以及 DLL 导出宏。
 */

#ifndef MUTIL_H
#define MUTIL_H

// === 1. 标准库引入 ===
#include <cstdint> // 提供 uint32_t, int64_t 等固定宽度整数
#include <cstdlib> // 提供 size_t, malloc, atoll 等
#include <cstring> // 提供 memcpy 等内存操作
#include <cstdio>  // 提供 FILE 等

// === 2. 类型引入 ===
// 将 std 命名空间下的定长类型引入全局，兼容旧代码习惯
using std::int8_t;
using std::uint8_t;
using std::int16_t;
using std::uint16_t;
using std::int32_t;
using std::uint32_t;
using std::int64_t;
using std::uint64_t;

// === 3. register_t 定义 ===
// Linux/Unix 系统在 <sys/types.h> 中已经定义了 register_t (通常为 long int)。
// 此处 typedef 仅在 Windows 环境下需要，否则会导致 Linux 编译报错 "conflicting declaration"。
#ifdef _WIN32
    typedef int32_t register_t;
#endif

// === 4. 字符串转 64 位整数宏 ===
#if defined(_MSC_VER)
    // MSVC (Windows) 使用 _atoi64
    #define ATOL64(x) _atoi64(x)
#else
    // GCC/Clang (Linux/Unix) 使用标准库 atoll
    #define ATOL64(x) atoll(x)
#endif

// === 5. 循环位移 (Bitwise Rotation) 宏 ===
// 哈希算法 (如 SHA1, XORO) 常用操作
#ifdef _MSC_VER
    // MSVC 使用内部指令优化
    #pragma intrinsic(_lrotl,_lrotr)
    #define ROL(x,n) _lrotl((x),(n)) // 循环左移
    #define ROR(x,n) _lrotr((x),(n)) // 循环右移
#else
    // 标准 C 实现循环左移
    #ifndef ROL
    #define ROL(a,b) (((a) << (b)) | ((a) >> (32 - (b))))
    #endif
    // 标准 C 实现循环右移
    #ifndef ROR
    #define ROR(a,b) (((a) >> (b)) | ((a) << (32 - (b))))
    #endif
#endif

// === 6. 大小端 (Endianness) 设置 ===
// 默认设为小端序 (Little Endian)，适用于 x86/x64 架构
#if !defined(BIGENDIAN) && !defined(LITTLEENDIAN)
    #if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        #define BIGENDIAN 1
        #define LITTLEENDIAN 0
    #else
        #define LITTLEENDIAN 1
        #define BIGENDIAN 0
    #endif
#endif

// === 7. 字节交换 (Byte Swapping) 宏 ===
// 用于在网络字节序 (Big Endian) 和主机字节序之间转换
#define SWAP2(num) ((((num) >> 8) & 0x00FF) | (((num) << 8) & 0xFF00))

#define SWAP4(num) ((((num) >> 24) & 0x000000FF) | \
                    (((num) >> 8)  & 0x0000FF00) | \
                    (((num) << 8)  & 0x00FF0000) | \
                    (((num) << 24) & 0xFF000000))

#define SWAP8(x) \
    (uint64_t)((((uint64_t)(x) & 0xff) << 56) | \
              (((uint64_t)(x) & 0xff00ULL) << 40) | \
              (((uint64_t)(x) & 0xff0000ULL) << 24) | \
              (((uint64_t)(x) & 0xff000000ULL) << 8) | \
              (((uint64_t)(x) & 0xff00000000ULL) >> 8) | \
              (((uint64_t)(x) & 0xff0000000000ULL) >> 24) | \
              (((uint64_t)(x) & 0xff000000000000ULL) >> 40) | \
              (((uint64_t)(x) & 0xff00000000000000ULL) >> 56))

// 别名定义
#define SWAP16 SWAP2
#define SWAP32 SWAP4
#define SWAP64 SWAP8

// === 8. 字节序转换宏 (LSB/MSB) ===
#if BIGENDIAN
    // 如果是很多的大端机 (如 PowerPC)，LSB 需要交换
    #define LSB2(num) SWAP2(num)
    #define LSB4(num) SWAP4(num)
    #define MSB2(num) (num)
    #define MSB4(num) (num)
#else
    // 如果是小端机 (x86/Intel/AMD)，LSB 不需要操作，MSB 需要交换
    #define LSB2(num) (num)
    #define LSB4(num) (num)
    #define MSB2(num) SWAP2(num)
    #define MSB4(num) SWAP4(num)
#endif

// === 9. DLL 导出/导入宏 ===
// Windows 下需要显式声明 dllexport/dllimport，Linux 下通常不需要
#ifdef MOS_WINDOWS
    #ifdef MUTIL_LIB_BUILD
        #define MEXP(type) __declspec(dllexport) type __stdcall
        #define MCEXP(name) class __declspec(dllexport) name
    #else
        #define MEXP(type) __declspec(dllimport) type __stdcall
        #define MCEXP(name) class __declspec(dllimport) name
    #endif
#else
    // Linux/Unix 定义为空即可
    #define MEXP(type) type
    #define MCEXP(name) class name
#endif

// === 10. 其他定义 ===
#define MYRIAD_UTIL

#ifndef NULL
#define NULL 0
#endif

#endif /* MUTIL_H */

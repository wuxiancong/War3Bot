#ifndef MUTIL_H
#define MUTIL_H

// === 核心修改：直接引入标准库 ===
#include <cstdint> // 提供 uint32_t, int64_t 等
#include <cstdlib> // 提供 size_t, malloc 等
#include <cstring> // 提供 memcpy 等
#include <cstdio>  // 提供 FILE

// 确保在 64 位 Linux 上也能正确使用
using std::int8_t;
using std::uint8_t;
using std::int16_t;
using std::uint16_t;
using std::int32_t;
using std::uint32_t;
using std::int64_t;
using std::uint64_t;

// 定义 register_t (部分旧代码需要)
typedef int32_t register_t;

#define LITTLEENDIAN 1

/* Specific-Sized Integers */
// functions for converting a string to a 64-bit number.
#if defined(_MSC_VER)
#define ATOL64(x) _atoi64(x)
#else
#define ATOL64(x) atoll(x)
#endif

#ifdef _MSC_VER
#pragma intrinsic(_lrotl,_lrotr)
#define	ROL(x,n)	_lrotl((x),(n))
#define	ROR(x,n)	_lrotr((x),(n))
#else
#ifndef ROL
#define ROL(a,b) (((a) << (b)) | ((a) >> (32 - (b))))
#endif
#ifndef ROR
#define ROR(a,b) (((a) >> (b)) | ((a) << (32 - (b))))
#endif
#endif

// Endianness handling
#if !defined(BIGENDIAN) && !defined(LITTLEENDIAN)
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BIGENDIAN 1
#define LITTLEENDIAN 0
#else
#define LITTLEENDIAN 1
#define BIGENDIAN 0
#endif
#endif

#define SWAP2(num) ((((num) >> 8) & 0x00FF) | (((num) << 8) & 0xFF00))
#define SWAP4(num) ((((num) >> 24) & 0x000000FF) | (((num) >> 8) & 0x0000FF00) | (((num) << 8) & 0x00FF0000) | (((num) << 24) & 0xFF000000))
#define SWAP8(x) \
(uint64_t)((((uint64_t)(x) & 0xff) << 56) | \
                                                  ((uint64_t)(x) & 0xff00ULL) << 40 | \
                  ((uint64_t)(x) & 0xff0000ULL) << 24 | \
                  ((uint64_t)(x) & 0xff000000ULL) << 8 | \
                  ((uint64_t)(x) & 0xff00000000ULL) >> 8 | \
                  ((uint64_t)(x) & 0xff0000000000ULL) >> 24 | \
                  ((uint64_t)(x) & 0xff000000000000ULL) >> 40 | \
                  ((uint64_t)(x) & 0xff00000000000000ULL) >> 56)

#define SWAP16 SWAP2
#define SWAP32 SWAP4
#define SWAP64 SWAP8

#if BIGENDIAN
#define LSB2(num) SWAP2(num)
#define LSB4(num) SWAP4(num)
#define MSB2(num) (num)
#define MSB4(num) (num)
#else
#define LSB2(num) (num)
#define LSB4(num) (num)
#define MSB2(num) SWAP2(num)
#define MSB4(num) SWAP4(num)
#endif

// DLL Export/Import macros
#ifdef MOS_WINDOWS
#ifdef MUTIL_LIB_BUILD
#define MEXP(type) __declspec(dllexport) type __stdcall
#define MCEXP(name) class __declspec(dllexport) name
#else
#define MEXP(type) __declspec(dllimport) type __stdcall
#define MCEXP(name) class __declspec(dllimport) name
#endif
#else
#define MEXP(type) type
#define MCEXP(name) class name
#endif

#define MYRIAD_UTIL

#ifndef NULL
#define NULL 0
#endif

#endif /* MUTIL_H */

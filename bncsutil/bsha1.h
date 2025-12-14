#ifndef BSHA1_H
#define BSHA1_H
#include <bncsutil/mutil.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * calcHashBuf
 * 计算数据的"Broken SHA-1"哈希值
 *
 * 参数:
 * data: 要哈希处理的数据
 * length: 数据的长度
 * hash: 至少20字节长度的缓冲区，用于接收哈希值
 */
MEXP(void) calcHashBuf(const char* data, size_t length, char* hash);

/*
 * 新的实现。有缺陷。暂无修复计划。
 */
MEXP(void) bsha1_hash(const char* input, unsigned int length, char* result);

#ifdef __cplusplus
} // extern "C"
#endif

#endif

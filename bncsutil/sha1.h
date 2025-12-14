#ifndef _SHA1_H_
#define _SHA1_H_

#include <bncsutil/mutil.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _SHA_enum_
#define _SHA_enum_
enum
{
    shaSuccess = 0,          /* 成功 */
    shaNull,                 /* 空指针参数 */
    shaInputTooLong,         /* 输入数据过长 */
    shaStateError            /* 在调用Result之后调用Input */
};
#endif

#define SHA1HashSize 20      /* SHA-1哈希值长度（字节数） */

/*
 * SHA-1上下文结构体
 * 用于存储SHA-1哈希运算的上下文信息
 */
typedef struct SHA1Context
{
    uint32_t Intermediate_Hash[SHA1HashSize/4]; /* 中间哈希值（消息摘要） */

    uint32_t Length_Low;            /* 消息长度低位部分（以位为单位） */
    uint32_t Length_High;           /* 消息长度高位部分（以位为单位） */

    /* 消息块数组索引 */
    int_least16_t Message_Block_Index;
    uint8_t Message_Block[64];      /* 512位消息块 */

    int Computed;               /* 摘要是否已计算完成？ */
    int Corrupted;             /* 消息摘要是否已损坏？ */
} SHA1Context;

/*
 * 函数原型声明
 */

/* 重置SHA-1上下文结构体为初始状态 */
int SHA1Reset(SHA1Context *);

/*
 * 向SHA-1计算输入数据
 * @param context SHA-1上下文
 * @param message_array 输入消息数据
 * @param length 输入数据长度（字节数）
 * @return 成功返回shaSuccess，失败返回相应错误码
 */
int SHA1Input(SHA1Context *context,
              const uint8_t *message_array,
              unsigned int length);

/*
 * 获取SHA-1计算结果
 * @param context SHA-1上下文
 * @param Message_Digest 输出缓冲区（至少20字节）
 * @return 成功返回shaSuccess，失败返回相应错误码
 */
int SHA1Result(SHA1Context *context,
               uint8_t Message_Digest[SHA1HashSize]);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* _SHA1_H_ */

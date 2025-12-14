#ifndef DECODEKEY_H
#define DECODEKEY_H
#include <bncsutil/mutil.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * 快速解码CD-key：一次性函数调用中解码CD-key、获取相关值并计算哈希
 *
 * @param cd_key CD-key字符串
 * @param client_token 客户端令牌
 * @param server_token 服务器令牌
 * @param public_value 输出公共值的指针
 * @param product 输出产品标识的指针
 * @param hash_buffer 哈希缓冲区
 * @param buffer_len 缓冲区长度
 * @return 成功返回1，失败返回0
 *
 * 注意：
 * 1. 该函数专为SID_AUTH_CHECK (0x51)协议设计
 * 2. 调用此函数前无需调用kd_init进行初始化
 * 3. 自BNCSutil 1.1.0版本起可用
 */
MEXP(int) kd_quick(const char* cd_key, uint32_t client_token,
         uint32_t server_token, uint32_t* public_value,
         uint32_t* product, char* hash_buffer, size_t buffer_len);

/**
 * 初始化CD-key解码C语言包装器
 * @return 成功返回1，失败返回0
 */
MEXP(int) kd_init();

/**
 * 创建新的CD-key解码器
 *
 * @param cdkey CD-key字符串
 * @param keyLength CD-key长度
 * @return 成功返回解码器ID，失败或CD-key无效返回-1
 */
MEXP(int) kd_create(const char* cdkey, int keyLength);

/**
 * 释放指定的CD-key解码器
 *
 * @param decoder 解码器ID
 * @return 成功返回1，失败返回0
 */
MEXP(int) kd_free(int decoder);

/**
 * 获取指定解码器的私有值长度
 *
 * @param decoder 解码器ID
 * @return 成功返回长度，失败返回0
 */
MEXP(int) kd_val2Length(int decoder);

/**
 * 获取指定解码器的产品标识值
 *
 * @param decoder 解码器ID
 * @return 成功返回产品标识，失败返回0
 */
MEXP(int) kd_product(int decoder);

/**
 * 获取指定解码器的公共值
 *
 * @param decoder 解码器ID
 * @return 成功返回公共值，失败返回0
 */
MEXP(int) kd_val1(int decoder);

/**
 * 获取指定解码器的私有值（短格式）
 *
 * @param decoder 解码器ID
 * @return 成功返回私有值，失败返回0
 *
 * 注意：仅当kd_val2Length返回值≤4时使用此函数
 */
MEXP(int) kd_val2(int decoder);

/**
 * 获取指定解码器的私有值（长格式）
 *
 * @param decoder 解码器ID
 * @param out 输出缓冲区
 * @return 成功返回私有值长度，失败返回0
 *
 * 注意：仅当kd_val2Length返回值>4时使用此函数
 */
MEXP(int) kd_longVal2(int decoder, char* out);

/**
 * 为SID_AUTH_CHECK (0x51)计算密钥值的哈希
 *
 * @param decoder 解码器ID
 * @param clientToken 客户端令牌
 * @param serverToken 服务器令牌
 * @return 成功返回哈希长度，失败返回0
 */
MEXP(int) kd_calculateHash(int decoder, uint32_t clientToken,
                 uint32_t serverToken);

/**
 * 将密钥哈希放入输出缓冲区
 *
 * @param decoder 解码器ID
 * @param out 输出缓冲区
 * @return 成功返回哈希长度，失败返回0
 *
 * 注意：输出缓冲区必须至少为kd_calculateHash返回的长度
 */
MEXP(int) kd_getHash(int decoder, char* out);

/**
 * [!] kd_isValid函数已弃用
 *
 * @param decoder 解码器ID
 * @return 始终返回1（仅用于兼容性）
 *
 * 注意：kd_create会在返回前检查密钥是否有效，
 *       如果无效则会销毁解码器并返回错误代码
 */
MEXP(int) kd_isValid(int decoder);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DECODEKEY_H

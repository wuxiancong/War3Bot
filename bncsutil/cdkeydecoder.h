#ifndef BNCSUTIL_CDKEYDECODER_H
#define BNCSUTIL_CDKEYDECODER_H

#include <bncsutil/mutil.h>

/**
 * 内部密钥类型常量
 */
#define KEY_STARCRAFT   1    /* 星际争霸 */
#define KEY_DIABLO2     2    /* 暗黑破坏神2 */
#define KEY_WARCRAFT2   2    /* 魔兽争霸2（注意：与DIABLO2相同）[原文标注] */
#define KEY_WARCRAFT3   3    /* 魔兽争霸3 */

/**
 * CD-key解码器类
 * 用于解码和验证各种暴雪游戏的CD-key
 */
MCEXP(CDKeyDecoder) {
protected:
    char* cdkey;            /* 原始CD-key字符串 */
    int initialized;        /* 是否已初始化标志 */
    int keyOK;              /* 密钥是否有效标志 */
    size_t keyLen;          /* 密钥长度 */
    char* keyHash;          /* 计算出的哈希值 */
    size_t hashLen;         /* 哈希值长度 */
    int keyType;            /* 密钥类型（见上面的常量） */
    unsigned long value1;   /* 解码值1 */
    unsigned long value2;   /* 解码值2 */
    unsigned long product;  /* 产品标识 */
    char* w3value2;         /* WarCraft3专用value2值 */

    /* 处理各游戏密钥的内部方法 */
    int processStarCraftKey();
    int processWarCraft2Key();
    int processWarCraft3Key();

    /* 解码密钥表 */
    void decodeKeyTable(int* keyTable);

    /* 辅助函数：十六进制转换 */
    inline char getHexValue(int v);
    inline int getNumValue(char v);

    /* 辅助函数：乘法运算 */
    inline void mult(const int r, const int x, int* a, int dcByte);

public:
    /**
     * 创建一个新的CD-key解码器对象
     * 除非被子类化，否则没有实际用途
     */
    CDKeyDecoder();

    /**
     * 使用指定CD-key创建解码器对象
     * @param cd_key 要解码的CD-key字符串
     */
    CDKeyDecoder(const char* cd_key);

    /**
     * 创建新的CD-key解码器对象，使用指定的密钥
     * @param cdKey CD-key字符串
     * @param keyLength 密钥长度（不包括空终止符）
     *
     * 注意：使用此构造函数后，应用程序应使用isKeyValid()检查提供的密钥的有效性
     */
    CDKeyDecoder(const char* cdKey, size_t keyLength);

    /**
     * 虚析构函数，确保正确清理内存
     */
    virtual ~CDKeyDecoder();

    /* 公开接口方法 */

    /**
     * 检查CD-key是否有效
     * @return 1表示有效，0表示无效
     */
    int isKeyValid();

    /**
     * 获取Val2的长度
     * @return Val2值的长度
     */
    int getVal2Length();

    /**
     * 获取产品标识
     * @return 产品标识号
     */
    uint32_t getProduct();

    /**
     * 获取解码值1
     * @return Val1值
     */
    uint32_t getVal1();

    /**
     * 获取解码值2
     * @return Val2值
     */
    uint32_t getVal2();

    /**
     * 获取Val2的长整型形式
     * @param out 输出缓冲区
     * @return 操作结果代码
     */
    int getLongVal2(char* out);

    /**
     * 计算用于SID_AUTH_CHECK (0x51)的CD-Key哈希
     *
     * @param clientToken 客户端令牌
     * @param serverToken 服务器令牌
     * @return 生成的哈希长度，失败返回0
     *
     * 注意：clientToken和serverToken将按原样添加到缓冲区并进行哈希处理，
     * 不考虑系统字节序。假设程序提取服务器令牌时不会改变其字节序，
     * 并且由于客户端令牌是由客户端生成的，字节序不是影响因素。
     */
    size_t calculateHash(uint32_t clientToken, uint32_t serverToken);

    /**
     * 将计算出的CD-key哈希放入outputBuffer
     *
     * 必须在调用getHash之前调用calculateHash
     *
     * @param outputBuffer 输出缓冲区
     * @return 复制到outputBuffer的哈希长度，失败返回0
     *
     * 注意：哈希在getHash运行后不会被删除；后续对getHash的调用仍然有效
     */
    size_t getHash(char* outputBuffer);
};

#endif /* BNCSUTIL_CDKEYDECODER_H */

#ifndef BNCSUTIL_NLS_H_INCLUDED
#define BNCSUTIL_NLS_H_INCLUDED

#include <bncsutil/mutil.h>
#include <cstdint>

// 模数（"N"），以16进制表示
#define NLS_VAR_N_STR \
"F8FF1A8B619918032186B68CA092B5557E976C78C73212D91216F6658523C787"
// 生成器变量（"g"）
#define NLS_VAR_g 0x2F
// SHA1(g) ^ SHA1(N) （"I"值）
#define NLS_VAR_I_STR "8018CF0A425BA8BEB8958B1AB6BF90AED970E6C"
// 服务器签名密钥
#define NLS_SIGNATURE_KEY 0x10001

#ifdef __cplusplus
    extern "C" {
#endif

    /**
 * NLS函数使用的数据结构类型
 */
    typedef struct _nls nls_t;

    /**
 * 分配并初始化一个nls_t结构体
 * @param username 用户名
 * @param password 密码
 * @return 成功返回nls_t指针，失败返回NULL指针
 */
    MEXP(nls_t*) nls_init(const char* username, const char* password);

    /**
 * 分配并初始化一个nls_t结构体，使用给定的字符串长度
 * @param username 用户名
 * @param username_length 用户名长度（不包括空终止符）
 * @param password 密码
 * @param password_length 密码长度（不包括空终止符）
 * @return 成功返回nls_t指针，失败返回NULL指针
 */
    MEXP(nls_t*) nls_init_l(const char* username, unsigned long username_length,
               const char* password, unsigned long password_length);

    /**
 * 释放nls_t结构体
 * @param nls 要释放的nls_t指针
 */
    MEXP(void) nls_free(nls_t* nls);

    /**
 * 使用新的用户名和密码重新初始化nls_t结构体
 * @param nls 要重新初始化的nls_t指针
 * @param username 新的用户名
 * @param password 新的密码
 * @return 成功返回原nls指针，失败返回NULL指针
 */
    MEXP(nls_t*) nls_reinit(nls_t* nls, const char* username,
               const char* password);

    /**
 * 使用新的用户名和密码及其给定长度重新初始化nls_t结构体
 * @param nls 要重新初始化的nls_t指针
 * @param username 新的用户名
 * @param username_length 新用户名长度
 * @param password 新的密码
 * @param password_length 新密码长度
 * @return 成功返回原nls指针，失败返回NULL指针
 */
    MEXP(nls_t*) nls_reinit_l(nls_t* nls, const char* username,
                 unsigned long username_length, const char* password,
                 unsigned long password_length);

    /* 数据包生成函数 */

    /**
 * 构建SID_AUTH_ACCOUNTCREATE（账户创建）数据包的内容
 * @param nls NLS上下文
 * @param buf 输出缓冲区
 * @param bufSize 缓冲区大小
 * @return 成功返回写入buf的字节数，错误返回0
 *
 * 注意：buf至少应为strlen(username) + 65字节长
 */
    MEXP(unsigned long) nls_account_create(nls_t* nls, char* buf, unsigned long bufSize);

    /**
 * [已弃用] 在生成数据包时使用内部用户名缓冲区（全部大写）
 *
 * 构建SID_AUTH_ACCOUNTLOGON（账户登录）数据包的内容
 * @param nls NLS上下文
 * @param buf 输出缓冲区
 * @param bufSize 缓冲区大小
 * @return 成功返回写入buf的字节数，错误返回0
 *
 * 注意：buf至少应为strlen(username) + 33字节长
 */
    MEXP(unsigned long) nls_account_logon(nls_t* nls, char* buf, unsigned long bufSize);

    /**
 * 注意：
 * 没有nls_account_logon_proof函数，因为它与nls_get_M1相同，
 * 也没有nls_account_change函数，因为手动组装该数据包可能更容易。
 */

    /**
 * 构建SID_AUTH_ACCOUNTCHANGEPROOF（账户密码更改证明）数据包的内容
 * @param nls NLS上下文
 * @param buf 输出缓冲区（至少84字节）
 * @param new_password 账户的新密码
 * @param B 服务器的公钥B值
 * @param salt 盐值
 * @return 成功返回新的nls_t指针（如同调用nls_init），失败返回NULL
 *
 * 注意：如果不打算检查服务器密码证明（对此数据包的响应），
 *       可以nls_free原nls值并使用返回值。
 *       使用新密码的任何登录操作应使用返回值。
 */
    MEXP(nls_t*) nls_account_change_proof(nls_t* nls, char* buf,
                             const char* new_password, const char* B, const char* salt);

    /* 计算函数 */

    /**
 * 获取"秘密"值（S），32字节
 * @param nls NLS上下文
 * @param out 输出缓冲区（32字节）
 * @param B 服务器的公钥B值
 * @param salt 盐值
 */
    MEXP(void) nls_get_S(nls_t* nls, char* out, const char* B, const char* salt);

    /**
 * 获取密码验证器（v），32字节
 * @param nls NLS上下文
 * @param out 输出缓冲区（32字节）
 * @param salt 盐值
 */
    MEXP(void) nls_get_v(nls_t* nls, char* out, const char* salt);

    /**
 * 获取公钥（A），32字节
 * @param nls NLS上下文
 * @param out 输出缓冲区（32字节）
 */
    MEXP(void) nls_get_A(nls_t* nls, char* out);

    /**
 * 获取基于秘密（S）的"K"值
 * @param nls NLS上下文
 * @param out 输出缓冲区（至少40字节）
 * @param S 秘密值S
 */
    MEXP(void) nls_get_K(nls_t* nls, char* out, const char* S);

    /**
 * 获取"M[1]"值，用于证明你知道密码
 * @param nls NLS上下文
 * @param out 输出缓冲区（至少20字节）
 * @param B 服务器的公钥B值
 * @param salt 盐值
 */
    MEXP(void) nls_get_M1(nls_t* nls, char* out, const char* B, const char* salt);

    /**
 * 检查"M[2]"值，用于证明服务器知道你的密码
 * @param nls NLS上下文
 * @param var_M2 服务器发送的M2值
 * @param B 服务器的公钥B值
 * @param salt 盐值
 * @return M2验证成功返回非零值，失败返回0
 *
 * 注意：添加计算值缓存后，B和salt可以安全地设置为NULL
 */
    MEXP(int) nls_check_M2(nls_t* nls, const char* var_M2, const char* B,
                 const char* salt);

    /**
 * 检查SID_AUTH_INFO（0x50）中接收到的服务器签名
 * @param address 要连接的服务器的IPv4地址（网络字节序，大端序）
 * @param signature_raw 128字节的原始服务器签名
 * @return 签名匹配返回非零值，失败返回0
 *
 * 注意：此函数不需要nls_t*参数！
 */
    MEXP(int) nls_check_signature(uint32_t address, const char* signature_raw);

#ifdef __cplusplus
} // extern "C"

#include <string>
#include <vector>
#include <utility>

/**
 * NLS便利类
 *
 * 注意：返回指针的getter函数从动态分配的内存中返回指针，
 *       当NLS对象被销毁时，这些内存将自动释放。
 *       不应手动free()或delete这些指针。
 */
class NLS
{
public:
    /**
     * 使用用户名和密码构造NLS对象
     */
    NLS(const char* username, const char* password) : n((nls_t*) 0)
    {
        n = nls_init(username, password);
    }

    /**
     * 使用用户名、密码及其长度构造NLS对象
     */
    NLS(const char* username, size_t username_length, const char* password,
        size_t password_length)
    {
        n = nls_init_l(username, username_length, password, password_length);
    }

    /**
     * 使用std::string构造NLS对象
     */
    NLS(const std::string& username, const std::string& password)
    {
        n = nls_init_l(username.c_str(), username.length(),
                       password.c_str(), password.length());
    }

    /**
     * 析构函数，自动清理资源
     */
    virtual ~NLS()
    {
        std::vector<char*>::iterator i;

        if (n)
            nls_free(n);

        for (i = blocks.begin(); i != blocks.end(); i++) {
            delete [] *i;
        }
    }

    /**
     * 获取秘密值S（写入提供的缓冲区）
     */
    void getSecret(char* out, const char* salt, const char* B)
    {
        nls_get_S(n, out, B, salt);
    }

    /**
     * 获取秘密值S（返回内部管理的指针）
     */
    const char* getSecret(const char* salt, const char* B)
    {
        char* buf = allocateBuffer(32);
        getSecret(buf, salt, B);
        return buf;
    }

    /**
     * 获取密码验证器v（写入提供的缓冲区）
     */
    void getVerifier(char* out, const char* salt)
    {
        nls_get_v(n, out, salt);
    }

    /**
     * 获取密码验证器v（返回内部管理的指针）
     */
    const char* getVerifier(const char* salt)
    {
        char* buf = allocateBuffer(32);
        getVerifier(buf, salt);
        return buf;
    }

    /**
     * 获取公钥A（写入提供的缓冲区）
     */
    void getPublicKey(char* out)
    {
        nls_get_A(n, out);
    }

    /**
     * 获取公钥A（返回内部管理的指针）
     */
    const char* getPublicKey(void)
    {
        char* buf = allocateBuffer(32);
        getPublicKey(buf);
        return buf;
    }

    /**
     * 获取哈希后的秘密值K（写入提供的缓冲区）
     */
    void getHashedSecret(char* out, const char* secret)
    {
        nls_get_K(n, out, secret);
    }

    /**
     * 获取哈希后的秘密值K（返回内部管理的指针）
     */
    const char* getHashedSecret(const char* secret)
    {
        char* buf = allocateBuffer(40);
        getHashedSecret(buf, secret);
        return buf;
    }

    /**
     * 获取客户端会话密钥M1（写入提供的缓冲区）
     */
    void getClientSessionKey(char* out, const char* salt, const char* B)
    {
        nls_get_M1(n, out, B, salt);
    }

    /**
     * 获取客户端会话密钥M1（返回内部管理的指针）
     */
    const char* getClientSessionKey(const char* salt, const char* B)
    {
        char* buf = allocateBuffer(20);
        getClientSessionKey(buf, salt, B);
        return buf;
    }

    /**
     * 检查服务器会话密钥M2
     * @return 验证成功返回true，失败返回false
     */
    bool checkServerSessionKey(const char* key, const char* salt,
                               const char* B)
    {
        return (nls_check_M2(n, key, B, salt) != 0);
    }

    /**
     * 生成密码更改证明（写入提供的缓冲区）
     * @return 包含新密码的NLS对象
     */
    NLS makeChangeProof(char* buf, const char* new_password, const char* salt,
                        const char* B)
    {
        return NLS(nls_account_change_proof(n, buf, new_password, B, salt));
    }

    /**
     * 生成密码更改证明（std::string版本）
     */
    NLS makeChangeProof(char* buf, const std::string& new_password,
                        const char* salt, const char* B)
    {
        return NLS(nls_account_change_proof(n, buf, new_password.c_str(), B,
                                            salt));
    }

    /**
     * 生成密码更改证明（自动分配缓冲区）
     * @return 包含新NLS对象和缓冲区指针的pair
     */
    std::pair<NLS, const char*> makeChangeProof(const char* new_password,
                                                 const char* salt, const char* B)
    {
        char* buf = allocateBuffer(84);
        NLS nls = NLS(nls_account_change_proof(n, buf, new_password, B, salt));
        return std::pair<NLS, const char*>(nls, buf);
    }

    /**
     * 生成密码更改证明（std::string版本）
     */
    std::pair<NLS, const char*> makeChangeProof(const std::string& new_password,
                                                 const char* salt, const char* B)
    {
        return makeChangeProof(new_password.c_str(), salt, B);
    }

private:
    std::vector<char*> blocks;  // 管理的缓冲区列表
    nls_t* n;                   // 底层C结构体指针

    /**
     * 私有构造函数，用于从nls_t指针创建NLS对象
     */
    NLS(nls_t* nls)
    {
        n = nls;
    }

    /**
     * 分配并跟踪内部缓冲区
     * @param length 缓冲区长度
     * @return 分配的缓冲区指针
     */
    char* allocateBuffer(size_t length)
    {
        char* buf = new char[length];
        blocks.push_back(buf);
        return buf;
    }
};

#endif

#endif /* BNCSUTIL_NLS_H_INCLUDED */

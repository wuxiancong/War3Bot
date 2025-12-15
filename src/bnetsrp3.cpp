/*
 * -------------------------------------------------------------------------
 * 文件名: bnetsrp3.cpp
 * 说明:
 *      Battle.net SRP3 协议核心逻辑实现。
 *      包含了大数运算、SHA1 哈希组合以及战网特有的协议细节。
 * -------------------------------------------------------------------------
 */

#include "bnetsrp3.h"
#include "bnethash.h"
#include <cstring>
#include <QDebug>

// SRP 协议常量定义
// g = 47
static const quint8 bnetsrp3_g_val = 0x2F;

// N: 32字节 (256位) 的大素数
static const unsigned char bnetsrp3_N_val[] = {
    0xF8, 0xFF, 0x1A, 0x8B, 0x61, 0x99, 0x18, 0x03,
    0x21, 0x86, 0xB6, 0x8C, 0xA0, 0x92, 0xB5, 0x55,
    0x7E, 0x97, 0x6C, 0x78, 0xC7, 0x32, 0x12, 0xD9,
    0x12, 0x16, 0xF6, 0x65, 0x85, 0x23, 0xC7, 0x87
};

// I: H(g) ^ H(N) 的预计算结果 (20字节)
static const unsigned char bnetsrp3_I_val[] = {
    0xF8, 0x01, 0x8C, 0xF0, 0xA4, 0x25, 0xBA, 0x8B,
    0xEB, 0x89, 0x58, 0xB1, 0xAB, 0x6B, 0xF9, 0x0A,
    0xED, 0x97, 0x0E, 0x6C
};

// 初始化静态 BigInt 常量
// 注意：PvPGN 中大数通常以 Little Endian 方式处理
BigInt BnetSRP3::N = BigInt(bnetsrp3_N_val, 32, 1, true);
BigInt BnetSRP3::g = BigInt((quint64)bnetsrp3_g_val);
BigInt BnetSRP3::I = BigInt(bnetsrp3_I_val, 20, 1, true);

/**
 * @brief 初始化函数
 * 处理用户名大写转换、随机数生成等通用逻辑。
 */
void BnetSRP3::init(const QString &username, const QString &password, BigInt* salt)
{
    // 战网协议对用户名和密码均采用大写形式进行哈希
    m_username = username.toUpper();

    if (!password.isNull()) {
        // === 客户端模式 或 注册模式 ===
        m_password = password.toUpper();
        // 生成随机私钥 a
        a = BigInt::random(32) % N;
        // 生成随机盐 s
        s = BigInt::random(32);
    } else {
        // === 服务端验证模式 ===
        m_password = QString();
        // 生成服务端随机私钥 b
        b = BigInt::random(32) % N;
        if (salt) s = *salt;
    }

    B = nullptr;

    // 缓存 Salt 的字节形式，便于后续哈希计算
    unsigned char buf[32];
    s.getData(buf, 32, 1, true);
    raw_salt = QByteArray((const char*)buf, 32);
}

// 构造函数转发
BnetSRP3::BnetSRP3(const QString &username, BigInt &salt)
{
    init(username, QString(), &salt);
}

BnetSRP3::BnetSRP3(const QString &username, const QString &password)
{
    init(username, password, nullptr);
}

BnetSRP3::~BnetSRP3()
{
    if (B) delete B;
}

/**
 * @brief 计算客户端私钥 x
 * 公式: x = H(s, H(U:P))
 * 其中 H 是 Little-Endian SHA1
 */
BigInt BnetSRP3::getClientPrivateKey() const
{
    // 1. 计算 H(P) = H("USERNAME:PASSWORD")
    QByteArray userpass = m_username.toLatin1() + ":" + m_password.toLatin1();

    t_hash userpass_hash;
    little_endian_sha1_hash(&userpass_hash, userpass.size(), userpass.constData());

    // 2. 计算 H(s, H(P))
    QByteArray private_value;
    private_value.append(raw_salt);
    private_value.append((const char*)userpass_hash, 20);

    t_hash private_value_hash;
    little_endian_sha1_hash(&private_value_hash, private_value.size(), private_value.constData());

    // 结果转为 BigInt
    return BigInt((unsigned char*)private_value_hash, 20, 1, false);
}

/**
 * @brief 计算扰码 u
 * 公式: u = H(B) (取哈希结果的前32位)
 */
BigInt BnetSRP3::getScrambler(BigInt &B_ref) const
{
    unsigned char raw_B[32];
    // 获取 B 的字节表示 (4字节块交换，小端序)
    B_ref.getData(raw_B, 32, 4, false);

    t_hash hash;
    sha1_hash(&hash, 32, raw_B);

    return BigInt((quint32)hash[0]);
}

/**
 * @brief 计算客户端原生会话密钥 S (Premaster Secret)
 * 公式: S = (B - g^x)^(a + ux) % N
 */
BigInt BnetSRP3::getClientSecret(BigInt &B_ref) const
{
    BigInt x = getClientPrivateKey();
    BigInt u = getScrambler(B_ref);
    return (N + B_ref - g.powm(x, N)).powm((x * u) + a, N);
}

/**
 * @brief 计算服务端原生会话密钥 S (Premaster Secret)
 * 公式: S = (A * v^u)^b % N
 */
BigInt BnetSRP3::getServerSecret(BigInt &A, BigInt &v)
{
    BigInt B_val = getServerSessionPublicKey(v);
    BigInt u = getScrambler(B_val);
    return ((A * v.powm(u, N)) % N).powm(b, N);
}

/**
 * @brief 生成最终会话密钥 K
 * 战网特殊算法：将 S 的字节流分为奇数位和偶数位，分别哈希，然后交错拼接。
 * K = Interleave(SHA1(Odd(S)), SHA1(Even(S)))
 */
BigInt BnetSRP3::hashSecret(BigInt &secret) const
{
    unsigned char raw_secret[32];
    secret.getData(raw_secret, 32, 4, false);

    unsigned char odd[16];
    unsigned char even[16];

    // 分离奇偶字节
    for (int i = 0; i < 16; i++) {
        odd[i] = raw_secret[i * 2];
        even[i] = raw_secret[i * 2 + 1];
    }

    t_hash odd_hash, even_hash;
    little_endian_sha1_hash(&odd_hash, 16, odd);
    little_endian_sha1_hash(&even_hash, 16, even);

    QByteArray hashedSecret;
    unsigned char* pOdd = (unsigned char*)odd_hash;
    unsigned char* pEven = (unsigned char*)even_hash;

    // 交错拼接结果
    for (int i = 0; i < 20; i++) {
        hashedSecret.append(pOdd[i]);
        hashedSecret.append(pEven[i]);
    }

    return BigInt((const unsigned char*)hashedSecret.constData(), 40, 1, false);
}

// 获取 Verifier: v = g^x % N
BigInt BnetSRP3::getVerifier() const
{
    return g.powm(getClientPrivateKey(), N);
}

BigInt BnetSRP3::getSalt() const
{
    return s;
}

void BnetSRP3::setSalt(BigInt salt_)
{
    s = salt_;
    unsigned char buf[32];
    s.getData(buf, 32, 1, true);
    raw_salt = QByteArray((const char*)buf, 32);
}

// 客户端公钥 A = g^a % N
BigInt BnetSRP3::getClientSessionPublicKey() const
{
    return g.powm(a, N);
}

// 服务端公钥 B = (v + g^b) % N
BigInt BnetSRP3::getServerSessionPublicKey(BigInt &v)
{
    if (!B) {
        B = new BigInt((v + g.powm(b, N)) % N);
    }
    return *B;
}

BigInt BnetSRP3::getHashedClientSecret(BigInt &B_ref) const
{
    BigInt clientSecret = getClientSecret(B_ref);
    return hashSecret(clientSecret);
}

BigInt BnetSRP3::getHashedServerSecret(BigInt &A, BigInt &v)
{
    BigInt serverSecret = getServerSecret(A, v);
    return hashSecret(serverSecret);
}

/**
 * @brief 生成客户端 Proof (M1)
 * M1 = H(I, H(U), s, A, B, K)
 * 注意：这里的字节序混合非常复杂，必须严格遵循 PvPGN/Bnet 标准。
 */
BigInt BnetSRP3::getClientPasswordProof(BigInt &A, BigInt &B_ref, BigInt &K) const
{
    QByteArray proofData;
    unsigned char buf[1024];

    // 1. I (20 bytes, Little Endian, 4-byte block swap)
    I.getData(buf, 20, 4, false);
    proofData.append((const char*)buf, 20);

    // 2. H(username)
    t_hash usernameHash;
    QByteArray userBytes = m_username.toLatin1();
    little_endian_sha1_hash(&usernameHash, userBytes.size(), userBytes.constData());
    proofData.append((const char*)usernameHash, 20);

    // 3. Salt (32 bytes, Big Endian)
    // 注意：PvPGN 此处直接使用了 s.getData (BigEndian)
    s.getData(buf, 32, 1, true);
    proofData.append((const char*)buf, 32);

    // 4. A (32 bytes, LE, 4-byte swap)
    A.getData(buf, 32, 4, false);
    proofData.append((const char*)buf, 32);

    // 5. B (32 bytes, LE, 4-byte swap)
    B_ref.getData(buf, 32, 4, false);
    proofData.append((const char*)buf, 32);

    // 6. K (40 bytes, LE, 4-byte swap)
    K.getData(buf, 40, 4, false);
    proofData.append((const char*)buf, 40);

    // 计算哈希
    t_hash proofHash;
    little_endian_sha1_hash(&proofHash, proofData.size(), proofData.constData());

    return BigInt((unsigned char*)proofHash, 20, 1, false);
}

/**
 * @brief 生成服务端 Proof (M2)
 * M2 = H(A, M1, K)
 */
BigInt BnetSRP3::getServerPasswordProof(BigInt &A, BigInt &M, BigInt &K) const
{
    QByteArray proofData;
    unsigned char buf[128];

    // A (32 bytes)
    A.getData(buf, 32, 4, false);
    proofData.append((const char*)buf, 32);

    // M1 (Client Proof) (20 bytes)
    M.getData(buf, 20, 4, false);
    proofData.append((const char*)buf, 20);

    // K (Session Key) (40 bytes)
    K.getData(buf, 40, 4, false);
    proofData.append((const char*)buf, 40);

    t_hash proofHash;
    little_endian_sha1_hash(&proofHash, proofData.size(), proofData.constData());

    return BigInt((unsigned char*)proofHash, 20, 1, false);
}

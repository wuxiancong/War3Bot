/*
 * -------------------------------------------------------------------------
 * 文件名: bnetsrp3.cpp
 * 说明:
 *      Battle.net SRP3 协议核心逻辑实现
 * -------------------------------------------------------------------------
 */

#include "bnetsrp3.h"
#include "bnethash.h"
#include "logger.h"
#include <cstring>
#include <QDebug>

// SRP 协议常量定义
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
BigInt BnetSRP3::N = BigInt(bnetsrp3_N_val, 32, 1, true);
BigInt BnetSRP3::g = BigInt((quint64)bnetsrp3_g_val);
BigInt BnetSRP3::I = BigInt(bnetsrp3_I_val, 20, 1, true);

// 内部静态辅助函数
static QString debug_buf_to_hex(const unsigned char *buf, size_t len)
{
    return QString(QByteArray((const char*)buf, len).toHex());
}

void BnetSRP3::init(const QString &username, const QString &password, BigInt* salt)
{
    m_username = username.toUpper();

    LOG_INFO(QString("[SRP 初始化] 原始输入 -> 用户名: \"%1\"").arg(m_username));

    if (!password.isNull()) {
        m_password = password;
        a = BigInt::random(32) % N;
        s = BigInt::random(32);
    } else {
        m_password = QString();
        b = BigInt::random(32) % N;
        if (salt) s = *salt;
    }

    B = nullptr;

    unsigned char buf[32];
    memset(buf, 0, 32);
    s.getData(buf, 32, 1, false);
    m_raw_salt = QByteArray((const char*)buf, 32);

    LOG_INFO(QString("[SRP 初始化] 原始盐值 (小端序/LE): %1").arg(debug_buf_to_hex(buf, 32)));
}

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

BigInt BnetSRP3::getClientPrivateKey() const
{
    QByteArray userpass = m_username.toUpper().toLatin1() + ":" + m_password.toLatin1();
    LOG_INFO(QString("[SRP 内部] 正在计算 x, 输入: \"%1\"").arg(QString(userpass)));

    // 1. 计算 H(I:P)
    t_hash userpass_hash;
    little_endian_sha1_hash(&userpass_hash, userpass.size(), userpass.constData());

    LOG_INFO(QString("[SRP 内部] H(P) (Hash字节流): %1").arg(debug_buf_to_hex((unsigned char*)userpass_hash, 20)));

    // 2. 拼接 Salt + H(I:P)
    QByteArray private_value;

    // 直接使用 m_raw_salt (Little Endian)，保证与 setSalt 收到的字节完全一致
    private_value.append(m_raw_salt);

    // 拼接 Hash
    private_value.append((const char*)userpass_hash, 20);

    // 3. 计算最终 Hash: x = H( Salt + H(I:P) )
    t_hash private_value_hash;
    little_endian_sha1_hash(&private_value_hash, private_value.size(), private_value.constData());

    // 4. 生成 x (BigInt)
    BigInt x((unsigned char*)private_value_hash, 20, 1, false);

    LOG_INFO(QString("[SRP 内部] 计算出的 x (BigInt): %1").arg(x.toHexString()));

    return x;
}

BigInt BnetSRP3::getScrambler(BigInt &B_ref) const
{
    unsigned char raw_B[32];
    B_ref.getData(raw_B, 32, 4, false);

    t_hash hash;
    sha1_hash(&hash, 32, raw_B);

    quint32 scrambler_val;
    memcpy(&scrambler_val, hash, 4);

    BigInt u(scrambler_val);

    LOG_INFO(QString("[SRP 内部] 计算出的扰码 u (BigInt): %1").arg(u.toHexString()));

    return u;
}

BigInt BnetSRP3::getClientSecret(BigInt &B_ref) const
{
    BigInt x = getClientPrivateKey();
    BigInt u = getScrambler(B_ref);

    // S = (B - g^x)^(a + ux) % N
    BigInt S = (N + B_ref - g.powm(x, N)).powm((x * u) + a, N);

    LOG_INFO(QString("[SRP 内部] 计算出的预主密钥 S (BigInt): %1").arg(S.toHexString()));

    return S;
}

BigInt BnetSRP3::getServerSecret(BigInt &A, BigInt &v)
{
    BigInt B_val = getServerSessionPublicKey(v);
    BigInt u = getScrambler(B_val);
    return ((A * v.powm(u, N)) % N).powm(b, N);
}

BigInt BnetSRP3::hashSecret(BigInt &secret) const
{
    unsigned char raw_secret[32];
    // S -> Bytes (Little Endian)
    secret.getData(raw_secret, 32, 4, false);

    unsigned char odd[16];
    unsigned char even[16];

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

    for (int i = 0; i < 20; i++) {
        hashedSecret.append(pOdd[i]);
        hashedSecret.append(pEven[i]);
    }

    BigInt K((const unsigned char*)hashedSecret.constData(), 40, 1, false);

    LOG_INFO(QString("[SRP 内部] 计算出的会话密钥 K (BigInt): %1").arg(K.toHexString()));

    return K;
}

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
    memset(buf, 0, 32);
    s.getData(buf, 32, 1, false);

    m_raw_salt = QByteArray((const char*)buf, 32);

    LOG_INFO(QString("[SRP 内部] 更新 BigInt 盐值 s: %1").arg(s.toHexString()));
    LOG_INFO(QString("[SRP 内部] 更新原始盐值 (小端序/LE): %1").arg(debug_buf_to_hex(buf, 32)));
}

BigInt BnetSRP3::getClientSessionPublicKey() const
{
    BigInt A = g.powm(a, N);
    LOG_INFO(QString("[SRP 步骤 1.1] 客户端公钥 A (BigInt): %1").arg(A.toHexString()));
    return A;
}

BigInt BnetSRP3::getServerSessionPublicKey(BigInt &v)
{
    if (!B) {
        B = new BigInt((v + g.powm(b, N)) % N);
    }
    return *B;
}

BigInt BnetSRP3::getHashedClientSecret(BigInt &B_ref) const
{
    LOG_INFO(QString("[SRP 步骤 2.2] 服务端公钥 B (BigInt): %1").arg(B_ref.toHexString()));
    BigInt clientSecret = getClientSecret(B_ref);
    return hashSecret(clientSecret);
}

BigInt BnetSRP3::getHashedServerSecret(BigInt &A, BigInt &v)
{
    BigInt serverSecret = getServerSecret(A, v);
    return hashSecret(serverSecret);
}

BigInt BnetSRP3::getClientPasswordProof(BigInt &A, BigInt &B_ref, BigInt &K) const
{
    LOG_INFO("[SRP 内部] 开始计算 M1 证明...");

    QByteArray proofData;
    unsigned char buf[256];

    // 1. I (H(g)^H(N))
    I.getData(buf, 20, 4, false);
    LOG_INFO(QString("  > I (Buffer):        %1").arg(debug_buf_to_hex(buf, 20)));
    proofData.append((const char*)buf, 20);

    // 2. H(username)
    t_hash usernameHash;
    QByteArray userBytes = m_username.toLatin1();
    little_endian_sha1_hash(&usernameHash, userBytes.size(), userBytes.constData());

    LOG_INFO(QString("  > H(U) (Buffer):     %1").arg(debug_buf_to_hex((unsigned char*)usernameHash, 20)));
    proofData.append((const char*)usernameHash, 20);

    // 3. Salt
    LOG_INFO(QString("  > 盐值 (小端序/LE):   %1").arg(QString(m_raw_salt.toHex())));
    proofData.append(m_raw_salt);

    // 4. A
    A.getData(buf, 32, 4, false);
    LOG_INFO(QString("  > A (小端序/LE):     %1").arg(debug_buf_to_hex(buf, 32)));
    proofData.append((const char*)buf, 32);

    // 5. B
    B_ref.getData(buf, 32, 4, false);
    LOG_INFO(QString("  > B (小端序/LE):     %1").arg(debug_buf_to_hex(buf, 32)));
    proofData.append((const char*)buf, 32);

    // 6. K
    K.getData(buf, 40, 4, false);
    LOG_INFO(QString("  > K (小端序/LE):     %1").arg(debug_buf_to_hex(buf, 40)));
    proofData.append((const char*)buf, 40);

    // 计算最终 Hash
    t_hash proofHash;
    little_endian_sha1_hash(&proofHash, proofData.size(), proofData.constData());

    BigInt M1((unsigned char*)proofHash, 20, 1, false);

    LOG_INFO(QString("[SRP 内部] M1 结果 (BigInt): %1").arg(M1.toHexString()));

    return M1;
}

BigInt BnetSRP3::getServerPasswordProof(BigInt &A, BigInt &M, BigInt &K) const
{
    QByteArray proofData;
    unsigned char buf[128];

    A.getData(buf, 32, 4, false);
    proofData.append((const char*)buf, 32);

    M.getData(buf, 20, 4, false);
    proofData.append((const char*)buf, 20);

    K.getData(buf, 40, 4, false);
    proofData.append((const char*)buf, 40);

    t_hash proofHash;
    little_endian_sha1_hash(&proofHash, proofData.size(), proofData.constData());

    return BigInt((unsigned char*)proofHash, 20, 1, false);
}

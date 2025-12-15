/*
 * -------------------------------------------------------------------------
 * 文件名: bnetsrp3.cpp
 * 说明:
 *      Battle.net SRP3 协议核心逻辑实现 (客户端修正版)。
 *      [修复] 1. Scrambler 计算使用 Little Endian。
 *      [修复] 2. 所有 getData blockSize 改为 1，防止字节序错乱。
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

void BnetSRP3::init(const QString &username, const QString &password, BigInt* salt)
{
    m_username = username.toUpper();

    if (!password.isNull()) {
        m_password = password.toUpper();
        a = BigInt::random(32) % N;
        s = BigInt::random(32);

        LOG_INFO(QString("[SRP Init] 用户名: %1").arg(m_username));
        // LOG_INFO(QString("[SRP Init] 生成临时私钥 a: %1").arg(a.toHexString()));
    } else {
        m_password = QString();
        b = BigInt::random(32) % N;
        if (salt) s = *salt;
    }

    B = nullptr;

    // 关键：raw_salt 存储 Little Endian 字节
    unsigned char buf[32];
    s.getData(buf, 32, 1, false);
    raw_salt = QByteArray((const char*)buf, 32);

    LOG_INFO(QString("[SRP Init] Raw Salt (LE): %1").arg(QString(raw_salt.toHex())));
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
    QByteArray userpass = m_username.toLatin1() + ":" + m_password.toLatin1();

    // 1. H(P)
    t_hash userpass_hash;
    little_endian_sha1_hash(&userpass_hash, userpass.size(), userpass.constData());

    QByteArray private_value;

    // 2. Salt (Little Endian)
    unsigned char saltBuf[32];
    s.getData(saltBuf, 32, 1, false);
    private_value.append((const char*)saltBuf, 32);

    // 3. 拼接 H(P) (Big Endian)
    private_value.append((const char*)userpass_hash, 20);

    // 4. 计算最终 Hash
    t_hash private_value_hash;
    little_endian_sha1_hash(&private_value_hash, private_value.size(), private_value.constData());

    // 5. 使用 blockSize=4 来翻转内存中 Big-Endian 的字节，将其还原为正确的整数值。
    BigInt x((unsigned char*)private_value_hash, 20, 4, false);

    LOG_INFO(QString("[SRP X] 计算出的私钥 x: %1").arg(x.toHexString()));

    return x;
}

BigInt BnetSRP3::getScrambler(BigInt &B_ref) const
{
    unsigned char raw_B[32];

    // 【修复1】 将 true 改为 false。服务端计算 Scrambler 使用的是 Little Endian 的 B。
    // 【修复2】 将 1 作为 blockSize。防止 BigInt 进行 Word Swap。
    B_ref.getData(raw_B, 32, 1, false);

    t_hash hash;
    sha1_hash(&hash, 32, raw_B);

    BigInt u((quint32)hash[0]);

    // DEBUG: 输出扰码 u
    LOG_INFO(QString("[SRP U] 计算出的扰码 u: %1").arg(u.toHexString()));

    return u;
}

BigInt BnetSRP3::getClientSecret(BigInt &B_ref) const
{
    BigInt x = getClientPrivateKey();
    BigInt u = getScrambler(B_ref);

    // S = (B - g^x)^(a + ux) % N
    BigInt S = (N + B_ref - g.powm(x, N)).powm((x * u) + a, N);

    // DEBUG: 输出原生密钥 S
    LOG_INFO(QString("[SRP S] 计算出的 Pre-Master Secret S: %1").arg(S.toHexString()));

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

    // 【修复3】 将 4 改为 1。S 需要以纯 Little Endian 提取用于交错混淆。
    secret.getData(raw_secret, 32, 1, false);

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

    // DEBUG: 输出会话密钥 K
    LOG_INFO(QString("[SRP K] 计算出的 Session Key K: %1").arg(K.toHexString()));

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
    s.getData(buf, 32, 1, false); // 保持 Little Endian
    raw_salt = QByteArray((const char*)buf, 32);

    LOG_INFO(QString("[SRP SetSalt] 更新 Salt (LE): %1").arg(QString(raw_salt.toHex())));
}

BigInt BnetSRP3::getClientSessionPublicKey() const
{
    BigInt A = g.powm(a, N);
    LOG_INFO(QString("[SRP A] 客户端公钥 A: %1").arg(A.toHexString()));
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
    LOG_INFO(QString("[SRP B] 服务端公钥 B: %1").arg(B_ref.toHexString()));
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
    LOG_INFO("=== 开始计算 M1 Proof ===");
    QByteArray proofData;
    unsigned char buf[1024];

    // 1. I (H(g)^H(N))
    // 【修复4】 blockSize=1
    I.getData(buf, 20, 1, false);
    proofData.append((const char*)buf, 20);

    // 2. H(username)
    t_hash usernameHash;
    QByteArray userBytes = m_username.toLatin1();
    little_endian_sha1_hash(&usernameHash, userBytes.size(), userBytes.constData());
    proofData.append((const char*)usernameHash, 20);

    // 3. Salt
    // 【修复5】 blockSize=1 (你的原代码是1，这里保持1是正确的)
    s.getData(buf, 32, 1, false);

    LOG_INFO(QString("  > M1 Salt (LE): %1").arg(QString(QByteArray((const char*)buf, 32).toHex())));
    proofData.append((const char*)buf, 32);

    // 4. A
    // 【修复6】 blockSize=1 (关键！之前是4，导致了 ce 12 14 94 的错乱)
    A.getData(buf, 32, 1, false);
    LOG_INFO(QString("  > M1 A (LE):    %1").arg(QString(QByteArray((const char*)buf, 32).toHex())));
    proofData.append((const char*)buf, 32);

    // 5. B
    // 【修复7】 blockSize=1
    B_ref.getData(buf, 32, 1, false);
    LOG_INFO(QString("  > M1 B (LE):    %1").arg(QString(QByteArray((const char*)buf, 32).toHex())));
    proofData.append((const char*)buf, 32);

    // 6. K
    // 【修复8】 blockSize=1
    K.getData(buf, 40, 1, false);
    LOG_INFO(QString("  > M1 K (LE):    %1").arg(QString(QByteArray((const char*)buf, 40).toHex())));
    proofData.append((const char*)buf, 40);

    t_hash proofHash;
    little_endian_sha1_hash(&proofHash, proofData.size(), proofData.constData());

    BigInt M1((unsigned char*)proofHash, 20, 1, false);
    LOG_INFO(QString("[SRP M1] 计算出的 Proof M1: %1").arg(M1.toHexString()));
    LOG_INFO("=== M1 计算结束 ===");

    return M1;
}

BigInt BnetSRP3::getServerPasswordProof(BigInt &A, BigInt &M, BigInt &K) const
{
    QByteArray proofData;
    unsigned char buf[128];

    // 【修复9】 blockSize=1
    A.getData(buf, 32, 1, false);
    proofData.append((const char*)buf, 32);

    M.getData(buf, 20, 1, false);
    proofData.append((const char*)buf, 20);

    K.getData(buf, 40, 1, false);
    proofData.append((const char*)buf, 40);

    t_hash proofHash;
    little_endian_sha1_hash(&proofHash, proofData.size(), proofData.constData());

    return BigInt((unsigned char*)proofHash, 20, 1, false);
}

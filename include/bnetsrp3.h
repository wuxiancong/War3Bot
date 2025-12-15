/*
 * -------------------------------------------------------------------------
 * 文件名: bnetsrp3.h
 * 说明:
 *      Battle.net SRP3 认证协议实现类。
 *
 *      SRP (Secure Remote Password) 是一种零知识证明认证协议。
 *      本类封装了战网 (Warcraft 3 等) 使用的 SRP-3 变种算法。
 *      它既支持客户端逻辑（生成 A, M1），也支持服务端逻辑（生成 B, M2）。
 *
 *      主要流程：
 *      1. 客户端发送用户名。
 *      2. 服务端返回 Salt (s) 和 Public Key (B)。
 *      3. 客户端计算 Public Key (A) 和 Proof (M1) 发送给服务端。
 *      4. 服务端验证 M1，如果正确，返回 Proof (M2)。
 *      5. 客户端验证 M2，握手完成。
 * -------------------------------------------------------------------------
 */

#ifndef BNETSRP3_H
#define BNETSRP3_H

#include "bigint.h"
#include <QString>
#include <QByteArray>

class BnetSRP3
{
public:
    /**
     * @brief 客户端模式构造函数
     * @param username 用户名
     * @param salt 从服务端接收到的盐值
     */
    BnetSRP3(const QString &username, BigInt &salt);

    /**
     * @brief 服务端/注册模式构造函数
     * @param username 用户名
     * @param password 密码
     */
    BnetSRP3(const QString &username, const QString &password);

    ~BnetSRP3();

    // === 通用 Getter/Setter ===

    // 获取验证子 (Verifier) v = g^x % N (用于注册或服务端验证)
    BigInt getVerifier() const;

    // 获取盐值
    BigInt getSalt() const;

    // 设置盐值 (通常是客户端收到服务端响应后调用)
    void   setSalt(BigInt salt);

    // === 握手第一阶段：密钥交换 ===

    // [客户端调用] 获取客户端会话公钥 A = g^a % N
    BigInt getClientSessionPublicKey() const;

    // [服务端调用] 获取服务端会话公钥 B = (v + g^b) % N
    // 参数 v 是用户的 Verifier
    BigInt getServerSessionPublicKey(BigInt &v);

    // === 握手第二阶段：生成会话密钥 K ===

    // [客户端调用] 计算经过哈希处理的客户端会话密钥 K
    // 参数 B 是服务端发来的公钥
    BigInt getHashedClientSecret(BigInt &B) const;

    // [服务端调用] 计算经过哈希处理的服务端会话密钥 K
    // 参数 A 是客户端发来的公钥, v 是 Verifier
    BigInt getHashedServerSecret(BigInt &A, BigInt &v);

    // === 握手第三阶段：生成认证证明 (Proof) ===

    /**
     * @brief [客户端调用] 生成客户端密码证明 (M1)
     * M1 = H(I, H(U), s, A, B, K)
     */
    BigInt getClientPasswordProof(BigInt &A, BigInt &B, BigInt &K) const;

    /**
     * @brief [服务端调用] 生成服务端密码证明 (M2)
     * M2 = H(A, M1, K)
     */
    BigInt getServerPasswordProof(BigInt &A, BigInt &M, BigInt &K) const;

private:
    // 初始化内部状态，生成随机私钥 a 或 b
    void init(const QString &username, const QString &password, BigInt* salt);

    // 计算 x = H(s, H(U:P))
    BigInt getClientPrivateKey() const;

    // 计算扰码 u = H(B)
    BigInt getScrambler(BigInt &B) const;

    // 计算客户端原生密钥 S = (B - g^x)^(a + ux)
    BigInt getClientSecret(BigInt &B) const;

    // 计算服务端原生密钥 S = (A * v^u)^b
    BigInt getServerSecret(BigInt &A, BigInt &v);

    // 对原生密钥 S 进行奇偶交错哈希，生成最终会话密钥 K
    BigInt hashSecret(BigInt &secret) const;

    // === SRP 协议常量 ===
    static BigInt N;    // 大素数模数 (Modulus)
    static BigInt g;    // 生成元 (Generator)
    static BigInt I;    // 预计算的 H(g) xor H(N)

    // === 会话变量 ===
    BigInt a;           // 客户端临时私钥 (Random)
    BigInt b;           // 服务端临时私钥 (Random)
    BigInt s;           // 盐 (Salt)
    BigInt *B;          // 服务端公钥缓存 (用于服务端模式)

    QString m_username;
    QString m_password;
    QByteArray raw_salt; // 原始字节序的 Salt
};

#endif // BNETSRP3_H

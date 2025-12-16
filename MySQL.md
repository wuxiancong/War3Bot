# War3Server (PvPGN) 数据库调试总结

本文档记录了排查 SRP3 (Protocol 0x53) 登录失败问题时的数据库操作步骤。
**核心问题**：客户端代码修复后，计算出的私钥 $x$ 与数据库中旧的验证子 ($v$) 不匹配，导致会话密钥 $K$ 验证失败。
**解决方案**：删除旧账号，使用修复后的客户端重新注册。

## 1. 数据库基本信息

*   **数据库名**: `pvpgn`
*   **核心表名**: `pvpgn_BNET` (存储用户账号信息)

### 查看数据库中的表
```sql
SHOW TABLES;
```

## 2. 表结构分析

通过 `DESCRIBE` 命令，我们确认了存储 SRP 认证数据的字段。

```sql
DESCRIBE pvpgn_BNET;
```

**关键字段说明：**
*   `username`: 用户名 (唯一索引)。
*   `uid`: 用户 ID (主键)。
*   **`acct_salt`** (`varchar(128)`): SRP 协议使用的 **Salt ($s$)**，十六进制字符串。
*   **`acct_verifier`** (`varchar(128)`): SRP 协议使用的 **Verifier ($v$)**，十六进制字符串。

*(注意：`acct_passhash1` 通常存储旧版 Broken SHA1 哈希，SRP 登录不使用它)*

## 3. 数据检查操作

为了对比服务端日志中的数据与数据库是否一致，执行以下查询。

### 查询特定用户的 SRP 数据
查看用户 `bot` 的盐值和验证子：

```sql
SELECT username, acct_salt, acct_verifier 
FROM pvpgn_BNET 
WHERE username = 'bot';
```

**验证逻辑：**
*   数据库的 `acct_salt` 必须等于日志中的 `DB Salt (s)`。
*   数据库的 `acct_verifier` 必须等于日志中的 `DB Verifier (v)`。

## 4. 故障修复操作 (核心步骤)

### 问题根源
1.  数据库中的 `acct_verifier` 是在**注册**时生成的，公式为 $v = g^{x_{old}} \pmod N$。
2.  当时的客户端代码存在字节序问题，导致生成的 $x_{old}$ 是错误的（与标准逻辑不符）。
3.  现在的客户端代码已修复 (`blockSize` / `Endian` 设置正确)，计算出了正确的 $x_{new}$。
4.  登录时，服务端尝试验证 $x_{new}$ 是否匹配 $v$，由于 $x_{new} \neq x_{old}$，验证失败。

### 解决方案：删除并重注册
无法直接修改数据库来修复 $v$（因为不知道密码），必须让客户端使用正确的逻辑重新生成 $v$。

**执行以下 SQL 删除账号：**

```sql
DELETE FROM pvpgn_BNET WHERE username = 'bot';
```

### 后续步骤
1.  **重启客户端**。
2.  客户端检测到账号不存在，触发**注册流程**。
3.  客户端使用修复后的代码计算正确的 Salt 和 Verifier 并发送给服务端。
4.  服务端将正确的数据存入数据库。
5.  **再次登录**，此时 $K$ 值将完全匹配，M1 验证通过。

## 5. 常用维护命令速查

| 操作 | SQL 语句 |
| :--- | :--- |
| **统计注册用户数** | `SELECT COUNT(*) FROM pvpgn_BNET;` |
| **列出所有用户名** | `SELECT uid, username FROM pvpgn_BNET;` |
| **检查是否有 SRP 数据** | `SELECT username FROM pvpgn_BNET WHERE acct_verifier IS NOT NULL;` |
| **清空所有用户(慎用)** | `TRUNCATE TABLE pvpgn_BNET;` |
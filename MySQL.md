# MySQL.md：Pvpgn 数据库操作速查手册

本文档总结了在 $\text{MySQL}$ 命令行客户端中管理 $\text{Pvpgn}$ 数据库（假定数据库名为 `pvpgn`）的常用命令，特别是数据清除和自增序号重置操作。

## 1. 登录和环境初始化

| 目的 | 命令 | 备注 |
| :--- | :--- | :--- |
| **登录 MySQL** | `mysql -u pvpgn -p` | 用户名为 `pvpgn`，按回车后输入密码。 |
| **查看数据库** | `SHOW DATABASES;` | 确认 $\text{pvpgn}$ 数据库存在。 |
| **选择数据库** | `USE pvpgn;` | 将操作对象切换到 $\text{pvpgn}$ 数据库。 |
| **查看所有表** | `SHOW TABLES;` | 列出所有 $\text{Pvpgn}$ 表，如 `pvpgn_BNET`, `pvpgn_Record` 等。 |

## 2. Pvpgn 核心表结构（DESCRIBE 结果）

| 表名 | 描述 | 主要字段 (用于定位数据) |
| :--- | :--- | :--- |
| **`pvpgn_BNET`** | 存储用户账号信息、密码哈希、SRP 验证器、权限、最后登录信息等。 | `uid` (主键), `username` (唯一键), `acct_passhash1`, `acct_verifier`, `acct_salt` |
| **`pvpgn_Record`** | 存储用户的游戏战绩、等级、$\text{XP}$、图标等详细统计数据。 | `uid` (主键，关联 $\text{pvpgn\_BNET}$), $\text{WAR3\_solo\_wins}$, $\text{STAR\_1\_rating}$ 等 |
| **`pvpgn_WOL`** | 存储 $\text{Warcraft Online}$ 客户端的特定信息。 | `uid` (主键), `auth_apgar`, `acct_locale` |
| **`pvpgn_arrangedteam`** | 存储战队信息。 | `teamid` (主键), `clienttag`, `member1`, `wins` |
| **`pvpgn_clan`** | 存储战队/公会的基本信息。 | `cid` (主键), `name`, `motd` |
| **`pvpgn_clanmember`** | 存储战队成员与其战队的关系。 | `uid` (主键), `cid` (外键), `status` |
| **`pvpgn_friend`** | 存储好友列表信息。 | `uid` (主键) |
| **`pvpgn_profile`** | 存储用户的个人资料信息（性别、年龄等）。 | `uid` (主键), `sex`, `location`, `description` |

## 3. 数据查询 (SELECT)

| 目的 | 命令 | 示例 |
| :--- | :--- | :--- |
| **查询所有数据** | `SELECT * FROM tablename;` | `SELECT * FROM pvpgn_BNET;` |
| **查询特定记录** | `SELECT * FROM tablename WHERE condition;` | `SELECT * FROM pvpgn_BNET WHERE username = 'MyAccount';` |

## 4. 数据修改 (UPDATE)

| 目的 | 命令 | 示例 |
| :--- | :--- | :--- |
| **修改单条数据** | `UPDATE tablename SET column = new_value WHERE condition;` | `UPDATE pvpgn_BNET SET auth_admin = 'true' WHERE username = 'AdminUser';` |

## 5. 数据删除和自增序号重置 (清库操作)

### 5.1 清除所有数据并重置自增序号 (推荐)

使用 $\text{TRUNCATE TABLE}$ 是最快且最直接的方法，它会自动重置表的 $\text{AUTO\_INCREMENT}$ 计数器。

| 目的 | 命令 |
| :--- | :--- |
| **通用命令** | `TRUNCATE TABLE tablename;` |

**Pvpgn 核心数据重置示例：**

```sql
-- 彻底清除所有账号数据和战绩，并重置自增ID
TRUNCATE TABLE pvpgn_BNET;
TRUNCATE TABLE pvpgn_Record;
TRUNCATE TABLE pvpgn_WOL;
TRUNCATE TABLE pvpgn_profile;

-- 彻底清除所有战队、好友关系数据
TRUNCATE TABLE pvpgn_arrangedteam;
TRUNCATE TABLE pvpgn_clan;
TRUNCATE TABLE pvpgn_clanmember;
TRUNCATE TABLE pvpgn_friend;
```

### 5.2 删除单条数据

删除某条记录，但**不会**自动重置自增序号。

| 目的 | 命令 | 示例 (删除 $\text{uid}$ 为 10 的账号) |
| :--- | :--- | :--- |
| **删除单条记录** | `DELETE FROM tablename WHERE condition;` | `DELETE FROM pvpgn_BNET WHERE uid = 10;` |

### 5.3 实现“自增序号减 1”（回收 $\text{ID}$）

在 $\text{MySQL}$ 中不能直接“减 1”，但可以删除最大 $\text{ID}$ 的记录，然后将 $\text{AUTO\_INCREMENT}$ 设为该被删除的值，从而实现 $\text{ID}$ 回收。

| 步骤 | 命令 | 示例 (假设 $\text{pvpgn\_BNET}$ 最大 $\text{uid}$ 是 $X$) |
| :--- | :--- | :--- |
| **步骤 1: 删除最新记录** | `DELETE FROM pvpgn_BNET WHERE uid = X;` | `DELETE FROM pvpgn_BNET WHERE uid = 10;` |
| **步骤 2: 重设下一个 ID** | `ALTER TABLE pvpgn_BNET AUTO_INCREMENT = X;` | `ALTER TABLE pvpgn_BNET AUTO_INCREMENT = 10;` |

> *结果：* 下次插入新数据时，$\text{uid}$ 将从 $X$ 开始。

## 6. Pvpgn 核心表字段描述

以下是 $\text{Pvpgn}$ 数据库中各个核心表的字段总结。

### 6.1 `pvpgn_BNET` (账号信息)

| 字段名 | 类型 | $\text{Null}$ | $\text{Key}$ | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`uid`** | `int` | $\text{NO}$ | $\text{PRI}$ | 用户唯一 $\text{ID}$ (主键) |
| `acct_username` | `varchar(32)` | $\text{YES}$ | | 账户用户名 |
| **`username`** | `varchar(32)` | $\text{YES}$ | $\text{UNI}$ | 登录用户名 ($\text{Pvpgn}$ 主 $\text{Username}$) |
| `acct_passhash1` | `varchar(40)` | $\text{YES}$ | | 传统 $\text{Pvpgn}$ / $\text{SC}$ 密码哈希 |
| **`acct_verifier`** | `varchar(128)` | $\text{YES}$ | | $\text{Warcraft 3 SRP-3}$ 验证器 |
| **`acct_salt`** | `varchar(128)` | $\text{YES}$ | | $\text{Warcraft 3 SRP-3}$ 盐 |
| `auth_admin` | `varchar(6)` | $\text{YES}$ | | 是否是管理员 |
| `acct_lastlogin_time` | `int` | $\text{YES}$ | | 最后登录时间戳 |
| `acct_lastlogin_ip` | `varchar(16)` | $\text{YES}$ | | 最后登录 $\text{IP}$ |
| *\[...其他权限/时间/锁定字段...\]* | | | | |

### 6.2 `pvpgn_Record` (战绩/统计数据)

| 字段名 | 类型 | $\text{Null}$ | $\text{Key}$ | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`uid`** | `int` | $\text{NO}$ | $\text{PRI}$ | 用户 $\text{ID}$ (主键，关联 $\text{pvpgn\_BNET}$) |
| `WAR3_solo_xp` | `int` | $\text{YES}$ | | $\text{WC3}$ 冰封王座单人经验值 |
| `WAR3_solo_wins` | `int` | $\text{YES}$ | | $\text{WC3}$ 冰封王座单人胜场 |
| `W3XP_solo_xp` | `int` | $\text{YES}$ | | $\text{WC3}$ 混乱之治单人经验值 |
| `STAR_1_rating` | `int` | $\text{YES}$ | | $\text{StarCraft}$ 扩展包 $\text{1vs1}$ 评分 |
| `SEXP_1_rating` | `int` | $\text{YES}$ | | $\text{StarCraft}$ 原始版 $\text{1vs1}$ 评分 |
| *\[...大量游戏类型/种族/断开连接统计字段...\]* | | | | |

### 6.3 `pvpgn_WOL` (Warcraft Online 客户端信息)

| 字段名 | 类型 | $\text{Null}$ | $\text{Key}$ | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`uid`** | `int` | $\text{NO}$ | $\text{PRI}$ | 用户 $\text{ID}$ (主键) |
| `auth_apgar` | `varchar(8)` | $\text{YES}$ | | 客户端 $\text{APGAR}$ 验证码 |
| `acct_locale` | `int` | $\text{YES}$ | | 账户区域设置 |

### 6.4 `pvpgn_arrangedteam` (战队/队伍)

| 字段名 | 类型 | $\text{Null}$ | $\text{Key}$ | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`teamid`** | `int` | $\text{NO}$ | $\text{PRI}$ | 队伍 $\text{ID}$ (主键) |
| `size` | `int` | $\text{YES}$ | | 队伍规模 ($\text{2v2, 3v3}$ 等) |
| `clienttag` | `varchar(8)` | $\text{YES}$ | | 队伍所属客户端标签 ($\text{W3XP, STAR}$ 等) |
| `member1`, `member2`... | `int` | $\text{YES}$ | | 成员 $\text{uid}$ |
| `wins`, `losses` | `int` | $\text{YES}$ | | 队伍胜负场次 |

### 6.5 `pvpgn_clan` (公会/战队主表)

| 字段名 | 类型 | $\text{Null}$ | $\text{Key}$ | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`cid`** | `int` | $\text{NO}$ | $\text{PRI}$ | 公会 $\text{ID}$ (主键) |
| `short` | `int` | $\text{YES}$ | | 短名称 $\text{ID}$ |
| `name` | `varchar(32)` | $\text{YES}$ | | 公会名称 |
| `motd` | `varchar(255)` | $\text{YES}$ | | 公会每日消息 ($\text{Message of the Day}$) |
| `creation_time` | `int` | $\text{YES}$ | | 创建时间戳 |

### 6.6 `pvpgn_clanmember` (公会成员关系)

| 字段名 | 类型 | $\text{Null}$ | $\text{Key}$ | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`uid`** | `int` | $\text{NO}$ | $\text{PRI}$ | 成员 $\text{uid}$ (主键) |
| **`cid`** | `int` | $\text{YES}$ | $\text{MUL}$ | 所属公会 $\text{cid}$ (索引) |
| `status` | `int` | $\text{YES}$ | | 成员状态/等级 (会长、成员等) |
| `join_time` | `int` | $\text{YES}$ | | 加入时间戳 |

### 6.7 `pvpgn_friend` (好友列表)

| 字段名 | 类型 | $\text{Null}$ | $\text{Key}$ | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`uid`** | `int` | $\text{NO}$ | $\text{PRI}$ | 用户 $\text{ID}$ (主键) |
| `0_uid` | `varchar(128)` | $\text{YES}$ | | 好友 $\text{UID}$ 列表 (通常为逗号分隔字符串) |
| `count` | `varchar(128)` | $\text{YES}$ | | 好友数量或状态计数 |

### 6.8 `pvpgn_profile` (用户资料)

| 字段名 | 类型 | $\text{Null}$ | $\text{Key}$ | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`uid`** | `int` | $\text{NO}$ | $\text{PRI}$ | 用户 $\text{ID}$ (主键) |
| `sex` | `varchar(8)` | $\text{YES}$ | | 性别 |
| `location` | `varchar(128)` | $\text{YES}$ | | 地理位置 |
| `description` | `varchar(256)` | $\text{YES}$ | | 个人描述 |
| `age` | `varchar(16)` | $\text{YES}$ | | 年龄 |
| `clanname` | `varchar(48)` | $\text{YES}$ | | 公会名称 (可能是冗余字段) |
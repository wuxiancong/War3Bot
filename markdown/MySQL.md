# MySQL.md：Pvpgn 数据库操作速查手册

本文档总结了在 MySQL 命令行客户端中管理 Pvpgn 数据库（假定数据库名为 `pvpgn`）的常用命令，涵盖了环境登录、表结构说明、数据清除及自增序号重置等操作。

## 1. 登录和环境初始化

| 目的 | 命令 | 备注 |
| :--- | :--- | :--- |
| **登录 MySQL** | `mysql -u pvpgn -p` | 用户名为 `pvpgn`，按回车后输入密码。 |
| **查看数据库** | `SHOW DATABASES;` | 确认 `pvpgn` 数据库存在。 |
| **选择数据库** | `USE pvpgn;` | 将操作对象切换到 `pvpgn` 数据库。 |
| **查看所有表** | `SHOW TABLES;` | 列出所有 Pvpgn 表，如 `pvpgn_BNET`, `pvpgn_Record` 等。 |

## 2. Pvpgn 核心表结构描述

以下表格总结了 `DESCRIBE` 命令的输出重点，用于快速了解表结构。

### 2.1 `pvpgn_BNET` (账号核心信息)
存储用户账号、密码哈希、权限、封禁状态及最后登录信息。

| 字段名 | 类型 | Null | Key | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`uid`** | `int` | NO | PRI | **用户唯一 ID (主键)** |
| `username` | `varchar(32)` | YES | UNI | 登录用户名 (唯一) |
| `acct_passhash1` | `varchar(40)` | YES | | 传统 SC/PvPGN 密码哈希 |
| `acct_verifier` | `varchar(128)` | YES | | Warcraft 3 SRP-3 验证器 |
| `acct_salt` | `varchar(128)` | YES | | Warcraft 3 SRP-3 盐 |
| `auth_admin` | `varchar(6)` | YES | | 是否为管理员 (true/false) |
| `acct_lastlogin_time`| `int` | YES | | 最后登录时间戳 |

### 2.2 `pvpgn_Record` (游戏战绩统计)
存储用户的胜负场次、经验值 (XP)、等级 (Level) 和天梯排名。

| 字段名 | 类型 | Null | Key | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`uid`** | `int` | NO | PRI | **用户 ID** (关联 `pvpgn_BNET`) |
| `WAR3_solo_xp` | `int` | YES | | WAR3 冰封王座单人 XP |
| `WAR3_solo_wins` | `int` | YES | | WAR3 冰封王座单人胜场 |
| `W3XP_solo_level` | `int` | YES | | WAR3 混乱之治单人等级 |
| `STAR_1_rating` | `int` | YES | | 星际争霸 1vs1 评分 |
| *...其他字段* | | | | *包含各族胜率、断线次数等* |

### 2.3 `pvpgn_WOL` (Warcraft Online 信息)
存储旧版客户端 (Warcraft Online) 相关的特定验证信息。

| 字段名 | 类型 | Null | Key | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`uid`** | `int` | NO | PRI | **用户 ID** (主键) |
| `auth_apgar` | `varchar(8)` | YES | | 客户端 APGAR 验证码 |
| `acct_locale` | `int` | YES | | 区域设置 ID |

### 2.4 `pvpgn_arrangedteam` (战队/队伍信息)
存储 2v2, 3v3 等预组建队伍的信息。

| 字段名 | 类型 | Null | Key | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`teamid`** | `int` | NO | PRI | **队伍 ID (主键)** |
| `size` | `int` | YES | | 队伍规模 (如 2) |
| `clienttag` | `varchar(8)` | YES | | 客户端标签 (如 W3XP) |
| `member1`...`4` | `int` | YES | | 队员的 UID |
| `wins` / `losses` | `int` | YES | | 队伍战绩 |

### 2.5 `pvpgn_clan` (公会/战队主表)
存储公会的基础信息。

| 字段名 | 类型 | Null | Key | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`cid`** | `int` | NO | PRI | **公会 ID (主键)** |
| `name` | `varchar(32)` | YES | | 公会名称 |
| `motd` | `varchar(255)`| YES | | 公会公告 (Message of the Day) |
| `creation_time` | `int` | YES | | 创建时间戳 |

### 2.6 `pvpgn_clanmember` (公会成员关联)
连接用户 (UID) 和公会 (CID)。

| 字段名 | 类型 | Null | Key | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`uid`** | `int` | NO | PRI | **成员 UID (主键)** |
| **`cid`** | `int` | YES | MUL | **所属公会 ID** |
| `status` | `int` | YES | | 职位 (酋长, 萨满, 苦工等) |
| `join_time` | `int` | YES | | 加入时间 |

### 2.7 `pvpgn_friend` (好友列表)
存储用户的好友关系。

| 字段名 | 类型 | Null | Key | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`uid`** | `int` | NO | PRI | **用户 ID (主键)** |
| `0_uid` | `varchar` | YES | | 好友 UID 列表字符串 |
| `count` | `varchar` | YES | | 好友数量记录 |

### 2.8 `pvpgn_profile` (用户个人资料)
存储用户填写的个人详细信息。

| 字段名 | 类型 | Null | Key | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| **`uid`** | `int` | NO | PRI | **用户 ID (主键)** |
| `sex` | `varchar(8)` | YES | | 性别 |
| `location` | `varchar(128)`| YES | | 地理位置 |
| `description` | `varchar(256)`| YES | | 自我介绍 |

## 3. 常用数据操作

### 3.1 查询 (SELECT)

| 目的 | 命令 | 示例 |
| :--- | :--- | :--- |
| **查询所有数据** | `SELECT * FROM 表名;` | `SELECT * FROM pvpgn_BNET;` |
| **查询特定记录** | `SELECT * FROM 表名 WHERE 条件;` | `SELECT * FROM pvpgn_BNET WHERE username = 'Admin';` |

### 3.2 修改 (UPDATE)

| 目的 | 命令 | 示例 |
| :--- | :--- | :--- |
| **修改字段值** | `UPDATE 表名 SET 字段=值 WHERE 条件;` | `UPDATE pvpgn_BNET SET auth_admin = 'true' WHERE uid = 1;` |

## 4. 清除数据与重置 ID (维护操作)

### 4.1 彻底重置 (推荐)
使用 `TRUNCATE` 可以清除数据并自动将自增 ID 重置为 0/1。

```sql
-- 重置账号系统
TRUNCATE TABLE pvpgn_BNET;
TRUNCATE TABLE pvpgn_Record;
TRUNCATE TABLE pvpgn_WOL;
TRUNCATE TABLE pvpgn_profile;

-- 重置社交系统
TRUNCATE TABLE pvpgn_friend;
TRUNCATE TABLE pvpgn_clan;
TRUNCATE TABLE pvpgn_clanmember;
TRUNCATE TABLE pvpgn_arrangedteam;
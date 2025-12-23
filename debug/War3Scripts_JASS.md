# 🛠️ War3 地图校验跨平台一致性指南 (Windows vs Linux)

## 🚩 问题描述

在开发魔兽争霸 III (Warcraft III) 的主机 Bot 或校验工具时，我们遇到了一个严重的问题：**同一个地图、同一套代码，在 Windows 本地计算的 Checksum (CRC) 与部署到 Linux (Ubuntu) 服务器上计算的结果不一致。**

### 1. 症状对比

**Windows (本地开发环境) - ✅ 正确结果**
```text
[INFO] 🔹 Common.j Hash:   32EA6CF8 (Size: 166690)
[INFO] 🔹 Blizzard.j Hash: CC4E5CF6 (Size: 451847)
[INFO] [War3Map] Final Checksum: F3CEEB3A (Matches Battle.net)
```

**Linux (Ubuntu 服务器) - ❌ 错误结果**
```text
[INFO] 🔹 Common.j Hash:   E6C89041 (Size: 164273)  <-- Hash 不同，大小变小
[INFO] 🔹 Blizzard.j Hash: BEED3C3B (Size: 441618)  <-- Hash 不同，大小变小
[INFO] [War3Map] Final Checksum: 10C2DF9F (Desync)
```

### 2. 问题排查

经过对比发现，Linux 上的 `.j` 脚本文件比 Windows 上 **小了约 10KB**：
*   Windows `blizzard.j`: **441 KB**
*   Linux `blizzard.j`: **431 KB**

## 🔍 根本原因 (Root Cause)

魔兽争霸的哈希算法（Blizzard Hash / XORO）是基于 **二进制数据流** 进行计算的。这意味着文件的每一个字节（包括不可见字符）都必须精确一致。

问题的根源在于 **Git 的自动换行符转换机制 (Auto CRLF)**：

1.  **Windows 换行符**: `\r\n` (CRLF, 占 2 字节)。
2.  **Linux 换行符**: `\n` (LF, 占 1 字节)。
3.  **Git 的默认行为**: Git 认为 `.j` (JASS 脚本) 是文本文件。
    *   当你在 Windows 提交时，Git 可能将其转换为 `LF` 存入仓库。
    *   当你从 Linux 拉取 (`git pull`) 时，Git 检出为 `LF`。
    *   **结果**: `blizzard.j` 拥有约 10,000 行代码，在 Linux 上丢失了 10,000 个 `\r` 字符，导致文件变小约 10KB。

**由于缺少了这些 `\r` 字节，CRC 计算结果完全改变。**

## ✅ 解决方案

必须强制 Git 对 War3 的脚本文件 (`.j`) 使用 **Windows 风格的换行符 (CRLF)**，无论当前是在什么操作系统上。

### 1. 配置 `.gitattributes`

在项目根目录创建或修改 `.gitattributes` 文件，添加以下内容：

```ini
# .gitattributes

# 1. 默认设置：自动检测文本文件
* text=auto

# 2. [关键] 强制 War3 JASS 脚本在所有系统下保持 CRLF (\r\n) 换行符
#    这是为了保证 CRC32/XORO 哈希计算在 Windows/Linux 上的结果二进制一致。
*.j text eol=crlf
*.J text eol=crlf
*.lua text eol=crlf

# 3. 强制二进制文件 (严禁 Git 修改任何字节)
*.mpq binary
*.w3x binary
*.w3m binary
*.w3g binary
*.dll binary
*.exe binary
```

### 2. 刷新 Git 索引 (重要)

仅修改配置文件是不够的，因为 Git 索引中可能已经缓存了错误的文件格式。请在本地执行以下命令来重新标准化文件：

```bash
# 1. 移除所有文件的索引缓存（不会删除物理文件）
git rm --cached -r .

# 2. 重新根据 .gitattributes 添加文件并重置
git reset --hard

# 3. 提交更改推送到服务器
git add .gitattributes
git commit -m "Fix: Enforce CRLF for war3 scripts to fix checksum mismatch on Linux"
git push
```

### 3. 服务器端验证

在 Ubuntu 服务器上拉取最新代码后，使用 `file` 命令验证文件格式：

```bash
$ git pull
$ file war3files/common.j
```

*   **✅ 正确输出**: `ASCII text, with CRLF line terminators`
*   **❌ 错误输出**: `ASCII text` (说明依然是 LF)

---

## 📝 总结

对于任何涉及 **文件哈希校验 (CRC/MD5/SHA)** 的跨平台项目，如果源文件是文本格式（如 `.j`, `.lua`, `.txt`），必须在 `.gitattributes` 中锁定换行符格式 (`eol=crlf` 或 `eol=lf`)，否则会导致二进制指纹在不同系统下不匹配。
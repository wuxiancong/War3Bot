# War3Bot

**War3Bot** 是一个专为《魔兽争霸 III》设计的游戏会话代理服务器，基于 C++ 和 Qt 框架开发。

---

## 🛠️ 快速安装 (Ubuntu)

### 1. 环境准备与编译

```bash
# 1. 更新软件源并安装基础构建工具
sudo apt update
sudo apt install -y build-essential cmake

# 2. 安装 Qt5 依赖库
sudo apt install -y qtbase5-dev qt5-qmake libqt5core5a libqt5network5

# 3. 安装其他依赖
sudo apt install -y libgmp-dev zlib1g-dev libbz2-dev libgmp-dev

- 安装方式1：
# 4. 克隆项目代码
git clone https://github.com/wuxiancong/War3Bot.git
cd War3Bot

# 5. 编译项目
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local/War3Bot ..
make -j$(nproc)
sudo make install

sudo journalctl -u war3bot -f

# 6. 验证编译结果
cd ~
War3Bot --help

- 安装方式二：

# 7. 脚本运行（推荐）
git clone https://github.com/wuxiancong/War3Bot.git
cd War3Bot
chmod +x install.sh
./install.sh

## 非 github 克隆需输入下面命令 
sed -i 's/\r$//' install.sh

```


### 2. 重新编译 (更新代码后)

```bash
# 清理旧构建并重新编译
cd ~/War3Bot/build
rm -rf *
cmake ..
make -j$(nproc)
```

---

## ⚙️ 系统服务配置

为了让 War3Bot 在后台稳定运行，建议配置 Systemd 服务。

### 1. 创建专用系统用户 (无登录权限)
- -r: 系统账户
- -d: 指定主目录
- -s: 禁止 shell 登录
```bash
sudo useradd -r -s /bin/false -d /opt/War3Bot war3bot
```
### 2. 创建目录结构
```bash
# /opt/War3Bot      -> 存放程序本体、war3files 资源、地图
# /etc/War3Bot      -> 仅存放 .ini 配置文件
# /var/log/War3Bot  -> 存放日志
sudo mkdir -p /opt/War3Bot/war3files
sudo mkdir -p /etc/War3Bot
sudo mkdir -p /var/log/War3Bot
```

### 3. 设置权限

#### --- A. 核心程序目录 (/opt) ---
```bash
# 将所有权给 war3bot 用户
sudo chown -R war3bot:war3bot /opt/War3Bot
# 权限设为 755 (目录) 或 644 (文件) 是可以的，因为这些是二进制文件，不怕看
sudo chmod -R 755 /opt/War3Bot
```
#### --- B. 日志目录 (/var/log) ---
```bash
# 必须给 war3bot 用户写入权限
sudo chown -R war3bot:war3bot /var/log/War3Bot
# 权限设为 750 (只有拥有者和组可读，其他人无权访问)
# 防止其他用户偷看日志里的 IP 或聊天记录
sudo chmod 750 /var/log/War3Bot
```
#### --- C. 配置文件目录 (/etc) ---
```bash
# ⚠️ 关键安全设置 ⚠️
# 将目录所有权给 war3bot
sudo chown -R war3bot:war3bot /etc/War3Bot
# 权限设为 700 (drwx------) 或 600 (-rw-------)
# **只允许 war3bot 用户读写，拒绝任何其他人偷看密码**
sudo chmod 700 /etc/War3Bot
# 如果里面已经有文件，确保文件也是保密的
sudo chmod 600 /etc/War3Bot/war3bot.ini 2>/dev/null || true
```

### 2. 安装配置文件

创建配置文件 `/etc/War3Bot/War3Bot.ini`：

```ini
[server]
control_port=6116
broadcast_port=6112
peer_timeout=300000
cleanup_interval=60000
enable_broadcast=false
broadcast_interval=30000

[log]
level=info
enable_console=true
log_file=/var/log/War3Bot/war3bot.log
max_size=10485760
backup_count=5

[bnet]
server=139.155.155.166
port=6112

[bots]
list_number=1
init_count=10
auto_generate=false

[mysql]
host=127.0.0.1
port=3306
user=pvpgn
pass=yourpassword
```

### 3. 配置 Systemd 服务

创建服务文件 `sudo nano /etc/systemd/system/war3bot.service`：

> **注意**：请确保 `ExecStart` 指向您实际编译生成的二进制文件路径。如果遵循上述安装步骤且未移动文件，路径可能为 `/root/War3Bot/build/War3Bot`（需 root 权限）或建议将其移动到 `/usr/local/bin/`。

以下配置假设您已将编译好的 `War3Bot` 移动到了 `/usr/local/bin/War3Bot`，并使用 root 运行（简易模式）：

```ini
[Unit]
Description=War3Bot Warcraft III Proxy
After=network.target

[Service]
Type=simple

User=war3bot
Group=war3bot
WorkingDirectory=/opt/War3Bot

ExecStart=/usr/local/War3Bot/bin/War3Bot -p 6116 --config /etc/War3Bot/war3bot.ini
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal
PrivateTmp=false

ProtectSystem=full
ProtectHome=true

[Install]
WantedBy=multi-user.target
```

### 4. 启动服务

```bash
# 重载配置
sudo systemctl daemon-reload

# 启用开机自启
sudo systemctl enable war3bot

# 启动服务
sudo systemctl start war3bot

# 停止服务
sudo systemctl stop war3bot
```

### 4. 编辑 logind 配置
```bash
sudo nano /etc/systemd/logind.conf

KillUserProcesses=yes

sudo systemctl restart systemd-logind

sudo timeout 30m journalctl -u war3bot -f

alias wlog="sudo pkill -f 'journalctl -u war3bot'; sudo timeout 1h journalctl -u war3bot -f"
```
---

## 💻 使用与管理

### 1. 常用后台管理命令

```bash
# 命令行运行1
sudo /root/War3Bot/build/War3Bot -a create bot

# 命令行运行2
sudo /root/War3Bot/build/War3Bot -a

# 查看服务状态
sudo systemctl status war3bot

# 查看实时日志
sudo journalctl -u war3bot -f

# 杀死所有相关进程
pkill -f War3Bot

# 强制杀死所有相关进程
pkill -9 -f War3Bot
```

### 2. 🎮 交互式控制台模式 (手动发送指令)

Systemd 后台服务无法接收键盘输入。如果您需要使用 **`connect`**、**`create`** 或 **`stop`** 等控制台命令，必须停止后台服务并在前台手动运行程序。

**操作步骤：**

1.  **停止后台服务** (必须执行，否则端口会被占用)：
    ```bash
    sudo systemctl stop war3bot
    ```

2.  **进入编译目录**：
    ```bash
    cd ~/War3Bot/build
    ```

3.  **前台启动程序**：
    ```bash
    ./War3Bot
    ```

4.  **输入指令**：
    等待出现 `[INFO] === 服务器启动完成，开始监听 ===` 后，直接在终端输入命令并回车。

    *   **连接战网**：
        ```text
        connect
        # 或手动指定参数
        connect 127.0.0.1 6112 myuser mypass
        ```
    *   **创建游戏**：
        ```text
        create Dota_6.83_CN
        # 或带密码
        create Dota_VIP 123
        ```
    *   **停止游戏**：
        ```text
        stop
        ```

### 3. 进程与端口监控

```bash
# 查看进程详情
ps -ef | grep War3Bot

# 查看端口监听状态 (6112)
ss -tulpn | grep :6112
# 或者
netstat -tulpn | grep 6112

# 开放 TCP 范围
sudo ufw allow 6113:7113/tcp

# 开放 UDP 范围
sudo ufw allow 6113:7113/udp

# 重新加载让配置生效
sudo ufw reload

# 检查一下
sudo ufw status

# 查看所有正在监听的 TCP 和 UDP 端口，并显示进程名
sudo ss -tulnp
```
---

## 🛡️ 防火墙配置

War3Bot 需要同时开放 TCP 和 UDP 的 6112 端口。

### 使用 UFW (Ubuntu 默认)

```bash
sudo ufw allow 6112/tcp
sudo ufw allow 6112/udp
sudo ufw status
```

### 使用 Firewalld (CentOS/RHEL)

```bash
# 永久开放端口
sudo firewall-cmd --add-port=6112/tcp --permanent
sudo firewall-cmd --add-port=6112/udp --permanent
sudo firewall-cmd --reload

# 验证配置
sudo firewall-cmd --list-ports
```

---

## 🧪 测试与验证

### 1. 基础连通性测试 (Linux)

```bash
# 检查本地端口是否监听
sudo netstat -tulpn | grep 6112

# 发送 UDP 测试包
echo "test" | nc -u localhost 6112

# 抓包监控流量
sudo tcpdump -i any -n udp port 6112
```

### 2. 远程连接测试 (Windows Client)

在 Windows 客户端机器上验证到服务器的连接。

```powershell
# 使用 PowerShell 测试 TCP 连接
Test-NetConnection <服务器IP> -Port 6112

# CMD: 跟踪路由
tracert <服务器IP>

# CMD: 查看本地 6112 端口占用
netstat -ano | findstr 6112
```

### 3. Python 模拟测试脚本

保存为 `test_bot.py` 并运行：

```python
#!/usr/bin/env python3
import socket
import struct

def test_War3Bot():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # 请修改为实际服务器地址
    War3Bot_addr = ('localhost', 6112)
    
    # 创建 W3GS PING 数据包 (Header: 0xF7)
    # 结构: Header(1B) + Length(2B) + Type(1B) + Data
    header = struct.pack('<BHHB', 0xF7, 8, 0x01, 0)
    
    try:
        sock.sendto(header, War3Bot_addr)
        print(f"测试数据包已发送至 {War3Bot_addr}")
    except Exception as e:
        print(f"发送失败: {e}")
    finally:
        sock.close()

if __name__ == "__main__":
    test_War3Bot()
```

---

## 📂 项目结构

```text
War3Bot/
├── CMakeLists.txt          # CMake 构建配置
|── bncsutil/               # bncsutil 目录
|── war3files/              # war3 文件目录
├── include/                # 头文件目录
│   ├── bigint.h            # 大数计算
│   ├── bitrotate.h         # 字节反转
│   ├── bnethash.h          # 哈希计算
│   ├── bnetsrp3.h          # srp登录
│   |── botmanager.h        # 机器人管理
│   |── calculate.h         # 相关计算
│   ├── client.h            # 连接战网
│   ├── command.h           # 内部命令
│   ├── dbmanager.h         # 数据库管理
│   ├── ingame.h            # 游戏数据
│   ├── logger.h            # 日志系统
│   ├── netmanager.h        # 网络连接
│   |── protocol.h          # 协议定义
│   ├── securitywatchdog.h  # 看门狗
│   ├── war3bot.h           # 机器人
│   └── war3map.h			# war3地图
├── src/                    # 源代码目录
│   ├── bigint.cpp          # 大数计算
│   ├── bnethash.cpp        # 哈希计算
│   ├── bnetsrp3.cpp        # srp登录
│   |── botmanager.cpp      # 机器人管理
│   |── calculate.cpp       # 相关计算
│   ├── client.cpp          # 连接战网
│   ├── command.cpp         # 内部命令
│   ├── dbmanager.cpp       # 数据库管理
│   ├── ingame.cpp          # 游戏数据
│   ├── logger.cpp          # 日志系统
│   ├── main.cpp            # 主函数
│   ├── netmanager.cpp      # 网络连接
│   ├── securitywatchdog.cpp# 看门狗
│   ├── war3bot.cpp         # 机器人
│   └── war3map.cpp			# war3地图
|── lib/                    # 库目录
└── config/                 # 配置相关
    ├── war3bot.ini         # 配置文件模板
    └── war3bot.service     # Systemd 服务文件
```

---

## 地图下载

1.  **安装 Nginx**:
    ```bash
    sudo apt update
    sudo apt install nginx
    ```

2.  **配置静态目录**:
    编辑配置：`sudo nano /etc/nginx/sites-available/default`
    在 `server` 块中添加一段逻辑：
    ```nginx
    server {
        listen 80; # 监听 80 端口
        server_name _;

        # 核心配置：将 /maps/ 这个 URL 路径指向你磁盘的物理路径
        location /maps/ {
            alias /opt/War3Bot/war3files/maps/;
            autoindex on;       # 允许查看文件列表（可选，方便调试）
            access_log off;     # 减少日志，提高性能
        }
    }
    ```

3.  **设置权限**:
    确保 Nginx 的用户（通常是 `www-data`）有权读取该目录：
    ```bash
    sudo chmod -R 755 /opt/War3Bot/war3files/maps/
    ```

4.  **重启服务**:
    ```bash
    sudo systemctl restart nginx
    ```

---
## 📚 协议与内部命令

### 支持的命令行 (CMD)

**创建游戏：**
```bash
# 注意：如果参数包含空格，请使用引号
create Dota
create "Dota 6.83"
sudo ./war3bot -x "create 'Dota 6.83' bot1"
```

**取消游戏：**
```bash
# 注意：如果参数包含空格，请使用引号
cancel Dota
cancel "Dota 6.83"
sudo ./war3bot -x "cancel 'Dota 6.83' bot1"
```

**连接服务器：**
```bash
# 注意：如果留空则是用默认配置
connect username [default] [default] [default]
connect username password ip port
sudo ./war3bot -x "connect bot1 123456 127.0.0.1"
```

**停止广播：**
```bash
stop
sudo ./war3bot -x "stop"
```

### 支持的数据包 (SID & W3GS)

```cpp
// TCP 协议 ID (BNET)
enum BNETPacketID {
    SID_NULL                    = 0x00, // [C->S] 空包
    SID_STOPADV                 = 0x02, // [C->S] 停止广播
    SID_ENTERCHAT               = 0x0A, // [C->S] 进入聊天
    SID_GETCHANNELLIST          = 0x0B, // [C->S] 获取频道列表
    SID_JOINCHANNEL             = 0x0C, // [C->S] 加入频道
    SID_CHATCOMMAND             = 0x0E, // [C->S] 聊天命令
    SID_CHATEVENT               = 0x0F, // [C->S] 聊天事件
    SID_STARTADVEX3             = 0x1C, // [C->S] 创建房间(TFT)
    SID_PING                    = 0x25, // [C->S] 心跳包
    SID_LOGONRESPONSE           = 0x29, // [C->S] 登录响应(旧)
    SID_LOGONRESPONSE2          = 0x3A, // [C->S] 登录响应(中)
    SID_NETGAMEPORT             = 0x45, // [C->S] 游戏端口通知
    SID_AUTH_INFO               = 0x50, // [C->S] 认证信息
    SID_AUTH_CHECK              = 0x51, // [C->S] 版本检查
    SID_AUTH_ACCOUNTCREATE      = 0x52, // [C->S] 账号创建
    SID_AUTH_ACCOUNTLOGON       = 0x53, // [C->S] SRP登录请求
    SID_AUTH_ACCOUNTLOGONPROOF  = 0x54  // [C->S] SRP登录验证
};

// W3GS 协议 ID 定义 (TCP 游戏逻辑 & UDP 局域网广播)
enum W3GSPacketID {
    // --- 基础握手与心跳 (TCP) ---
    W3GS_PING_FROM_HOST         = 0x01, // [S->C] 主机发送的心跳包 (每30秒)
    W3GS_PONG_TO_HOST           = 0x46, // [C->S] 客户端回复的心跳包

    // --- 房间/大厅信息 (TCP) ---
    W3GS_SLOTINFOJOIN           = 0x04, // [S->C] 玩家加入时发送的完整槽位信息
    W3GS_REJECTJOIN             = 0x05, // [S->C] 拒绝加入 (房间满/游戏开始)
    W3GS_PLAYERINFO             = 0x06, // [S->C] 玩家详细信息 (PID, IP, 端口等)
    W3GS_PLAYERLEFT             = 0x07, // [S->C] 通知某玩家离开
    W3GS_PLAYERLOADED           = 0x08, // [S->C] 通知某玩家加载完毕
    W3GS_SLOTINFO               = 0x09, // [S->C] 槽位状态更新 (Slot Update)
    W3GS_REQJOIN                = 0x1E, // [C->S] 客户端请求加入房间
    W3GS_LEAVEREQ               = 0x21, // [C->S] 客户端请求离开
    W3GS_GAMELOADED_SELF        = 0x23, // [C->S] 客户端通知自己加载完毕
    W3GS_CLIENTINFO             = 0x37, // [C->S] 客户端发送自身信息 (端口等)
    W3GS_LEAVERS                = 0x1B, // [S->C] 离开者列表 / 踢人响应 (部分版本)
    W3GS_HOST_KICK_PLAYER       = 0x1C, // [S->C] 主机踢人

    // --- 游戏流程控制 (TCP) ---
    W3GS_COUNTDOWN_START        = 0x0A, // [S->C] 开始倒计时
    W3GS_COUNTDOWN_END          = 0x0B, // [S->C] 倒计时结束 (游戏开始)
    W3GS_START_LAG              = 0x10, // [S->C] 开始等待界面 (有人卡顿)
    W3GS_STOP_LAG               = 0x11, // [S->C] 停止等待界面
    W3GS_OUTGOING_KEEPALIVE     = 0x27, // [C->S] 客户端保持连接
    W3GS_DROPREQ                = 0x29, // [C->S] 请求断开连接 (掉线/强退)

    // --- 游戏内动作与同步 (TCP) ---
    W3GS_INCOMING_ACTION        = 0x0C, // [S->C] 广播玩家动作 (核心同步包)
    W3GS_OUTGOING_ACTION        = 0x26, // [C->S] 客户端发送动作 (点击/技能)
    W3GS_INCOMING_ACTION2       = 0x48, // [S->C] 扩展动作包 (通常用于录像/裁判)

    // --- 聊天系统 (TCP) ---
    W3GS_CHAT_FROM_HOST         = 0x0F, // [S->C] 主机转发的聊天消息
    W3GS_CHAT_TO_HOST           = 0x28, // [C->S] 客户端发送的聊天消息

    // --- 地图下载与校验 (TCP) ---
    W3GS_MAPCHECK               = 0x3D, // [S->C] 主机发起地图校验请求
    W3GS_STARTDOWNLOAD          = 0x3F, // [S->C] 主机通知开始下载 / [C->S] 客户端请求开始
    W3GS_MAPSIZE                = 0x42, // [C->S] 客户端报告地图大小/CRC状态
    W3GS_MAPPART                = 0x43, // [S->C] 地图文件分片数据
    W3GS_MAPPARTOK              = 0x44, // [C->S] 客户端确认收到分片 (ACK)
    W3GS_MAPPARTNOTOK           = 0x45, // [C->S] 客户端报告分片错误 (NACK)

    // --- 局域网发现与 P2P (UDP) ---
    W3GS_SEARCHGAME             = 0x2F, // [UDP] 搜索局域网游戏
    W3GS_GAMEINFO               = 0x30, // [UDP] 游戏信息广播 (Game Info)
    W3GS_CREATEGAME             = 0x31, // [UDP] 创建游戏广播
    W3GS_REFRESHGAME            = 0x32, // [UDP] 刷新游戏信息 (Refresh)
    W3GS_DECREATEGAME           = 0x33, // [UDP] 取消/销毁游戏
    W3GS_PING_FROM_OTHERS       = 0x35, // [UDP] P2P Ping
    W3GS_PONG_TO_OTHERS         = 0x36, // [UDP] P2P Pong
    W3GS_TEST                   = 0x88  // [UDP] 测试/私有包
};
```

---

## 🗑️ 卸载指南 (Ubuntu)

如果需要移除开发环境和 War3Bot：

```bash
# 1. 停止服务
sudo systemctl stop war3bot
sudo systemctl disable war3bot
sudo rm /etc/systemd/system/war3bot.service
sudo systemctl daemon-reload

# 2. 删除文件
sudo rm -rf /etc/War3Bot /var/log/War3Bot /opt/War3Bot

# 3. 移除依赖库 (可选)
sudo apt remove qtbase5-dev qt5-qmake libqt5core5a libqt5network5
sudo apt autoremove
```

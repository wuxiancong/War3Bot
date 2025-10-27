# War3Bot

War3Bot 是一个专为《魔兽争霸 III》设计的游戏会话代理服务器，基于 C++ 和 Qt 框架开发。

## 功能特性

- 完整的 W3GS 协议支持
- 双向数据转发 (C->S 和 S->C)
- 多会话管理
- 玩家状态跟踪
- 高性能异步网络处理

## 快速安装

### Ubuntu 系统

```bash
# 1. 安装依赖
sudo apt update
sudo apt install -y build-essential cmake
sudo apt install qtbase5-dev qt5-qmake libqt5core5a libqt5network5

# 2. 克隆项目
git clone https://github.com/wuxiancong/War3Bot.git
cd War3Bot

# 3. 编译安装
mkdir build && cd build
cmake ..
make -j$(nproc)

# 4. 测试运行
./war3bot --help

# 5. 重新编译
cd /root/War3Bot/build
rm -rf *
cd ~
cd War3Bot
rm -rf *

```
##系统服务配置
# 创建系统用户
```bash
sudo useradd -r -s /bin/false -d /opt/war3bot war3bot
```
# 创建目录
```bash
sudo mkdir -p /var/log/war3bot /etc/war3bot
sudo chown -R war3bot:war3bot /var/log/war3bot
```
# 配置服务
war3bot.service:
```bash
[Unit]
Description=War3Bot Warcraft III Proxy
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/root/War3Bot/build
ExecStart=/root/War3Bot/build/war3bot -p 6113
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable war3bot
sudo systemctl start war3bot
```
##配置文件
/etc/war3bot/war3bot.ini:
```bash
[server]
port=6113
max_sessions=100
ping_interval=30000

[game]
host=127.0.0.1
port=6112

[logging]
level=info
file=/var/log/war3bot/war3bot.log
```

##使用方法
#命令行运行
```bash
# 启动服务
sudo systemctl start war3bot
```
```bash
# 查看状态
sudo systemctl status war3bot
```
```bash
# 查看日志
sudo journalctl -u war3bot -f
```

##测试验证
#基本测试
```bash
# 检查端口
sudo netstat -tulpn | grep 6113
```
```bash
# 发送测试数据
echo "test" | nc -u localhost 6113
```
```bash
# 监控流量
sudo tcpdump -i lo -n udp port 6112 or port 6113
```
#Python 测试客户端
```bash
#!/usr/bin/env python3
import socket
import struct

def test_war3bot():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    war3bot_addr = ('localhost', 6113)
    
    # 创建 W3GS PING 数据包
    header = struct.pack('<BHHB', 0xF7, 8, 0x01, 0)
    sock.sendto(header, war3bot_addr)
    print("测试数据包已发送")

if __name__ == "__main__":
    test_war3bot()

```
##项目结构
```bash
War3Bot/
├── CMakeLists.txt
├── include/
│   ├── war3bot.h
│   ├── gamesession.h
│   ├── w3gs_protocol.h
│   └── logger.h
├── src/
│   ├── main.cpp
│   ├── war3bot.cpp
│   ├── gamesession.cpp
│   ├── w3gs_protocol.cpp
│   └── logger.cpp
└── config/
    ├── war3bot.ini
    └── war3bot.service
```

##故障排查
```bash
# 检查服务状态
sudo systemctl status war3bot

# 查看详细日志
sudo journalctl -u war3bot --no-pager -n 50

# 检查防火墙
sudo ufw status
sudo ufw allow 6113/udp

# 调试模式运行
./war3bot -l debug -p 6113
```

##协议支持
#C->S 数据包
- 0x01 - PING_FROM_HOST

- 0x04 - SLOT_INFOJOIN

- 0x0F - CHAT_TO_HOST

- 0x11 - LEAVE_GAME

- 0x0A - INCOMING_ACTION

#S->C 数据包
- 0x02 - PONG_TO_HOST

- 0x03 - REJECT

- 0x08 - SLOT_INFO

- 0x18 - PLAYER_LEFT

- 0x0E - CHAT_FROM_HOST

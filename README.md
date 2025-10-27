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
# 安装依赖
sudo apt update
sudo apt install -y build-essential cmake qt5-default qtbase5-dev

# 编译安装
git clone https://github.com/wuxiancong/War3Bot.git
cd War3Bot
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install

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


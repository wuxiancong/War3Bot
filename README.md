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

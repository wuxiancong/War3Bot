#!/bin/bash

# ==========================================
#  War3Bot 编译安装与安全配置脚本 (修正版)
# ==========================================

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 安装路径配置
INSTALL_PREFIX="/opt/War3Bot"
CONFIG_DIR="/etc/War3Bot"
LOG_DIR="/var/log/War3Bot"
SERVICE_NAME="war3bot"
USER_NAME="war3bot"
SERVICE_PORT=6116  # 默认监听端口

info() { echo -e "${GREEN}[INFO] $1${NC}"; }
error() { echo -e "${RED}[ERROR] $1${NC}"; exit 1; }
warn()  { echo -e "${YELLOW}[WARN] $1${NC}"; }

# 1. 权限检查
if [ "$EUID" -ne 0 ]; then
    error "请使用 sudo 或 root 用户运行此脚本！"
fi

# 2. 依赖安装
info "检查编译依赖..."
if [ -f /etc/debian_version ]; then
    apt-get update
    apt-get install -y git cmake build-essential qtbase5-dev libqt5network5
elif [ -f /etc/redhat-release ]; then
    yum groupinstall -y "Development Tools"
    yum install -y git cmake qt5-qtbase-devel
fi

# 3. 代码更新
if [ -d ".git" ]; then
    info "正在拉取最新代码..."
    git pull || warn "代码更新失败，将尝试使用当前代码编译..."
fi

# 4. 准备构建目录
info "清理旧构建..."
rm -rf build
mkdir build
cd build

# 5. 编译与安装
info "CMake 配置..."
cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" .. || error "CMake 配置失败"

info "开始编译..."
make -j$(nproc) || error "编译失败"

info "正在安装二进制文件..."
make install || error "安装失败"

# ==========================================
#  ⚙️ 系统配置与安全加固
# ==========================================

info "正在配置系统用户与目录权限..."

# 6. 创建专用系统用户
if ! id "$USER_NAME" &>/dev/null; then
    info "创建系统用户: $USER_NAME"
    useradd -r -s /bin/false -d "$INSTALL_PREFIX" "$USER_NAME"
fi

# 7. 创建标准目录结构
mkdir -p "$INSTALL_PREFIX/war3files"
mkdir -p "$CONFIG_DIR"
mkdir -p "$LOG_DIR"

# 8. 同步资源文件
cd .. 
if [ -d "war3files" ]; then
    info "同步 war3files 到安装目录..."
    cp -r war3files/* "$INSTALL_PREFIX/war3files/"
else
    warn "源码目录下未找到 war3files 目录"
fi

# 9. 设置权限 (⚡️ 重点修复区域)
info "强制修复权限..."

# A. 程序目录
chown -R $USER_NAME:$USER_NAME "$INSTALL_PREFIX"
chmod -R 755 "$INSTALL_PREFIX"

# B. 配置目录
chown -R $USER_NAME:$USER_NAME "$CONFIG_DIR"
chmod 700 "$CONFIG_DIR"
if [ -f "$CONFIG_DIR/war3bot.ini" ]; then
    chmod 600 "$CONFIG_DIR/war3bot.ini"
fi

# C. 日志目录 (⚡️ 修复 Permission denied)
# 先尝试强制修改所有现有日志文件的所有者
if [ -d "$LOG_DIR" ]; then
    chown -R $USER_NAME:$USER_NAME "$LOG_DIR"
    chmod -R 750 "$LOG_DIR"
    
    # 如果存在 root 拥有的旧日志且无法修改，直接删除让程序重建
    # rm -f "$LOG_DIR/war3bot.log" 
fi

# ==========================================
#  📝 生成 Systemd 服务文件
# ==========================================
info "更新 Systemd 服务配置..."

SERVICE_FILE="/etc/systemd/system/$SERVICE_NAME.service"

# ✅ 修复点：ExecStart 路径增加了 /bin
cat > $SERVICE_FILE <<EOF
[Unit]
Description=War3Bot Hosting Service
After=network.target

[Service]
Type=simple
User=$USER_NAME
Group=$USER_NAME
WorkingDirectory=$INSTALL_PREFIX

# ⚡️ 修正路径：/opt/War3Bot/bin/War3Bot
ExecStart=$INSTALL_PREFIX/bin/War3Bot --config $CONFIG_DIR/war3bot.ini -p $SERVICE_PORT

Restart=always
RestartSec=10
ProtectSystem=full
ProtectHome=true

[Install]
WantedBy=multi-user.target
EOF

# ==========================================
#  服务重启
# ==========================================

echo -e "${BLUE}==============================================${NC}"
echo -e "${BLUE}   安装完成，正在重启服务...${NC}"
echo -e "${BLUE}==============================================${NC}"

# 10. 停止旧进程
pkill -9 -f War3Bot || true

# 11. 刷新并重启
systemctl daemon-reload
systemctl enable $SERVICE_NAME

info "启动 $SERVICE_NAME ..."
if systemctl restart $SERVICE_NAME; then
    echo -e "${GREEN}✅ 服务启动成功！${NC}"
    echo -e "   ├─ 执行文件: $INSTALL_PREFIX/bin/War3Bot"
    echo -e "   ├─ 配置文件: $CONFIG_DIR/war3bot.ini"
    echo -e "   └─ 日志文件: $LOG_DIR/war3bot.log"
else
    error "❌ 服务启动失败，请检查: sudo systemctl status $SERVICE_NAME"
fi

# 12. 查看日志
echo -e "${YELLOW}正在打开实时日志 (按 Ctrl+C 退出)...${NC}"
echo ""
sleep 2
journalctl -u $SERVICE_NAME -f
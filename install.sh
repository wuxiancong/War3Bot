#!/bin/bash

# ==========================================
#  War3Bot 编译安装脚本
# ==========================================

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

INSTALL_PREFIX="/usr/local/War3Bot"
SERVICE_NAME="war3bot"

info() { echo -e "${GREEN}[INFO] $1${NC}"; }
error() { echo -e "${RED}[ERROR] $1${NC}"; exit 1; }
warn()  { echo -e "${YELLOW}[WARN] $1${NC}"; }

# 1. 权限检查
if [ "$EUID" -ne 0 ]; then
    warn "建议使用 sudo 或 root 用户运行"
    SUDO="sudo"
else
    SUDO=""
fi

# 2. 依赖安装 (Ubuntu/CentOS)
info "检查编译依赖..."
if [ -f /etc/debian_version ]; then
    $SUDO apt-get update
    $SUDO apt-get install -y git cmake build-essential qtbase5-dev libqt5network5
elif [ -f /etc/redhat-release ]; then
    $SUDO yum groupinstall -y "Development Tools"
    $SUDO yum install -y git cmake qt5-qtbase-devel
fi

# 3. 代码更新 (修正点：不再克隆，仅更新)
if [ -d ".git" ]; then
    info "正在拉取最新代码..."
    git pull || warn "代码更新失败，将尝试使用当前代码编译..."
else
    warn "当前不是 Git 仓库（可能是 ZIP 下载），跳过更新，直接编译..."
fi

# 4. 准备构建目录
info "清理旧构建..."
if [ -d "build" ]; then
    rm -rf build
fi
mkdir build
cd build

# 5. 编译与安装
info "CMake 配置..."
cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" .. || error "CMake 配置失败"

info "开始编译..."
make -j$(nproc) || error "编译失败"

info "正在安装..."
$SUDO make install || error "安装失败"

# ==========================================
#  服务重启与日志
# ==========================================

echo -e "${BLUE}==============================================${NC}"
echo -e "${BLUE}   安装完成，正在重启服务...${NC}"
echo -e "${BLUE}==============================================${NC}"

# 6. 杀掉旧进程 (防止文件占用或逻辑冲突)
info "停止旧进程..."
$SUDO pkill -9 -f War3Bot || true

# 7. 刷新 Systemd
info "刷新服务配置..."
$SUDO systemctl daemon-reload

# 8. 启动服务
info "启动 $SERVICE_NAME ..."
if $SUDO systemctl start $SERVICE_NAME; then
    echo -e "${GREEN}✅ 服务启动成功！${NC}"
else
    error "❌ 服务启动失败，请检查: sudo systemctl status $SERVICE_NAME"
fi

# 9. 查看日志
echo -e "${YELLOW}正在打开实时日志 (按 Ctrl+C 退出)...${NC}"
echo ""
sleep 1
$SUDO journalctl -u $SERVICE_NAME -f
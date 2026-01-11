#!/bin/bash

# ==========================================
#  War3Bot 目录内专用安装脚本
#  功能：拉取代码 -> 编译 -> 安装 -> 重启服务 -> 看日志
# ==========================================

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

REPO_URL="https://github.com/wuxiancong/War3Bot.git"
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
info "检查依赖环境..."
if [ -f /etc/debian_version ]; then
    $SUDO apt-get update
    $SUDO apt-get install -y git cmake build-essential qtbase5-dev libqt5network5
elif [ -f /etc/redhat-release ]; then
    $SUDO yum groupinstall -y "Development Tools"
    $SUDO yum install -y git cmake qt5-qtbase-devel
fi

# 3. 代码同步
if [ -d ".git" ]; then
    info "当前已是 Git 仓库，执行更新..."
    git pull || error "代码更新失败"
else
    info "当前目录未初始化，尝试克隆代码..."
    git init
    git remote add origin "$REPO_URL"
    git fetch
    git checkout -t origin/master -f || error "代码检出失败"
fi

# 4. 准备构建目录
info "准备构建..."
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
#  服务重启与日志查看逻辑
# ==========================================

echo -e "${BLUE}==============================================${NC}"
echo -e "${BLUE}   编译安装完成，正在重启服务...${NC}"
echo -e "${BLUE}==============================================${NC}"

# 6. 清理旧进程
# 使用 || true 防止因为找不到进程导致脚本报错退出
info "清理旧进程..."
$SUDO pkill -9 -f War3Bot || true

# 7. 刷新 Systemd 配置 (防止服务文件变更未生效)
info "刷新系统服务配置..."
$SUDO systemctl daemon-reload

# 8. 启动服务
info "启动 $SERVICE_NAME 服务..."
if $SUDO systemctl start $SERVICE_NAME; then
    echo -e "${GREEN}✅ 服务启动成功！${NC}"
else
    error "❌ 服务启动失败，请检查 systemctl status $SERVICE_NAME"
fi

# 9. 查看日志
echo -e "${YELLOW}正在打开日志监控 (按 Ctrl+C 退出)...${NC}"
echo ""
sleep 1
$SUDO journalctl -u $SERVICE_NAME -f
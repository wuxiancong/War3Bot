#!/bin/bash

# ==========================================
#  War3Bot 编译安装与安全配置脚本
# ==========================================

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# ✅ 修改：遵循最佳实践，安装到 /opt
INSTALL_PREFIX="/opt/War3Bot"
CONFIG_DIR="/etc/War3Bot"
LOG_DIR="/var/log/War3Bot"
SERVICE_NAME="war3bot"
USER_NAME="war3bot"

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

# 2. 依赖安装
info "检查编译依赖..."
if [ -f /etc/debian_version ]; then
    $SUDO apt-get update
    $SUDO apt-get install -y git cmake build-essential qtbase5-dev libqt5network5
elif [ -f /etc/redhat-release ]; then
    $SUDO yum groupinstall -y "Development Tools"
    $SUDO yum install -y git cmake qt5-qtbase-devel
fi

# 3. 代码更新
if [ -d ".git" ]; then
    info "正在拉取最新代码..."
    git pull || warn "代码更新失败，将尝试使用当前代码编译..."
else
    warn "当前不是 Git 仓库，跳过更新，直接编译..."
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

info "正在安装二进制文件..."
$SUDO make install || error "安装失败"

# ==========================================
#  ⚙️ 系统配置与安全加固 (新增核心部分)
# ==========================================

info "正在配置系统用户与目录权限..."

# 6. 创建专用系统用户 (如果不存在)
if ! id "$USER_NAME" &>/dev/null; then
    info "创建系统用户: $USER_NAME"
    $SUDO useradd -r -s /bin/false -d "$INSTALL_PREFIX" "$USER_NAME"
fi

# 7. 创建标准目录结构
$SUDO mkdir -p "$INSTALL_PREFIX/war3files"
$SUDO mkdir -p "$CONFIG_DIR"
$SUDO mkdir -p "$LOG_DIR"

# 8. 同步资源文件 (确保 War3 核心文件存在)
# 回到源码根目录
cd .. 
if [ -d "war3files" ]; then
    info "同步 war3files 到安装目录..."
    $SUDO cp -r war3files/* "$INSTALL_PREFIX/war3files/"
else
    warn "源码目录下未找到 war3files 目录，请手动将 War3.exe 等文件放入 $INSTALL_PREFIX/war3files"
fi

# 9. 设置权限 (安全加固)
info "应用安全权限设置..."

# A. 程序目录: war3bot用户拥有，普通权限
$SUDO chown -R $USER_NAME:$USER_NAME "$INSTALL_PREFIX"
$SUDO chmod -R 755 "$INSTALL_PREFIX"

# B. 配置目录: war3bot用户拥有，仅拥有者可读写 (保护密码)
$SUDO chown -R $USER_NAME:$USER_NAME "$CONFIG_DIR"
$SUDO chmod 700 "$CONFIG_DIR"
# 如果存在配置文件，确保也是 600
if [ -f "$CONFIG_DIR/war3bot.ini" ]; then
    $SUDO chmod 600 "$CONFIG_DIR/war3bot.ini"
fi

# C. 日志目录: war3bot用户拥有，仅拥有者和组可读
$SUDO chown -R $USER_NAME:$USER_NAME "$LOG_DIR"
$SUDO chmod 750 "$LOG_DIR"

# ==========================================
#  📝 生成/更新 Systemd 服务文件
# ==========================================
info "更新 Systemd 服务配置..."

SERVICE_FILE="/etc/systemd/system/$SERVICE_NAME.service"

$SUDO bash -c "cat > $SERVICE_FILE" <<EOF
[Unit]
Description=War3Bot Hosting Service
After=network.target

[Service]
Type=simple
User=$USER_NAME
Group=$USER_NAME
WorkingDirectory=$INSTALL_PREFIX
# 显式指定配置文件路径
ExecStart=$INSTALL_PREFIX/War3Bot --config $CONFIG_DIR/war3bot.ini -p 6116

# 自动重启策略
Restart=always
RestartSec=10

# 安全限制
ProtectSystem=full
ProtectHome=true

[Install]
WantedBy=multi-user.target
EOF

# ==========================================
#  服务重启与日志
# ==========================================

echo -e "${BLUE}==============================================${NC}"
echo -e "${BLUE}   安装完成，正在重启服务...${NC}"
echo -e "${BLUE}==============================================${NC}"

# 10. 停止旧进程
info "停止可能存在的旧进程..."
$SUDO pkill -9 -f War3Bot || true

# 11. 刷新并重启服务
info "刷新 Systemd..."
$SUDO systemctl daemon-reload
$SUDO systemctl enable $SERVICE_NAME

info "启动 $SERVICE_NAME ..."
if $SUDO systemctl restart $SERVICE_NAME; then
    echo -e "${GREEN}✅ 服务启动成功！${NC}"
    echo -e "   ├─ 程序路径: $INSTALL_PREFIX"
    echo -e "   ├─ 配置文件: $CONFIG_DIR/war3bot.ini"
    echo -e "   └─ 日志文件: $LOG_DIR/war3bot.log"
else
    error "❌ 服务启动失败，请检查: sudo systemctl status $SERVICE_NAME"
fi

# 12. 查看日志
echo -e "${YELLOW}正在打开实时日志 (按 Ctrl+C 退出)...${NC}"
echo ""
sleep 2
$SUDO journalctl -u $SERVICE_NAME -f
#!/bin/bash

# ==========================================
#  War3Bot 编译安装与安全配置脚本 (交互增强版)
# ==========================================

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# --- 基础路径配置 ---
INSTALL_PREFIX="/opt/War3Bot"
CONFIG_DIR="/etc/War3Bot"
LOG_DIR="/var/log/War3Bot"
SERVICE_NAME="war3bot"
USER_NAME="war3bot"
SERVICE_PORT=6116

info() { echo -e "${GREEN}[INFO] $1${NC}"; }
error() { echo -e "${RED}[ERROR] $1${NC}"; exit 1; }
warn()  { echo -e "${YELLOW}[WARN] $1${NC}"; }

# 1. 权限检查
if [ "$EUID" -ne 0 ]; then
    error "请使用 sudo 或 root 用户运行此脚本！"
fi

# ==========================================
#  ✨ 新增：交互式字段设置
# ==========================================
echo -e "${BLUE}==============================================${NC}"
echo -e "${BLUE}        War3Bot 自动化配置工具                ${NC}"
echo -e "${BLUE}==============================================${NC}"

# 询问 list_number
read -p "请输入要使用的机器人列表编号 (list_number) [默认: 1]: " INPUT_LIST_NUMBER
BOT_LIST_NUMBER=${INPUT_LIST_NUMBER:-"1"}

# 询问 display_name
read -p "请输入机器人显示名称 (display_name) [默认: CC.Dota.XX]: " INPUT_DISPLAY_NAME
BOT_DISPLAY_NAME=${INPUT_DISPLAY_NAME:-"CC.Dota.XX"}

echo -e "${GREEN}配置已确认: 编号=$BOT_LIST_NUMBER, 名称=$BOT_DISPLAY_NAME${NC}"
echo ""

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

# ==========================================
#  📝 配置文件处理逻辑
# ==========================================
INI_FILE="$CONFIG_DIR/war3bot.ini"

if [ ! -f "$INI_FILE" ]; then
    info "配置文件不存在，正在创建默认配置..."
    cat > "$INI_FILE" <<EOF
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
log_file=$LOG_DIR/war3bot.log
max_size=10
backup_count=5

[bnet]
server=139.155.155.166
port=6112

[bots]
list_number=$BOT_LIST_NUMBER
init_count=10
auto_generate=false
display_name=$BOT_DISPLAY_NAME
EOF
else
    info "配置文件已存在，正在更新字段: list_number=$BOT_LIST_NUMBER, display_name=$BOT_DISPLAY_NAME"
    # 使用 sed 修改现有文件
    sed -i "s/^list_number=.*/list_number=$BOT_LIST_NUMBER/" "$INI_FILE"
    sed -i "s/^display_name=.*/display_name=$BOT_DISPLAY_NAME/" "$INI_FILE"
fi

# ==========================================
#  🛡️ 权限修复
# ==========================================
info "强制修复权限..."

chown -R $USER_NAME:$USER_NAME "$INSTALL_PREFIX"
chmod -R 755 "$INSTALL_PREFIX"

chown -R $USER_NAME:$USER_NAME "$CONFIG_DIR"
chmod 755 "$CONFIG_DIR"
if [ -f "$INI_FILE" ]; then
    chmod 644 "$INI_FILE"
fi

if [ -d "$LOG_DIR" ]; then
    chown -R $USER_NAME:$USER_NAME "$LOG_DIR"
    chmod -R 750 "$LOG_DIR"
fi

# ==========================================
#  📝 生成 Systemd 服务文件
# ==========================================
info "更新 Systemd 服务配置..."

SERVICE_FILE="/etc/systemd/system/$SERVICE_NAME.service"

cat > $SERVICE_FILE <<EOF
[Unit]
Description=War3Bot Hosting Service
After=network.target

[Service]
Type=simple
User=$USER_NAME
Group=$USER_NAME
WorkingDirectory=$INSTALL_PREFIX
ExecStart=$INSTALL_PREFIX/bin/War3Bot --config $INI_FILE -p $SERVICE_PORT
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

# 10. 强制清理残留进程
pkill -9 -f War3Bot || true

# 11. 刷新并重启
systemctl daemon-reload
systemctl enable $SERVICE_NAME

info "启动 $SERVICE_NAME ..."
if systemctl restart $SERVICE_NAME; then
    echo -e "${GREEN}✅ 服务启动成功！${NC}"
    echo -e "   ├─ 执行文件: $INSTALL_PREFIX/bin/War3Bot"
    echo -e "   ├─ 配置文件: $INI_FILE"
    echo -e "   └─ 当前配置: list_number=$BOT_LIST_NUMBER, display_name=$BOT_DISPLAY_NAME"
else
    error "❌ 服务启动失败，请检查: sudo systemctl status $SERVICE_NAME"
fi

# 12. 查看日志
echo -e "${YELLOW}正在打开实时日志 (按 Ctrl+C 退出)...${NC}"
echo ""
sleep 2
journalctl -u $SERVICE_NAME -f
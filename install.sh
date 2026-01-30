#!/bin/bash

# ==========================================
#  War3Bot 自动化安装与多路径配置同步脚本
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

# 0. 自动修复脚本自身的 Windows 换行符 (针对本地上传)
if [[ $(cat -v $0 | grep -c "\^M") -gt 0 ]]; then
    warn "检测到 Windows 换行符，正在自动修复并重启脚本..."
    sed -i 's/\r$//' "$0"
    exec bash "$0" "$@"
fi

# 1. 权限检查
if [ "$EUID" -ne 0 ]; then
    error "请使用 sudo 或 root 用户运行此脚本！"
fi

# ==========================================
#  ✨ 步骤 A: 交互式获取参数
# ==========================================
echo -e "${BLUE}==============================================${NC}"
echo -e "${BLUE}        War3Bot 自动化配置 (交互版)           ${NC}"
echo -e "${BLUE}==============================================${NC}"

read -p "请输入服务器准备使用的机器人列表编号 (list_number) [默认: 1]: " INPUT_LIST_NUMBER
BOT_LIST_NUMBER=${INPUT_LIST_NUMBER:-"1"}

read -p "请输入机器人显示名称 (display_name) [默认: CC.Dota.US1]: " INPUT_DISPLAY_NAME
BOT_DISPLAY_NAME=${INPUT_DISPLAY_NAME:-"CC.Dota.XXX"}

info "设置确认: 编号=$BOT_LIST_NUMBER, 名称=$BOT_DISPLAY_NAME"
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

# 3. 准备构建
if [ -d ".git" ]; then
    info "更新源码..."
    git pull || warn "Git pull 失败，尝试本地代码直接编译"
fi

info "清理并准备构建目录..."
rm -rf build
mkdir build
cd build

# 4. 编译与安装
info "CMake 配置中..."
cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" .. || error "CMake 失败"

info "开始多线程编译..."
make -j$(nproc) || error "编译失败"

info "执行安装..."
make install || error "安装失败"

# ==========================================
#  ⚙️ 步骤 B: 配置同步更新 (关键部分)
# ==========================================
cd .. # 回到源码根目录
info "正在同步更新所有路径下的配置文件..."

# 定义所有需要修改的路径
ETC_INI="$CONFIG_DIR/war3bot.ini"
BUILD_INI="./build/config/war3bot.ini"
SOURCE_INI="./config/war3bot.ini"

# 如果系统配置不存在，则创建默认模板
if [ ! -f "$ETC_INI" ]; then
    mkdir -p "$CONFIG_DIR"
    cat > "$ETC_INI" <<EOF
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
list_number=1
init_count=10
auto_generate=false
display_name=CC.Dota.XX
EOF
fi

# 统一更新所有存在的 INI 文件
TARGET_FILES=("$ETC_INI" "$BUILD_INI" "$SOURCE_INI")

for FILE_PATH in "${TARGET_FILES[@]}"; do
    if [ -f "$FILE_PATH" ]; then
        # 修复可能存在的换行符问题
        sed -i 's/\r$//' "$FILE_PATH"
        # 更新字段
        sed -i "s/^list_number=.*/list_number=$BOT_LIST_NUMBER/" "$FILE_PATH"
        sed -i "s/^display_name=.*/display_name=$BOT_DISPLAY_NAME/" "$FILE_PATH"
        info "  -> 已同步更新: $FILE_PATH"
    fi
done

# ==========================================
#  🛡️ 步骤 C: 系统加固与服务配置
# ==========================================

# 5. 用户与权限
if ! id "$USER_NAME" &>/dev/null; then
    useradd -r -s /bin/false "$USER_NAME"
fi

mkdir -p "$INSTALL_PREFIX/war3files" "$LOG_DIR"
chown -R $USER_NAME:$USER_NAME "$INSTALL_PREFIX" "$CONFIG_DIR" "$LOG_DIR"
chmod -R 755 "$INSTALL_PREFIX"
chmod 644 "$ETC_INI"

# 6. 生成 Systemd 服务
SERVICE_FILE="/etc/systemd/system/$SERVICE_NAME.service"
cat > "$SERVICE_FILE" <<EOF
[Unit]
Description=War3Bot Service
After=network.target

[Service]
Type=simple
User=$USER_NAME
Group=$USER_NAME
WorkingDirectory=$INSTALL_PREFIX
ExecStart=$INSTALL_PREFIX/bin/War3Bot --config $ETC_INI -p $SERVICE_PORT
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# 7. 重启服务
pkill -9 -f War3Bot || true
systemctl daemon-reload
systemctl enable $SERVICE_NAME
systemctl restart $SERVICE_NAME

echo -e "${GREEN}==============================================${NC}"
echo -e "${GREEN}✅ 安装与配置已完成！${NC}"
echo -e "   ├─ 机器人编号: $BOT_LIST_NUMBER"
echo -e "   ├─ 显示名称: $BOT_DISPLAY_NAME"
echo -e "   └─ 运行状态: 可通过 'systemctl status $SERVICE_NAME' 查看"
echo -e "${GREEN}==============================================${NC}"

# 查看日志
sleep 2
journalctl -u $SERVICE_NAME -f
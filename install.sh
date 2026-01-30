#!/bin/bash

# ==========================================
#  War3Bot 自动化安装与“全路径”配置同步脚本
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

# 0. 自动修复脚本自身的 Windows 换行符
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
echo -e "${BLUE}        War3Bot 自动化配置 (全路径版)         ${NC}"
echo -e "${BLUE}==============================================${NC}"

read -p "请输入机器人列表编号 (list_number) [默认: 1]: " INPUT_LIST_NUMBER
BOT_LIST_NUMBER=${INPUT_LIST_NUMBER:-"1"}

read -p "请输入机器人显示名称 (display_name) [默认: CC.Dota.XXX]: " INPUT_DISPLAY_NAME
BOT_DISPLAY_NAME=${INPUT_DISPLAY_NAME:-"CC.Dota.XXX"}

info "设置确认: 编号=$BOT_LIST_NUMBER, 名称=$BOT_DISPLAY_NAME"
echo ""

# 2. 依赖安装与源码编译 (保持不变)
info "检查编译依赖..."
apt-get update && apt-get install -y git cmake build-essential qtbase5-dev libqt5network5 || warn "依赖安装可能存在问题"

info "清理并准备构建目录..."
rm -rf build && mkdir build && cd build

info "开始编译与安装..."
cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" .. && make -j$(nproc) && make install || error "编译安装失败"

# ==========================================
#  ⚙️ 步骤 B: “全路径”配置同步更新 (包含 /opt/...)
# ==========================================
cd .. # 回到源码根目录
info "正在同步更新全系统所有路径下的配置文件..."

# 定义所有可能存在的配置文件路径
ETC_INI="$CONFIG_DIR/war3bot.ini"               # 系统服务运行路径
INSTALL_INI="$INSTALL_PREFIX/config/war3bot.ini" # 安装目标路径 (你提到的)
BUILD_INI="./build/config/war3bot.ini"          # 编译临时路径
SOURCE_INI="./config/war3bot.ini"               # 源码备份路径

# 如果 /etc 下的配置不存在，则先初始化一个
if [ ! -f "$ETC_INI" ]; then
    mkdir -p "$CONFIG_DIR"
    cat > "$ETC_INI" <<EOF
[server]
control_port=6116
broadcast_port=6112

[log]
log_file=$LOG_DIR/war3bot.log

[bnet]
server=139.155.155.166
port=6112

[bots]
list_number=1
display_name=CC.Dota.XX
EOF
fi

# ⚡️ 重点：将所有路径加入循环进行修改
TARGET_FILES=("$ETC_INI" "$INSTALL_INI" "$BUILD_INI" "$SOURCE_INI")

for FILE_PATH in "${TARGET_FILES[@]}"; do
    if [ -f "$FILE_PATH" ]; then
        # 1. 修复可能带入的 Windows 换行符
        sed -i 's/\r$//' "$FILE_PATH"
        # 2. 替换字段
        sed -i "s/^list_number=.*/list_number=$BOT_LIST_NUMBER/" "$FILE_PATH"
        sed -i "s/^display_name=.*/display_name=$BOT_DISPLAY_NAME/" "$FILE_PATH"
        info "  -> 已同步修改: $FILE_PATH"
    else
        # 如果 /opt/War3Bot/config 目录不存在则创建并拷贝，确保它存在
        if [[ "$FILE_PATH" == "$INSTALL_INI" ]]; then
            mkdir -p "$INSTALL_PREFIX/config"
            cp "$ETC_INI" "$INSTALL_INI"
            info "  -> 已创建并同步: $INSTALL_INI"
        fi
    fi
done

# ==========================================
#  🛡️ 步骤 C: 权限与服务
# ==========================================
info "正在应用权限与服务重启..."

if ! id "$USER_NAME" &>/dev/null; then
    useradd -r -s /bin/false "$USER_NAME"
fi

chown -R $USER_NAME:$USER_NAME "$INSTALL_PREFIX" "$CONFIG_DIR" "$LOG_DIR"
chmod -R 755 "$INSTALL_PREFIX"
chmod 644 "$ETC_INI" "$INSTALL_INI"

# 生成/更新 Systemd 服务
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

pkill -9 -f War3Bot || true
systemctl daemon-reload
systemctl enable $SERVICE_NAME
systemctl restart $SERVICE_NAME

echo -e "${GREEN}==============================================${NC}"
echo -e "${GREEN}✅ 所有路径下的配置已同步完成！${NC}"
echo -e "   ├─ 编号: $BOT_LIST_NUMBER / 名称: $BOT_DISPLAY_NAME"
echo -e "   ├─ 已同步: /etc/War3Bot/war3bot.ini"
echo -e "   ├─ 已同步: $INSTALL_PREFIX/config/war3bot.ini"
echo -e "   └─ 已同步: ./build/config/war3bot.ini"
echo -e "${GREEN}==============================================${NC}"

sleep 2
journalctl -u $SERVICE_NAME -f
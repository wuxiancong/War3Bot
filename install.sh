#!/bin/bash

# ==========================================
#  War3Bot 自动化安装与“多路径同步”修正版
# ==========================================

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# --- 基础路径配置 ---
INSTALL_PREFIX="/opt/War3Bot"
CONFIG_DIR="/etc/War3Bot/config"  # ✨ 已修正为带 config 子目录的路径
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
#  ✨ 步骤 A: 交互式获取参数
# ==========================================
echo -e "${BLUE}==============================================${NC}"
echo -e "${BLUE}        War3Bot 自动化配置 (路径修正版)       ${NC}"
echo -e "${BLUE}==============================================${NC}"

read -p "请输入机器人列表编号 (list_number) [默认: 1]: " INPUT_LIST_NUMBER
BOT_LIST_NUMBER=${INPUT_LIST_NUMBER:-"1"}

read -p "请输入机器人显示名称 (display_name) [默认: CC.Dota.XXX]: " INPUT_DISPLAY_NAME
BOT_DISPLAY_NAME=${INPUT_DISPLAY_NAME:-"CC.Dota.XXX"}

info "配置确认: 编号=$BOT_LIST_NUMBER, 名称=$BOT_DISPLAY_NAME"
echo ""

# 2. 依赖安装与源码编译
info "安装依赖并开始编译..."
apt-get update && apt-get install -y git cmake build-essential qtbase5-dev libqt5network5 || warn "依赖检查异常"

rm -rf build && mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" .. && make -j$(nproc) && make install || error "编译安装失败"

# ==========================================
#  ⚙️ 步骤 B: 修正全路径同步修改逻辑
# ==========================================
cd .. # 回到源码根目录
info "正在同步更新以下 4 个位置的配置文件..."

# 路径 1: 系统服务运行路径 (你要求的 /etc/War3Bot/config/)
ETC_INI="$CONFIG_DIR/war3bot.ini"
# 路径 2: 安装目标备份路径
INSTALL_INI="$INSTALL_PREFIX/config/war3bot.ini"
# 路径 3: 编译输出路径
BUILD_INI="./build/config/war3bot.ini"
# 路径 4: 源码默认配置路径
SOURCE_INI="./config/war3bot.ini"

# 初始化系统配置文件夹和文件（如果不存在）
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

# 定义需要同步的文件列表
TARGET_FILES=("$ETC_INI" "$INSTALL_INI" "$BUILD_INI" "$SOURCE_INI")

for FILE_PATH in "${TARGET_FILES[@]}"; do
    if [ -f "$FILE_PATH" ]; then
        # 移除 Windows 换行符
        sed -i 's/\r$//' "$FILE_PATH"
        # 替换字段内容
        sed -i "s/^list_number=.*/list_number=$BOT_LIST_NUMBER/" "$FILE_PATH"
        sed -i "s/^display_name=.*/display_name=$BOT_DISPLAY_NAME/" "$FILE_PATH"
        info "  -> 已同步: $FILE_PATH"
    else
        # 特别处理：如果安装目录或系统目录不存在该文件，则创建它
        if [[ "$FILE_PATH" == "$ETC_INI" || "$FILE_PATH" == "$INSTALL_INI" ]]; then
            DIR_PATH=$(dirname "$FILE_PATH")
            mkdir -p "$DIR_PATH"
            # 如果是安装路径且没有文件，从源码拷一份过来
            if [ -f "$SOURCE_INI" ]; then
                cp "$SOURCE_INI" "$FILE_PATH"
                sed -i "s/^list_number=.*/list_number=$BOT_LIST_NUMBER/" "$FILE_PATH"
                sed -i "s/^display_name=.*/display_name=$BOT_DISPLAY_NAME/" "$FILE_PATH"
                info "  -> 已补齐并更新: $FILE_PATH"
            fi
        fi
    fi
done

# ==========================================
#  🛡️ 步骤 C: 权限与服务启动
# ==========================================
info "正在应用权限设置..."

if ! id "$USER_NAME" &>/dev/null; then
    useradd -r -s /bin/false "$USER_NAME"
fi

# 确保 /etc/War3Bot 整个目录的所有权正确
chown -R $USER_NAME:$USER_NAME "/etc/War3Bot"
chown -R $USER_NAME:$USER_NAME "$INSTALL_PREFIX" "$LOG_DIR"
chmod -R 755 "$INSTALL_PREFIX"
chmod 644 "$ETC_INI"

# 更新 Systemd 服务文件
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
# ✨ 启动参数指向正确的 /etc/War3Bot/config 路径
ExecStart=$INSTALL_PREFIX/bin/War3Bot --config $ETC_INI -p $SERVICE_PORT
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

info "正在启动机器人..."
pkill -9 -f War3Bot || true
systemctl daemon-reload
systemctl enable $SERVICE_NAME
systemctl restart $SERVICE_NAME

echo -e "${GREEN}==============================================${NC}"
echo -e "${GREEN}✅ 配置已全量同步并启动！${NC}"
echo -e "   ├─ 系统运行配置: $ETC_INI"
echo -e "   ├─ 机器人编号: $BOT_LIST_NUMBER"
echo -e "   ├─ 显示名称: $BOT_DISPLAY_NAME"
echo -e "${GREEN}==============================================${NC}"

sleep 2
journalctl -u $SERVICE_NAME -f
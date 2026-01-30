#!/bin/bash

# ==========================================
#  War3Bot 自动化安装脚本 (路径修复版)
# ==========================================

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

INSTALL_PREFIX="/opt/War3Bot"
CONFIG_DIR="/etc/War3Bot/config"
LOG_DIR="/var/log/War3Bot"
SERVICE_NAME="war3bot"
USER_NAME="war3bot"
SERVICE_PORT=6116

info() { echo -e "${GREEN}[INFO] $1${NC}"; }
error() { echo -e "${RED}[ERROR] $1${NC}"; exit 1; }
warn()  { echo -e "${YELLOW}[WARN] $1${NC}"; }

if [ "$EUID" -ne 0 ]; then
    error "请使用 sudo 或 root 运行！"
fi

# 交互式参数
read -p "请输入机器人列表编号 (list_number) [默认: 1]: " INPUT_LIST_NUMBER
BOT_LIST_NUMBER=${INPUT_LIST_NUMBER:-"1"}

read -p "请输入机器人显示名称 (display_name) [默认: CC.Dota.XXX]: " INPUT_DISPLAY_NAME
BOT_DISPLAY_NAME=${INPUT_DISPLAY_NAME:-"CC.Dota.XXX"}

# 编译
info "开始清理与编译..."
rm -rf build && mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" .. && make -j$(nproc) && make install || error "编译失败"

# ==========================================
# ✨ 路径纠偏：删除 CMake 错误生成的嵌套目录
# ==========================================
if [ -d "$INSTALL_PREFIX/war3files/war3files" ]; then
    warn "检测到嵌套目录 /war3files/war3files，正在修正..."
    # 移动文件到上一级
    cp -rn $INSTALL_PREFIX/war3files/war3files/* $INSTALL_PREFIX/war3files/
    # 删除嵌套的文件夹
    rm -rf $INSTALL_PREFIX/war3files/war3files
    info "路径修正成功。"
fi

# 配置同步
cd .. 
ETC_INI="$CONFIG_DIR/war3bot.ini"
INSTALL_INI="$INSTALL_PREFIX/config/war3bot.ini"
BUILD_INI="./build/config/war3bot.ini"
SOURCE_INI="./config/war3bot.ini"

mkdir -p "$CONFIG_DIR" "$INSTALL_PREFIX/config"

# 更新所有路径
TARGET_FILES=("$ETC_INI" "$INSTALL_INI" "$BUILD_INI" "$SOURCE_INI")
for FILE_PATH in "${TARGET_FILES[@]}"; do
    if [ -f "$FILE_PATH" ] || [ "$FILE_PATH" == "$ETC_INI" ]; then
        [ ! -f "$FILE_PATH" ] && cp "$SOURCE_INI" "$FILE_PATH" 2>/dev/null
        sed -i 's/\r$//' "$FILE_PATH" 2>/dev/null
        sed -i "s/^list_number=.*/list_number=$BOT_LIST_NUMBER/" "$FILE_PATH" 2>/dev/null
        sed -i "s/^display_name=.*/display_name=$BOT_DISPLAY_NAME/" "$FILE_PATH" 2>/dev/null
        info "已同步: $FILE_PATH"
    fi
done

# 权限与服务
chown -R root:root "$INSTALL_PREFIX"
# 给予执行用户读取权限
if ! id "$USER_NAME" &>/dev/null; then useradd -r -s /bin/false "$USER_NAME"; fi
chown -R $USER_NAME:$USER_NAME "$CONFIG_DIR" "$LOG_DIR" "$INSTALL_PREFIX/war3files"
chmod -R 755 "$INSTALL_PREFIX"

# Systemd
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

info "✅ 安装成功！请检查日志。"
sleep 2
journalctl -u $SERVICE_NAME -f
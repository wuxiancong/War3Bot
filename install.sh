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

# 1. 权限检查
if [ "$EUID" -ne 0 ]; then
    error "请使用 sudo 或 root 用户运行此脚本！"
fi

# ==========================================
#  ✨ 步骤 A: 交互式获取参数
# ==========================================
echo -e "${BLUE}==============================================${NC}"
echo -e "${BLUE}        War3Bot 自动化配置 (修复版)           ${NC}"
echo -e "${BLUE}==============================================${NC}"

read -p "请输入机器人列表编号 (list_number) [默认: 1]: " INPUT_LIST_NUMBER
BOT_LIST_NUMBER=${INPUT_LIST_NUMBER:-"1"}

read -p "请输入机器人显示名称 (display_name) [默认: CC.Dota.XXX]: " INPUT_DISPLAY_NAME
BOT_DISPLAY_NAME=${INPUT_DISPLAY_NAME:-"CC.Dota.XXX"}

info "设置确认: 编号=$BOT_LIST_NUMBER, 名称=$BOT_DISPLAY_NAME"
echo ""

# 2. 依赖安装
info "检查编译依赖..."
apt-get update && apt-get install -y git cmake build-essential qtbase5-dev libqt5network5 || warn "依赖安装可能存在问题"

# 3. 编译安装
info "清理并准备构建目录..."
rm -rf build && mkdir build && cd build

info "开始编译与安装..."
cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" .. && make -j$(nproc) && make install || error "编译安装失败"

# ==========================================
#  ⚙️ 步骤 B: 全路径配置同步更新
# ==========================================
cd .. # 回到源码根目录
info "正在同步更新全系统所有路径下的配置文件..."

ETC_INI="$CONFIG_DIR/war3bot.ini"
INSTALL_INI="$INSTALL_PREFIX/config/war3bot.ini"
BUILD_INI="./build/config/war3bot.ini"
SOURCE_INI="./config/war3bot.ini"

# 初始化 /etc 配置（如果不存在）
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
        # 针对安装路径特别处理：如果不存在则创建
        if [[ "$FILE_PATH" == "$INSTALL_INI" ]]; then
            mkdir -p "$INSTALL_PREFIX/config"
            cp "$ETC_INI" "$INSTALL_INI"
            info "  -> 已补齐并同步: $INSTALL_INI"
        fi
    fi
done

# ==========================================
#  🛡️ 步骤 C: 权限与服务管理
# ==========================================
info "配置系统权限..."

if ! id "$USER_NAME" &>/dev/null; then
    useradd -r -s /bin/false "$USER_NAME"
fi

chown -R $USER_NAME:$USER_NAME "$INSTALL_PREFIX" "$CONFIG_DIR" "$LOG_DIR"
chmod -R 755 "$INSTALL_PREFIX"
chmod 644 "$ETC_INI" "$INSTALL_INI"

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
ExecStart=$INSTALL_PREFIX/bin/War3Bot --config $ETC_INI -p $SERVICE_PORT
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

info "正在重启服务..."
pkill -9 -f War3Bot || true
systemctl daemon-reload
systemctl enable $SERVICE_NAME
systemctl restart $SERVICE_NAME

echo -e "${GREEN}==============================================${NC}"
echo -e "${GREEN}✅ 配置同步完成！${NC}"
echo -e "   ├─ 编号: $BOT_LIST_NUMBER"
echo -e "   ├─ 名称: $BOT_DISPLAY_NAME"
echo -e "   └─ 路径: $INSTALL_PREFIX"
echo -e "${GREEN}==============================================${NC}"

sleep 2
journalctl -u $SERVICE_NAME -f
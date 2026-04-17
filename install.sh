#!/bin/bash

# ======================================================
#  War3Bot 自动化安装脚本
# ======================================================

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

# ==========================================
# 1. 交互式参数获取与变量深度清洗
# ==========================================
read -p "请输入机器人列表编号 (list_number) [默认: 1]: " INPUT_LIST_NUMBER
BOT_LIST_NUMBER=$(echo "${INPUT_LIST_NUMBER:-"1"}" | tr -d '\r\n' | tr -cd '[:print:]')

read -p "请输入机器人显示名称 (display_name) [默认: CC.Dota.XXX]: " INPUT_DISPLAY_NAME
# 删除回车换行，并只保留可打印字符，从源头切断乱码
CLEAN_DISPLAY_NAME=$(echo "${INPUT_DISPLAY_NAME:-"CC.Dota.XXX"}" | tr -d '\r\n' | tr -cd '[:print:]')

info "准备安装机器人: $CLEAN_DISPLAY_NAME (编号: $BOT_LIST_NUMBER)"

# ==========================================
# 2. 预安装清理 (防止旧乱码干扰)
# ==========================================
info "正在清理旧的配置文件以防止编码污染..."
# 删除可能已经损坏或编码错误的旧文件，确保从干净的模板重新生成
sudo rm -f "$CONFIG_DIR/war3bot.ini"
sudo rm -f "$INSTALL_PREFIX/config/war3bot.ini"

# ==========================================
# 3. 编译与安装
# ==========================================
info "开始清理与编译..."
rm -rf build && mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" .. && make -j$(nproc) && make install || error "编译失败"

# ✨ 路径纠偏
if [ -d "$INSTALL_PREFIX/war3files/war3files" ]; then
    warn "检测到嵌套目录，正在修正..."
    cp -rn $INSTALL_PREFIX/war3files/war3files/* $INSTALL_PREFIX/war3files/
    rm -rf $INSTALL_PREFIX/war3files/war3files
fi

# ==========================================
# 4. 配置文件同步与编码修复
# ==========================================
cd .. 
ETC_INI="$CONFIG_DIR/war3bot.ini"
INSTALL_INI="$INSTALL_PREFIX/config/war3bot.ini"
BUILD_INI="./build/config/war3bot.ini"
SOURCE_INI="./config/war3bot.ini"

# 先修复源码里的模板文件（防止源码本身带回车）
if [ -f "$SOURCE_INI" ]; then
    sed -i 's/\r//g' "$SOURCE_INI"
    info "源码模板已修复 (移除 CRLF)"
fi

mkdir -p "$CONFIG_DIR" "$INSTALL_PREFIX/config"
mkdir -p "$LOG_DIR"

TARGET_FILES=("${ETC_INI}" "${INSTALL_INI}" "${BUILD_INI}" "${SOURCE_INI}")

for FILE_PATH in "${TARGET_FILES[@]}"; do
    # 如果目标文件不存在（刚才我们删了），则从修复过的源码拷贝
    if [ ! -f "$FILE_PATH" ]; then
        cp "$SOURCE_INI" "$FILE_PATH" 2>/dev/null
    fi

    if [ -f "$FILE_PATH" ]; then
        # 1. 再次强制移除所有回车符
        sed -i 's/\r//g' "$FILE_PATH"
        
        # 2. 强制剔除所有非打印字符（针对已经“中毒”的文件）
        tr -cd '\11\12\15\40-\176' < "$FILE_PATH" > "${FILE_PATH}.tmp"
        mv "${FILE_PATH}.tmp" "$FILE_PATH"

        # 3. 使用 | 定界符精准替换。^.* 是为了确保把旧名字残留的乱码也一并覆盖
        sed -i "s|^display_name=.*|display_name=${CLEAN_DISPLAY_NAME}|" "$FILE_PATH"
        sed -i "s|^list_number=.*|list_number=${BOT_LIST_NUMBER}|" "$FILE_PATH"
        
        # 4. 确保文件末尾有且只有一个换行
        sed -i '$a\' "$FILE_PATH"
        
        info "已同步并重置配置文件: $FILE_PATH"
    fi
done

# ==========================================
# 5. 权限与 Systemd 设置
# ==========================================
info "设置权限与服务..."
chown -R root:root "$INSTALL_PREFIX"
if ! id "$USER_NAME" &>/dev/null; then useradd -r -s /bin/false "$USER_NAME"; fi
chown -R $USER_NAME:$USER_NAME "$CONFIG_DIR" "$LOG_DIR" "$INSTALL_PREFIX/war3files"
chmod -R 755 "$INSTALL_PREFIX"

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

# ==========================================
# 6. 启动
# ==========================================
pkill -9 -f War3Bot || true
systemctl daemon-reload
systemctl enable $SERVICE_NAME
systemctl restart $SERVICE_NAME

info "✅ 安装成功！"
info "配置路径: $ETC_INI"
info "最终显示名称: $CLEAN_DISPLAY_NAME"
info "----------------------------------------"
sleep 2
journalctl -u $SERVICE_NAME -f
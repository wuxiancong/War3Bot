#!/bin/bash

# ======================================================
#  War3Bot 自动化安装脚本 (编码修复 & 路径纠偏版)
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
# 1. 交互式参数获取与变量清洗
# ==========================================
read -p "请输入机器人列表编号 (list_number) [默认: 1]: " INPUT_LIST_NUMBER
BOT_LIST_NUMBER=$(echo "${INPUT_LIST_NUMBER:-"1"}" | tr -d '\r\n' | tr -cd '[:print:]')

read -p "请输入机器人显示名称 (display_name) [默认: CC.Dota.XXX]: " INPUT_DISPLAY_NAME
# 【关键修复】剔除 \r, \n 和所有非打印控制字符，防止乱码和“双重行”现象
CLEAN_DISPLAY_NAME=$(echo "${INPUT_DISPLAY_NAME:-"CC.Dota.XXX"}" | tr -d '\r\n' | tr -cd '[:print:]')

info "准备安装机器人: $CLEAN_DISPLAY_NAME (编号: $BOT_LIST_NUMBER)"

# ==========================================
# 2. 编译与安装
# ==========================================
info "开始清理与编译..."
rm -rf build && mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" .. && make -j$(nproc) && make install || error "编译失败"

# ✨ 路径纠偏：删除 CMake 错误生成的嵌套目录
if [ -d "$INSTALL_PREFIX/war3files/war3files" ]; then
    warn "检测到嵌套目录 /war3files/war3files，正在修正..."
    cp -rn $INSTALL_PREFIX/war3files/war3files/* $INSTALL_PREFIX/war3files/
    rm -rf $INSTALL_PREFIX/war3files/war3files
    info "路径修正成功。"
fi

# ==========================================
# 3. 配置文件同步与编码修复
# ==========================================
cd .. 
ETC_INI="$CONFIG_DIR/war3bot.ini"
INSTALL_INI="$INSTALL_PREFIX/config/war3bot.ini"
BUILD_INI="./build/config/war3bot.ini"
SOURCE_INI="./config/war3bot.ini"

mkdir -p "$CONFIG_DIR" "$INSTALL_PREFIX/config"
mkdir -p "$LOG_DIR"

# 需要同步的所有配置文件列表
TARGET_FILES=("$ETC_INI" "$INSTALL_INI" "$BUILD_INI" "$SOURCE_INI")

for FILE_PATH in "${TARGET_FILES[@]}"; do
    # 如果是 /etc 下的文件且不存在，则从源码拷贝
    if [ ! -f "$FILE_PATH" ]; then
        cp "$SOURCE_INI" "$FILE_PATH" 2>/dev/null
    fi

    if [ -f "$FILE_PATH" ]; then
        # 删除所有回车符 \r，防止文件被识别为二进制或产生重叠乱码
        sed -i 's/\r//g' "$FILE_PATH"
        
        # 使用 | 作为 sed 定界符，防止内容包含 / 导致语法错误
        # 确保 display_name 和 list_number 被正确替换为清洗后的变量
        sed -i "s|^display_name=.*|display_name=${CLEAN_DISPLAY_NAME}|" "$FILE_PATH"
        sed -i "s|^list_number=.*|list_number=${BOT_LIST_NUMBER}|" "$FILE_PATH"
        
        # 确保文件结尾有一个换行符
        sed -i '$a\' "$FILE_PATH"
        
        info "已同步并修正编码: $FILE_PATH"
    fi
done

# ==========================================
# 4. 权限设置
# ==========================================
info "设置系统权限..."
chown -R root:root "$INSTALL_PREFIX"

if ! id "$USER_NAME" &>/dev/null; then 
    useradd -r -s /bin/false "$USER_NAME"
fi

# 确保日志、配置和地图目录对运行用户可见
chown -R $USER_NAME:$USER_NAME "$CONFIG_DIR" "$LOG_DIR" "$INSTALL_PREFIX/war3files"
chmod -R 755 "$INSTALL_PREFIX"

# ==========================================
# 5. Systemd 服务配置
# ==========================================
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
# 明确指定配置文件路径
ExecStart=$INSTALL_PREFIX/bin/War3Bot --config $ETC_INI -p $SERVICE_PORT
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# ==========================================
# 6. 启动服务
# ==========================================
info "正在重启服务..."
pkill -9 -f War3Bot || true
systemctl daemon-reload
systemctl enable $SERVICE_NAME
systemctl restart $SERVICE_NAME

info "✅ 安装成功！"
info "配置路径: $ETC_INI"
info "显示名称: $CLEAN_DISPLAY_NAME"
info "----------------------------------------"
sleep 2
journalctl -u $SERVICE_NAME -f
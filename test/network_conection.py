import socket
import struct
import sys
import time

# ================= 默认配置 =================
DEFAULT_IP = "207.90.237.73"  # 你的服务器IP
DEFAULT_PORT = 6112           # 🔥 已修改默认端口为 6112
# ===========================================

def create_w3gs_packet(msg_id, payload_str=""):
    """
    构造符合 C++ 代码解析逻辑的 UDP 包
    结构: [Header 0xF7] [MsgID 1B] [Length 2B LittleEndian] [Payload...]
    """
    header = 0xF7
    payload_bytes = payload_str.encode('utf-8')
    # 长度 = 头(1) + ID(1) + 长度位(2) + 数据长度
    total_length = 1 + 1 + 2 + len(payload_bytes)
    
    # <BBH 代表: LittleEndian模式, Unsigned Char, Unsigned Char, Unsigned Short
    packet = struct.pack('<BBH', header, msg_id, total_length) + payload_bytes
    return packet

def test_udp_protocol(ip, port):
    print(f"\n[*] 正在进行 UDP 协议测试 {ip}:{port} ...")
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(2.0) # 设置2秒超时等待回复

    try:
        # --- 测试 1: 发送自定义测试包 (0x88) ---
        print("   -> [1/2] 发送自定义测试包 (ID: 0x88)...")
        pkt_test = create_w3gs_packet(0x88, "Hello C++ UDP!")
        s.sendto(pkt_test, (ip, port))
        print("     ✅ 已发送。请查看 C++ 服务器日志是否显示 '收到测试包'。")

        # --- 测试 2: 发送 PING 包 (0x35) ---
        # 你的 C++ 代码中: case W3GS_PING_FROM_OTHERS (0x35) 会回复 PONG
        print("   -> [2/2] 发送 PING 包 (ID: 0x35) 并等待回复...")
        pkt_ping = create_w3gs_packet(0x35) # 0x35 是 W3GS_PING_FROM_OTHERS
        s.sendto(pkt_ping, (ip, port))
        
        # 尝试接收回复
        try:
            data, addr = s.recvfrom(1024)
            if len(data) >= 4 and data[0] == 0xF7:
                reply_id = data[1]
                print(f"     🎉 收到服务器回复! 来自 {addr}")
                print(f"     📦 回复包 ID: 0x{reply_id:02X} (如果是 0x36 或 0x88 则测试完美成功)")
            else:
                print(f"     ⚠️ 收到未知数据: {data}")
        except socket.timeout:
            print("     ❌ 等待回复超时。")
            print("        原因可能是: 1. 服务器防火墙拦截了出站流量(UDP Outbound)")
            print("                   2. 云服务器安全组未放行 UDP")
            print("                   3. C++ 程序未正确运行或崩溃")

    except Exception as e:
        print(f"❌ UDP 发送发生错误: {e}")
    finally:
        s.close()

if __name__ == "__main__":
    print(f"=== W3GS UDP 协议测试工具 ===")
    
    # 获取 IP
    user_ip = input(f"请输入服务器IP (回车用 {DEFAULT_IP}): ").strip()
    target_ip = user_ip if user_ip else DEFAULT_IP

    # 获取 端口
    while True:
        user_port = input(f"请输入端口号 (回车用 {DEFAULT_PORT}): ").strip()
        if not user_port:
            target_port = DEFAULT_PORT
            break
        elif user_port.isdigit():
            target_port = int(user_port)
            break
        else:
            print("⚠️ 端口必须是数字")

    print(f"\n🚀 目标确认: {target_ip}:{target_port}")
    
    test_udp_protocol(target_ip, target_port)

    print("\n测试结束。")
    input("按回车键退出...")
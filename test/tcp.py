import socket
import struct
import sys
import time

# ================= 默认配置 =================
DEFAULT_IP = "207.90.237.73"  # 你的服务器IP
DEFAULT_PORT = 6112           # 🔥 默认端口
# ===========================================

def create_reqjoin_packet():
    """
    构造 W3GS_REQJOIN (0x1E) TCP 包
    对应 C++ 代码中的解析逻辑
    结构: [Header 4B] [HostCounter 4B] [EntryKey 4B] [Unk 1B] [ListenPort 2B] 
          [PeerKey 4B] [Name String...] [Unk 4B] [IntPort 2B] [IntIP 4B]
    """
    # 1. 构造包体 (Payload)
    # 模拟数据
    client_host_counter = 0
    client_entry_key = 0
    client_unk8 = 0
    client_listen_port = 6112
    client_peer_key = 0x12345678
    client_name = "TestClient"
    client_unk32 = 0
    client_int_port = 6112
    client_int_ip = socket.inet_aton("127.0.0.1") # 转换 IP 为 4字节

    # 基础部分
    payload = struct.pack('<IIBH', 
        client_host_counter, 
        client_entry_key, 
        client_unk8, 
        client_listen_port
    )
    payload += struct.pack('<I', client_peer_key)
    
    # 字符串 (名字 + \0)
    payload += client_name.encode('utf-8') + b'\x00'
    
    # 尾部部分
    payload += struct.pack('<IH', client_unk32, client_int_port)
    payload += client_int_ip # 已经是bytes了

    # 2. 构造头部 (Header)
    header_sig = 0xF7
    msg_id = 0x1E
    # 长度 = 头部(4) + 数据长度
    total_length = 4 + len(payload)
    
    header = struct.pack('<BBH', header_sig, msg_id, total_length)
    
    return header + payload

def parse_w3gs_responses(data):
    """简单解析收到的 TCP 数据流，识别包 ID"""
    offset = 0
    packets_found = []
    
    while offset < len(data):
        # 检查剩余长度是否够读一个头
        if len(data) - offset < 4:
            break
            
        # 读取头
        sig = data[offset]
        msg_id = data[offset+1]
        length = struct.unpack('<H', data[offset+2:offset+4])[0]
        
        if sig != 0xF7:
            print(f"     ⚠️ 发现非 W3GS 协议头: 0x{sig:02X}")
            break
            
        packets_found.append(msg_id)
        
        # 移动到下一个包
        offset += length
        
    return packets_found

def test_tcp_protocol(ip, port):
    print(f"\n[*] 正在进行 TCP 协议测试 {ip}:{port} ...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(3.0) # 设置3秒超时

    try:
        # --- 步骤 1: 建立连接 ---
        print(f"   -> [1/3] 正在尝试 TCP 连接...")
        s.connect((ip, port))
        print(f"     ✅ TCP 连接建立成功！")

        # --- 步骤 2: 发送加入请求 (0x1E) ---
        print(f"   -> [2/3] 发送 W3GS_REQJOIN (ID: 0x1E)...")
        pkt_join = create_reqjoin_packet()
        s.sendall(pkt_join)
        print(f"     ✅ 数据已发送 ({len(pkt_join)} 字节)。")

        # --- 步骤 3: 接收回复 ---
        print(f"   -> [3/3] 等待服务器响应握手包...")
        
        # 接收数据 (缓冲区稍大一点，因为握手包很多)
        try:
            recv_data = s.recv(4096)
            
            if not recv_data:
                print("     ❌ 服务器关闭了连接 (未收到任何数据)。")
                return

            print(f"     🎉 收到服务器回复! 长度: {len(recv_data)} 字节")
            
            # 解析收到的包 ID
            packet_ids = parse_w3gs_responses(recv_data)
            
            # 打印详情
            print(f"     📦 识别到的数据包序列: {[hex(pid) for pid in packet_ids]}")
            
            # 验证逻辑
            expected_packets = {
                0x04: "SlotInfoJoin (握手成功)",
                0x06: "PlayerInfo (主机信息)",
                0x3D: "MapCheck (地图验证)",
                0x09: "SlotInfo (槽位信息)",
                0x05: "RejectJoin (拒绝加入)"
            }

            success = False
            for pid in packet_ids:
                if pid in expected_packets:
                    print(f"        -> 包含: 0x{pid:02X} - {expected_packets[pid]}")
                    if pid == 0x04:
                        success = True
            
            if success:
                print("\n     🌟 测试完美通过！服务器正确处理了加入请求。")
            elif 0x05 in packet_ids:
                print("\n     ⚠️ 测试通过，但服务器拒绝了加入 (可能是房间满了或游戏已开始)。")
            else:
                print("\n     ⚠️ 收到了数据，但没有发现关键握手包 (0x04)。请检查 C++ 日志。")

        except socket.timeout:
            print("     ❌ 等待回复超时。服务器收到了包但没有回复，或逻辑卡死。")

    except ConnectionRefusedError:
        print(f"     ❌ 连接被拒绝！")
        print(f"        原因: 端口 {port} 没有程序在监听。请确认 Bot 正在运行且端口配置正确。")
    except Exception as e:
        print(f"     ❌ TCP 测试发生错误: {e}")
    finally:
        s.close()
        print("   -> 连接已关闭。")

if __name__ == "__main__":
    print(f"=== W3GS TCP 协议测试工具 ===")
    
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
    
    test_tcp_protocol(target_ip, target_port)

    print("\n测试结束。")
    input("按回车键退出...")
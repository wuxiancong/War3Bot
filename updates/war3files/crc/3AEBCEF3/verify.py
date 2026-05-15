# -*- coding: utf-8 -*-
import struct
import os
import sys

# ==========================================
# 算法实现
# ==========================================
def rotate_left(val, shift):
    return ((val << shift) | (val >> (32 - shift))) & 0xFFFFFFFF

def blizzard_hash(data):
    hash_val = 0
    length = len(data)
    ptr = 0

    # 4字节一组
    while length >= 4:
        chunk = struct.unpack('<I', data[ptr:ptr+4])[0]
        hash_val = hash_val ^ chunk
        hash_val = rotate_left(hash_val, 3)
        ptr += 4
        length -= 4

    # 剩余字节
    while length > 0:
        byte_val = data[ptr]
        hash_val = hash_val ^ byte_val
        hash_val = rotate_left(hash_val, 3)
        ptr += 1
        length -= 1

    return hash_val

# ==========================================
# 主校验逻辑
# ==========================================
def main():
    print("Verifying Warcraft III CRC (Full Stage 1 + 2)...")

    # 1. 读取基础脚本
    try:
        with open("common.j", "rb") as f: d_com = f.read()
        with open("blizzard.j", "rb") as f: d_bliz = f.read()
        with open("war3map.j", "rb") as f: d_map = f.read()
    except FileNotFoundError:
        print("Error: Base script files (common/blizzard/war3map) missing.")
        return

    h_com = blizzard_hash(d_com)
    h_bliz = blizzard_hash(d_bliz)
    h_map = blizzard_hash(d_map)

    print(f"Common Hash:   {h_com:08X}")
    print(f"Blizzard Hash: {h_bliz:08X}")
    print(f"Map Hash:      {h_map:08X}")

    # ------------------------------------------
    # Stage 1: 脚本环境混合
    # ------------------------------------------
    val = h_bliz ^ h_com
    val = rotate_left(val, 3)           # ROL 1
    val = val ^ 0x03F1379E              # Magic Salt
    val = rotate_left(val, 3)           # ROL 2
    val = h_map ^ val
    val = rotate_left(val, 3)           # ROL 3

    print(f"Stage 1 Result: {val:08X}")

    # ------------------------------------------
    # Stage 2: 组件混合 (Components)
    # ------------------------------------------
    # 必须严格遵守此顺序
    components = [
        "war3map.w3e", "war3map.wpm", "war3map.doo", "war3map.w3u",
        "war3map.w3b", "war3map.w3d", "war3map.w3a", "war3map.w3q"
    ]

    for name in components:
        if os.path.exists(name):
            with open(name, "rb") as f:
                data = f.read()
                if len(data) > 0:
                    f_hash = blizzard_hash(data)
                    val = val ^ f_hash
                    val = rotate_left(val, 3)
                    print(f"   + Mixed {name} ({f_hash:08X})")
        else:
            # 如果文件不存在，C++ 逻辑是跳过 (不混入0)
            pass

    final_hex = f"{val:08X}"

    # 获取当前目录名作为 C++ 的计算结果
    expected = os.path.basename(os.getcwd()).upper()

    print("-" * 30)
    print(f"Python Calc: {final_hex}")
    print(f"C++ Expect:  {expected}")

    if final_hex == expected:
        print("✅ MATCH: Algorithm verification successful.")
    else:
        print("❌ MISMATCH: Check if binary components (.w3e etc) were dumped correctly.")

if __name__ == "__main__":
    main()

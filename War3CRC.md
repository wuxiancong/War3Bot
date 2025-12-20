这是基于你提供的反汇编代码（判定为 Warcraft III 1.26 版本 `Game.dll`）的完整逆向分析总结。

这段逻辑用于计算**房间地图校验和（Map Checksum）**。如果计算结果与主机不一致，玩家将无法加入房间或被自动踢出。

---

# Warcraft III (1.26) 地图校验和计算逻辑总结

## 1. 核心算法流程
游戏按顺序读取三个核心脚本文件，计算其基础 CRC32 值，然后通过位运算混合生成最终的 Checksum。

### 涉及文件
1.  **`common.j`** (基础 JASS 库)
2.  **`blizzard.j`** (暴雪扩展库)
3.  **`war3map.j`** (当前地图脚本)

### 数学公式
假设三个文件的 CRC 值分别为 $CRC_{com}$、$CRC_{bliz}$、$CRC_{map}$。

1.  **第一阶段（混合环境包）**：
    $$
    Val_{temp} = \text{ROL}\Big( \text{ROL}( CRC_{bliz} \oplus CRC_{com}, \ 3 ) \oplus \text{0x03F1379E}, \ 3 \Big)
    $$
2.  **第二阶段（混合地图脚本）**：
    $$
    \text{Checksum} = \text{ROL}\Big( CRC_{map} \oplus Val_{temp}, \ 3 \Big)
    $$

> **注**：`ROL` 表示 **循环左移 (Rotate Left)**，`XOR` ($\oplus$) 表示 **异或**。

---

## 2. 关键反汇编代码分析
以下代码片段展示了该算法的具体实现步骤。

### A. 获取基础 CRC (调用 Storm.dll)
在进入混合逻辑前，游戏调用底层函数获取单个文件的 CRC。
```assembly
; 加载 common.j / blizzard.j / war3map.j
6F3B1A80 | call game.6F3B1970      ; 读取并计算文件 CRC (内部调用 Storm Ordinal #279)
6F3B1AA0 | call game.6F39E5C0      ; 获取 common.j 的 CRC -> 存入 EBP
6F3B1ADC | call game.6F39E5C0      ; 获取 blizzard.j 的 CRC -> 存入 EAX (随后给EBX)
```

### B. 核心混合逻辑 (Magic Number 0x3F1379E)
这是整个校验的核心指纹。
```assembly
6F3B1AE1 | 8BD8                | mov ebx,eax                ; EBX = CRC(blizzard.j)
; [Step 1] 混合 Common 和 Blizzard
6F3B1AEB | 33DD                | xor ebx,ebp                ; EBX = CRC(blizzard) XOR CRC(common)
; [Step 2] 第一次循环左移 3位
6F3B1AED | C1C3 03             | rol ebx,3                  ; EBX = ROL(EBX, 3)
; [Step 3] 异或魔数 (关键特征码)
6F3B1AF0 | 81F3 9E37F103       | xor ebx,3F1379E            ; EBX = EBX XOR 0x03F1379E
; [Step 4] 第二次循环左移 3位
6F3B1AF6 | C1C3 03             | rol ebx,3                  ; EBX = ROL(EBX, 3)
; 此时 EBX 中存放的是环境校验和 (中间值)
```

### C. 最终混合 (加入 war3map.j)
```assembly
; 获取 war3map.j 的 CRC
6F3B1B1C | call game.6F39E5C0  | call ...                   ; 获取 war3map.j CRC -> EAX
6F3B1B21 | 8BD0                | mov edx,eax                ; EDX = CRC(war3map.j)

; [Step 5] 与环境校验和异或
6F3B1B23 | 8B4424 30           | mov eax,dword ptr ss:[esp+30] ; 取出上面算好的中间值 EBX
6F3B1B27 | 3310                | xor edx,dword ptr ds:[eax]    ; EDX = CRC(war3map) XOR 中间值

; [Step 6] 最终循环左移 3位
6F3B1B2B | C1C2 03             | rol edx,3                  ; EDX = ROL(EDX, 3)
6F3B1B2F | 8910                | mov dword ptr ds:[eax],edx ; 保存最终 Checksum
```

---

## 3. C++ 还原代码
如果你需要编写工具（如改图工具、版本伪装器），可以使用以下代码还原该逻辑：

```cpp
#include <cstdint>
#include <cstdlib>

// 循环左移函数 (对应汇编 ROL)
uint32_t RotateLeft(uint32_t value, int shift) {
    return (value << shift) | (value >> (32 - shift));
}

/**
 * 计算 War3 1.26 地图校验和
 * @param crc_common   common.j 的原始 CRC32
 * @param crc_blizzard blizzard.j 的原始 CRC32
 * @param crc_war3map  war3map.j 的原始 CRC32
 * @return 最终的房间校验和 (Map Checksum)
 */
uint32_t CalculateWarcraftChecksum(uint32_t crc_common, uint32_t crc_blizzard, uint32_t crc_war3map) {
    // 1. 混合 Common 和 Blizzard
    uint32_t result = crc_blizzard ^ crc_common;
    
    // 2. 变换 1
    result = RotateLeft(result, 3);
    
    // 3. 异或魔数 (特征码 0x03F1379E)
    result = result ^ 0x03F1379E;
    
    // 4. 变换 2
    result = RotateLeft(result, 3);
    
    // 5. 混合地图脚本
    result = crc_war3map ^ result;
    
    // 6. 最终变换
    result = RotateLeft(result, 3);
    
    return result;
}
```

## 4. 逆向结论
1.  **特征码**：`0x03F1379E` 是 War3 1.26 版本特有的 XOR Key。不同版本的 War3 这个常数可能不同。
2.  **过检测思路**：
    *   如果你修改了 `war3map.j`，其 `crc_war3map` 会改变，导致最终结果改变，无法进别人房间。
    *   **方法一 (内存补丁)**：Hook `Storm.dll` 的 CRC 计算函数，当检测到文件名为 `war3map.j` 时，返回未修改前的原始 CRC。
    *   **方法二 (暴力计算)**：根据上述公式，逆推出一个“碰撞”值，或者直接修改内存中最终计算出的 Result。
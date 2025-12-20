这是一份整合了之前所有分析结论、公式推导以及你提供的完整反汇编代码（1.26版本）的最终 Markdown 文档。

---

# Warcraft III (1.26) 地图校验和计算逻辑总结

## 1. 核心算法流程
游戏在建立房间或玩家加入时，会按顺序读取三个核心脚本文件，计算其基础 CRC32 值，然后通过特定的位运算混合生成最终的 Checksum。这个值用于判定玩家是否与主机地图一致。

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

> **注**：`ROL` 表示 **循环左移 (Rotate Left)**，`XOR` ($\oplus$) 表示 **异或**。`0x03F1379E` 是 1.26 版本的特征码。

---

## 2. 关键反汇编代码分析

### A. 获取基础 CRC (底层)
在进入混合逻辑前，游戏调用 `Storm.dll` (Ordinal #279) 获取单个文件的 CRC。
```assembly
; 示意逻辑
call game.6F3B1970      ; 读取并计算文件 CRC
call game.6F39E5C0      ; 从对象中获取计算好的 CRC 值
```

### B. 核心混合逻辑 (关键特征)
```assembly
; [Step 1] 混合 Common 和 Blizzard
6F3B1AEB | 33DD                | xor ebx,ebp                ; EBX = CRC(blizzard) XOR CRC(common)
; [Step 2] 第一次循环左移 3位
6F3B1AED | C1C3 03             | rol ebx,3                  ; EBX = ROL(EBX, 3)
; [Step 3] 异或魔数 (特征码)
6F3B1AF0 | 81F3 9E37F103       | xor ebx,3F1379E            ; EBX = EBX XOR 0x03F1379E
; [Step 4] 第二次循环左移 3位
6F3B1AF6 | C1C3 03             | rol ebx,3                  ; EBX = ROL(EBX, 3)
```

### C. 最终混合 (加入 war3map.j)
```assembly
; [Step 5] 与环境校验和异或
6F3B1B27 | 3310                | xor edx,dword ptr ds:[eax] ; EDX = CRC(war3map) XOR 中间值
; [Step 6] 最终循环左移 3位
6F3B1B2B | C1C2 03             | rol edx,3                  ; EDX = ROL(EDX, 3)
6F3B1B2F | 8910                | mov dword ptr ds:[eax],edx ; 保存最终 Checksum (断点位)
```

### D. 完整反汇编（v1.26）
以下是关键调用链的完整反汇编代码，包含版本检查入口、参数准备封装以及核心算法实现。

#### 偏移：39ED70 (入口与版本检查)
此函数负责判断游戏版本是否大于 `1770` (Hex 6000, 即 1.22+)，若满足则调用新的校验逻辑。
```assembly
6F39ED70 | 81EC 08010000            | sub esp,108                                  |
6F39ED76 | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]              |
6F39ED7B | 33C4                     | xor eax,esp                                  |
6F39ED7D | 898424 04010000          | mov dword ptr ss:[esp+104],eax               |
6F39ED84 | 56                       | push esi                                     |
6F39ED85 | 8BF2                     | mov esi,edx                                  |
6F39ED87 | 68 04010000              | push 104                                     |
6F39ED8C | 8D5424 08                | lea edx,dword ptr ss:[esp+8]                 |
6F39ED90 | E8 DB2FC7FF              | call game.6F011D70                           |
6F39ED95 | 85C0                     | test eax,eax                                 |
6F39ED97 | 75 18                    | jne game.6F39EDB1                            |
6F39ED99 | 5E                       | pop esi                                      |
6F39ED9A | 8B8C24 04010000          | mov ecx,dword ptr ss:[esp+104]               |
6F39EDA1 | 33CC                     | xor ecx,esp                                  |
6F39EDA3 | E8 B1224400              | call game.6F7E1059                           |
6F39EDA8 | 81C4 08010000            | add esp,108                                  |
6F39EDAE | C2 0800                  | ret 8                                        |
6F39EDB1 | 57                       | push edi                                     |
6F39EDB2 | 8BBC24 18010000          | mov edi,dword ptr ss:[esp+118]               |
6F39EDB9 | 85FF                     | test edi,edi                                 |
6F39EDBB | 75 05                    | jne game.6F39EDC2                            |
6F39EDBD | BF AB170000              | mov edi,17AB                                 |
6F39EDC2 | 8B9424 14010000          | mov edx,dword ptr ss:[esp+114]               |
6F39EDC9 | 57                       | push edi                                     |
6F39EDCA | 52                       | push edx                                     |
6F39EDCB | 8D4C24 10                | lea ecx,dword ptr ss:[esp+10]                |
6F39EDCF | E8 DC2D0100              | call game.6F3B1BB0                           | <--- 调用参数准备层
6F39EDD4 | 81FF 70170000            | cmp edi,1770                                 | <--- 检查版本是否 >= 6000 (1.22+)
6F39EDDA | 1BD2                     | sbb edx,edx                                  |
6F39EDDC | 83C2 01                  | add edx,1                                    |
6F39EDDF | 8BCE                     | mov ecx,esi                                  |
6F39EDE1 | 8906                     | mov dword ptr ds:[esi],eax                   |
6F39EDE3 | E8 18FEFFFF              | call game.6F39EC00                           |
6F39EDE8 | 8B8C24 0C010000          | mov ecx,dword ptr ss:[esp+10C]               |
6F39EDEF | 5F                       | pop edi                                      |
6F39EDF0 | 5E                       | pop esi                                      |
6F39EDF1 | 33CC                     | xor ecx,esp                                  |
6F39EDF3 | B8 01000000              | mov eax,1                                    |
6F39EDF8 | E8 5C224400              | call game.6F7E1059                           |
6F39EDFD | 81C4 08010000            | add esp,108                                  |
6F39EE03 | C2 0800                  | ret 8                                        |
```

#### 偏移：3B1BB0 (参数封装层)
此函数负责将三个文件名（common.j, blizzard.j, war3map.j）和内存指针压栈，准备调用核心算法。
```assembly
6F3B1BB0 | 83EC 08                  | sub esp,8                                    |
6F3B1BB3 | 8B4424 10                | mov eax,dword ptr ss:[esp+10]                |
6F3B1BB7 | 50                       | push eax                                     |
6F3B1BB8 | 8B4424 10                | mov eax,dword ptr ss:[esp+10]                |
6F3B1BBC | 50                       | push eax                                     |
6F3B1BBD | 52                       | push edx                                     |
6F3B1BBE | 8D5424 0C                | lea edx,dword ptr ss:[esp+C]                 |
6F3B1BC2 | 52                       | push edx                                     |
6F3B1BC3 | 8D4424 14                | lea eax,dword ptr ss:[esp+14]                |
6F3B1BC7 | 50                       | push eax                                     |
6F3B1BC8 | 8D5424 20                | lea edx,dword ptr ss:[esp+20]                |
6F3B1BCC | 52                       | push edx                                     |
6F3B1BCD | 8D4424 28                | lea eax,dword ptr ss:[esp+28]                |
6F3B1BD1 | 50                       | push eax                                     |
6F3B1BD2 | 51                       | push ecx                                     |
6F3B1BD3 | C74424 20 00000000       | mov dword ptr ss:[esp+20],0                  |
6F3B1BDB | E8 40FEFFFF              | call game.6F3B1A20                           | <--- 调用核心计算逻辑
6F3B1BE0 | 85C0                     | test eax,eax                                 |
6F3B1BE2 | 74 36                    | je game.6F3B1C1A                             |
6F3B1BE4 | 8B4C24 10                | mov ecx,dword ptr ss:[esp+10]                |
6F3B1BE8 | 85C9                     | test ecx,ecx                                 |
6F3B1BEA | 74 0A                    | je game.6F3B1BF6                             |
6F3B1BEC | BA 01000000              | mov edx,1                                    |
6F3B1BF1 | E8 BAAA1000              | call game.6F4BC6B0                           |
6F3B1BF6 | 8B4C24 0C                | mov ecx,dword ptr ss:[esp+C]                 |
6F3B1BFA | 85C9                     | test ecx,ecx                                 |
6F3B1BFC | 74 0A                    | je game.6F3B1C08                             |
6F3B1BFE | BA 01000000              | mov edx,1                                    |
6F3B1C03 | E8 A8AA1000              | call game.6F4BC6B0                           |
6F3B1C08 | 8B4C24 04                | mov ecx,dword ptr ss:[esp+4]                 |
6F3B1C0C | 85C9                     | test ecx,ecx                                 |
6F3B1C0E | 74 0A                    | je game.6F3B1C1A                             |
6F3B1C10 | BA 01000000              | mov edx,1                                    |
6F3B1C15 | E8 96AA1000              | call game.6F4BC6B0                           |
6F3B1C1A | 8B0424                   | mov eax,dword ptr ss:[esp]                   |
6F3B1C1D | 83C4 08                  | add esp,8                                    |
6F3B1C20 | C2 0800                  | ret 8                                        |
```

#### 偏移：3B1A20 (核心计算算法)
此处包含 `0x3F1379E` 魔数，是最终混合三个文件 CRC 的地方。
```assembly
6F3B1A20 | 83EC 0C                  | sub esp,C                                    |
6F3B1A23 | 8B4424 1C                | mov eax,dword ptr ss:[esp+1C]                |
6F3B1A27 | 8B4C24 20                | mov ecx,dword ptr ss:[esp+20]                |
6F3B1A2B | 53                       | push ebx                                     |
6F3B1A2C | 55                       | push ebp                                     |
6F3B1A2D | 33DB                     | xor ebx,ebx                                  |
6F3B1A2F | 56                       | push esi                                     |
6F3B1A30 | 8B7424 20                | mov esi,dword ptr ss:[esp+20]                |
6F3B1A34 | 57                       | push edi                                     |
6F3B1A35 | 8B7C24 28                | mov edi,dword ptr ss:[esp+28]                |
6F3B1A39 | 891E                     | mov dword ptr ds:[esi],ebx                   |
6F3B1A3B | 891F                     | mov dword ptr ds:[edi],ebx                   |
6F3B1A3D | 33ED                     | xor ebp,ebp                                  |
6F3B1A3F | 395C24 34                | cmp dword ptr ss:[esp+34],ebx                |
6F3B1A43 | 8918                     | mov dword ptr ds:[eax],ebx                   |
6F3B1A45 | 8919                     | mov dword ptr ds:[ecx],ebx                   |
6F3B1A47 | 895C24 10                | mov dword ptr ss:[esp+10],ebx                |
6F3B1A4B | 896C24 14                | mov dword ptr ss:[esp+14],ebp                |
6F3B1A4F | 895C24 18                | mov dword ptr ss:[esp+18],ebx                |
6F3B1A53 | 75 22                    | jne game.6F3B1A77                            |
6F3B1A55 | 8B4C24 3C                | mov ecx,dword ptr ss:[esp+3C]                |
6F3B1A59 | 8D5424 18                | lea edx,dword ptr ss:[esp+18]                |
6F3B1A5D | 52                       | push edx                                     |
6F3B1A5E | 8D5424 18                | lea edx,dword ptr ss:[esp+18]                |
6F3B1A62 | E8 F9FCFFFF              | call game.6F3B1760                           |
6F3B1A67 | 85C0                     | test eax,eax                                 |
6F3B1A69 | 0F84 D6000000            | je game.6F3B1B45                             |
6F3B1A6F | 8B6C24 14                | mov ebp,dword ptr ss:[esp+14]                |
6F3B1A73 | 8B5C24 18                | mov ebx,dword ptr ss:[esp+18]                |
6F3B1A77 | 8D7C24 10                | lea edi,dword ptr ss:[esp+10]                |
6F3B1A7B | BE 1831936F              | mov esi,game.6F933118                        | esi:"common.j"
6F3B1A80 | E8 EBFEFFFF              | call game.6F3B1970                           |
6F3B1A85 | 837C24 34 00             | cmp dword ptr ss:[esp+34],0                  |
6F3B1A8A | 8B4C24 24                | mov ecx,dword ptr ss:[esp+24]                |
6F3B1A8E | 8901                     | mov dword ptr ds:[ecx],eax                   |
6F3B1A90 | 74 17                    | je game.6F3B1AA9                             |
6F3B1A92 | 85C0                     | test eax,eax                                 |
6F3B1A94 | 0F84 A3000000            | je game.6F3B1B3D                             |
6F3B1A9A | 8B5424 10                | mov edx,dword ptr ss:[esp+10]                |
6F3B1A9E | 8BC8                     | mov ecx,eax                                  |
6F3B1AA0 | E8 1BCBFEFF              | call game.6F39E5C0                           | 获取 common.j CRC
6F3B1AA5 | 8BE8                     | mov ebp,eax                                  |
6F3B1AA7 | EB 06                    | jmp game.6F3B1AAF                            |
6F3B1AA9 | 8B5424 30                | mov edx,dword ptr ss:[esp+30]                |
6F3B1AAD | 892A                     | mov dword ptr ds:[edx],ebp                   |
6F3B1AAF | 8D7C24 10                | lea edi,dword ptr ss:[esp+10]                |
6F3B1AB3 | BE C825946F              | mov esi,game.6F9425C8                        | esi:"blizzard.j"
6F3B1AB8 | C74424 10 00000000       | mov dword ptr ss:[esp+10],0                  |
6F3B1AC0 | E8 ABFEFFFF              | call game.6F3B1970                           |
6F3B1AC5 | 837C24 38 00             | cmp dword ptr ss:[esp+38],0                  |
6F3B1ACA | 8B4C24 28                | mov ecx,dword ptr ss:[esp+28]                |
6F3B1ACE | 8901                     | mov dword ptr ds:[ecx],eax                   |
6F3B1AD0 | 74 11                    | je game.6F3B1AE3                             |
6F3B1AD2 | 85C0                     | test eax,eax                                 |
6F3B1AD4 | 74 67                    | je game.6F3B1B3D                             |
6F3B1AD6 | 8B5424 10                | mov edx,dword ptr ss:[esp+10]                |
6F3B1ADA | 8BC8                     | mov ecx,eax                                  |
6F3B1ADC | E8 DFCAFEFF              | call game.6F39E5C0                           | 获取 blizzard.j CRC
6F3B1AE1 | 8BD8                     | mov ebx,eax                                  |
6F3B1AE3 | 8B5424 30                | mov edx,dword ptr ss:[esp+30]                |
6F3B1AE7 | 8B7424 20                | mov esi,dword ptr ss:[esp+20]                |
6F3B1AEB | 33DD                     | xor ebx,ebp                                  | CRC(bliz) ^ CRC(com)
6F3B1AED | C1C3 03                  | rol ebx,3                                    | ROL(..., 3)
6F3B1AF0 | 81F3 9E37F103            | xor ebx,3F1379E                              | ^ 0x3F1379E
6F3B1AF6 | C1C3 03                  | rol ebx,3                                    | ROL(..., 3)
6F3B1AF9 | 8D7C24 10                | lea edi,dword ptr ss:[esp+10]                |
6F3B1AFD | 891A                     | mov dword ptr ds:[edx],ebx                   |
6F3B1AFF | C74424 10 00000000       | mov dword ptr ss:[esp+10],0                  |
6F3B1B07 | E8 64FEFFFF              | call game.6F3B1970                           | 读取 war3map.j
6F3B1B0C | 85C0                     | test eax,eax                                 |
6F3B1B0E | 8B4C24 2C                | mov ecx,dword ptr ss:[esp+2C]                |
6F3B1B12 | 8901                     | mov dword ptr ds:[ecx],eax                   |
6F3B1B14 | 74 27                    | je game.6F3B1B3D                             |
6F3B1B16 | 8B5424 10                | mov edx,dword ptr ss:[esp+10]                |
6F3B1B1A | 8BC8                     | mov ecx,eax                                  |
6F3B1B1C | E8 9FCAFEFF              | call game.6F39E5C0                           | 获取 war3map.j CRC
6F3B1B21 | 8BD0                     | mov edx,eax                                  |
6F3B1B23 | 8B4424 30                | mov eax,dword ptr ss:[esp+30]                |
6F3B1B27 | 3310                     | xor edx,dword ptr ds:[eax]                   | CRC(map) ^ Temp
6F3B1B29 | 5F                       | pop edi                                      |
6F3B1B2A | 5E                       | pop esi                                      |
6F3B1B2B | C1C2 03                  | rol edx,3                                    | ROL(..., 3)
6F3B1B2E | 5D                       | pop ebp                                      |
6F3B1B2F | 8910                     | mov dword ptr ds:[eax],edx                   | 写入最终结果 (断点位置)
6F3B1B31 | B8 01000000              | mov eax,1                                    |
6F3B1B36 | 5B                       | pop ebx                                      |
6F3B1B37 | 83C4 0C                  | add esp,C                                    |
6F3B1B3A | C2 2000                  | ret 20                                       |
6F3B1B3D | 8B7C24 28                | mov edi,dword ptr ss:[esp+28]                |
6F3B1B41 | 8B7424 24                | mov esi,dword ptr ss:[esp+24]                |
6F3B1B45 | 8B0E                     | mov ecx,dword ptr ds:[esi]                   |
6F3B1B47 | 85C9                     | test ecx,ecx                                 |
6F3B1B49 | 74 0A                    | je game.6F3B1B55                             |
6F3B1B4B | BA 01000000              | mov edx,1                                    |
6F3B1B50 | E8 5BAB1000              | call game.6F4BC6B0                           |
6F3B1B55 | C706 00000000            | mov dword ptr ds:[esi],0                     |
6F3B1B5B | 8B0F                     | mov ecx,dword ptr ds:[edi]                   |
6F3B1B5D | 85C9                     | test ecx,ecx                                 |
6F3B1B5F | 74 0A                    | je game.6F3B1B6B                             |
6F3B1B61 | BA 01000000              | mov edx,1                                    |
6F3B1B66 | E8 45AB1000              | call game.6F4BC6B0                           |
6F3B1B6B | 8B7424 2C                | mov esi,dword ptr ss:[esp+2C]                |
6F3B1B6F | C707 00000000            | mov dword ptr ds:[edi],0                     |
6F3B1B75 | 8B0E                     | mov ecx,dword ptr ds:[esi]                   |
6F3B1B77 | 85C9                     | test ecx,ecx                                 |
6F3B1B79 | 74 0A                    | je game.6F3B1B85                             |
6F3B1B7B | BA 01000000              | mov edx,1                                    |
6F3B1B80 | E8 2BAB1000              | call game.6F4BC6B0                           |
6F3B1B85 | 8B4424 30                | mov eax,dword ptr ss:[esp+30]                |
6F3B1B89 | 5F                       | pop edi                                      |
6F3B1B8A | C706 00000000            | mov dword ptr ds:[esi],0                     |
6F3B1B90 | 5E                       | pop esi                                      |
6F3B1B91 | 5D                       | pop ebp                                      |
6F3B1B92 | C700 00000000            | mov dword ptr ds:[eax],0                     |
6F3B1B98 | 33C0                     | xor eax,eax                                  |
6F3B1B9A | 5B                       | pop ebx                                      |
6F3B1B9B | 83C4 0C                  | add esp,C                                    |
6F3B1B9E | C2 2000                  | ret 20                                       |
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
1.  **特征码**：`0x03F1379E` 是 War3 1.26 版本特有的 XOR Key。此值用于在 1.22 及以上版本中进行“环境校验”。
2.  **过检测思路**：
    *   **方法一 (内存 Hook)**：在 `6F3B1B2F` 处（最后一次 `MOV [EAX], EDX`）下钩子，将 EDX 修改为原始地图的 Checksum。
    *   **方法二 (Storm Hook)**：Hook `Storm.dll` 的文件读取或 CRC 函数，当请求 `war3map.j` 时欺骗游戏返回原始文件的 CRC。
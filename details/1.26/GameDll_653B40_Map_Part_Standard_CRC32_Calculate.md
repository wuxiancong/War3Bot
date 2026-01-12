# 魔兽争霸 III (W3GS) 网络包 CRC32 算法逆向核心文档
**Warcraft III Packet Checksum (CRC32) Algorithm Reverse Engineering**

*   **目标模块**: `Game.dll`
*   **分析基址**: `0x6F000000`
*   **核心入口**: `Game.dll + 0x653B40` (`0x6F653B40`)
*   **算法类型**: IEEE 802.3 CRC-32 (Standard)

---

```mermaid
graph TD
    Entry[入口 6F653B40] --> CheckArgs{参数检查 ECX/EDX}
    CheckArgs --NULL/Len=0--> InitFail[返回 -1]
    CheckArgs --Valid--> InitCRC[初始化 EAX = 0xFFFFFFFF]
    
    InitCRC --> LoopStart[循环起点 6F653B70]
    
    subgraph CoreLoop [标准 CRC32 核心循环]
        LoopStart --> FetchByte[读取字节 MOVZX ECX, [ESI]]
        FetchByte --> CalcIndex[计算索引: (CRC ^ Byte) & 0xFF]
        CalcIndex --> TableLookup[查表: [ECX*4 + 6F970A30]]
        TableLookup --> ShiftCRC[右移: CRC >> 8]
        ShiftCRC --> XorCRC[异或: CRC ^ TableVal]
        XorCRC --> NextIter[指针++, 长度--]
    end

    NextIter --Length > 0--> LoopStart
    NextIter --Length == 0--> Finalize[取反 NOT EAX]
    Finalize --> Return[返回结果]
```

## 1. 核心架构与偏移概览 (Architecture & Offsets)

| 逻辑层级 | 功能描述 | 绝对地址 | **基址偏移 (Offset)** |
| :--- | :--- | :--- | :--- |
| **Interface** | **算法主入口 (Main)** | `6F653B40` | **`+ 653B40`** |
| **Kernel** | **核心循环 (Loop)** | `6F653B70` | **`+ 653B70`** |
| **Kernel** | 异常断言 (Assert) | `6F653B53` | **`+ 653B53`** |
| **Data** | **CRC32 查找表 (Table)** | `6F970A30` | **`+ 970A30`** |

---

## 2. 第一层：接口层 (The Interface)

### 2.1 初始化与参数校验 `CRC32_Init`
*   **Location**: `Game.dll + 0x653B40`
*   **输入**:
    *   `ECX`: 数据缓冲区指针 (Buffer Pointer)
    *   `EDX`: 数据长度 (Length)
*   **输出**: `EAX`: 计算出的 CRC32 值

```asm
; 参数校验
6F653B40 | 85C9             | test ecx, ecx             ; 检查 Buffer 是否为 NULL
6F653B42 | 75 08            | jne game.6F653B4C         ; 有效则跳转
6F653B44 | 85D2             | test edx, edx             ; 检查 Length 是否为 0
6F653B46 | 74 04            | je game.6F653B4C          ; 长度为0也跳转(但在下一行初始化)

; 异常/特殊初始化路径 (Dead Code/Assert Path)
6F653B48 | 33C0             | xor eax, eax              ; EAX = 0
6F653B4A | EB 03            | jmp game.6F653B4F

; [标准初始化]
6F653B4C | 83C8 FF          | or eax, FFFFFFFF          ; EAX = 0xFFFFFFFF (标准 CRC 初始值)

; 断言检查 (如果走入 3B48 分支且 EAX=0，会触发 Storm 错误)
6F653B4F | 85C0             | test eax, eax
6F653B51 | 75 0A            | jne game.6F653B5D         ; 正常情况跳转至循环准备
6F653B53 | 6A 57            | push 57                   ; Error Code 87 (INVALID_PARAMETER)?
6F653B55 | E8 527A0900      | call <JMP.&Ordinal#465>   ; Storm.SErrSetLastError
6F653B5C | C3               | ret

; 循环准备
6F653B5D | 83C8 FF          | or eax, FFFFFFFF          ; 再次确保 EAX = -1
6F653B60 | 85D2             | test edx, edx             ; 再次检查长度
6F653B62 | 56               | push esi                  ; 保存寄存器
6F653B63 | 8BF1             | mov esi, ecx              ; ESI = Buffer Pointer
6F653B65 | 74 28            | je game.6F653B8F          ; 长度为0直接去取反结束
```

---

## 3. 第二层：内核层 (The Kernel)

### 3.1 标准循环 `CRC32_Update`
*   **Location**: `Game.dll + 0x653B70`
*   **功能**: 标准的查表法 CRC32 实现。

```asm
; [核心循环入口]
6F653B70 | 0FB60E           | movzx ecx, byte ptr [esi] ; [LOAD] 取出一个字节 (无符号扩展!)
                                                        ; 对应代码: (unsigned char)*buf

6F653B73 | 33C8             | xor ecx, eax              ; [XOR] 与当前 CRC 低位异或
6F653B75 | 81E1 FF000000    | and ecx, FF               ; [MASK] 取最低 8 位作为索引
                                                        ; 对应代码: (crc ^ (*p)) & 0xFF

6F653B7B | C1E8 08          | shr eax, 8                ; [SHIFT] CRC 右移 8 位
                                                        ; 对应代码: crc >> 8

; [查表与合并]
; 6F970A30 是 CRC 表的基地址
6F653B7E | 33048D 300A976F  | xor eax, [ecx*4 + 6F970A30] ; CRC = (CRC >> 8) ^ Table[Index]

6F653B85 | 83EA 01          | sub edx, 1                ; Length--
6F653B88 | 83C6 01          | add esi, 1                ; Pointer++
6F653B8B | 85D2             | test edx, edx             ; Check Loop
6F653B8D | 75 E1            | jne game.6F653B70         ; Continue Loop
```

### 3.2 结算 `CRC32_Final`
*   **Location**: `Game.dll + 0x653B8F`
*   **功能**: 结果取反。

```asm
6F653B8F | F7D0             | not eax                   ; [NOT] 结果取反 (~CRC)
                                                        ; 对应代码: return crc ^ 0xFFFFFFFF
6F653B91 | 5E               | pop esi                   ; 恢复寄存器
6F653B92 | C3               | ret                       ; 返回
```

---

## 4. 逻辑还原 (Reconstruction)

### 4.1 算法特征验证
*   **多项式**: `0xEDB88320` (IEEE 802.3)
*   **初始值**: `0xFFFFFFFF` (`6F653B4C`)
*   **结果处理**: 取反 (`6F653B8F`)
*   **查找表地址**: `0x6F970A30`
    *   验证表项: 若 `ECX` (Index) 为 `0xB7`
    *   计算地址: `0xB7 * 4 + 0x6F970A30 = 0x6F970D0C`
    *   你的日志验证: `dword ptr ds:[game.6F970D0C] = 5505262F` (与标准表完全一致)

### 4.2 C++ 实现代码 (Direct Port)

这是对汇编逻辑的直接 C++ 翻译，用于验证理解：

```cpp
#include <cstdint>
#include <vector>

// 偏移：0x970A30
// 实际可以直接使用 zlib 的 crc32()，这里为了展示逆向逻辑
uint32_t Game_CRC32_6F653B40(const void* buffer, size_t length) {
    if (!buffer || length == 0) return 0xFFFFFFFF; // 对应 3B4C 的初始化

    // [Init] 6F653B4C
    uint32_t eax = 0xFFFFFFFF;
    
    // [Setup] 6F653B63
    const uint8_t* esi = static_cast<const uint8_t*>(buffer);
    size_t edx = length;

    // [Table Base] 6F970A30
    // 为了运行需要填入标准表，此处略去完整表内容
    static const uint32_t crc_table[256] = { 0x00000000, 0x77073096, /* ... */ }; 

    // [Loop] 6F653B70
    while (edx > 0) {
        // movzx ecx, byte ptr [esi]
        uint32_t ecx = *esi;

        // xor ecx, eax  &  and ecx, FF
        uint8_t index = (ecx ^ eax) & 0xFF;

        // shr eax, 8
        eax = eax >> 8;

        // xor eax, table[index]
        eax = eax ^ crc_table[index];

        // next
        edx--;
        esi++;
    }

    // [Final] 6F653B8F
    return ~eax; // not eax
}
```

### 4.3 最佳实践代码 (Zlib Wrapper)

由于逆向分析确认了它是标准算法，生产环境应直接使用 Zlib：

```cpp
#include <zlib.h>

quint32 CalculateW3GSChecksum(const QByteArray &data) {
    // Game.dll 6F653B40 等价于:
    uLong crc = crc32(0L, Z_NULL, 0); // Init
    crc = crc32(crc, (const Bytef*)data.constData(), data.size());
    return (quint32)crc;
}
```
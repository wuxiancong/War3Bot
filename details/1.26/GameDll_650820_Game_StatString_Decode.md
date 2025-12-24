# War3 Game StatString Decode 解码函数逆向分析

- **模块**: `Game.dll` (War3 1.26a)
- **地址**: `Game.dll + 0x650820`
- **函数**: `DecodeStatString`
- **功能**: 将输入的编码 StatString（二进制流）还原为明文数据，并写入 `CBitStream` 对象。

---

## 1. 算法核心逻辑 (数学模型)

该解密算法本质上是一个**基于 8 字节周期的流式位移异或算法**。它遍历输入字节流 $S$，维护一个滚动密钥 $K$（Rolling Key）。

### 符号定义
*   $S[i]$: 输入字节流的第 $i$ 个字节。
*   $K$: 滚动密钥，初始值为 $0$。
*   $Out$: 解密后的输出字节流。

### 解密流程
遍历输入数组 $S$，对于索引 $i = 0, 1, 2, ...$：

#### A. 头部字节 (Header Byte)
当 $i \pmod 8 = 0$ 时，该字节仅用于重置密钥，**不产生输出**。
$$ K_{new} = S[i] \gg 1 $$

#### B. 数据字节 (Data Byte)
当 $i \pmod 8 \neq 0$ 时，该字节为数据负载。
1.  **计算修正位 (Delta Bit)**:
    $$ \Delta = (S[i] \oplus K) \ \& \ 1 $$
2.  **计算输出 (Output)**:
    $$ Out_{append} = S[i] \oplus \Delta $$
    > **注意**: 此时输出字节仅在最低位 (LSB) 可能发生翻转，高 7 位保持原样。
3.  **更新密钥**:
    $$ K_{new} = K \gg 1 $$

### 终止条件
*   遇到 `0x00` 字节：**成功**。
*   输入流超过 128 (`0x80`) 字节：**失败** (缓冲区溢出保护)。

---

## 2. 算法验证 (Python 实现)

为了验证上述逻辑，以下是等效的 Python 代码实现：

```python
def decode_stat_string(data_bytes):
    """
    Game.dll + 0x650820 算法复现
    """
    key = 0
    decoded_output = []
    
    # 限制最大长度 128 (0x80)
    limit = min(len(data_bytes), 128)

    for i in range(limit):
        byte = data_bytes[i]
        
        # 遇到 0x00 结束符则终止
        if byte == 0:
            break

        # 分支 A: 头部字节 (Header) - 仅更新 Key
        if (i % 8) == 0:
            key = byte >> 1
        # 分支 B: 数据字节 (Data) - 解密并输出
        else:
            # 算法: Out = In ^ ((In ^ Key) & 1)
            delta = (byte ^ key) & 1
            out_byte = byte ^ delta
            
            decoded_output.append(out_byte)
            
            # 滚动 Key
            key = key >> 1
            
    return bytes(decoded_output)
```

---

## 3. 反汇编详细注释 (Proof)

以下代码证实了上述数学模型的正确性。

```assembly
; ========================================================================
; 函数入口: DecodeStatString
; ECX = CBitStream 对象指针 (输出目标)
; ESP+18 = StatString 原始数据指针 (输入源)
; ========================================================================
6F650820 | 51                       | push ecx                                                 |
6F650821 | 53                       | push ebx                                                 |
6F650822 | 55                       | push ebp                                                 |
6F650823 | 56                       | push esi                                                 |
6F650824 | 57                       | push edi                                                 |
6F650825 | 8BF9                     | mov edi,ecx                                              | EDI = CBitStream 对象
6F650827 | 8B07                     | mov eax,dword ptr ds:[edi]                               |
6F650829 | 8B50 1C                  | mov edx,dword ptr ds:[eax+1C]                            | 获取虚函数 (Reset/Init)
6F65082C | 33DB                     | xor ebx,ebx                                              | EBX = 输入索引 i
6F65082E | 33F6                     | xor esi,esi                                              | ESI = 输出计数
6F650830 | FFD2                     | call edx                                                 | 初始化 BitStream
6F650832 | 8B6C24 18                | mov ebp,dword ptr ss:[esp+18]                            | EBP = 输入数据指针
6F650836 | 32C9                     | xor cl,cl                                                | CL = 初始密钥 Key = 0
6F650838 | EB 0A                    | jmp game.6F650844                                        | -> 进入循环

; --- 循环重入点 ---
6F65083A | 8D9B 00000000            | lea ebx,dword ptr ds:[ebx]                               | (Padding)
6F650840 | 8A4C24 18                | mov cl,byte ptr ss:[esp+18]                              | 从栈恢复 Key (CL)

; ========================================================================
; 主解密循环
; ========================================================================
6F650844 | 8A042B                   | mov al,byte ptr ds:[ebx+ebp]                             | 读取 S[i] -> AL
6F650847 | 84C0                     | test al,al                                               | 检查 0x00 结束符
6F650849 | 74 3F                    | je game.6F65088A                                         | -> 若为 0，成功跳转

6F65084B | F6C3 07                  | test bl,7                                                | 检查 i % 8
6F65084E | 75 08                    | jne game.6F650858                                        | -> 不为 0，跳转至 [数据字节]

; [分支 A] 头部字节 (i % 8 == 0)
; 逻辑: Key = S[i] >> 1
6F650850 | D0E8                     | shr al,1                                                 | AL 右移 1 位
6F650852 | 884424 18                | mov byte ptr ss:[esp+18],al                              | 更新栈上的 Key
6F650856 | EB 27                    | jmp game.6F65087F                                        | 跳过输出，处理下一个

; [分支 B] 数据字节 (i % 8 != 0)
; 逻辑: Delta = (In ^ Key) & 1; Out = In ^ Delta; Key = Key >> 1
6F650858 | 8AD0                     | mov dl,al                                                | DL = S[i]
6F65085A | 32D1                     | xor dl,cl                                                | DL = S[i] ^ Key
6F65085C | 80E2 01                  | and dl,1                                                 | DL = (S[i] ^ Key) & 1 (即 Delta)
6F65085F | 32C2                     | xor al,dl                                                | AL = S[i] ^ Delta (即 Out)
6F650861 | D0E9                     | shr cl,1                                                 | Key = Key >> 1

; --- 写入解密结果 ---
6F650863 | 83FE 68                  | cmp esi,68                                               | 检查 BitStream 容量 (104字节)
6F650866 | 884424 10                | mov byte ptr ss:[esp+10],al                              | 暂存 Out
6F65086A | 884C24 18                | mov byte ptr ss:[esp+18],cl                              | 保存 Key
6F65086E | 73 2C                    | jae game.6F65089C                                        | -> 溢出报错
6F650870 | 8B4424 10                | mov eax,dword ptr ss:[esp+10]                            |
6F650874 | 50                       | push eax                                                 |
6F650875 | 8BCF                     | mov ecx,edi                                              |
6F650877 | E8 E418E7FF              | call game.6F4C2160                                       | CBitStream::WriteBits(Out)
6F65087C | 83C6 01                  | add esi,1                                                | 输出计数++

; --- 循环控制 ---
6F65087F | 83C3 01                  | add ebx,1                                                | 输入索引 i++
6F650882 | 81FB 80000000            | cmp ebx,80                                               | 检查 i < 128
6F650888 | 72 B6                    | jb game.6F650840                                         | -> 继续循环

; [成功返回]
6F65088A | ... (省略堆栈恢复) ...
6F650895 | F7D8                     | neg eax                                                  | 返回 1 (True)
6F650899 | C2 0400                  | ret 4

; [失败返回]
6F65089F | 33C0                     | xor eax,eax                                              | 返回 0 (False)
6F6508A3 | C2 0400                  | ret 4
```

---
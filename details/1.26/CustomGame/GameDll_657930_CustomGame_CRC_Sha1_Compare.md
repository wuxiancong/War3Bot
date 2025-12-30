好的，这是对您提供的反汇编代码的 Markdown 文档总结。

---

# Warcraft III 地图校验 (Map Check) 核心逻辑分析

## 概述

本文档分析了 Warcraft III (1.26a) `game.dll` 中负责处理来自 PVPGN 服务端的地图校验请求的核心函数。通过 `x64dbg` 调试，我们定位并解析了两个关键函数，揭示了地图校验失败并导致客户端发送 `W3GS_MAPSIZE` (0x42, Size=0) 数据包的根本原因。

**环境:**
- **客户端:** Warcraft III (1.26a)
- **模块:** `game.dll`
- **基地址:** `6F000000`

---

## 1. 核心对比函数: `game.dll+657930`

此函数是整个地图校验逻辑的**决策中心**。它负责接收服务端的校验数据，调用子函数计算本地数据，并执行最终的比对。

**地址**: `6F679630` (`game.dll+657930`)


```assembly
6F657930  | 83EC 44                 | sub esp,44                          |
6F657933  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]     |
6F657938  | 33C4                    | xor eax,esp                         |
6F65793A  | 894424 40               | mov dword ptr ss:[esp+40],eax       |
6F65793E  | 8B4424 48               | mov eax,dword ptr ss:[esp+48]       |
6F657942  | 53                      | push ebx                            |
6F657943  | 56                      | push esi                            |
6F657944  | 57                      | push edi                            |
6F657945  | 8BF9                    | mov edi,ecx                         |
6F657947  | 8B4C24 58               | mov ecx,dword ptr ss:[esp+58]       |
6F65794B  | 8B5C24 70               | mov ebx,dword ptr ss:[esp+70]       |
6F65794F  | 894C24 24               | mov dword ptr ss:[esp+24],ecx       |
6F657953  | 8B4C24 60               | mov ecx,dword ptr ss:[esp+60]       |
6F657957  | 8BF2                    | mov esi,edx                         |
6F657959  | 8B5424 5C               | mov edx,dword ptr ss:[esp+5C]       |
6F65795D  | 894C24 2C               | mov dword ptr ss:[esp+2C],ecx       |
6F657961  | 8B4C24 68               | mov ecx,dword ptr ss:[esp+68]       |
6F657965  | 895424 28               | mov dword ptr ss:[esp+28],edx       |
6F657969  | 8B5424 64               | mov edx,dword ptr ss:[esp+64]       |
6F65796D  | 894C24 34               | mov dword ptr ss:[esp+34],ecx       |
6F657971  | 8B4C24 6C               | mov ecx,dword ptr ss:[esp+6C]       |
6F657975  | 51                      | push ecx                            |
6F657976  | 895424 34               | mov dword ptr ss:[esp+34],edx       |
6F65797A  | 8B5424 78               | mov edx,dword ptr ss:[esp+78]       |
6F65797E  | 895424 20               | mov dword ptr ss:[esp+20],edx       |
6F657982  | 8B5424 7C               | mov edx,dword ptr ss:[esp+7C]       |
6F657986  | 8D4C24 3C               | lea ecx,dword ptr ss:[esp+3C]       |
6F65798A  | 51                      | push ecx                            |
6F65798B  | 50                      | push eax                            |
6F65798C  | 895424 20               | mov dword ptr ss:[esp+20],edx       |
6F657990  | 8D5424 24               | lea edx,dword ptr ss:[esp+24]       |
6F657994  | 52                      | push edx                            |
6F657995  | 8D4424 30               | lea eax,dword ptr ss:[esp+30]       |
6F657999  | 50                      | push eax                            |
6F65799A  | 8D4C24 20               | lea ecx,dword ptr ss:[esp+20]       |
6F65799E  | 51                      | push ecx                            |
6F65799F  | 8D5424 28               | lea edx,dword ptr ss:[esp+28]       |
6F6579A3  | 8BCF                    | mov ecx,edi                         |
6F6579A5  | E8 C6FDFFFF             | call game.6F657770                  |
6F6579AA  | 83F8 01                 | cmp eax,1                           |
6F6579AD  | 0F85 BE000000           | jne game.6F657A71                   |
6F6579B3  | 397424 18               | cmp dword ptr ss:[esp+18],esi       |
6F6579B7  | 0F85 B4000000           | jne game.6F657A71                   |
6F6579BD  | B8 14000000             | mov eax,14                          |
6F6579C2  | 8D4C24 24               | lea ecx,dword ptr ss:[esp+24]       |
6F6579C6  | 8D5424 38               | lea edx,dword ptr ss:[esp+38]       |
6F6579CA  | 55                      | push ebp                            |
6F6579CB  | EB 03                   | jmp game.6F6579D0                   |
6F6579CD  | 8D49 00                 | lea ecx,dword ptr ds:[ecx]          |
6F6579D0  | 8B32                    | mov esi,dword ptr ds:[edx]          |
6F6579D2  | 3B31                    | cmp esi,dword ptr ds:[ecx]          |
6F6579D4  | 75 12                   | jne game.6F6579E8                   |
6F6579D6  | 83E8 04                 | sub eax,4                           |
6F6579D9  | 83C1 04                 | add ecx,4                           |
6F6579DC  | 83C2 04                 | add edx,4                           |
6F6579DF  | 83F8 04                 | cmp eax,4                           |
6F6579E2  | 73 EC                   | jae game.6F6579D0                   |
6F6579E4  | 85C0                    | test eax,eax                        |
6F6579E6  | 74 5D                   | je game.6F657A45                    |
6F6579E8  | 0FB632                  | movzx esi,byte ptr ds:[edx]         |
6F6579EB  | 0FB629                  | movzx ebp,byte ptr ds:[ecx]         |
6F6579EE  | 2BF5                    | sub esi,ebp                         |
6F6579F0  | 75 45                   | jne game.6F657A37                   |
6F6579F2  | 83E8 01                 | sub eax,1                           |
6F6579F5  | 83C1 01                 | add ecx,1                           |
6F6579F8  | 83C2 01                 | add edx,1                           |
6F6579FB  | 85C0                    | test eax,eax                        |
6F6579FD  | 74 46                   | je game.6F657A45                    |
6F6579FF  | 0FB632                  | movzx esi,byte ptr ds:[edx]         |
6F657A02  | 0FB629                  | movzx ebp,byte ptr ds:[ecx]         |
6F657A05  | 2BF5                    | sub esi,ebp                         |
6F657A07  | 75 2E                   | jne game.6F657A37                   |
6F657A09  | 83E8 01                 | sub eax,1                           |
6F657A0C  | 83C1 01                 | add ecx,1                           |
6F657A0F  | 83C2 01                 | add edx,1                           |
6F657A12  | 85C0                    | test eax,eax                        |
6F657A14  | 74 2F                   | je game.6F657A45                    |
6F657A16  | 0FB632                  | movzx esi,byte ptr ds:[edx]         |
6F657A19  | 0FB629                  | movzx ebp,byte ptr ds:[ecx]         |
6F657A1C  | 2BF5                    | sub esi,ebp                         |
6F657A1E  | 75 17                   | jne game.6F657A37                   |
6F657A20  | 83E8 01                 | sub eax,1                           |
6F657A23  | 83C1 01                 | add ecx,1                           |
6F657A26  | 83C2 01                 | add edx,1                           |
6F657A29  | 85C0                    | test eax,eax                        |
6F657A2B  | 74 18                   | je game.6F657A45                    |
6F657A2D  | 0FB632                  | movzx esi,byte ptr ds:[edx]         |
6F657A30  | 0FB611                  | movzx edx,byte ptr ds:[ecx]         |
6F657A33  | 2BF2                    | sub esi,edx                         |
6F657A35  | 74 0E                   | je game.6F657A45                    |
6F657A37  | 85F6                    | test esi,esi                        |
6F657A39  | B8 01000000             | mov eax,1                           |
6F657A3E  | 7F 07                   | jg game.6F657A47                    |
6F657A40  | 83C8 FF                 | or eax,FFFFFFFF                     |
6F657A43  | EB 02                   | jmp game.6F657A47                   |
6F657A45  | 33C0                    | xor eax,eax                         |
6F657A47  | 85C0                    | test eax,eax                        |
6F657A49  | 5D                      | pop ebp                             |
6F657A4A  | 75 25                   | jne game.6F657A71                   |
6F657A4C  | 8B4C24 14               | mov ecx,dword ptr ss:[esp+14]       |
6F657A50  | 8D70 01                 | lea esi,dword ptr ds:[eax+1]        |
6F657A53  | 8B4424 7C               | mov eax,dword ptr ss:[esp+7C]       |
6F657A57  | 50                      | push eax                            |
6F657A58  | 57                      | push edi                            |
6F657A59  | 51                      | push ecx                            |
6F657A5A  | E8 653B0900             | call <JMP.&Ordinal#501>             |
6F657A5F  | 8B5424 10               | mov edx,dword ptr ss:[esp+10]       |
6F657A63  | 8B4424 0C               | mov eax,dword ptr ss:[esp+C]        |
6F657A67  | 8B4C24 1C               | mov ecx,dword ptr ss:[esp+1C]       |
6F657A6B  | 8913                    | mov dword ptr ds:[ebx],edx          |
6F657A6D  | 8901                    | mov dword ptr ds:[ecx],eax          |
6F657A6F  | EB 0F                   | jmp game.6F657A80                   |
6F657A71  | 8D5424 0C               | lea edx,dword ptr ss:[esp+C]        |
6F657A75  | 8D4C24 10               | lea ecx,dword ptr ss:[esp+10]       |
6F657A79  | 33F6                    | xor esi,esi                         |
6F657A7B  | E8 80F3FFFF             | call game.6F656E00                  |
6F657A80  | 8B4C24 4C               | mov ecx,dword ptr ss:[esp+4C]       |
6F657A84  | 5F                      | pop edi                             |
6F657A85  | 8BC6                    | mov eax,esi                         |
6F657A87  | 5E                      | pop esi                             |
6F657A88  | 5B                      | pop ebx                             |
6F657A89  | 33CC                    | xor ecx,esp                         |
6F657A8B  | E8 C9951800             | call game.6F7E1059                  |
6F657A90  | 83C4 44                 | add esp,44                          |
6F657A93  | C2 2C00                 | ret 2C                              |
```
## 1. 核心对比函数: `game.dll+657930`

此函数是整个地图校验逻辑的**决策中心**。它负责接收服务端的校验数据，调用子函数计算本地数据，并执行最终的比对。

**地址**: `6F679630` (`game.dll+657930`)

### 工作流程:

1.  **接收参数**: 函数通过栈和寄存器接收来自服务端的地图 **CRC32** 和 **SHA1** (20字节) 哈希值。
2.  **计算本地哈希**:
    ```assembly
    6F6579A5  | E8 C6FDFFFF             | call game.6F657770
    ```
    调用 `game.dll+657770` 函数，该函数负责打开并计算本地地图文件的 CRC32 和 SHA1。
3.  **校验 CRC32**:
    ```assembly
    6F6579B3  | 397424 18               | cmp dword ptr ss:[esp+18],esi
    6F6579B7  | 0F85 B4000000           | jne game.6F657A71
    ```
    首先对 CRC32 进行快速比对。如果 CRC 不匹配，则直接跳转到失败处理流程。
4.  **校验 SHA1 (关键)**:
    ```assembly
    ; --- 准备对比 SHA1 ---
    6F6579C2  | 8D4C24 24               | lea ecx,dword ptr ss:[esp+24]       ; ecx -> 本地计算的 SHA1
    6F6579C6  | 8D5424 38               | lea edx,dword ptr ss:[esp+38]       ; edx -> 服务端发送的 SHA1

    ; --- 循环对比 (优化的 memcmp) ---
    6F6579D0  | 8B32                    | mov esi,dword ptr ds:[edx]          ; 读取服务端 SHA1 的 4 字节
    6F6579D2  | 3B31                    | cmp esi,dword ptr ds:[ecx]          ; 对比本地 SHA1 的 4 字节
    6F6579D4  | 75 12                   | jne game.6F6579E8                   ; 如果不相等，跳转到失败处理
    ```
    这是问题的核心。函数以 4 字节为单位循环比较 SHA1 哈希。**任何不匹配都会导致 `jne` 指令跳转**，从而使整个校验失败。
5.  **返回结果**:
    - 如果所有比对都通过，函数最终会返回 `1` (成功)。
    - 如果任何比对失败，函数会跳转到 `6F657A71`，最终返回 `0` (失败)。

---

## 2. 本地哈希计算函数: `game.dll+657770`

此函数是一个**工具函数**，负责实际的文件 I/O 和哈希计算。

**地址**: `6F657770` (`game.dll+657770`)

```assembly
6F657770  | 51                      | push ecx                            |
6F657771  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F657775  | 53                      | push ebx                            |
6F657776  | 55                      | push ebp                            |
6F657777  | 8B6C24 14               | mov ebp,dword ptr ss:[esp+14]       |
6F65777B  | 56                      | push esi                            |
6F65777C  | 57                      | push edi                            |
6F65777D  | 8B7C24 18               | mov edi,dword ptr ss:[esp+18]       |
6F657781  | 8BD9                    | mov ebx,ecx                         |
6F657783  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]       |
6F657787  | 51                      | push ecx                            |
6F657788  | 6A 01                   | push 1                              |
6F65778A  | 8BF2                    | mov esi,edx                         |
6F65778C  | C706 00000000           | mov dword ptr ds:[esi],0            |
6F657792  | C707 00000000           | mov dword ptr ds:[edi],0            |
6F657798  | 53                      | push ebx                            |
6F657799  | C745 00 00000000        | mov dword ptr ss:[ebp],0            |
6F6577A0  | 6A 00                   | push 0                              |
6F6577A2  | C700 00000000           | mov dword ptr ds:[eax],0            |
6F6577A8  | C74424 28 00000000      | mov dword ptr ss:[esp+28],0         |
6F6577B0  | E8 E53D0900             | call <JMP.&Ordinal#268>             |
6F6577B5  | 85C0                    | test eax,eax                        |
6F6577B7  | 75 08                   | jne game.6F6577C1                   |
6F6577B9  | 8D58 2C                 | lea ebx,dword ptr ds:[eax+2C]       |
6F6577BC  | E9 D7000000             | jmp game.6F657898                   |
6F6577C1  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F6577C5  | 8D5424 10               | lea edx,dword ptr ss:[esp+10]       |
6F6577C9  | 52                      | push edx                            |
6F6577CA  | 50                      | push eax                            |
6F6577CB  | E8 503F0900             | call <JMP.&Ordinal#265>             |
6F6577D0  | 837C24 10 00            | cmp dword ptr ss:[esp+10],0         |
6F6577D5  | 0F87 B8000000           | ja game.6F657893                    |
6F6577DB  | 85C0                    | test eax,eax                        |
6F6577DD  | 0F84 B0000000           | je game.6F657893                    |
6F6577E3  | 3D 00008000             | cmp eax,800000                      |
6F6577E8  | 0F87 A5000000           | ja game.6F657893                    |
6F6577EE  | 6A 00                   | push 0                              |
6F6577F0  | 68 EB030000             | push 3EB                            |
6F6577F5  | 68 8C10976F             | push game.6F97108C                  |
6F6577FA  | 50                      | push eax                            |
6F6577FB  | 8907                    | mov dword ptr ds:[edi],eax          |
6F6577FD  | E8 B03D0900             | call <JMP.&Ordinal#401>             |
6F657802  | 85C0                    | test eax,eax                        |
6F657804  | 8906                    | mov dword ptr ds:[esi],eax          |
6F657806  | 0F84 87000000           | je game.6F657893                    |
6F65780C  | 8B17                    | mov edx,dword ptr ds:[edi]          |
6F65780E  | 6A 00                   | push 0                              |
6F657810  | 8D4C24 20               | lea ecx,dword ptr ss:[esp+20]       |
6F657814  | 51                      | push ecx                            |
6F657815  | 52                      | push edx                            |
6F657816  | 50                      | push eax                            |
6F657817  | 8B4424 28               | mov eax,dword ptr ss:[esp+28]       |
6F65781B  | 50                      | push eax                            |
6F65781C  | C74424 30 00000000      | mov dword ptr ss:[esp+30],0         |
6F657824  | E8 F13E0900             | call <JMP.&Ordinal#269>             |
6F657829  | 85C0                    | test eax,eax                        |
6F65782B  | 74 5F                   | je game.6F65788C                    |
6F65782D  | 8B17                    | mov edx,dword ptr ds:[edi]          |
6F65782F  | 395424 1C               | cmp dword ptr ss:[esp+1C],edx       |
6F657833  | 75 57                   | jne game.6F65788C                   |
6F657835  | 8B0E                    | mov ecx,dword ptr ds:[esi]          |
6F657837  | E8 04C3FFFF             | call game.6F653B40                  |
6F65783C  | 8B4C24 24               | mov ecx,dword ptr ss:[esp+24]       |
6F657840  | 85C9                    | test ecx,ecx                        |
6F657842  | 8945 00                 | mov dword ptr ss:[ebp],eax          |
6F657845  | 74 11                   | je game.6F657858                    |
6F657847  | 8B5424 20               | mov edx,dword ptr ss:[esp+20]       |
6F65784B  | 52                      | push edx                            |
6F65784C  | 53                      | push ebx                            |
6F65784D  | FFD1                    | call ecx                            |
6F65784F  | 85C0                    | test eax,eax                        |
6F657851  | 75 0B                   | jne game.6F65785E                   |
6F657853  | 8D58 2C                 | lea ebx,dword ptr ds:[eax+2C]       |
6F657856  | EB 40                   | jmp game.6F657898                   |
6F657858  | 8B4C24 20               | mov ecx,dword ptr ss:[esp+20]       |
6F65785C  | 8901                    | mov dword ptr ds:[ecx],eax          |
6F65785E  | 8B4424 2C               | mov eax,dword ptr ss:[esp+2C]       |
6F657862  | 85C0                    | test eax,eax                        |
6F657864  | 74 11                   | je game.6F657877                    |
6F657866  | 8B5424 28               | mov edx,dword ptr ss:[esp+28]       |
6F65786A  | 52                      | push edx                            |
6F65786B  | 53                      | push ebx                            |
6F65786C  | FFD0                    | call eax                            |
6F65786E  | 85C0                    | test eax,eax                        |
6F657870  | 75 13                   | jne game.6F657885                   |
6F657872  | 8D58 2C                 | lea ebx,dword ptr ds:[eax+2C]       |
6F657875  | EB 21                   | jmp game.6F657898                   |
6F657877  | 8B07                    | mov eax,dword ptr ds:[edi]          |
6F657879  | 8B16                    | mov edx,dword ptr ds:[esi]          |
6F65787B  | 8B4C24 28               | mov ecx,dword ptr ss:[esp+28]       |
6F65787F  | 50                      | push eax                            |
6F657880  | E8 0BBE0100             | call game.6F673690                  |
6F657885  | BB 01000000             | mov ebx,1                           |
6F65788A  | EB 41                   | jmp game.6F6578CD                   |
6F65788C  | BB 34000000             | mov ebx,34                          |
6F657891  | EB 05                   | jmp game.6F657898                   |
6F657893  | BB 33000000             | mov ebx,33                          |
6F657898  | 8B06                    | mov eax,dword ptr ds:[esi]          |
6F65789A  | 85C0                    | test eax,eax                        |
6F65789C  | 74 12                   | je game.6F6578B0                    |
6F65789E  | 6A 00                   | push 0                              |
6F6578A0  | 68 19040000             | push 419                            |
6F6578A5  | 68 8C10976F             | push game.6F97108C                  |
6F6578AA  | 50                      | push eax                            |
6F6578AB  | E8 A83C0900             | call <JMP.&Ordinal#403>             |
6F6578B0  | 8B4C24 20               | mov ecx,dword ptr ss:[esp+20]       |
6F6578B4  | C706 00000000           | mov dword ptr ds:[esi],0            |
6F6578BA  | C707 00000000           | mov dword ptr ds:[edi],0            |
6F6578C0  | C745 00 00000000        | mov dword ptr ss:[ebp],0            |
6F6578C7  | C701 00000000           | mov dword ptr ds:[ecx],0            |
6F6578CD  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F6578D1  | 85C0                    | test eax,eax                        |
6F6578D3  | 74 06                   | je game.6F6578DB                    |
6F6578D5  | 50                      | push eax                            |
6F6578D6  | E8 953C0900             | call <JMP.&Ordinal#253>             |
6F6578DB  | 5F                      | pop edi                             |
6F6578DC  | 5E                      | pop esi                             |
6F6578DD  | 5D                      | pop ebp                             |
6F6578DE  | 8BC3                    | mov eax,ebx                         |
6F6578E0  | 5B                      | pop ebx                             |
6F6578E1  | 59                      | pop ecx                             |
6F6578E2  | C2 1800                 | ret 18                              |
```

### 工作流程:

1.  **打开文件**:
    ```assembly
    6F6577CB  | E8 503F0900             | call <JMP.&Ordinal#265>             ; SFileOpenFileEx
    ```
    通过调用 `Storm.dll` 的 `SFileOpenFileEx` (序数 265)，打开传入的本地地图文件路径 (例如 `"Maps\Download\DotA v6.83d.w3x"`)。
2.  **计算哈希**:
    ```assembly
    6F657837  | E8 04C3FFFF             | call game.6F653B40                  ; (推测) 核心哈希计算逻辑
    ```
    在打开文件后，调用内部函数读取文件内容并执行 CRC32 和 SHA1 算法。
3.  **返回数据**: 函数将计算出的 CRC32 和 SHA1 值写入由调用者（`game.dll+657930`）提供的内存地址，并返回一个成功状态码。

---

## 结论

整个地图校验流程清晰地分为**计算**和**对比**两个阶段。客户端崩溃或无法进入游戏房间的根本原因在于 **`game.dll+657930` 函数中的 SHA1 对比失败**。

服务端发送的 SHA1 哈希值 (`SHA1_Server`) 必须与客户端使用 `game.dll+657770` 函数计算出的本地地图文件的 SHA1 哈希值 (`SHA1_Local`) **完全一致**。

在本次调试中，由于服务端发送了一个错误的 SHA1，导致 `cmp` 指令在 `6F6579D2` 处判定失败，程序跳转至错误处理分支，最终外层逻辑根据 `0` 的返回值，构建并发送了表示“地图不存在/不匹配”的 `W3GS_MAPSIZE` 数据包。修正服务端发送的 SHA1 值为客户端本地计算出的正确值后，问题得到解决。
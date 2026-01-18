# War3 加载过程中直接到积分面板调试日志
**Error Code: 0x03 (地图验证不通过)**

![x64dbg调试截图](https://github.com/wuxiancong/War3Bot/raw/main/debug/images/Dota_Game_MAPERROR_LOADING_SHOW_SCORE_PANEL.PNG)

**环境 (Environment):**
- **客户端:** Warcraft III (v1.26.0.6401 / Game.dll)
- **服务端:** PVPGN
- **调试工具:** x64dbg
- **基地址:** 6F000000

## 1. 现象描述 (Symptom)
在游戏加载完成后直接到积分面板，无法进入游戏！

## 2. 寻找来源 (Error Source)

搜寻字符串 "Score"，在像 "ScoreScreenPageTab" 这样的字样下断点，会找到下面的反汇编。
这只是处理游戏结果并显示积分面板的函数，我们要寻找父函数是谁调用了它？为什么要调用它？

- **地址**：`6F3A6900`
- **偏移**：`game.dll + 3A6900`
- **作用**：`处理游戏结果`

```assembly
6F3A6900  | 81EC 0C020000           | sub esp,20C                                     |
6F3A6906  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]                 |
6F3A690B  | 33C4                    | xor eax,esp                                     |
6F3A690D  | 898424 08020000         | mov dword ptr ss:[esp+208],eax                  |
6F3A6914  | 56                      | push esi                                        |
6F3A6915  | 57                      | push edi                                        |
6F3A6916  | 8BF9                    | mov edi,ecx                                     |
6F3A6918  | 8DB7 FC020000           | lea esi,dword ptr ds:[edi+2FC]                  |
6F3A691E  | 8BCE                    | mov ecx,esi                                     |
6F3A6920  | E8 3BDD1100             | call game.6F4C4660                              |
6F3A6925  | 85C0                    | test eax,eax                                    |
6F3A6927  | 74 48                   | je game.6F3A6971                                |
6F3A6929  | 68 04010000             | push 104                                        |
6F3A692E  | 8BCE                    | mov ecx,esi                                     |
6F3A6930  | E8 2BDD1100             | call game.6F4C4660                              |
6F3A6935  | 8D5424 0C               | lea edx,dword ptr ss:[esp+C]                    |
6F3A6939  | 8BC8                    | mov ecx,eax                                     |
6F3A693B  | E8 605F2100             | call game.6F5BC8A0                              |
6F3A6940  | 68 04010000             | push 104                                        |
6F3A6945  | 8BCE                    | mov ecx,esi                                     |
6F3A6947  | E8 14DD1100             | call game.6F4C4660                              |
6F3A694C  | 8D9424 10010000         | lea edx,dword ptr ss:[esp+110]                  |
6F3A6953  | 8BC8                    | mov ecx,eax                                     |
6F3A6955  | E8 A65F2100             | call game.6F5BC900                              |
6F3A695A  | 807C24 08 00            | cmp byte ptr ss:[esp+8],0                       |
6F3A695F  | 74 10                   | je game.6F3A6971                                |
6F3A6961  | 8D9424 0C010000         | lea edx,dword ptr ss:[esp+10C]                  |
6F3A6968  | 8D4C24 08               | lea ecx,dword ptr ss:[esp+8]                    |
6F3A696C  | E8 DF5D2100             | call game.6F5BC750                              |
6F3A6971  | 83BF 24030000 00        | cmp dword ptr ds:[edi+324],0                    |
6F3A6978  | 75 1E                   | jne game.6F3A6998                               |
6F3A697A  | 8BCF                    | mov ecx,edi                                     |
6F3A697C  | E8 5FE0FFFF             | call game.6F3A49E0                              |
6F3A6981  | 5F                      | pop edi                                         |
6F3A6982  | 5E                      | pop esi                                         |
6F3A6983  | 8B8C24 08020000         | mov ecx,dword ptr ss:[esp+208]                  |
6F3A698A  | 33CC                    | xor ecx,esp                                     |
6F3A698C  | E8 C8A64300             | call game.6F7E1059                              |
6F3A6991  | 81C4 0C020000           | add esp,20C                                     |
6F3A6997  | C3                      | ret                                             |
6F3A6998  | 33D2                    | xor edx,edx                                     |
6F3A699A  | 33C9                    | xor ecx,ecx                                     |
6F3A699C  | E8 6F9DF5FF             | call game.6F300710                              | ---> 获取 GameUI 对象
6F3A69A1  | 8BF0                    | mov esi,eax                                     | ---> eax是GameUI 对象
6F3A69A3  | E8 B8871900             | call game.6F53F160                              | ---> 获取某个全局对象
6F3A69A8  | 85C0                    | test eax,eax                                    |
6F3A69AA  | 75 67                   | jne game.6F3A6A13                               |
6F3A69AC  | 33C9                    | xor ecx,ecx                                     |
6F3A69AE  | E8 DD861900             | call game.6F53F090                              | ---> 申请内存
6F3A69B3  | 85C0                    | test eax,eax                                    | ---> 跳转将会执行
6F3A69B5  | 74 5C                   | je game.6F3A6A13                                |
6F3A69B7  | 85F6                    | test esi,esi                                    |
6F3A69B9  | 74 3C                   | je game.6F3A69F7                                |
6F3A69BB  | B9 01000000             | mov ecx,1                                       |
6F3A69C0  | E8 8BCD1E00             | call game.6F593750                              |
6F3A69C5  | 8B06                    | mov eax,dword ptr ds:[esi]                      |
6F3A69C7  | 8B50 18                 | mov edx,dword ptr ds:[eax+18]                   |
6F3A69CA  | 6A 01                   | push 1                                          |
6F3A69CC  | 8BCE                    | mov ecx,esi                                     |
6F3A69CE  | FFD2                    | call edx                                        |
6F3A69D0  | 0FB747 28               | movzx eax,word ptr ds:[edi+28]                  |
6F3A69D4  | 50                      | push eax                                        |
6F3A69D5  | 8BCF                    | mov ecx,edi                                     |
6F3A69D7  | E8 74ACFFFF             | call game.6F3A1650                              |
6F3A69DC  | 8D48 40                 | lea ecx,dword ptr ds:[eax+40]                   |
6F3A69DF  | E8 8CC70C00             | call game.6F473170                              |
6F3A69E4  | 6A 01                   | push 1                                          |
6F3A69E6  | 6A 01                   | push 1                                          |
6F3A69E8  | 6A 00                   | push 0                                          |
6F3A69EA  | 50                      | push eax                                        | ---> eax=3
6F3A69EB  | 8BCE                    | mov ecx,esi                                     |
6F3A69ED  | E8 8E58F5FF             | call game.6F2FC280                              | ---> 掉线会走这个分支
6F3A69F2  | E9 87000000             | jmp game.6F3A6A7E                               |
6F3A69F7  | 5F                      | pop edi                                         |
6F3A69F8  | B8 01000000             | mov eax,1                                       |
6F3A69FD  | 5E                      | pop esi                                         |
6F3A69FE  | 8B8C24 08020000         | mov ecx,dword ptr ss:[esp+208]                  |
6F3A6A05  | 33CC                    | xor ecx,esp                                     |
6F3A6A07  | E8 4DA64300             | call game.6F7E1059                              |
6F3A6A0C  | 81C4 0C020000           | add esp,20C                                     |
6F3A6A12  | C3                      | ret                                             |
6F3A6A13  | 33D2                    | xor edx,edx                                     |
6F3A6A15  | 8D4A 0C                 | lea ecx,dword ptr ds:[edx+C]                    | ---> ecx=C
6F3A6A18  | E8 D3CA1E00             | call game.6F5934F0                              | ---> 显示记分板
6F3A6A1D  | 6A 00                   | push 0                                          |
6F3A6A1F  | E8 6CC91E00             | call game.6F593390                              |
6F3A6A24  | 8BC8                    | mov ecx,eax                                     |
6F3A6A26  | E8 B53C2600             | call game.6F60A6E0                              |
6F3A6A2B  | 8B10                    | mov edx,dword ptr ds:[eax]                      |
6F3A6A2D  | 8BC8                    | mov ecx,eax                                     |
6F3A6A2F  | 8B82 D4000000           | mov eax,dword ptr ds:[edx+D4]                   |
6F3A6A35  | FFD0                    | call eax                                        |
6F3A6A37  | 85F6                    | test esi,esi                                    |
6F3A6A39  | 74 43                   | je game.6F3A6A7E                                |
6F3A6A3B  | 0FB74F 28               | movzx ecx,word ptr ds:[edi+28]                  |
6F3A6A3F  | 51                      | push ecx                                        |
6F3A6A40  | 8BCF                    | mov ecx,edi                                     |
6F3A6A42  | E8 09ACFFFF             | call game.6F3A1650                              |
6F3A6A47  | 8BF8                    | mov edi,eax                                     |
6F3A6A49  | E8 D233FEFF             | call game.6F389E20                              |
6F3A6A4E  | E8 3DABFDFF             | call game.6F381590                              |
6F3A6A53  | E8 2847FCFF             | call game.6F36B180                              |
6F3A6A58  | E8 E392FCFF             | call game.6F36FD40                              |
6F3A6A5D  | 8B96 BC030000           | mov edx,dword ptr ds:[esi+3BC]                  |
6F3A6A63  | 52                      | push edx                                        |
6F3A6A64  | 68 59020800             | push 80259                                      |
6F3A6A69  | 8BCF                    | mov ecx,edi                                     |
6F3A6A6B  | E8 003B2800             | call game.6F62A570                              |
6F3A6A70  | 8B06                    | mov eax,dword ptr ds:[esi]                      |
6F3A6A72  | 8B90 D8000000           | mov edx,dword ptr ds:[eax+D8]                   |
6F3A6A78  | 6A 01                   | push 1                                          |
6F3A6A7A  | 8BCE                    | mov ecx,esi                                     |
6F3A6A7C  | FFD2                    | call edx                                        |
6F3A6A7E  | 8B8C24 10020000         | mov ecx,dword ptr ss:[esp+210]                  |
6F3A6A85  | 5F                      | pop edi                                         |
6F3A6A86  | 5E                      | pop esi                                         |
6F3A6A87  | 33CC                    | xor ecx,esp                                     |
6F3A6A89  | 33C0                    | xor eax,eax                                     |
6F3A6A8B  | E8 C9A54300             | call game.6F7E1059                              |
6F3A6A90  | 81C4 0C020000           | add esp,20C                                     |
6F3A6A96  | C3                      | ret                                             |
```

---


- **地址**：`6F300710`
- **偏移**：`game.dll + 300710`
- **作用**：`积分面板相关`

```assembly
6F300710  | 6A FF                   | push FFFFFFFF                                   |
6F300712  | 68 C1B8816F             | push game.6F81B8C1                              |
6F300717  | 64:A1 00000000          | mov eax,dword ptr fs:[0]                        |
6F30071D  | 50                      | push eax                                        |
6F30071E  | 83EC 08                 | sub esp,8                                       |
6F300721  | 56                      | push esi                                        |
6F300722  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]                 |
6F300727  | 33C4                    | xor eax,esp                                     |
6F300729  | 50                      | push eax                                        |
6F30072A  | 8D4424 10               | lea eax,dword ptr ss:[esp+10]                   |
6F30072E  | 64:A3 00000000          | mov dword ptr fs:[0],eax                        |
6F300734  | 8BF2                    | mov esi,edx                                     |
6F300736  | 833D 804FAB6F 00        | cmp dword ptr ds:[6FAB4F80],0                   |
6F30073D  | 75 35                   | jne game.6F300774                               |
6F30073F  | 85C9                    | test ecx,ecx                                    |
6F300741  | 74 31                   | je game.6F300774                                |
6F300743  | 6A 00                   | push 0                                          |
6F300745  | 68 AF1A0000             | push 1AAF                                       |
6F30074A  | 68 585B936F             | push game.6F935B58                              | ---> ".\\CGameUI.cpp"
6F30074F  | 68 54040000             | push 454                                        |
6F300754  | E8 59AE3E00             | call <JMP.&Ordinal#401>                         |
6F300759  | 894424 08               | mov dword ptr ss:[esp+8],eax                    |
6F30075D  | 894424 0C               | mov dword ptr ss:[esp+C],eax                    |
6F300761  | 85C0                    | test eax,eax                                    |
6F300763  | C74424 18 00000000      | mov dword ptr ss:[esp+18],0                     |
6F30076B  | 74 07                   | je game.6F300774                                |
6F30076D  | 8BC8                    | mov ecx,eax                                     |
6F30076F  | E8 7CE2FFFF             | call game.6F2FE9F0                              |
6F300774  | 85F6                    | test esi,esi                                    |
6F300776  | A1 804FAB6F             | mov eax,dword ptr ds:[6FAB4F80]                 |
6F30077B  | 74 0A                   | je game.6F300787                                |
6F30077D  | C705 804FAB6F 00000000  | mov dword ptr ds:[6FAB4F80],0                   |
6F300787  | 8B4C24 10               | mov ecx,dword ptr ss:[esp+10]                   |
6F30078B  | 64:890D 00000000        | mov dword ptr fs:[0],ecx                        |
6F300792  | 59                      | pop ecx                                         |
6F300793  | 5E                      | pop esi                                         |
6F300794  | 83C4 14                 | add esp,14                                      |
6F300797  | C3                      | ret                                             |
```

---


- **地址**：`6F53F160`
- **偏移**：`game.dll + 53F160`
- **作用**：`游戏全局状态检查 (CheckGameState)`

```assembly
6F53F160  | B9 0D000000             | mov ecx,D                                       |
6F53F165  | E8 6643F8FF             | call game.6F4C34D0                              |
6F53F16A  | 8B40 10                 | mov eax,dword ptr ds:[eax+10]                   |
6F53F16D  | 8B48 08                 | mov ecx,dword ptr ds:[eax+8]                    |
6F53F170  | 33C0                    | xor eax,eax                                     |
6F53F172  | 83B9 10060000 01        | cmp dword ptr ds:[ecx+610],1                    |
6F53F179  | 0F94C0                  | sete al                                         |
6F53F17C  | C3                      | ret                                             |
```
```assembly
6F53F160  | B9 0D000000      | mov ecx,D               ; 获取第13号系统
...
6F53F172  | 83B9 10060000 01 | cmp dword ptr ds:[ecx+610],1 ; 检查偏移 0x610
6F53F179  | 0F94C0           | sete al                 ; 如果等于1，返回True
```

- **功能**：检查第 13 号系统的一个全局标志位（偏移 `0x610`）。
- **含义**：这个标志位通常代表 **"游戏是否已经结束 (Game Over)"** 或者 **"是否处于记分板状态"**。
- **结论**：这个函数的作用是 **`GetGameSystem(13)`**。它获取 ID 为 13 的子系统指针，并判断偏移 610 处的值是否为1。

---


- **地址**：`6F4C34D0`
- **偏移**：`game.dll + 4C34D0`
- **作用**：`全局系统查找器(GetSystemByIndex)`

```
6F4C34D0  | 56                      | push esi                                        |
6F4C34D1  | 8BF1                    | mov esi,ecx                                     |
6F4C34D3  | 8B0D F47BAB6F           | mov ecx,dword ptr ds:[6FAB7BF4]                 |
6F4C34D9  | E8 02642100             | call game.6F6D98E0                              |
6F4C34DE  | 85C0                    | test eax,eax                                    |
6F4C34E0  | 74 05                   | je game.6F4C34E7                                |
6F4C34E2  | 8B04B0                  | mov eax,dword ptr ds:[eax+esi*4]                |
6F4C34E5  | 5E                      | pop esi                                         |
6F4C34E6  | C3                      | ret                                             |
6F4C34E7  | 33C0                    | xor eax,eax                                     |
6F4C34E9  | 5E                      | pop esi                                         |
6F4C34EA  | C3                      | ret                                             |
```
- **功能**：这是一个**查找函数**。
- **逻辑**：War3 内部有一个全局数组（由 `6FAB7BF4` 指向），里面存放了各个子系统的指针（例如：网络系统、音效系统、游戏逻辑系统等）。
- **参数**：`esi` (来自 `ecx`) 是**系统 ID**。
- **结论**：这个函数的作用是 **`GetGameSystem(13)`**。它获取 ID 为 13 的子系统指针。

---


- **地址**：`6F53F090`
- **偏移**：`game.dll + 53F090 (CheckPlayerStatus)`
- **作用**：`玩家槽位状态检查`

```assembly
6F53F090  | 56                      | push esi                                        |
6F53F091  | 8BF1                    | mov esi,ecx                                     |
6F53F093  | B9 0D000000             | mov ecx,D                                       |
6F53F098  | E8 3344F8FF             | call game.6F4C34D0                              |
6F53F09D  | 8B40 10                 | mov eax,dword ptr ds:[eax+10]                   |
6F53F0A0  | 8B40 08                 | mov eax,dword ptr ds:[eax+8]                    |
6F53F0A3  | 85C0                    | test eax,eax                                    |
6F53F0A5  | 74 16                   | je game.6F53F0BD                                |
6F53F0A7  | 69F6 04030000           | imul esi,esi,304                                |
6F53F0AD  | 33C9                    | xor ecx,ecx                                     |
6F53F0AF  | 398C06 78020000         | cmp dword ptr ds:[esi+eax+278],ecx              |
6F53F0B6  | 5E                      | pop esi                                         |
6F53F0B7  | 0F95C1                  | setne cl                                        |
6F53F0BA  | 8BC1                    | mov eax,ecx                                     |
6F53F0BC  | C3                      | ret                                             |
6F53F0BD  | 33C0                    | xor eax,eax                                     |
6F53F0BF  | 5E                      | pop esi                                         |
6F53F0C0  | C3                      | ret                                             |
```

这是最关键的一个函数，包含了很多核心信息：

```assembly
6F53F091  | 8BF1          | mov esi,ecx             ; 输入参数：槽位索引 (Slot Index)
6F53F093  | B9 0D000000   | mov ecx,D               ; 这里的 D 是 13，代表要获取第13号系统
6F53F098  | E8 3344F8FF   | call game.6F4C34D0      ; 获取第13号系统指针
...
6F53F0A7  | 69F6 04030000 | imul esi,esi,304        ; 关键！esi = SlotIndex * 0x304
6F53F0AF  | 398C06 78020000 | cmp dword ptr ds:[esi+eax+278],ecx ; 检查偏移 0x278
```

*   **`0x304` (772)**：这是 War3 (1.26a) 中 **`CGamePlayer` (或 `CPlayerSlot`) 结构体的大小**。
*   **`imul ... 304`**：这证明代码正在访问一个**玩家数组**。
*   **`+ 0x278`**：这是该结构体中的一个成员变量。

**功能推测**：
这个函数用于检查 **指定槽位（Slot）的玩家是否有效/存活/已连接**。
*   如果 `[PlayerBase + 0x278]` 不为 0，返回 1 (True)。
*   如果为 0，返回 0 (False)。

结合之前“直接跳记分板”的问题：**如果这个函数对本地玩家返回 0，说明游戏引擎认为本地玩家的数据结构是空的或者未初始化，因此判定你不在游戏中，直接送去记分板。**

### 🔗 串联起来：发生了什么？

之前的调用栈中看到了这样的逻辑：

1.  调用 `6F53F160`：检查游戏是否已经结束（或者是否已经在显示记分板）。
2.  调用 `6F53F090`（传入本地玩家 ID）：检查本地玩家是否有效。
    *   它获取第 13 号系统（可能是 `CNetGame` 或 `CGameWar3`）。
    *   它计算本地玩家的内存地址 (`Base + Slot * 0x304`)。
    *   它检查偏移 `0x278`。
3.  **结果**：`0x278` 处是 0（无效）。
4.  **后果**：游戏判定本地玩家数据异常（没进游戏），于是执行退出逻辑，显示记分板。

### 总结回答

*   ** 检查 **“玩家是否存在？”** 以及 **“游戏是否正在运行？”**。
*   **`0xD` 是一个内部系统的 ID 编号（System ID 13）。

**根本原因依然指向：地图加载校验失败，导致玩家结构体（特别是 `0x278` 这个状态位）没有被正确初始化为“存活/在游戏中”。**

---


找到了父函数！在 `6F5C418A` 处判断是否为0，不为0表示异常，我遇到的情况是这里为1，显然存在异常！

```assembly
6F5C4120  | 56                      | push esi                                        |
6F5C4121  | 8BF1                    | mov esi,ecx                                     |
6F5C4123  | E8 58A5FFFF             | call game.6F5BE680                              |
6F5C4128  | E8 9390FFFF             | call game.6F5BD1C0                              |
6F5C412D  | 8B46 10                 | mov eax,dword ptr ds:[esi+10]                   |
6F5C4130  | 50                      | push eax                                        |
6F5C4131  | BA B00E5C6F             | mov edx,game.6F5C0EB0                           |
6F5C4136  | B9 7A000940             | mov ecx,4009007A                                |
6F5C413B  | E8 001AF8FF             | call game.6F545B40                              |
6F5C4140  | 8B4E 10                 | mov ecx,dword ptr ds:[esi+10]                   |
6F5C4143  | 51                      | push ecx                                        |
6F5C4144  | BA 20415C6F             | mov edx,game.6F5C4120                           |
6F5C4149  | B9 81000940             | mov ecx,40090081                                |
6F5C414E  | E8 ED19F8FF             | call game.6F545B40                              |
6F5C4153  | 8B56 10                 | mov edx,dword ptr ds:[esi+10]                   |
6F5C4156  | 52                      | push edx                                        |
6F5C4157  | BA 30D25B6F             | mov edx,game.6F5BD230                           |
6F5C415C  | B9 79000940             | mov ecx,40090079                                |
6F5C4161  | E8 DA19F8FF             | call game.6F545B40                              |
6F5C4166  | 8B46 10                 | mov eax,dword ptr ds:[esi+10]                   |
6F5C4169  | 50                      | push eax                                        |
6F5C416A  | BA 30405C6F             | mov edx,game.6F5C4030                           |
6F5C416F  | B9 7E000940             | mov ecx,4009007E                                |
6F5C4174  | E8 C719F8FF             | call game.6F545B40                              |
6F5C4179  | 837E 10 00              | cmp dword ptr ds:[esi+10],0                     |
6F5C417D  | 5E                      | pop esi                                         |
6F5C417E  | 75 35                   | jne game.6F5C41B5                               |
6F5C4180  | 8B0D F465AB6F           | mov ecx,dword ptr ds:[6FAB65F4]                 |
6F5C4186  | 85C9                    | test ecx,ecx                                    |
6F5C4188  | 74 26                   | je game.6F5C41B0                                |
6F5C418A  | 8379 4C 00              | cmp dword ptr ds:[ecx+4C],0                     | <--- 关键判断
6F5C418E  | 74 07                   | je game.6F5C4197                                |
6F5C4190  | E8 4B3FDEFF             | call game.6F3A80E0                              |
6F5C4195  | EB 05                   | jmp game.6F5C419C                               |
6F5C4197  | E8 4408DEFF             | call game.6F3A49E0                              |
6F5C419C  | 85C0                    | test eax,eax                                    |
6F5C419E  | 74 15                   | je game.6F5C41B5                                |
6F5C41A0  | 6A 00                   | push 0                                          |
6F5C41A2  | BA 01000000             | mov edx,1                                       |
6F5C41A7  | 6A 00                   | push 0                                          |
6F5C41A9  | 8BCA                    | mov ecx,edx                                     |
6F5C41AB  | E8 20F9DDFF             | call game.6F3A3AD0                              |
6F5C41B0  | E8 2BF6FCFF             | call game.6F5937E0                              |
6F5C41B5  | B9 01000000             | mov ecx,1                                       |
6F5C41BA  | E8 019D1000             | call game.6F6CDEC0                              |
6F5C41BF  | B8 01000000             | mov eax,1                                       |
6F5C41C4  | C3                      | ret                                             |
```

这段反汇编代码非常清晰地揭示了为什么游戏会直接跳到记分板（Scoreboard）

这是由于 **游戏核心状态检测失败** 导致的。

下面是针对你提供的代码段的详细分析：

### 1. 为什么直接跳到记分板？

**凶手在这里：**

```assembly
6F3A69AE  | E8 DD861900             | call game.6F53F090  <--- 关键检测函数
6F3A69B3  | 85C0                    | test eax,eax        <--- 检查返回值是否为 0
6F3A69B5  | 74 5C                   | je game.6F3A6A13    <--- 如果为 0，跳转到 6F3A6A13 (记分板逻辑)
...
6F3A6A13  | 33D2                    | xor edx,edx
6F3A6A15  | 8D4A 0C                 | lea ecx,dword ptr ds:[edx+C]
6F3A6A18  | E8 D3CA1E00             | call game.6F5934F0  <--- 显示记分板 (ShowScoreboard)
```

**分析函数 `6F53F090`：**

```assembly
6F53F090  | 56            | push esi
6F53F091  | 8BF1          | mov esi,ecx             ; 输入参数 ecx (在外部被清零)
6F53F093  | B9 0D000000   | mov ecx,D               ; ecx = 0xD (13)
6F53F098  | E8 3344F8FF   | call game.6F4C34D0      ; 获取全局游戏状态管理器 (GetGlobalManager)
6F53F09D  | 8B40 10       | mov eax,dword ptr ds:[eax+10]
6F53F0A0  | 8B40 08       | mov eax,dword ptr ds:[eax+8] ; 获取具体的游戏数据结构指针
6F53F0A3  | 85C0          | test eax,eax            ; 检查指针是否为空
6F53F0A5  | 74 16         | je game.6F53F0BD        ; 空则返回 0
6F53F0A7  | 69F6 04030000 | imul esi,esi,304        ; 计算偏移 (esi是输入参数)
6F53F0AD  | 33C9          | xor ecx,ecx
6F53F0AF  | 398C06 78020000 | cmp dword ptr ds:[esi+eax+278],ecx ; 关键检查：偏移 0x278 的值是否为 0
6F53F0B6  | 5E            | pop esi
6F53F0B7  | 0F95C1        | setne cl                ; 如果不等于0，返回1；否则返回0
6F53F0BA  | 8BC1          | mov eax,ecx
6F53F0BC  | C3            | ret
```

**结论：**
代码逻辑正在检查全局游戏数据中偏移 `0x278` 处的值。
*   因为调用时 `xor ecx,ecx` (ecx=0)，所以它检查的是 `[GlobalGameState + 0x278]`。
*   这个值目前是 **0**。
*   在 War3 内部，偏移 `0x278` 通常代表 **"本地玩家是否存活/是否在游戏中"** 或者 **"游戏逻辑是否已初始化"**。
*   因为这个检查失败（返回 0），逻辑认为**当前没有进行中的游戏**，所以直接跳转到 `6F3A6A13` 显示记分板。

---


### 总结原因

游戏直接跳到记分板，是因为**游戏核心状态未能正确初始化**。

**根本原因**：`call game.6F53F090` 返回了 `False` (0)。这意味着游戏引擎认为**当前玩家的数据结构无效**（偏移 `0x278` 处的数据缺失）。

**简单来说：游戏引擎认为"配置不符合要求"，所以送玩家去了记分板。**

## 3. 错误根源 (Error Root Source)

```assembly
6F3B0750  | 6A FF                   | push FFFFFFFF                                   |
6F3B0752  | 68 1846826F             | push game.6F824618                              |
6F3B0757  | 64:A1 00000000          | mov eax,dword ptr fs:[0]                        |
6F3B075D  | 50                      | push eax                                        |
6F3B075E  | 83EC 08                 | sub esp,8                                       |
6F3B0761  | 53                      | push ebx                                        |
6F3B0762  | 55                      | push ebp                                        |
6F3B0763  | 56                      | push esi                                        |
6F3B0764  | 57                      | push edi                                        |
6F3B0765  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]                 |
6F3B076A  | 33C4                    | xor eax,esp                                     |
6F3B076C  | 50                      | push eax                                        |
6F3B076D  | 8D4424 1C               | lea eax,dword ptr ss:[esp+1C]                   |
6F3B0771  | 64:A3 00000000          | mov dword ptr fs:[0],eax                        |
6F3B0777  | 8BF1                    | mov esi,ecx                                     |
6F3B0779  | 8B46 30                 | mov eax,dword ptr ds:[esi+30]                   |
6F3B077C  | 8D48 24                 | lea ecx,dword ptr ds:[eax+24]                   |
6F3B077F  | E8 AC3E1100             | call game.6F4C4630                              |
6F3B0784  | 8BF8                    | mov edi,eax                                     |
6F3B0786  | 33C9                    | xor ecx,ecx                                     |
6F3B0788  | 897C24 18               | mov dword ptr ss:[esp+18],edi                   |
6F3B078C  | C786 E0030000 01000000  | mov dword ptr ds:[esi+3E0],1                    |
6F3B0796  | E8 959CF5FF             | call game.6F30A430                              |
6F3B079B  | 8D4C24 14               | lea ecx,dword ptr ss:[esp+14]                   |
6F3B079F  | E8 8CE9FEFF             | call game.6F39F130                              |
6F3B07A4  | A1 D8ED926F             | mov eax,dword ptr ds:[6F92EDD8]                 |
6F3B07A9  | 68 502B936F             | push game.6F932B50                              | ---> "smart"
6F3B07AE  | 33ED                    | xor ebp,ebp                                     |
6F3B07B0  | 50                      | push eax                                        |
6F3B07B1  | 8BCE                    | mov ecx,esi                                     |
6F3B07B3  | 896C24 2C               | mov dword ptr ss:[esp+2C],ebp                   |
6F3B07B7  | E8 74FFFFFF             | call game.6F3B0730                              |
6F3B07BC  | E8 3F92E7FF             | call game.6F229A00                              |
6F3B07C1  | E8 9AE91800             | call game.6F53F160                              |
6F3B07C6  | 3BC5                    | cmp eax,ebp                                     |
6F3B07C8  | 8986 C8030000           | mov dword ptr ds:[esi+3C8],eax                  |
6F3B07CE  | 74 08                   | je game.6F3B07D8                                |
6F3B07D0  | E8 0BDF1800             | call game.6F53E6E0                              |
6F3B07D5  | 50                      | push eax                                        |
6F3B07D6  | EB 01                   | jmp game.6F3B07D9                               |
6F3B07D8  | 55                      | push ebp                                        |
6F3B07D9  | 8D8E CC030000           | lea ecx,dword ptr ds:[esi+3CC]                  |
6F3B07DF  | E8 0C551100             | call game.6F4C5CF0                              |
6F3B07E4  | D905 941E946F           | fld dword ptr ds:[6F941E94]                     |
6F3B07EA  | 51                      | push ecx                                        |
6F3B07EB  | D91C24                  | fstp dword ptr ss:[esp]                         |
6F3B07EE  | E8 FD9B1D00             | call game.6F58A3F0                              |
6F3B07F3  | 6A 01                   | push 1                                          |
6F3B07F5  | 6A 01                   | push 1                                          |
6F3B07F7  | BA 01000000             | mov edx,1                                       |
6F3B07FC  | 8BCF                    | mov ecx,edi                                     |
6F3B07FE  | E8 7DD6C6FF             | call game.6F01DE80                              |
6F3B0803  | 3BC5                    | cmp eax,ebp                                     |
6F3B0805  | 0F84 31030000           | je game.6F3B0B3C                                |
6F3B080B  | 33D2                    | xor edx,edx                                     |
6F3B080D  | B9 A0F83A6F             | mov ecx,game.6F3AF8A0                           |
6F3B0812  | E8 49ECC5FF             | call game.6F00F460                              |
6F3B0817  | E8 84E9E9FF             | call game.6F24F1A0                              |
6F3B081C  | E8 5FE9E9FF             | call game.6F24F180                              |
6F3B0821  | E8 AAE9E9FF             | call game.6F24F1D0                              |
6F3B0826  | E8 75C53600             | call game.6F71CDA0                              |
6F3B082B  | E8 E0ECC6FF             | call game.6F01F510                              |
6F3B0830  | E8 8BB0EAFF             | call game.6F25B8C0                              |
6F3B0835  | E8 86E9E9FF             | call game.6F24F1C0                              |
6F3B083A  | E8 31E9E9FF             | call game.6F24F170                              |
6F3B083F  | E8 4CE9E9FF             | call game.6F24F190                              |
6F3B0844  | E8 3769FDFF             | call game.6F387180                              |
6F3B0849  | 39AE C8030000           | cmp dword ptr ds:[esi+3C8],ebp                  |
6F3B084F  | 74 08                   | je game.6F3B0859                                |
6F3B0851  | 66:C746 2C FF0F         | mov word ptr ds:[esi+2C],FFF                    |
6F3B0857  | EB 0E                   | jmp game.6F3B0867                               |
6F3B0859  | 8A4E 28                 | mov cl,byte ptr ds:[esi+28]                     |
6F3B085C  | BA 01000000             | mov edx,1                                       |
6F3B0861  | D3E2                    | shl edx,cl                                      |
6F3B0863  | 66:8956 2C              | mov word ptr ds:[esi+2C],dx                     |
6F3B0867  | E8 140AC6FF             | call game.6F011280                              |
6F3B086C  | 396E 1C                 | cmp dword ptr ds:[esi+1C],ebp                   |
6F3B086F  | 8D7E 1C                 | lea edi,dword ptr ds:[esi+1C]                   |
6F3B0872  | 8986 C0030000           | mov dword ptr ds:[esi+3C0],eax                  |
6F3B0878  | 75 11                   | jne game.6F3B088B                               |
6F3B087A  | 55                      | push ebp                                        |
6F3B087B  | 55                      | push ebp                                        |
6F3B087C  | 55                      | push ebp                                        |
6F3B087D  | 8BCF                    | mov ecx,edi                                     |
6F3B087F  | E8 0C68FFFF             | call game.6F3A7090                              |
6F3B0884  | 8B0F                    | mov ecx,dword ptr ds:[edi]                      |
6F3B0886  | E8 85B90700             | call game.6F42C210                              |
6F3B088B  | 8BCE                    | mov ecx,esi                                     |
6F3B088D  | E8 4E7BFFFF             | call game.6F3A83E0                              |
6F3B0892  | 8BCE                    | mov ecx,esi                                     |
6F3B0894  | E8 A7E8FFFF             | call game.6F3AF140                              |
6F3B0899  | D9EE                    | fldz                                            |
6F3B089B  | 51                      | push ecx                                        |
6F3B089C  | D91C24                  | fstp dword ptr ss:[esp]                         |
6F3B089F  | E8 4C9B1D00             | call game.6F58A3F0                              |
6F3B08A4  | 8BCE                    | mov ecx,esi                                     |
6F3B08A6  | E8 F528FFFF             | call game.6F3A31A0                              |
6F3B08AB  | 0FB746 28               | movzx eax,word ptr ds:[esi+28]                  |
6F3B08AF  | 50                      | push eax                                        |
6F3B08B0  | 8BCE                    | mov ecx,esi                                     |
6F3B08B2  | E8 990DFFFF             | call game.6F3A1650                              |
6F3B08B7  | 8B88 5C020000           | mov ecx,dword ptr ds:[eax+25C]                  |
6F3B08BD  | 55                      | push ebp                                        |
6F3B08BE  | 83E1 BF                 | and ecx,FFFFFFBF                                |
6F3B08C1  | 55                      | push ebp                                        |
6F3B08C2  | 51                      | push ecx                                        |
6F3B08C3  | 8A48 30                 | mov cl,byte ptr ds:[eax+30]                     |
6F3B08C6  | BA 03000000             | mov edx,3                                       |
6F3B08CB  | E8 208E1900             | call game.6F5496F0                              |
6F3B08D0  | 8BCE                    | mov ecx,esi                                     |
6F3B08D2  | E8 691FFFFF             | call game.6F3A2840                              |
6F3B08D7  | 8BCE                    | mov ecx,esi                                     |
6F3B08D9  | E8 722AFFFF             | call game.6F3A3350                              |
6F3B08DE  | 8BCE                    | mov ecx,esi                                     |
6F3B08E0  | E8 6B52FFFF             | call game.6F3A5B50                              |
6F3B08E5  | D9EE                    | fldz                                            |
6F3B08E7  | 51                      | push ecx                                        |
6F3B08E8  | D91C24                  | fstp dword ptr ss:[esp]                         |
6F3B08EB  | E8 009B1D00             | call game.6F58A3F0                              |
6F3B08F0  | 896E 4C                 | mov dword ptr ds:[esi+4C],ebp                   |
6F3B08F3  | D9EE                    | fldz                                            |
6F3B08F5  | 51                      | push ecx                                        |
6F3B08F6  | D91C24                  | fstp dword ptr ss:[esp]                         |
6F3B08F9  | E8 F29A1D00             | call game.6F58A3F0                              |
6F3B08FE  | 55                      | push ebp                                        |
6F3B08FF  | 8BCE                    | mov ecx,esi                                     |
6F3B0901  | E8 4A0DFFFF             | call game.6F3A1650                              |
6F3B0906  | 8BF8                    | mov edi,eax                                     |
6F3B0908  | 8B9F 78020000           | mov ebx,dword ptr ds:[edi+278]                  |
6F3B090E  | 8D8F F0000000           | lea ecx,dword ptr ds:[edi+F0]                   |
6F3B0914  | E8 57280C00             | call game.6F473170                              | ---> 校验地图/初始化玩家
6F3B0919  | 85C0                    | test eax,eax                                    | ---> 检查返回值
6F3B091B  | 74 14                   | je game.6F3B0931                                | ---> 如果失败(0)，直接跳去检查状态(肯定不是1)
6F3B091D  | C787 70020000 02000000  | mov dword ptr ds:[edi+270],2                    | ---> 如果成功，居然赋值为 2 ？(这里可能有特殊逻辑，或者2代表Loading)
6F3B0927  | C787 1C030000 00000000  | mov dword ptr ds:[edi+31C],0                    |
6F3B0931  | 8B87 70020000           | mov eax,dword ptr ds:[edi+270]                  | ---> 读取玩家状态 [edi+270]
6F3B0937  | 33C9                    | xor ecx,ecx                                     |
6F3B0939  | 83F8 01                 | cmp eax,1                                       | ---> 必须等于 1 (Ready/Verified)
6F3B093C  | 0F94C1                  | sete cl                                         |
6F3B093F  | 8BC1                    | mov eax,ecx                                     |
6F3B0941  | 85C0                    | test eax,eax                                    | ---> 如果状态是 1，跳过报错，继续循环
6F3B0943  | 74 26                   | je game.6F3B096B                                |
6F3B0945  | E8 C698C5FF             | call game.6F00A210                              |
6F3B094A  | 8D8F A0000000           | lea ecx,dword ptr ds:[edi+A0]                   |
6F3B0950  | 50                      | push eax                                        | ---> eax=64：下载状态 100%
6F3B0951  | E8 EA270C00             | call game.6F473140                              |
6F3B0956  | 8D4F 40                 | lea ecx,dword ptr ds:[edi+40]                   |
6F3B0959  | 6A 03                   | push 3                                          |
6F3B095B  | E8 E0270C00             | call game.6F473140                              |
6F3B0960  | 83FD 0C                 | cmp ebp,C                                       | ---> 检查槽位索引是否 < 12
6F3B0963  | 73 14                   | jae game.6F3B0979                               | ---> 如果是裁判/观察者(>=12)，跳过报错
6F3B0965  | 8346 4C 01              | add dword ptr ds:[esi+4C],1                     | ---> 硬件断点写入，1 的来源找到！
6F3B0969  | EB 0E                   | jmp game.6F3B0979                               |
6F3B096B  | 83FB FF                 | cmp ebx,FFFFFFFF                                |
6F3B096E  | 74 09                   | je game.6F3B0979                                |
6F3B0970  | 55                      | push ebp                                        |
6F3B0971  | 53                      | push ebx                                        |
6F3B0972  | 8BCE                    | mov ecx,esi                                     |
6F3B0974  | E8 8779FFFF             | call game.6F3A8300                              |
6F3B0979  | 83C5 01                 | add ebp,1                                       |
6F3B097C  | 83FD 10                 | cmp ebp,10                                      |
6F3B097F  | 0F82 6EFFFFFF           | jb game.6F3B08F3                                |
6F3B0985  | D905 04ED926F           | fld dword ptr ds:[6F92ED04]                     |
6F3B098B  | 51                      | push ecx                                        |
6F3B098C  | D91C24                  | fstp dword ptr ss:[esp]                         |
6F3B098F  | E8 5C9A1D00             | call game.6F58A3F0                              |
6F3B0994  | 8BCE                    | mov ecx,esi                                     |
6F3B0996  | E8 659DFFFF             | call game.6F3AA700                              |
6F3B099B  | 8B4E 34                 | mov ecx,dword ptr ds:[esi+34]                   |
6F3B099E  | E8 6D830500             | call game.6F408D10                              |
6F3B09A3  | D905 E809946F           | fld dword ptr ds:[6F9409E8]                     |
6F3B09A9  | 51                      | push ecx                                        |
6F3B09AA  | D91C24                  | fstp dword ptr ss:[esp]                         |
6F3B09AD  | E8 3E9A1D00             | call game.6F58A3F0                              |
6F3B09B2  | 8B4E 34                 | mov ecx,dword ptr ds:[esi+34]                   |
6F3B09B5  | E8 066E0500             | call game.6F4077C0                              |
6F3B09BA  | 8B4E 34                 | mov ecx,dword ptr ds:[esi+34]                   |
6F3B09BD  | E8 9E5C0500             | call game.6F406660                              |
6F3B09C2  | 33C9                    | xor ecx,ecx                                     |
6F3B09C4  | E8 17A7EFFF             | call game.6F2AB0E0                              |
6F3B09C9  | 8B5C24 18               | mov ebx,dword ptr ss:[esp+18]                   |
6F3B09CD  | 6A 01                   | push 1                                          |
6F3B09CF  | 6A 01                   | push 1                                          |
6F3B09D1  | BA 01000000             | mov edx,1                                       |
6F3B09D6  | 8BCB                    | mov ecx,ebx                                     |
6F3B09D8  | E8 A315C6FF             | call game.6F011F80                              |
6F3B09DD  | D905 901E946F           | fld dword ptr ds:[6F941E90]                     |
6F3B09E3  | 51                      | push ecx                                        |
6F3B09E4  | D91C24                  | fstp dword ptr ss:[esp]                         |
6F3B09E7  | E8 049A1D00             | call game.6F58A3F0                              |
6F3B09EC  | 8B56 08                 | mov edx,dword ptr ds:[esi+8]                    |
6F3B09EF  | 52                      | push edx                                        |
6F3B09F0  | 8BCE                    | mov ecx,esi                                     |
6F3B09F2  | E8 D9EBFEFF             | call game.6F39F5D0                              |
6F3B09F7  | 8B4E 08                 | mov ecx,dword ptr ds:[esi+8]                    |
6F3B09FA  | 8D7E 1C                 | lea edi,dword ptr ds:[esi+1C]                   |
6F3B09FD  | 57                      | push edi                                        |
6F3B09FE  | BA 10DC426F             | mov edx,game.6F42DC10                           |
6F3B0A03  | E8 18AD0900             | call game.6F44B720                              |
6F3B0A08  | E8 C33EEEFF             | call game.6F2948D0                              |
6F3B0A0D  | E8 2E7EF0FF             | call game.6F2B8840                              |
6F3B0A12  | 8B46 30                 | mov eax,dword ptr ds:[esi+30]                   |
6F3B0A15  | 8B40 38                 | mov eax,dword ptr ds:[eax+38]                   |
6F3B0A18  | A8 08                   | test al,8                                       |
6F3B0A1A  | 77 5E                   | ja game.6F3B0A7A                                |
6F3B0A1C  | A8 02                   | test al,2                                       |
6F3B0A1E  | 76 22                   | jbe game.6F3B0A42                               |
6F3B0A20  | 8B4E 34                 | mov ecx,dword ptr ds:[esi+34]                   |
6F3B0A23  | 6A 01                   | push 1                                          |
6F3B0A25  | E8 36740500             | call game.6F407E60                              |
6F3B0A2A  | 8B4E 34                 | mov ecx,dword ptr ds:[esi+34]                   |
6F3B0A2D  | 6A 00                   | push 0                                          |
6F3B0A2F  | 6A 01                   | push 1                                          |
6F3B0A31  | E8 9A730500             | call game.6F407DD0                              |
6F3B0A36  | C786 C0030000 01000000  | mov dword ptr ds:[esi+3C0],1                    |
6F3B0A40  | EB 38                   | jmp game.6F3B0A7A                               |
6F3B0A42  | A8 01                   | test al,1                                       |
6F3B0A44  | 76 10                   | jbe game.6F3B0A56                               |
6F3B0A46  | 8B4E 34                 | mov ecx,dword ptr ds:[esi+34]                   |
6F3B0A49  | 6A 01                   | push 1                                          |
6F3B0A4B  | E8 10740500             | call game.6F407E60                              |
6F3B0A50  | 6A 00                   | push 0                                          |
6F3B0A52  | 6A 01                   | push 1                                          |
6F3B0A54  | EB 12                   | jmp game.6F3B0A68                               |
6F3B0A56  | A8 04                   | test al,4                                       |
6F3B0A58  | 76 20                   | jbe game.6F3B0A7A                               |
6F3B0A5A  | 8B4E 34                 | mov ecx,dword ptr ds:[esi+34]                   |
6F3B0A5D  | 6A 00                   | push 0                                          |
6F3B0A5F  | E8 FC730500             | call game.6F407E60                              |
6F3B0A64  | 6A 00                   | push 0                                          |
6F3B0A66  | 6A 00                   | push 0                                          |
6F3B0A68  | 8B4E 34                 | mov ecx,dword ptr ds:[esi+34]                   |
6F3B0A6B  | E8 60730500             | call game.6F407DD0                              |
6F3B0A70  | C786 C0030000 00000000  | mov dword ptr ds:[esi+3C0],0                    |
6F3B0A7A  | 8B8E C0030000           | mov ecx,dword ptr ds:[esi+3C0]                  |
6F3B0A80  | E8 FBB4C5FF             | call game.6F00BF80                              |
6F3B0A85  | 66:8B4E 28              | mov cx,word ptr ds:[esi+28]                     |
6F3B0A89  | 66:894E 2A              | mov word ptr ds:[esi+2A],cx                     |
6F3B0A8D  | 8B4E 08                 | mov ecx,dword ptr ds:[esi+8]                    |
6F3B0A90  | E8 1B4A0000             | call game.6F3B54B0                              |
6F3B0A95  | B9 01000000             | mov ecx,1                                       |
6F3B0A9A  | E8 41A6EFFF             | call game.6F2AB0E0                              |
6F3B0A9F  | 8D8E F0020000           | lea ecx,dword ptr ds:[esi+2F0]                  |
6F3B0AA5  | 53                      | push ebx                                        |
6F3B0AA6  | E8 45521100             | call game.6F4C5CF0                              |
6F3B0AAB  | 8B0F                    | mov ecx,dword ptr ds:[edi]                      |
6F3B0AAD  | 6A 01                   | push 1                                          |
6F3B0AAF  | E8 9C610700             | call game.6F426C50                              |
6F3B0AB4  | 8B0F                    | mov ecx,dword ptr ds:[edi]                      |
6F3B0AB6  | 6A 01                   | push 1                                          |
6F3B0AB8  | E8 33610700             | call game.6F426BF0                              |
6F3B0ABD  | D9EE                    | fldz                                            |
6F3B0ABF  | 51                      | push ecx                                        |
6F3B0AC0  | D91C24                  | fstp dword ptr ss:[esp]                         |
6F3B0AC3  | E8 28991D00             | call game.6F58A3F0                              |
6F3B0AC8  | 8B0F                    | mov ecx,dword ptr ds:[edi]                      |
6F3B0ACA  | 8379 6C 00              | cmp dword ptr ds:[ecx+6C],0                     |
6F3B0ACE  | 75 05                   | jne game.6F3B0AD5                               |
6F3B0AD0  | E8 9B8C0700             | call game.6F429770                              |
6F3B0AD5  | 8BCE                    | mov ecx,esi                                     |
6F3B0AD7  | E8 441EFFFF             | call game.6F3A2920                              |
6F3B0ADC  | E8 6FE8FEFF             | call game.6F39F350                              |
6F3B0AE1  | 68 FFFFFF7F             | push 7FFFFFFF                                   |
6F3B0AE6  | 68 681E946F             | push game.6F941E68                              | ---> "maps\\campaign\\WarcraftIIICredits.w3m"
6F3B0AEB  | 53                      | push ebx                                        |
6F3B0AEC  | 8BF0                    | mov esi,eax                                     |
6F3B0AEE  | E8 EFAA3300             | call <JMP.&Ordinal#509>                         |
6F3B0AF3  | 85C0                    | test eax,eax                                    |
6F3B0AF5  | 74 40                   | je game.6F3B0B37                                |
6F3B0AF7  | 68 FFFFFF7F             | push 7FFFFFFF                                   |
6F3B0AFC  | 68 481E946F             | push game.6F941E48                              | ---> "maps\\campaign\\BonusCredits.w3m"
6F3B0B01  | 53                      | push ebx                                        |
6F3B0B02  | E8 DBAA3300             | call <JMP.&Ordinal#509>                         |
6F3B0B07  | 85C0                    | test eax,eax                                    |
6F3B0B09  | 74 2C                   | je game.6F3B0B37                                |
6F3B0B0B  | 68 FFFFFF7F             | push 7FFFFFFF                                   |
6F3B0B10  | 68 1C1E946F             | push game.6F941E1C                              | ---> "maps\\campaign\\War3XRegularCreditsIce.w3x"
6F3B0B15  | 53                      | push ebx                                        |
6F3B0B16  | E8 C7AA3300             | call <JMP.&Ordinal#509>                         |
6F3B0B1B  | 85C0                    | test eax,eax                                    |
6F3B0B1D  | 74 18                   | je game.6F3B0B37                                |
6F3B0B1F  | 68 FFFFFF7F             | push 7FFFFFFF                                   |
6F3B0B24  | 68 F81D946F             | push game.6F941DF8                              | ---> "maps\\campaign\\War3XBonusCredits.w3x"
6F3B0B29  | 53                      | push ebx                                        |
6F3B0B2A  | E8 B3AA3300             | call <JMP.&Ordinal#509>                         |
6F3B0B2F  | 85C0                    | test eax,eax                                    |
6F3B0B31  | 74 04                   | je game.6F3B0B37                                |
6F3B0B33  | 85F6                    | test esi,esi                                    |
6F3B0B35  | 74 05                   | je game.6F3B0B3C                                |
6F3B0B37  | E8 F4EC1800             | call game.6F53F830                              |
6F3B0B3C  | 8D4C24 14               | lea ecx,dword ptr ss:[esp+14]                   |
6F3B0B40  | C74424 24 FFFFFFFF      | mov dword ptr ss:[esp+24],FFFFFFFF              |
6F3B0B48  | E8 03E6FEFF             | call game.6F39F150                              |
6F3B0B4D  | 8B4C24 1C               | mov ecx,dword ptr ss:[esp+1C]                   |
6F3B0B51  | 64:890D 00000000        | mov dword ptr fs:[0],ecx                        |
6F3B0B58  | 59                      | pop ecx                                         |
6F3B0B59  | 5F                      | pop edi                                         |
6F3B0B5A  | 5E                      | pop esi                                         |
6F3B0B5B  | 5D                      | pop ebp                                         |
6F3B0B5C  | 5B                      | pop ebx                                         |
6F3B0B5D  | 83C4 14                 | add esp,14                                      |
6F3B0B60  | C3                      | ret                                             |
```

### 1. `cmp ebp, C` (ebp 和 0xC 是什么？)

*   **`ebp`**：**槽位索引 (Slot Index)**。
    *   这是一个循环计数器，代表当前正在检查第几个玩家槽位。
    *   **`ebp = 1`**：代表 **Slot 1**（即通常的蓝色玩家，HostBot 里的第一个真实玩家）。这确认了**出问题的是房主（或进来的第一个玩家）**。
*   **`0xC` (12)**：**最大对战玩家数量**。
    *   War3 的标准对战位是 12 个（Slot 0 - Slot 11）。
    *   Slot 12 及以后通常是裁判（Observer）或中立单位。
*   **逻辑含义**：
    ```assembly
    cmp ebp, 0C   ; 检查当前槽位是否 < 12
    jae ...       ; 如果 >= 12 (是裁判)，跳转跳过错误计数
    ```
    *   这说明：**如果是裁判数据不对，游戏不会炸；但如果是前 12 个打比赛的人数据不对，必须终止游戏。**
    *   因为 `ebp = 1` (小于 12)，所以它没有跳转，而是继续执行了后面那句“死刑判决” (`add [esi+4C], 1`)。

### 2. `push 3` (3 代表什么？)

*   **含义**：**断开连接/错误原因代码 (Disconnect Reason / Error Code)**。
*   在 War3 `Game.dll` 的加载逻辑中，这个参数传递给 `6F473140`（这是一个设置玩家状态或报告错误的函数）。
*   **代码 3 的具体含义**：通常代表 **Map Critical Error** 或 **Checksum Mismatch (校验和不匹配)**。
    *   `1` = 主动离开 / 掉线
    *   `3` = **致命错误：地图文件不匹配 / 数据损坏**
    *   `4` = 被踢出

### 3. `push eax` (eax=64 / 100 是什么？)

*   **含义**：**下载/加载状态 (Download/Load Status)**。
*   **数值 100 (0x64)**：代表 **100%**。
*   **逻辑推演**：
    *   之前在代码里通过 `W3GS_MAPSIZE` 包把玩家状态设为了 `100%`。
    *   这里汇编显示 `push 100`，说明游戏引擎读取到了这个状态：“该玩家声称他拥有地图 (100%)”。
    *   **但是！** 紧接着的 `push 3` 打脸了：虽然声称有图 (100)，但经过底层计算，图是错的 (Error 3)。

### 🕵️‍♂️ 完整的“案发现场”还原

1.  **循环检查**：游戏正在遍历所有槽位，现在检查到了 **Slot 1 (`ebp=1`)**。
2.  **状态读取**：读取到该玩家的加载进度是 **100% (`eax=64`)**。
4.  **校验失败**：关键数据结构（如 `[edi+270]`）状态异常。
5.  **报错准备**：
    *   `push 100`：记录当前进度是 100%。
    *   `push 3`：记录错误类型是 **“实际数据与地图文件不符”**。
6.  **裁判豁免权**：检查 `ebp (1) < 12`？是的。如果是裁判就不管了，但他是选手。
7.  **执行死刑**：`add dword ptr ds:[esi+4C], 1`。将全局游戏状态置为 **“异常终止”**。
8.  **结果**：主循环检测到状态异常，直接跳转 `ShowScoreboard`。

### ✅ 最终结论

这证实了我们之前的推断：
**问题不在网络包，不在 PID，不在记分板 UI。**
**问题就在于：War3 客户端本地地图配置，与服务端（Bot）发送过来的不一致。**

**必须：**
1.  确保 Bot 目录下的地图文件与客户端 **完全一致**（同一个文件）。
3.  或者在 Bot 代码里 **硬编码 (Hardcode)** 正确的 CRC32 和 FileSize 发送给客户端（在 `0x3D` 包中）。

```assembly
6F473170  | 8B51 0C                 | mov edx,dword ptr ds:[ecx+C]                    |
6F473173  | 8B49 08                 | mov ecx,dword ptr ds:[ecx+8]                    |
6F473176  | E8 B5C8BCFF             | call game.6F03FA30                              | <--- 查找玩家对象
6F47317B  | 8B40 78                 | mov eax,dword ptr ds:[eax+78]                   | <--- 这里的返回值会导致断开连接（eax=3）
6F47317E  | C3                      | ret                                             |
```

```assembly
6F03FA30  | 56                      | push esi                                        |
6F03FA31  | 8B35 8877AB6F           | mov esi,dword ptr ds:[6FAB7788]                 |
6F03FA37  | 57                      | push edi                                        |
6F03FA38  | 8BF9                    | mov edi,ecx                                     |
6F03FA3A  | C1EF 1F                 | shr edi,1F                                      |
6F03FA3D  | 75 0E                   | jne game.6F03FA4D                               |
6F03FA3F  | 3B4E 1C                 | cmp ecx,dword ptr ds:[esi+1C]                   |
6F03FA42  | 73 20                   | jae game.6F03FA64                               |
6F03FA44  | 8B46 0C                 | mov eax,dword ptr ds:[esi+C]                    |
6F03FA47  | 833CC8 FE               | cmp dword ptr ds:[eax+ecx*8],FFFFFFFE           |
6F03FA4B  | EB 15                   | jmp game.6F03FA62                               |
6F03FA4D  | 8BC1                    | mov eax,ecx                                     |
6F03FA4F  | 25 FFFFFF7F             | and eax,7FFFFFFF                                |
6F03FA54  | 3B46 3C                 | cmp eax,dword ptr ds:[esi+3C]                   |
6F03FA57  | 73 0B                   | jae game.6F03FA64                               |
6F03FA59  | 53                      | push ebx                                        |
6F03FA5A  | 8B5E 2C                 | mov ebx,dword ptr ds:[esi+2C]                   |
6F03FA5D  | 833CC3 FE               | cmp dword ptr ds:[ebx+eax*8],FFFFFFFE           |
6F03FA61  | 5B                      | pop ebx                                         |
6F03FA62  | 74 05                   | je game.6F03FA69                                |
6F03FA64  | 5F                      | pop edi                                         |
6F03FA65  | 33C0                    | xor eax,eax                                     |
6F03FA67  | 5E                      | pop esi                                         |
6F03FA68  | C3                      | ret                                             |
6F03FA69  | 85FF                    | test edi,edi                                    |
6F03FA6B  | 74 1D                   | je game.6F03FA8A                                |
6F03FA6D  | 8B46 2C                 | mov eax,dword ptr ds:[esi+2C]                   |
6F03FA70  | 81E1 FFFFFF7F           | and ecx,7FFFFFFF                                |
6F03FA76  | 8B4CC8 04               | mov ecx,dword ptr ds:[eax+ecx*8+4]              |
6F03FA7A  | 33C0                    | xor eax,eax                                     |
6F03FA7C  | 3951 18                 | cmp dword ptr ds:[ecx+18],edx                   |
6F03FA7F  | 5F                      | pop edi                                         |
6F03FA80  | 0F95C0                  | setne al                                        |
6F03FA83  | 5E                      | pop esi                                         |
6F03FA84  | 83E8 01                 | sub eax,1                                       |
6F03FA87  | 23C1                    | and eax,ecx                                     |
6F03FA89  | C3                      | ret                                             |
6F03FA8A  | 8B46 0C                 | mov eax,dword ptr ds:[esi+C]                    | ---> eax来源于[esi + C]
6F03FA8D  | 8B4CC8 04               | mov ecx,dword ptr ds:[eax+ecx*8+4]              | ---> ecx来源于[eax + offset * 8 + 4] ecx=29
6F03FA91  | 33C0                    | xor eax,eax                                     |
6F03FA93  | 3951 18                 | cmp dword ptr ds:[ecx+18],edx                   | ---> 比较 edx=29
6F03FA96  | 5F                      | pop edi                                         |
6F03FA97  | 0F95C0                  | setne al                                        |
6F03FA9A  | 5E                      | pop esi                                         |
6F03FA9B  | 83E8 01                 | sub eax,1                                       |
6F03FA9E  | 23C1                    | and eax,ecx                                     |
6F03FAA0  | C3                      | ret                                             |
```

*   **`6F473170`**：这是一个 **"Get Player Status" (获取玩家状态)** 的函数。
*   **`6F03FA30`**：这是一个 **"Handle Lookup" (句柄/对象查找)** 的函数（War3 的哈希表查找算法）。

---

### 🕵️‍♂️ 深度解析反汇编逻辑

#### 1. `6F03FA30` 它是 War3 的对象查找器。**

War3 的所有对象（单位、玩家、技能）都存储在一个巨大的哈希表（Handle Table）中。
*   `mov esi, dword ptr ds:[6FAB7788]`：获取全局句柄表管理器。
*   `cmp dword ptr ds:[eax+ecx*8], FFFFFFFE`：检查这个句柄槽位是否为空（`FFFFFFFE` 代表空/无效）。
*   `cmp dword ptr ds:[ecx+18], edx`：**这是关键校验！**
    *   它比较对象的 **"生成 ID" (Generation ID)**。
    *   为防止访问一个已经销毁并被新对象复用的槽位。
*   **返回值**：如果找到并校验通过，返回 **对象指针 (Player Object Pointer)**；否则返回 0。

#### 2. `6F473170` (调用者)
```assembly
6F473176  | call game.6F03FA30  ; 查找玩家对象
6F47317B  | mov eax, [eax+78]   ; <--- 核心真相在这里！
```
这个函数做的事情非常简单：
1.  找到玩家对象。
2.  **读取偏移 `0x78` 的值并返回。**

---

### 🧩 拼图完成了

结合之前的死刑代码 `cmp [edi+270], 1` (这里的 270 可能是外层结构偏移，对应内部对象的 78)：

1.  **`[eax+0x78]` 是什么？**
    它是 **玩家的地图验证状态 (Map Verification Status)**。
    *   **0** = **Disconnected/Empty**。
    *   **1** = **Verified (通过)**。客户端本地计算的 CRC 与服务端一致。
    *   **2** = **Downloading (下载中/未验证)**。
    *   **3** = **Critical Error (CRC 不匹配)**。

```assembly
6F3B0959  | 6A 03                   | push 3               ; <--- 关键证据
6F3B095B  | E8 E0270C00             | call game.6F473140   ; 设置错误状态
```

2. **在 War3 的 `Game.dll` 内部逻辑中，这个参数 `3` 有明确定义**：
*   `0x01` = **DISCONNECT_QUIT** (主动退出)
*   `0x03` = **DISCONNECT_MAPERROR** (地图严重错误/校验不匹配)
*   `0x04` = **DISCONNECT_KICK** (被踢出)

3.  **发生了什么？**
    *   `6F473170` 获取了这个状态值。
    *   之前的逻辑检查发现这个值 **不是 1**。
    *   于是游戏引擎判定：**该玩家地图数据异常，终止游戏**。

---

## 4. 数据来源

### 4.1 `game.dll + 3AEE13`

```assembly
6F3AEB20  | 6A FF                   | push FFFFFFFF                                   |
6F3AEB22  | 68 1244826F             | push game.6F824412                              |
6F3AEB27  | 64:A1 00000000          | mov eax,dword ptr fs:[0]                        |
6F3AEB2D  | 50                      | push eax                                        |
6F3AEB2E  | 83EC 38                 | sub esp,38                                      |
6F3AEB31  | 53                      | push ebx                                        |
6F3AEB32  | 55                      | push ebp                                        |
6F3AEB33  | 56                      | push esi                                        |
6F3AEB34  | 57                      | push edi                                        |
6F3AEB35  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]                 |
6F3AEB3A  | 33C4                    | xor eax,esp                                     |
6F3AEB3C  | 50                      | push eax                                        |
6F3AEB3D  | 8D4424 4C               | lea eax,dword ptr ss:[esp+4C]                   |
6F3AEB41  | 64:A3 00000000          | mov dword ptr fs:[0],eax                        |
6F3AEB47  | 8BE9                    | mov ebp,ecx                                     |
6F3AEB49  | 33FF                    | xor edi,edi                                     |
6F3AEB4B  | 8DB5 98000000           | lea esi,dword ptr ss:[ebp+98]                   |
6F3AEB51  | BB 0C000000             | mov ebx,C                                       |
6F3AEB56  | 8BCE                    | mov ecx,esi                                     |
6F3AEB58  | 895D 50                 | mov dword ptr ss:[ebp+50],ebx                   |
6F3AEB5B  | 897D 44                 | mov dword ptr ss:[ebp+44],edi                   |
6F3AEB5E  | 897D 48                 | mov dword ptr ss:[ebp+48],edi                   |
6F3AEB61  | E8 2A86FFFF             | call game.6F3A7190                              |
6F3AEB66  | 53                      | push ebx                                        |
6F3AEB67  | 8BCE                    | mov ecx,esi                                     |
6F3AEB69  | 893E                    | mov dword ptr ds:[esi],edi                      |
6F3AEB6B  | 897E 04                 | mov dword ptr ds:[esi+4],edi                    |
6F3AEB6E  | 897E 08                 | mov dword ptr ds:[esi+8],edi                    |
6F3AEB71  | E8 FAF7FFFF             | call game.6F3AE370                              |
6F3AEB76  | 8DB5 B4000000           | lea esi,dword ptr ss:[ebp+B4]                   |
6F3AEB7C  | 8D6424 00               | lea esp,dword ptr ss:[esp]                      |
6F3AEB80  | 57                      | push edi                                        |
6F3AEB81  | 8BCE                    | mov ecx,esi                                     |
6F3AEB83  | 897E 1C                 | mov dword ptr ds:[esi+1C],edi                   |
6F3AEB86  | E8 D550FFFF             | call game.6F3A3C60                              |
6F3AEB8B  | 83C6 2C                 | add esi,2C                                      |
6F3AEB8E  | 83EB 01                 | sub ebx,1                                       |
6F3AEB91  | 75 ED                   | jne game.6F3AEB80                               |
6F3AEB93  | 8B75 30                 | mov esi,dword ptr ss:[ebp+30]                   |
6F3AEB96  | 3BF7                    | cmp esi,edi                                     |
6F3AEB98  | 74 15                   | je game.6F3AEBAF                                |
6F3AEB9A  | 8BCE                    | mov ecx,esi                                     |
6F3AEB9C  | E8 1FFFFEFF             | call game.6F39EAC0                              |
6F3AEBA1  | 57                      | push edi                                        |
6F3AEBA2  | 6A FF                   | push FFFFFFFF                                   |
6F3AEBA4  | 68 044F876F             | push game.6F874F04                              | ---> "delete"
6F3AEBA9  | 56                      | push esi                                        |
6F3AEBAA  | E8 A9C93300             | call <JMP.&Ordinal#403>                         |
6F3AEBAF  | 57                      | push edi                                        |
6F3AEBB0  | 68 5B020000             | push 25B                                        |
6F3AEBB5  | 68 3819946F             | push game.6F941938                              | ---> ".\\CGameWar3.cpp"
6F3AEBBA  | 6A 64                   | push 64                                         |
6F3AEBBC  | E8 F1C93300             | call <JMP.&Ordinal#401>                         |
6F3AEBC1  | 894424 1C               | mov dword ptr ss:[esp+1C],eax                   |
6F3AEBC5  | 894424 18               | mov dword ptr ss:[esp+18],eax                   |
6F3AEBC9  | 3BC7                    | cmp eax,edi                                     |
6F3AEBCB  | 897C24 54               | mov dword ptr ss:[esp+54],edi                   |
6F3AEBCF  | 74 09                   | je game.6F3AEBDA                                |
6F3AEBD1  | 8BC8                    | mov ecx,eax                                     |
6F3AEBD3  | E8 A821FFFF             | call game.6F3A0D80                              |
6F3AEBD8  | EB 02                   | jmp game.6F3AEBDC                               |
6F3AEBDA  | 33C0                    | xor eax,eax                                     |
6F3AEBDC  | 8B4D 34                 | mov ecx,dword ptr ss:[ebp+34]                   | ---> "\\drive1\\temp\\buildwar3x\\war3\\source\\game\\CFogMaskTable.h"
6F3AEBDF  | 83CE FF                 | or esi,FFFFFFFF                                 |
6F3AEBE2  | 3BCF                    | cmp ecx,edi                                     |
6F3AEBE4  | 897424 54               | mov dword ptr ss:[esp+54],esi                   |
6F3AEBE8  | 8945 30                 | mov dword ptr ss:[ebp+30],eax                   |
6F3AEBEB  | 74 08                   | je game.6F3AEBF5                                |
6F3AEBED  | 8B01                    | mov eax,dword ptr ds:[ecx]                      |
6F3AEBEF  | 8B10                    | mov edx,dword ptr ds:[eax]                      |
6F3AEBF1  | 6A 01                   | push 1                                          |
6F3AEBF3  | FFD2                    | call edx                                        |
6F3AEBF5  | 57                      | push edi                                        |
6F3AEBF6  | 68 5E020000             | push 25E                                        |
6F3AEBFB  | 68 3819946F             | push game.6F941938                              | ---> ".\\CGameWar3.cpp"
6F3AEC00  | 68 8C000000             | push 8C                                         |
6F3AEC05  | E8 A8C93300             | call <JMP.&Ordinal#401>                         |
6F3AEC0A  | 894424 1C               | mov dword ptr ss:[esp+1C],eax                   |
6F3AEC0E  | 894424 18               | mov dword ptr ss:[esp+18],eax                   |
6F3AEC12  | 3BC7                    | cmp eax,edi                                     |
6F3AEC14  | C74424 54 01000000      | mov dword ptr ss:[esp+54],1                     |
6F3AEC1C  | 74 09                   | je game.6F3AEC27                                |
6F3AEC1E  | 8BC8                    | mov ecx,eax                                     |
6F3AEC20  | E8 DB9F0500             | call game.6F408C00                              |
6F3AEC25  | EB 02                   | jmp game.6F3AEC29                               |
6F3AEC27  | 33C0                    | xor eax,eax                                     |
6F3AEC29  | 8BCD                    | mov ecx,ebp                                     |
6F3AEC2B  | 897424 54               | mov dword ptr ss:[esp+54],esi                   |
6F3AEC2F  | 8945 34                 | mov dword ptr ss:[ebp+34],eax                   | ---> "\\drive1\\temp\\buildwar3x\\war3\\source\\game\\CFogMaskTable.h"
6F3AEC32  | E8 09FFFEFF             | call game.6F39EB40                              |
6F3AEC37  | 33F6                    | xor esi,esi                                     |
6F3AEC39  | 397D 54                 | cmp dword ptr ss:[ebp+54],edi                   |
6F3AEC3C  | 76 1B                   | jbe game.6F3AEC59                               |
6F3AEC3E  | 8D5D 58                 | lea ebx,dword ptr ss:[ebp+58]                   |
6F3AEC41  | 8B0B                    | mov ecx,dword ptr ds:[ebx]                      |
6F3AEC43  | 3BCF                    | cmp ecx,edi                                     |
6F3AEC45  | 74 07                   | je game.6F3AEC4E                                |
6F3AEC47  | 8B01                    | mov eax,dword ptr ds:[ecx]                      |
6F3AEC49  | 8B50 5C                 | mov edx,dword ptr ds:[eax+5C]                   |
6F3AEC4C  | FFD2                    | call edx                                        |
6F3AEC4E  | 83C6 01                 | add esi,1                                       |
6F3AEC51  | 83C3 04                 | add ebx,4                                       |
6F3AEC54  | 3B75 54                 | cmp esi,dword ptr ss:[ebp+54]                   |
6F3AEC57  | 72 E8                   | jb game.6F3AEC41                                |
6F3AEC59  | 837D 54 00              | cmp dword ptr ss:[ebp+54],0                     |
6F3AEC5D  | 897C24 18               | mov dword ptr ss:[esp+18],edi                   |
6F3AEC61  | 0F86 EE000000           | jbe game.6F3AED55                               |
6F3AEC67  | 8D5D 58                 | lea ebx,dword ptr ss:[ebp+58]                   |
6F3AEC6A  | 8D9B 00000000           | lea ebx,dword ptr ds:[ebx]                      |
6F3AEC70  | E8 2BC30500             | call game.6F40AFA0                              |
6F3AEC75  | 8B35 6873AB6F           | mov esi,dword ptr ds:[6FAB7368]                 |
6F3AEC7B  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]                   |
6F3AEC7F  | 894424 1C               | mov dword ptr ss:[esp+1C],eax                   |
6F3AEC83  | E8 98981100             | call game.6F4C8520                              |
6F3AEC88  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]                   |
6F3AEC8C  | 51                      | push ecx                                        |
6F3AEC8D  | 50                      | push eax                                        |
6F3AEC8E  | 8D4E 0C                 | lea ecx,dword ptr ds:[esi+C]                    |
6F3AEC91  | E8 2A32C5FF             | call game.6F001EC0                              |
6F3AEC96  | 8B50 70                 | mov edx,dword ptr ds:[eax+70]                   |
6F3AEC99  | 52                      | push edx                                        |
6F3AEC9A  | E8 01C30500             | call game.6F40AFA0                              |
6F3AEC9F  | 8BD0                    | mov edx,eax                                     |
6F3AECA1  | 8D4C24 24               | lea ecx,dword ptr ss:[esp+24]                   |
6F3AECA5  | E8 763F0C00             | call game.6F472C20                              |
6F3AECAA  | 6A 01                   | push 1                                          |
6F3AECAC  | BA 01000000             | mov edx,1                                       |
6F3AECB1  | 8D4C24 24               | lea ecx,dword ptr ss:[esp+24]                   |
6F3AECB5  | C74424 48 FFFFFFFF      | mov dword ptr ss:[esp+48],FFFFFFFF              |
6F3AECBD  | E8 BEBE0D00             | call game.6F48AB80                              |
6F3AECC2  | 8B70 54                 | mov esi,dword ptr ds:[eax+54]                   |
6F3AECC5  | 85F6                    | test esi,esi                                    |
6F3AECC7  | 74 29                   | je game.6F3AECF2                                |
6F3AECC9  | E8 D2C20500             | call game.6F40AFA0                              |
6F3AECCE  | 8BF8                    | mov edi,eax                                     |
6F3AECD0  | 8B06                    | mov eax,dword ptr ds:[esi]                      |
6F3AECD2  | 8B50 1C                 | mov edx,dword ptr ds:[eax+1C]                   |
6F3AECD5  | 8BCE                    | mov ecx,esi                                     |
6F3AECD7  | FFD2                    | call edx                                        |
6F3AECD9  | 8BD7                    | mov edx,edi                                     |
6F3AECDB  | 8BC8                    | mov ecx,eax                                     |
6F3AECDD  | E8 2E2C0C00             | call game.6F471910                              |
6F3AECE2  | 85C0                    | test eax,eax                                    |
6F3AECE4  | 8B7C24 18               | mov edi,dword ptr ss:[esp+18]                   |
6F3AECE8  | 74 08                   | je game.6F3AECF2                                |
6F3AECEA  | 8BC6                    | mov eax,esi                                     |
6F3AECEC  | 894424 14               | mov dword ptr ss:[esp+14],eax                   |
6F3AECF0  | EB 0C                   | jmp game.6F3AECFE                               |
6F3AECF2  | C74424 14 00000000      | mov dword ptr ss:[esp+14],0                     |
6F3AECFA  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]                   |
6F3AECFE  | 8B0B                    | mov ecx,dword ptr ds:[ebx]                      |
6F3AED00  | 3BC8                    | cmp ecx,eax                                     |
6F3AED02  | 74 22                   | je game.6F3AED26                                |
6F3AED04  | 85C9                    | test ecx,ecx                                    |
6F3AED06  | 74 10                   | je game.6F3AED18                                |
6F3AED08  | 8341 04 FF              | add dword ptr ds:[ecx+4],FFFFFFFF               |
6F3AED0C  | 75 0A                   | jne game.6F3AED18                               |
6F3AED0E  | 8B01                    | mov eax,dword ptr ds:[ecx]                      |
6F3AED10  | 8B10                    | mov edx,dword ptr ds:[eax]                      |
6F3AED12  | FFD2                    | call edx                                        |
6F3AED14  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]                   |
6F3AED18  | 85C0                    | test eax,eax                                    |
6F3AED1A  | 74 08                   | je game.6F3AED24                                |
6F3AED1C  | 85F6                    | test esi,esi                                    |
6F3AED1E  | 74 04                   | je game.6F3AED24                                |
6F3AED20  | 8346 04 01              | add dword ptr ds:[esi+4],1                      |
6F3AED24  | 8903                    | mov dword ptr ds:[ebx],eax                      |
6F3AED26  | 8B0B                    | mov ecx,dword ptr ds:[ebx]                      |
6F3AED28  | 8B01                    | mov eax,dword ptr ds:[ecx]                      |
6F3AED2A  | 8B50 78                 | mov edx,dword ptr ds:[eax+78]                   |
6F3AED2D  | FFD2                    | call edx                                        |
6F3AED2F  | 8B0B                    | mov ecx,dword ptr ds:[ebx]                      |
6F3AED31  | 57                      | push edi                                        |
6F3AED32  | E8 69C70600             | call game.6F41B4A0                              |
6F3AED37  | 8B03                    | mov eax,dword ptr ds:[ebx]                      |
6F3AED39  | 8B48 34                 | mov ecx,dword ptr ds:[eax+34]                   |
6F3AED3C  | 89B9 AC010000           | mov dword ptr ds:[ecx+1AC],edi                  |
6F3AED42  | 83C7 01                 | add edi,1                                       |
6F3AED45  | 83C3 04                 | add ebx,4                                       |
6F3AED48  | 3B7D 54                 | cmp edi,dword ptr ss:[ebp+54]                   |
6F3AED4B  | 897C24 18               | mov dword ptr ss:[esp+18],edi                   |
6F3AED4F  | 0F82 1BFFFFFF           | jb game.6F3AEC70                                |
6F3AED55  | 33F6                    | xor esi,esi                                     |
6F3AED57  | 3975 54                 | cmp dword ptr ss:[ebp+54],esi                   |
6F3AED5A  | 0F86 23010000           | jbe game.6F3AEE83                               |
6F3AED60  | 8D5D 58                 | lea ebx,dword ptr ss:[ebp+58]                   |
6F3AED63  | 8B0B                    | mov ecx,dword ptr ds:[ebx]                      |
6F3AED65  | 56                      | push esi                                        |
6F3AED66  | E8 35C70600             | call game.6F41B4A0                              |
6F3AED6B  | 83FE 0C                 | cmp esi,C                                       |
6F3AED6E  | 8B13                    | mov edx,dword ptr ds:[ebx]                      |
6F3AED70  | 89B2 64020000           | mov dword ptr ds:[edx+264],esi                  |
6F3AED76  | 73 0A                   | jae game.6F3AED82                               |
6F3AED78  | 56                      | push esi                                        |
6F3AED79  | 6A 00                   | push 0                                          |
6F3AED7B  | 8BCD                    | mov ecx,ebp                                     |
6F3AED7D  | E8 2E95FFFF             | call game.6F3A82B0                              |
6F3AED82  | 8B0B                    | mov ecx,dword ptr ds:[ebx]                      |
6F3AED84  | E8 97C60600             | call game.6F41B420                              |
6F3AED89  | 8BF8                    | mov edi,eax                                     |
6F3AED8B  | 56                      | push esi                                        |
6F3AED8C  | 6A 00                   | push 0                                          |
6F3AED8E  | 8BCF                    | mov ecx,edi                                     |
6F3AED90  | E8 FB7A0300             | call game.6F3E6890                              |
6F3AED95  | 56                      | push esi                                        |
6F3AED96  | 6A 01                   | push 1                                          |
6F3AED98  | 8BCF                    | mov ecx,edi                                     |
6F3AED9A  | E8 F17A0300             | call game.6F3E6890                              |
6F3AED9F  | 56                      | push esi                                        |
6F3AEDA0  | 6A 02                   | push 2                                          |
6F3AEDA2  | 8BCF                    | mov ecx,edi                                     |
6F3AEDA4  | E8 E77A0300             | call game.6F3E6890                              |
6F3AEDA9  | 56                      | push esi                                        |
6F3AEDAA  | 6A 03                   | push 3                                          |
6F3AEDAC  | 8BCF                    | mov ecx,edi                                     |
6F3AEDAE  | E8 DD7A0300             | call game.6F3E6890                              |
6F3AEDB3  | 56                      | push esi                                        |
6F3AEDB4  | 6A 04                   | push 4                                          |
6F3AEDB6  | 8BCF                    | mov ecx,edi                                     |
6F3AEDB8  | E8 D37A0300             | call game.6F3E6890                              |
6F3AEDBD  | 8B0B                    | mov ecx,dword ptr ds:[ebx]                      |
6F3AEDBF  | 56                      | push esi                                        |
6F3AEDC0  | E8 7B030700             | call game.6F41F140                              |
6F3AEDC5  | 56                      | push esi                                        |
6F3AEDC6  | 6A 06                   | push 6                                          |
6F3AEDC8  | 8BCF                    | mov ecx,edi                                     |
6F3AEDCA  | E8 C17A0300             | call game.6F3E6890                              |
6F3AEDCF  | 56                      | push esi                                        |
6F3AEDD0  | 6A 07                   | push 7                                          |
6F3AEDD2  | 8BCF                    | mov ecx,edi                                     |
6F3AEDD4  | E8 B77A0300             | call game.6F3E6890                              |
6F3AEDD9  | 6A 0F                   | push F                                          |
6F3AEDDB  | 6A 00                   | push 0                                          |
6F3AEDDD  | 8BCF                    | mov ecx,edi                                     |
6F3AEDDF  | E8 AC7A0300             | call game.6F3E6890                              |
6F3AEDE4  | 8B8D 94000000           | mov ecx,dword ptr ss:[ebp+94]                   |
6F3AEDEA  | E8 31C60600             | call game.6F41B420                              |
6F3AEDEF  | 56                      | push esi                                        |
6F3AEDF0  | 6A 00                   | push 0                                          |
6F3AEDF2  | 8BC8                    | mov ecx,eax                                     |
6F3AEDF4  | E8 977A0300             | call game.6F3E6890                              |
6F3AEDF9  | 8B3B                    | mov edi,dword ptr ds:[ebx]                      |
6F3AEDFB  | E8 10B4C5FF             | call game.6F00A210                              |
6F3AEE00  | 50                      | push eax                                        |
6F3AEE01  | 8D8F A0000000           | lea ecx,dword ptr ds:[edi+A0]                   |
6F3AEE07  | E8 34430C00             | call game.6F473140                              |
6F3AEE0C  | 8B03                    | mov eax,dword ptr ds:[ebx]                      |
6F3AEE0E  | 6A 03                   | push 3                                          |
6F3AEE10  | 8D48 40                 | lea ecx,dword ptr ds:[eax+40]                   |
6F3AEE13  | E8 28430C00             | call game.6F473140                              | 这里是网络连接断开弹出窗口的错误码来源：3
6F3AEE18  | A1 F4E4AA6F             | mov eax,dword ptr ds:[6FAAE4F4]                 |
6F3AEE1D  | 8B0B                    | mov ecx,dword ptr ds:[ebx]                      |
6F3AEE1F  | 894424 14               | mov dword ptr ss:[esp+14],eax                   |
6F3AEE23  | D94424 14               | fld dword ptr ss:[esp+14]                       |
6F3AEE27  | D905 80E4AA6F           | fld dword ptr ds:[6FAAE480]                     |
6F3AEE2D  | D8D9                    | fcomp st(1)                                     |
6F3AEE2F  | DFE0                    | fnstsw ax                                       |
6F3AEE31  | F6C4 41                 | test ah,41                                      |
6F3AEE34  | 75 09                   | jne game.6F3AEE3F                               |
6F3AEE36  | DDD8                    | fstp st(0)                                      |
6F3AEE38  | B8 80E4AA6F             | mov eax,game.6FAAE480                           |
6F3AEE3D  | EB 18                   | jmp game.6F3AEE57                               |
6F3AEE3F  | D905 74E5AA6F           | fld dword ptr ds:[6FAAE574]                     |
6F3AEE45  | DED9                    | fcompp                                          |
6F3AEE47  | DFE0                    | fnstsw ax                                       |
6F3AEE49  | F6C4 05                 | test ah,5                                       |
6F3AEE4C  | B8 74E5AA6F             | mov eax,game.6FAAE574                           |
6F3AEE51  | 7B 04                   | jnp game.6F3AEE57                               |
6F3AEE53  | 8D4424 14               | lea eax,dword ptr ss:[esp+14]                   |
6F3AEE57  | 8B10                    | mov edx,dword ptr ds:[eax]                      |
6F3AEE59  | 8B81 98020000           | mov eax,dword ptr ds:[ecx+298]                  |
6F3AEE5F  | 8B00                    | mov eax,dword ptr ds:[eax]                      |
6F3AEE61  | 81C1 98020000           | add ecx,298                                     |
6F3AEE67  | 895424 14               | mov dword ptr ss:[esp+14],edx                   |
6F3AEE6B  | 6A 01                   | push 1                                          |
6F3AEE6D  | 8D5424 18               | lea edx,dword ptr ss:[esp+18]                   |
6F3AEE71  | 52                      | push edx                                        |
6F3AEE72  | FFD0                    | call eax                                        |
6F3AEE74  | 83C6 01                 | add esi,1                                       |
6F3AEE77  | 83C3 04                 | add ebx,4                                       |
6F3AEE7A  | 3B75 54                 | cmp esi,dword ptr ss:[ebp+54]                   |
6F3AEE7D  | 0F82 E0FEFFFF           | jb game.6F3AED63                                |
6F3AEE83  | 8B85 94000000           | mov eax,dword ptr ss:[ebp+94]                   |
6F3AEE89  | 6A 01                   | push 1                                          |
6F3AEE8B  | 8D88 B0000000           | lea ecx,dword ptr ds:[eax+B0]                   |
6F3AEE91  | E8 AA420C00             | call game.6F473140                              |
6F3AEE96  | 8B85 88000000           | mov eax,dword ptr ss:[ebp+88]                   |
6F3AEE9C  | 8DB5 88000000           | lea esi,dword ptr ss:[ebp+88]                   |
6F3AEEA2  | 6A 01                   | push 1                                          |
6F3AEEA4  | 8D88 B0000000           | lea ecx,dword ptr ds:[eax+B0]                   |
6F3AEEAA  | E8 91420C00             | call game.6F473140                              |
6F3AEEAF  | 8B85 8C000000           | mov eax,dword ptr ss:[ebp+8C]                   |
6F3AEEB5  | 6A 01                   | push 1                                          |
6F3AEEB7  | 8D88 B0000000           | lea ecx,dword ptr ds:[eax+B0]                   |
6F3AEEBD  | E8 7E420C00             | call game.6F473140                              |
6F3AEEC2  | 8B85 90000000           | mov eax,dword ptr ss:[ebp+90]                   |
6F3AEEC8  | 6A 01                   | push 1                                          |
6F3AEECA  | 8D88 B0000000           | lea ecx,dword ptr ds:[eax+B0]                   |
6F3AEED0  | E8 6B420C00             | call game.6F473140                              |
6F3AEED5  | BF 04000000             | mov edi,4                                       |
6F3AEEDA  | 8BDF                    | mov ebx,edi                                     |
6F3AEEDC  | 8D6424 00               | lea esp,dword ptr ss:[esp]                      |
6F3AEEE0  | 8B0E                    | mov ecx,dword ptr ds:[esi]                      |
6F3AEEE2  | 89B9 68020000           | mov dword ptr ds:[ecx+268],edi                  |
6F3AEEE8  | E8 13780600             | call game.6F416700                              |
6F3AEEED  | 8B0E                    | mov ecx,dword ptr ds:[esi]                      |
6F3AEEEF  | 03F7                    | add esi,edi                                     |
6F3AEEF1  | 83EB 01                 | sub ebx,1                                       |
6F3AEEF4  | C781 70020000 01000000  | mov dword ptr ds:[ecx+270],1                    | <--- 4个预留位置，不用管
6F3AEEFE  | 75 E0                   | jne game.6F3AEEE0                               |
6F3AEF00  | 8B4D 1C                 | mov ecx,dword ptr ss:[ebp+1C]                   |
6F3AEF03  | 33F6                    | xor esi,esi                                     |
6F3AEF05  | 3BCE                    | cmp ecx,esi                                     |
6F3AEF07  | 74 1D                   | je game.6F3AEF26                                |
6F3AEF09  | 8B11                    | mov edx,dword ptr ds:[ecx]                      |
6F3AEF0B  | 8B42 5C                 | mov eax,dword ptr ds:[edx+5C]                   |
6F3AEF0E  | FFD0                    | call eax                                        |
6F3AEF10  | 8B4D 1C                 | mov ecx,dword ptr ss:[ebp+1C]                   |
6F3AEF13  | 3BCE                    | cmp ecx,esi                                     |
6F3AEF15  | 74 0F                   | je game.6F3AEF26                                |
6F3AEF17  | 8341 04 FF              | add dword ptr ds:[ecx+4],FFFFFFFF               |
6F3AEF1B  | 75 06                   | jne game.6F3AEF23                               |
6F3AEF1D  | 8B11                    | mov edx,dword ptr ds:[ecx]                      |
6F3AEF1F  | 8B02                    | mov eax,dword ptr ds:[edx]                      |
6F3AEF21  | FFD0                    | call eax                                        |
6F3AEF23  | 8975 1C                 | mov dword ptr ss:[ebp+1C],esi                   |
6F3AEF26  | 8B8D 00040000           | mov ecx,dword ptr ss:[ebp+400]                  |
6F3AEF2C  | 3BCE                    | cmp ecx,esi                                     |
6F3AEF2E  | 74 23                   | je game.6F3AEF53                                |
6F3AEF30  | 8B11                    | mov edx,dword ptr ds:[ecx]                      |
6F3AEF32  | 8B42 5C                 | mov eax,dword ptr ds:[edx+5C]                   |
6F3AEF35  | FFD0                    | call eax                                        |
6F3AEF37  | 8B8D 00040000           | mov ecx,dword ptr ss:[ebp+400]                  |
6F3AEF3D  | 3BCE                    | cmp ecx,esi                                     |
6F3AEF3F  | 74 12                   | je game.6F3AEF53                                |
6F3AEF41  | 8341 04 FF              | add dword ptr ds:[ecx+4],FFFFFFFF               |
6F3AEF45  | 75 06                   | jne game.6F3AEF4D                               |
6F3AEF47  | 8B11                    | mov edx,dword ptr ds:[ecx]                      |
6F3AEF49  | 8B02                    | mov eax,dword ptr ds:[edx]                      |
6F3AEF4B  | FFD0                    | call eax                                        |
6F3AEF4D  | 89B5 00040000           | mov dword ptr ss:[ebp+400],esi                  |
6F3AEF53  | 8B8D 04040000           | mov ecx,dword ptr ss:[ebp+404]                  |
6F3AEF59  | 3BCE                    | cmp ecx,esi                                     |
6F3AEF5B  | 74 0E                   | je game.6F3AEF6B                                |
6F3AEF5D  | 8B11                    | mov edx,dword ptr ds:[ecx]                      |
6F3AEF5F  | 8B02                    | mov eax,dword ptr ds:[edx]                      |
6F3AEF61  | 6A 01                   | push 1                                          |
6F3AEF63  | FFD0                    | call eax                                        |
6F3AEF65  | 89B5 04040000           | mov dword ptr ss:[ebp+404],esi                  |
6F3AEF6B  | 8B4C24 4C               | mov ecx,dword ptr ss:[esp+4C]                   |
6F3AEF6F  | 64:890D 00000000        | mov dword ptr fs:[0],ecx                        |
6F3AEF76  | 59                      | pop ecx                                         |
6F3AEF77  | 5F                      | pop edi                                         |
6F3AEF78  | 5E                      | pop esi                                         |
6F3AEF79  | 5D                      | pop ebp                                         |
6F3AEF7A  | 5B                      | pop ebx                                         |
6F3AEF7B  | 83C4 44                 | add esp,44                                      |
6F3AEF7E  | C3                      | ret                                             |
```

### 4.2 `game.dll + 5C0A03`

```assembly
6F5C0830  | 81EC AC000000           | sub esp,AC                                      |
6F5C0836  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]                 |
6F5C083B  | 33C4                    | xor eax,esp                                     |
6F5C083D  | 898424 A8000000         | mov dword ptr ss:[esp+A8],eax                   |
6F5C0844  | 53                      | push ebx                                        |
6F5C0845  | 55                      | push ebp                                        |
6F5C0846  | 8B2D F465AB6F           | mov ebp,dword ptr ds:[6FAB65F4]                 |
6F5C084C  | 56                      | push esi                                        |
6F5C084D  | 57                      | push edi                                        |
6F5C084E  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                   |
6F5C0852  | 8BF1                    | mov esi,ecx                                     |
6F5C0854  | 50                      | push eax                                        |
6F5C0855  | 8D5C24 18               | lea ebx,dword ptr ss:[esp+18]                   |
6F5C0859  | 8D7C24 6C               | lea edi,dword ptr ss:[esp+6C]                   |
6F5C085D  | 897424 24               | mov dword ptr ss:[esp+24],esi                   |
6F5C0861  | 896C24 2C               | mov dword ptr ss:[esp+2C],ebp                   |
6F5C0865  | E8 16FFFFFF             | call <game.sub_6F5C0780>                        |
6F5C086A  | 8B4C24 68               | mov ecx,dword ptr ss:[esp+68]                   |
6F5C086E  | 8B4424 6C               | mov eax,dword ptr ss:[esp+6C]                   |
6F5C0872  | 894C24 38               | mov dword ptr ss:[esp+38],ecx                   |
6F5C0876  | 8B4C24 70               | mov ecx,dword ptr ss:[esp+70]                   |
6F5C087A  | 894424 3C               | mov dword ptr ss:[esp+3C],eax                   |
6F5C087E  | 8B4424 74               | mov eax,dword ptr ss:[esp+74]                   |
6F5C0882  | 894C24 40               | mov dword ptr ss:[esp+40],ecx                   |
6F5C0886  | 8B4C24 78               | mov ecx,dword ptr ss:[esp+78]                   |
6F5C088A  | 894424 44               | mov dword ptr ss:[esp+44],eax                   |
6F5C088E  | 8B4424 7C               | mov eax,dword ptr ss:[esp+7C]                   |
6F5C0892  | 894C24 48               | mov dword ptr ss:[esp+48],ecx                   |
6F5C0896  | 8B8C24 80000000         | mov ecx,dword ptr ss:[esp+80]                   |
6F5C089D  | 894424 4C               | mov dword ptr ss:[esp+4C],eax                   |
6F5C08A1  | 8B8424 84000000         | mov eax,dword ptr ss:[esp+84]                   |
6F5C08A8  | 894C24 50               | mov dword ptr ss:[esp+50],ecx                   |
6F5C08AC  | 8B8C24 88000000         | mov ecx,dword ptr ss:[esp+88]                   |
6F5C08B3  | 894424 54               | mov dword ptr ss:[esp+54],eax                   |
6F5C08B7  | 8B8424 8C000000         | mov eax,dword ptr ss:[esp+8C]                   |
6F5C08BE  | 894C24 58               | mov dword ptr ss:[esp+58],ecx                   |
6F5C08C2  | 8B8C24 90000000         | mov ecx,dword ptr ss:[esp+90]                   |
6F5C08C9  | 894424 5C               | mov dword ptr ss:[esp+5C],eax                   |
6F5C08CD  | 8B8424 94000000         | mov eax,dword ptr ss:[esp+94]                   |
6F5C08D4  | 894C24 60               | mov dword ptr ss:[esp+60],ecx                   |
6F5C08D8  | 894424 64               | mov dword ptr ss:[esp+64],eax                   |
6F5C08DC  | 8D85 BC020000           | lea eax,dword ptr ss:[ebp+2BC]                  |
6F5C08E2  | 8D4C24 34               | lea ecx,dword ptr ss:[esp+34]                   |
6F5C08E6  | BA 0C000000             | mov edx,C                                       |
6F5C08EB  | 33DB                    | xor ebx,ebx                                     |
6F5C08ED  | 3BC1                    | cmp eax,ecx                                     |
6F5C08EF  | 895424 34               | mov dword ptr ss:[esp+34],edx                   |
6F5C08F3  | 74 15                   | je game.6F5C090A                                |
6F5C08F5  | 3BD3                    | cmp edx,ebx                                     |
6F5C08F7  | 76 0F                   | jbe game.6F5C0908                               |
6F5C08F9  | 8D78 04                 | lea edi,dword ptr ds:[eax+4]                    |
6F5C08FC  | 8BCA                    | mov ecx,edx                                     |
6F5C08FE  | 8D7424 38               | lea esi,dword ptr ss:[esp+38]                   |
6F5C0902  | F3:A5                   | rep movsd                                       |
6F5C0904  | 8B7424 20               | mov esi,dword ptr ss:[esp+20]                   |
6F5C0908  | 8910                    | mov dword ptr ds:[eax],edx                      |
6F5C090A  | 0FB656 11               | movzx edx,byte ptr ds:[esi+11]                  |
6F5C090E  | 395424 14               | cmp dword ptr ss:[esp+14],edx                   |
6F5C0912  | 74 07                   | je game.6F5C091B                                |
6F5C0914  | 33C0                    | xor eax,eax                                     |
6F5C0916  | E9 33030000             | jmp game.6F5C0C4E                               |
6F5C091B  | 8B7C24 2C               | mov edi,dword ptr ss:[esp+2C]                   |
6F5C091F  | 3B7E 04                 | cmp edi,dword ptr ds:[esi+4]                    |
6F5C0922  | 75 F0                   | jne game.6F5C0914                               |
6F5C0924  | 33F6                    | xor esi,esi                                     |
6F5C0926  | 56                      | push esi                                        |
6F5C0927  | 8BCD                    | mov ecx,ebp                                     |
6F5C0929  | E8 220DDEFF             | call <game.sub_6F3A1650>                        |
6F5C092E  | 83B8 68020000 05        | cmp dword ptr ds:[eax+268],5                    |
6F5C0935  | 75 16                   | jne game.6F5C094D                               |
6F5C0937  | 56                      | push esi                                        |
6F5C0938  | 8BCD                    | mov ecx,ebp                                     |
6F5C093A  | E8 210DDEFF             | call <game.sub_6F3A1660>                        |
6F5C093F  | 83F8 FF                 | cmp eax,FFFFFFFF                                |
6F5C0942  | 74 09                   | je game.6F5C094D                                |
6F5C0944  | 56                      | push esi                                        |
6F5C0945  | 50                      | push eax                                        |
6F5C0946  | 8BCD                    | mov ecx,ebp                                     |
6F5C0948  | E8 B379DEFF             | call <game.sub_6F3A8300>                        |
6F5C094D  | 83C6 01                 | add esi,1                                       |
6F5C0950  | 83FE 0C                 | cmp esi,C                                       |
6F5C0953  | 72 D1                   | jb game.6F5C0926                                |
6F5C0955  | 8B4C24 20               | mov ecx,dword ptr ss:[esp+20]                   |
6F5C0959  | F641 10 01              | test byte ptr ds:[ecx+10],1                     |
6F5C095D  | 8B45 30                 | mov eax,dword ptr ss:[ebp+30]                   |
6F5C0960  | 74 09                   | je game.6F5C096B                                |
6F5C0962  | 8148 38 00001000        | or dword ptr ds:[eax+38],100000                 |
6F5C0969  | EB 07                   | jmp game.6F5C0972                               |
6F5C096B  | 8160 38 FFFFEFFF        | and dword ptr ds:[eax+38],FFEFFFFF              |
6F5C0972  | F641 10 02              | test byte ptr ds:[ecx+10],2                     |
6F5C0976  | 8B45 30                 | mov eax,dword ptr ss:[ebp+30]                   |
6F5C0979  | 74 09                   | je game.6F5C0984                                |
6F5C097B  | 8148 38 00002000        | or dword ptr ds:[eax+38],200000                 |
6F5C0982  | EB 07                   | jmp game.6F5C098B                               |
6F5C0984  | 8160 38 FFFFDFFF        | and dword ptr ds:[eax+38],FFDFFFFF              |
6F5C098B  | 3BFB                    | cmp edi,ebx                                     |
6F5C098D  | 895C24 14               | mov dword ptr ss:[esp+14],ebx                   |
6F5C0991  | 895C24 24               | mov dword ptr ss:[esp+24],ebx                   |
6F5C0995  | 0F86 AE020000           | jbe game.6F5C0C49                               |
6F5C099B  | 895C24 1C               | mov dword ptr ss:[esp+1C],ebx                   |
6F5C099F  | EB 06                   | jmp game.6F5C09A7                               |
6F5C09A1  | 8B4C24 20               | mov ecx,dword ptr ss:[esp+20]                   |
6F5C09A5  | 8BEE                    | mov ebp,esi                                     |
6F5C09A7  | 8B4424 24               | mov eax,dword ptr ss:[esp+24]                   |
6F5C09AB  | 8B71 08                 | mov esi,dword ptr ds:[ecx+8]                    |
6F5C09AE  | 8B5C84 68               | mov ebx,dword ptr ss:[esp+eax*4+68]             |
6F5C09B2  | 037424 1C               | add esi,dword ptr ss:[esp+1C]                   |
6F5C09B6  | 53                      | push ebx                                        |
6F5C09B7  | 8BCD                    | mov ecx,ebp                                     |
6F5C09B9  | E8 920CDEFF             | call <game.sub_6F3A1650>                        |
6F5C09BE  | 8A0E                    | mov cl,byte ptr ds:[esi]                        |
6F5C09C0  | 33D2                    | xor edx,edx                                     |
6F5C09C2  | 8BF8                    | mov edi,eax                                     |
6F5C09C4  | 83CD FF                 | or ebp,FFFFFFFF                                 |
6F5C09C7  | E8 24EAF8FF             | call <game.sub_6F54F3F0>                        |
6F5C09CC  | 807E 02 02              | cmp byte ptr ds:[esi+2],2                       |
6F5C09D0  | 884424 13               | mov byte ptr ss:[esp+13],al                     |
6F5C09D4  | 0F85 B0010000           | jne game.6F5C0B8A                               |
6F5C09DA  | 8A4E 03                 | mov cl,byte ptr ds:[esi+3]                      |
6F5C09DD  | 84C9                    | test cl,cl                                      |
6F5C09DF  | 75 08                   | jne game.6F5C09E9                               |
6F5C09E1  | 3C FF                   | cmp al,FF                                       |
6F5C09E3  | 0F84 A1010000           | je game.6F5C0B8A                                |
6F5C09E9  | 0FB6C9                  | movzx ecx,cl                                    |
6F5C09EC  | 898F 68020000           | mov dword ptr ds:[edi+268],ecx                  |
6F5C09F2  | 8BCF                    | mov ecx,edi                                     |
6F5C09F4  | E8 075DE5FF             | call <game.sub_6F416700>                        |
6F5C09F9  | 0FB656 07               | movzx edx,byte ptr ds:[esi+7]                   |
6F5C09FD  | 8997 6C020000           | mov dword ptr ds:[edi+26C],edx                  |
6F5C0A03  | C787 70020000 01000000  | mov dword ptr ds:[edi+270],1                    |
6F5C0A0D  | 0FB646 05               | movzx eax,byte ptr ds:[esi+5]                   |
6F5C0A11  | 8987 64020000           | mov dword ptr ds:[edi+264],eax                  |
6F5C0A17  | 0FB64E 06               | movzx ecx,byte ptr ds:[esi+6]                   |
6F5C0A1B  | 338F 5C020000           | xor ecx,dword ptr ds:[edi+25C]                  |
6F5C0A21  | 0FB656 06               | movzx edx,byte ptr ds:[esi+6]                   |
6F5C0A25  | 83E1 40                 | and ecx,40                                      |
6F5C0A28  | 33CA                    | xor ecx,edx                                     |
6F5C0A2A  | 898F 5C020000           | mov dword ptr ds:[edi+25C],ecx                  |
6F5C0A30  | 0FB646 08               | movzx eax,byte ptr ds:[esi+8]                   |
6F5C0A34  | 894424 30               | mov dword ptr ss:[esp+30],eax                   |
6F5C0A38  | DB4424 30               | fild dword ptr ss:[esp+30]                      |
6F5C0A3C  | DC0D E064936F           | fmul qword ptr ds:[6F9364E0]                    |
6F5C0A42  | D95C24 18               | fstp dword ptr ss:[esp+18]                      |
6F5C0A46  | D94424 18               | fld dword ptr ss:[esp+18]                       |
6F5C0A4A  | D905 80E4AA6F           | fld dword ptr ds:[6FAAE480]                     |
6F5C0A50  | D8D9                    | fcomp st(1)                                     |
6F5C0A52  | DFE0                    | fnstsw ax                                       |
6F5C0A54  | F6C4 41                 | test ah,41                                      |
6F5C0A57  | 75 09                   | jne game.6F5C0A62                               |
6F5C0A59  | DDD8                    | fstp st(0)                                      |
6F5C0A5B  | B8 80E4AA6F             | mov eax,game.6FAAE480                           |
6F5C0A60  | EB 18                   | jmp game.6F5C0A7A                               |
6F5C0A62  | D905 74E5AA6F           | fld dword ptr ds:[6FAAE574]                     |
6F5C0A68  | DED9                    | fcompp                                          |
6F5C0A6A  | DFE0                    | fnstsw ax                                       |
6F5C0A6C  | F6C4 05                 | test ah,5                                       |
6F5C0A6F  | B8 74E5AA6F             | mov eax,game.6FAAE574                           |
6F5C0A74  | 7B 04                   | jnp game.6F5C0A7A                               |
6F5C0A76  | 8D4424 18               | lea eax,dword ptr ss:[esp+18]                   |
6F5C0A7A  | 8B08                    | mov ecx,dword ptr ds:[eax]                      |
6F5C0A7C  | 8B97 98020000           | mov edx,dword ptr ds:[edi+298]                  |
6F5C0A82  | 8B12                    | mov edx,dword ptr ds:[edx]                      |
6F5C0A84  | 894C24 18               | mov dword ptr ss:[esp+18],ecx                   |
6F5C0A88  | 8D8F 98020000           | lea ecx,dword ptr ds:[edi+298]                  |
6F5C0A8E  | 6A 01                   | push 1                                          |
6F5C0A90  | 8D4424 1C               | lea eax,dword ptr ss:[esp+1C]                   |
6F5C0A94  | 50                      | push eax                                        |
6F5C0A95  | FFD2                    | call edx                                        |
6F5C0A97  | 8A46 03                 | mov al,byte ptr ds:[esi+3]                      |
6F5C0A9A  | 84C0                    | test al,al                                      |
6F5C0A9C  | 0FB66E 04               | movzx ebp,byte ptr ds:[esi+4]                   |
6F5C0AA0  | 75 6F                   | jne game.6F5C0B11                               |
6F5C0AA2  | 8A0E                    | mov cl,byte ptr ds:[esi]                        |
6F5C0AA4  | 33D2                    | xor edx,edx                                     |
6F5C0AA6  | E8 45E9F8FF             | call <game.sub_6F54F3F0>                        |
6F5C0AAB  | 33D2                    | xor edx,edx                                     |
6F5C0AAD  | 8AC8                    | mov cl,al                                       |
6F5C0AAF  | E8 4CE3F7FF             | call <game.sub_6F53EE00>                        |
6F5C0AB4  | 85C0                    | test eax,eax                                    |
6F5C0AB6  | 75 05                   | jne game.6F5C0ABD                               |
6F5C0AB8  | B8 9C52876F             | mov eax,game.6F87529C                           |
6F5C0ABD  | 50                      | push eax                                        |
6F5C0ABE  | 8D4F 24                 | lea ecx,dword ptr ds:[edi+24]                   |
6F5C0AC1  | E8 2A52F0FF             | call <game.sub_6F4C5CF0>                        |
6F5C0AC6  | 8A4C24 13               | mov cl,byte ptr ss:[esp+13]                     |
6F5C0ACA  | 6A 00                   | push 0                                          |
6F5C0ACC  | 8AD3                    | mov dl,bl                                       |
6F5C0ACE  | E8 CD8BF8FF             | call <game.sub_6F5496A0>                        |
6F5C0AD3  | 33C9                    | xor ecx,ecx                                     |
6F5C0AD5  | E8 F6E2F7FF             | call <game.sub_6F53EDD0>                        |
6F5C0ADA  | 0FB6C0                  | movzx eax,al                                    |
6F5C0ADD  | 3BD8                    | cmp ebx,eax                                     |
6F5C0ADF  | 75 08                   | jne game.6F5C0AE9                               |
6F5C0AE1  | 8B4C24 28               | mov ecx,dword ptr ss:[esp+28]                   |
6F5C0AE5  | 66:8959 28              | mov word ptr ds:[ecx+28],bx                     |
6F5C0AE9  | 83BC24 C0000000 05      | cmp dword ptr ss:[esp+C0],5                     |
6F5C0AF1  | 0F85 AA000000           | jne game.6F5C0BA1                               |
6F5C0AF7  | 6A 00                   | push 0                                          |
6F5C0AF9  | BA 01000000             | mov edx,1                                       |
6F5C0AFE  | 8ACB                    | mov cl,bl                                       |
6F5C0B00  | E8 3BE4F7FF             | call <game.sub_6F53EF40>                        |
6F5C0B05  | 33C9                    | xor ecx,ecx                                     |
6F5C0B07  | E8 E4DEF7FF             | call <game.sub_6F53E9F0>                        |
6F5C0B0C  | E9 90000000             | jmp game.6F5C0BA1                               |
6F5C0B11  | 3C 01                   | cmp al,1                                        |
6F5C0B13  | 0F85 88000000           | jne game.6F5C0BA1                               |
6F5C0B19  | 8B87 6C020000           | mov eax,dword ptr ds:[edi+26C]                  |
6F5C0B1F  | 83E8 00                 | sub eax,0                                       |
6F5C0B22  | B9 3C9A966F             | mov ecx,game.6F969A3C                           | <--- "COMPUTER"
6F5C0B27  | 74 44                   | je game.6F5C0B6D                                |
6F5C0B29  | 83E8 01                 | sub eax,1                                       |
6F5C0B2C  | 74 22                   | je game.6F5C0B50                                |
6F5C0B2E  | 83E8 01                 | sub eax,1                                       |
6F5C0B31  | 75 3F                   | jne game.6F5C0B72                               |
6F5C0B33  | B9 D8A4956F             | mov ecx,game.6F95A4D8                           | <--- "COMPUTER_INSANE"
6F5C0B38  | 6A 20                   | push 20                                         |
6F5C0B3A  | 8D9424 9C000000         | lea edx,dword ptr ss:[esp+9C]                   |
6F5C0B41  | E8 0A8B0000             | call <game.sub_6F5C9650>                        |
6F5C0B46  | 8D9424 98000000         | lea edx,dword ptr ss:[esp+98]                   |
6F5C0B4D  | 52                      | push edx                                        |
6F5C0B4E  | EB 49                   | jmp game.6F5C0B99                               |
6F5C0B50  | B9 E8A4956F             | mov ecx,game.6F95A4E8                           | <--- "COMPUTER_NORMAL"
6F5C0B55  | 6A 20                   | push 20                                         |
6F5C0B57  | 8D9424 9C000000         | lea edx,dword ptr ss:[esp+9C]                   |
6F5C0B5E  | E8 ED8A0000             | call <game.sub_6F5C9650>                        |
6F5C0B63  | 8D9424 98000000         | lea edx,dword ptr ss:[esp+98]                   |
6F5C0B6A  | 52                      | push edx                                        |
6F5C0B6B  | EB 2C                   | jmp game.6F5C0B99                               |
6F5C0B6D  | B9 F8A4956F             | mov ecx,game.6F95A4F8                           |
6F5C0B72  | 6A 20                   | push 20                                         |
6F5C0B74  | 8D9424 9C000000         | lea edx,dword ptr ss:[esp+9C]                   |
6F5C0B7B  | E8 D08A0000             | call <game.sub_6F5C9650>                        |
6F5C0B80  | 8D9424 98000000         | lea edx,dword ptr ss:[esp+98]                   |
6F5C0B87  | 52                      | push edx                                        |
6F5C0B88  | EB 0F                   | jmp game.6F5C0B99                               |
6F5C0B8A  | C787 70020000 00000000  | mov dword ptr ds:[edi+270],0                    |
6F5C0B94  | 68 9C52876F             | push game.6F87529C                              |
6F5C0B99  | 8D4F 24                 | lea ecx,dword ptr ds:[edi+24]                   |
6F5C0B9C  | E8 4F51F0FF             | call <game.sub_6F4C5CF0>                        |
6F5C0BA1  | 8B7424 28               | mov esi,dword ptr ss:[esp+28]                   |
6F5C0BA5  | 53                      | push ebx                                        |
6F5C0BA6  | 8BCE                    | mov ecx,esi                                     |
6F5C0BA8  | E8 B30ADEFF             | call <game.sub_6F3A1660>                        |
6F5C0BAD  | 83F8 FF                 | cmp eax,FFFFFFFF                                |
6F5C0BB0  | 74 09                   | je game.6F5C0BBB                                |
6F5C0BB2  | 53                      | push ebx                                        |
6F5C0BB3  | 50                      | push eax                                        |
6F5C0BB4  | 8BCE                    | mov ecx,esi                                     |
6F5C0BB6  | E8 4577DEFF             | call <game.sub_6F3A8300>                        |
6F5C0BBB  | 83FD 0C                 | cmp ebp,C                                       |
6F5C0BBE  | 75 11                   | jne game.6F5C0BD1                               |
6F5C0BC0  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]                   |
6F5C0BC4  | 897C84 34               | mov dword ptr ss:[esp+eax*4+34],edi             |
6F5C0BC8  | 83C0 01                 | add eax,1                                       |
6F5C0BCB  | 894424 14               | mov dword ptr ss:[esp+14],eax                   |
6F5C0BCF  | EB 0E                   | jmp game.6F5C0BDF                               |
6F5C0BD1  | 83FD FF                 | cmp ebp,FFFFFFFF                                |
6F5C0BD4  | 74 09                   | je game.6F5C0BDF                                |
6F5C0BD6  | 53                      | push ebx                                        |
6F5C0BD7  | 55                      | push ebp                                        |
6F5C0BD8  | 8BCE                    | mov ecx,esi                                     |
6F5C0BDA  | E8 D176DEFF             | call <game.sub_6F3A82B0>                        |
6F5C0BDF  | 8B4424 24               | mov eax,dword ptr ss:[esp+24]                   |
6F5C0BE3  | 834424 1C 09            | add dword ptr ss:[esp+1C],9                     |
6F5C0BE8  | 83C0 01                 | add eax,1                                       |
6F5C0BEB  | 3B4424 2C               | cmp eax,dword ptr ss:[esp+2C]                   |
6F5C0BEF  | 894424 24               | mov dword ptr ss:[esp+24],eax                   |
6F5C0BF3  | 0F82 A8FDFFFF           | jb game.6F5C09A1                                |
6F5C0BF9  | 8B7C24 14               | mov edi,dword ptr ss:[esp+14]                   |
6F5C0BFD  | 33ED                    | xor ebp,ebp                                     |
6F5C0BFF  | 3BFD                    | cmp edi,ebp                                     |
6F5C0C01  | 74 46                   | je game.6F5C0C49                                |
6F5C0C03  | BB 00004000             | mov ebx,war3.400000                             |
6F5C0C08  | EB 06                   | jmp game.6F5C0C10                               |
6F5C0C0A  | 8D9B 00000000           | lea ebx,dword ptr ds:[ebx]                      |
6F5C0C10  | 8B74BC 30               | mov esi,dword ptr ss:[esp+edi*4+30]             |
6F5C0C14  | 83EF 01                 | sub edi,1                                       |
6F5C0C17  | 6A 01                   | push 1                                          |
6F5C0C19  | 8BCE                    | mov ecx,esi                                     |
6F5C0C1B  | E8 E0E4E5FF             | call <game.sub_6F41F100>                        |
6F5C0C20  | 8B8424 C4000000         | mov eax,dword ptr ss:[esp+C4]                   |
6F5C0C27  | 8558 34                 | test dword ptr ds:[eax+34],ebx                  |
6F5C0C2A  | 74 09                   | je game.6F5C0C35                                |
6F5C0C2C  | 6A 01                   | push 1                                          |
6F5C0C2E  | 8BCE                    | mov ecx,esi                                     |
6F5C0C30  | E8 ABEAE4FF             | call <game.sub_6F40F6E0>                        |
6F5C0C35  | 3BFD                    | cmp edi,ebp                                     |
6F5C0C37  | C786 70020000 02000000  | mov dword ptr ds:[esi+270],2                    |
6F5C0C41  | 89AE 1C030000           | mov dword ptr ds:[esi+31C],ebp                  |
6F5C0C47  | 75 C7                   | jne game.6F5C0C10                               |
6F5C0C49  | B8 01000000             | mov eax,1                                       |
6F5C0C4E  | 8B8C24 B8000000         | mov ecx,dword ptr ss:[esp+B8]                   |
6F5C0C55  | 5F                      | pop edi                                         |
6F5C0C56  | 5E                      | pop esi                                         |
6F5C0C57  | 5D                      | pop ebp                                         |
6F5C0C58  | 5B                      | pop ebx                                         |
6F5C0C59  | 33CC                    | xor ecx,esp                                     |
6F5C0C5B  | E8 F9032200             | call game.6F7E1059                              |
6F5C0C60  | 81C4 AC000000           | add esp,AC                                      |
6F5C0C66  | C2 0800                 | ret 8                                           |
```
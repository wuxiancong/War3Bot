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
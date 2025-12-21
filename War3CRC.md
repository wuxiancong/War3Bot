# Warcraft III (1.26) 地图校验和计算机制逆向分析报告

**版本**: 1.26 (Build 6059)  
**目标**: 解析游戏在建立主机（Host Game）及本地加载（Local Check）时的地图一致性校验逻辑。

---

## 1. 核心算法流程概述

游戏在创建房间或玩家加入时，为了防止作弊（如修改地图脚本去迷雾、修改数据），会计算一个环境指纹（Checksum）。该计算涉及三个核心文件：

1.  **`common.j`**: JASS 语言的原生函数库（Native Functions）。
2.  **`blizzard.j`**: 暴雪提供的辅助函数库（BJ Functions）。
3.  **`war3map.j`**: 当前地图的脚本文件。

**关键结论**：Warcraft III 1.26 **并没有**使用标准的 CRC32 算法，而是使用了一套**“自定义 ROL-XOR 哈希算法”**，并在最终混合阶段引入了一个**魔法数（Magic Number）** `0x03F1379E` 作为盐值。

---

## 2. 数学原理

假设三个文件经过**暴雪自定义哈希函数**（见下文 4.2 节）计算后的值分别为 $H_{com}$、$H_{bliz}$、$H_{map}$。

最终的 **Map Checksum** 计算公式如下：

### 第一阶段：环境混合 (Environment Mixing)
计算基础环境的哈希值，引入魔法数防止篡改。

$$Val_{temp} = \text{ROL}\left( \text{ROL}\left( H_{bliz} \oplus H_{com}, 3 \right) \oplus \text{0x03F1379E}, 3 \right)$$

### 第二阶段：地图混合 (Map Mixing)
将地图脚本哈希混入环境哈希中。

$$\text{Checksum} = \text{ROL}\left( H_{map} \oplus Val_{temp}, 3 \right)$$

> **符号说明**：
> *   $\oplus$ : **异或运算 (XOR)**
> *   $\text{ROL}(x, n)$ : **循环左移 (Rotate Left)**，将 $x$ 的二进制位向左循环移动 $n$ 位。
> *   `0x03F1379E` : 1.26 版本的特定特征码（盐）。

---

## 3. 逆向工程分析 (Top-Down)

**调用调用链**：
`HostGame` / `LocalCheck` $\rightarrow$ `VersionCheck` $\rightarrow$ `Wrapper` $\rightarrow$ `CoreCalc` $\rightarrow$ `HashAlgo`

### 3.1 入口层：建房与加载
游戏在两个主要位置触发校验计算。

<details open>
<summary><strong>[展开汇编] A. 建房初始化 (HostGame Entry) - <code>Game.dll + 5C25C0</code></strong></summary>

功能：建房流程的高层入口。负责收集房间名、主机名、地图路径，并调用下层计算 CRC。
```assembly
6F5C25C0 | B8 5C390000              | mov eax,395C                                 |
6F5C25C5 | E8 C6EB2100              | call game.6F7E1190                           |
6F5C25CA | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]              |
6F5C25CF | 33C4                     | xor eax,esp                                  |
6F5C25D1 | 898424 58390000          | mov dword ptr ss:[esp+3958],eax              |
6F5C25D8 | 8B8424 60390000          | mov eax,dword ptr ss:[esp+3960]              |
6F5C25DF | 894424 1C                | mov dword ptr ss:[esp+1C],eax                |
6F5C25E3 | 8B8424 7C390000          | mov eax,dword ptr ss:[esp+397C]              |
6F5C25EA | A8 10                    | test al,10                                   |
6F5C25EC | 56                       | push esi                                     |
6F5C25ED | 8BB424 6C390000          | mov esi,dword ptr ss:[esp+396C]              |
6F5C25F4 | 894C24 10                | mov dword ptr ss:[esp+10],ecx                |
6F5C25F8 | 8B8C24 68390000          | mov ecx,dword ptr ss:[esp+3968]              |
6F5C25FF | 895424 14                | mov dword ptr ss:[esp+14],edx                |
6F5C2603 | 8B9424 84390000          | mov edx,dword ptr ss:[esp+3984]              |
6F5C260A | 57                       | push edi                                     |
6F5C260B | 894C24 1C                | mov dword ptr ss:[esp+1C],ecx                |
6F5C260F | 895424 10                | mov dword ptr ss:[esp+10],edx                |
6F5C2613 | 75 07                    | jne game.6F5C261C                            |
6F5C2615 | A9 00004000              | test eax,war3.400000                         |
6F5C261A | 74 0B                    | je game.6F5C2627                             |
6F5C261C | C78424 74390000 0C000000 | mov dword ptr ss:[esp+3974],C                |
6F5C2627 | 8BBC24 7C390000          | mov edi,dword ptr ss:[esp+397C]              |
6F5C262E | 53                       | push ebx                                     |
6F5C262F | 33DB                     | xor ebx,ebx                                  |
6F5C2631 | 55                       | push ebp                                     |
6F5C2632 | 33ED                     | xor ebp,ebp                                  |
6F5C2634 | F7C7 00020000            | test edi,200                                 |
6F5C263A | 895C24 10                | mov dword ptr ss:[esp+10],ebx                |
6F5C263E | 895C24 14                | mov dword ptr ss:[esp+14],ebx                |
6F5C2642 | 8BCE                     | mov ecx,esi                                  |
6F5C2644 | 75 6B                    | jne game.6F5C26B1                            |
6F5C2646 | 53                       | push ebx                                     |
6F5C2647 | 6A 01                    | push 1                                       |
6F5C2649 | 8D5424 18                | lea edx,dword ptr ss:[esp+18]                |
6F5C264D | E8 1EC7DDFF              | call game.6F39ED70                           | <--- [关键调用] 进入版本检查与CRC计算
6F5C2652 | 53                       | push ebx                                     |
6F5C2653 | 6A 01                    | push 1                                       |
6F5C2655 | 8D9424 5C390000          | lea edx,dword ptr ss:[esp+395C]              |
6F5C265C | 8BCE                     | mov ecx,esi                                  |
6F5C265E | E8 ADC7DDFF              | call game.6F39EE10                           | <--- 执行 Config 配置
6F5C2663 | 8D8C24 D4000000          | lea ecx,dword ptr ss:[esp+D4]                |
6F5C266A | E8 B1F7A4FF              | call game.6F011E20                           |
6F5C266F | 8D4424 28                | lea eax,dword ptr ss:[esp+28]                |
6F5C2673 | 50                       | push eax                                     |
6F5C2674 | 53                       | push ebx                                     |
6F5C2675 | 53                       | push ebx                                     |
6F5C2676 | 8D9424 E0000000          | lea edx,dword ptr ss:[esp+E0]                |
6F5C267D | 8BCE                     | mov ecx,esi                                  |
6F5C267F | E8 1CB3A5FF              | call game.6F01D9A0                           | <--- 获取/广播 GameInfo
6F5C2684 | 395C24 28                | cmp dword ptr ss:[esp+28],ebx                |
6F5C2688 | 8B8C24 E0040000          | mov ecx,dword ptr ss:[esp+4E0]               |
6F5C268F | 8BAC24 DC040000          | mov ebp,dword ptr ss:[esp+4DC]               |
6F5C2696 | 894C24 14                | mov dword ptr ss:[esp+14],ecx                |
6F5C269A | 74 05                    | je game.6F5C26A1                             |
6F5C269C | 83CF 08                  | or edi,8                                     |
6F5C269F | EB 28                    | jmp game.6F5C26C9                            |
6F5C26A1 | 8BCE                     | mov ecx,esi                                  |
6F5C26A3 | E8 78ECA3FF              | call game.6F001320                           |
6F5C26A8 | 85C0                     | test eax,eax                                 |
6F5C26AA | 74 1D                    | je game.6F5C26C9                             |
6F5C26AC | 83CF 08                  | or edi,8                                     |
6F5C26AF | EB 18                    | jmp game.6F5C26C9                            |
6F5C26B1 | E8 2AFEFFFF              | call game.6F5C24E0                           |
6F5C26B6 | 85C0                     | test eax,eax                                 |
6F5C26B8 | 74 0F                    | je game.6F5C26C9                             |
6F5C26BA | E8 F1AAFFFF              | call game.6F5BD1B0                           |
6F5C26BF | 8B90 30040000            | mov edx,dword ptr ds:[eax+430]               |
6F5C26C5 | 895424 10                | mov dword ptr ss:[esp+10],edx                |
6F5C26C9 | 399C24 9C390000          | cmp dword ptr ss:[esp+399C],ebx              |
6F5C26D0 | 74 06                    | je game.6F5C26D8                             |
6F5C26D2 | 81CF 00000080            | or edi,80000000                              |
6F5C26D8 | 8B4424 2C                | mov eax,dword ptr ss:[esp+2C]                |
6F5C26DC | 6A 20                    | push 20                                      |
6F5C26DE | 50                       | push eax                                     |
6F5C26DF | 8D4C24 38                | lea ecx,dword ptr ss:[esp+38]                |
6F5C26E3 | 51                       | push ecx                                     |
6F5C26E4 | 885C24 3C                | mov byte ptr ss:[esp+3C],bl                  |
6F5C26E8 | 885C24 5C                | mov byte ptr ss:[esp+5C],bl                  |
6F5C26EC | 889C24 90000000          | mov byte ptr ss:[esp+90],bl                  |
6F5C26F3 | 889C24 C6000000          | mov byte ptr ss:[esp+C6],bl                  |
6F5C26FA | E8 C58E1200              | call <JMP.&Ordinal#501>                      |
6F5C26FF | 8B5424 24                | mov edx,dword ptr ss:[esp+24]                |
6F5C2703 | 6A 10                    | push 10                                      |
6F5C2705 | 52                       | push edx                                     |
6F5C2706 | 8D4424 58                | lea eax,dword ptr ss:[esp+58]                |
6F5C270A | 50                       | push eax                                     |
6F5C270B | E8 B48E1200              | call <JMP.&Ordinal#501>                      |
6F5C2710 | 8A8C24 88390000          | mov cl,byte ptr ss:[esp+3988]                |
6F5C2717 | 8B9424 8C390000          | mov edx,dword ptr ss:[esp+398C]              |
6F5C271E | 66:8B4424 14             | mov ax,word ptr ss:[esp+14]                  |
6F5C2723 | 884C24 61                | mov byte ptr ss:[esp+61],cl                  |
6F5C2727 | 8B8C24 54390000          | mov ecx,dword ptr ss:[esp+3954]              |
6F5C272E | 894C24 70                | mov dword ptr ss:[esp+70],ecx                |
6F5C2732 | 8B8C24 60390000          | mov ecx,dword ptr ss:[esp+3960]              |
6F5C2739 | 895424 64                | mov dword ptr ss:[esp+64],edx                |
6F5C273D | 8B9424 58390000          | mov edx,dword ptr ss:[esp+3958]              |
6F5C2744 | 66:894424 6A             | mov word ptr ss:[esp+6A],ax                  |
6F5C2749 | 8B8424 5C390000          | mov eax,dword ptr ss:[esp+395C]              |
6F5C2750 | 6A 36                    | push 36                                      |
6F5C2752 | 898C24 80000000          | mov dword ptr ss:[esp+80],ecx                |
6F5C2759 | 895424 78                | mov dword ptr ss:[esp+78],edx                |
6F5C275D | 8B9424 68390000          | mov edx,dword ptr ss:[esp+3968]              |
6F5C2764 | 894424 7C                | mov dword ptr ss:[esp+7C],eax                |
6F5C2768 | 8B4424 14                | mov eax,dword ptr ss:[esp+14]                |
6F5C276C | 56                       | push esi                                     |
6F5C276D | 8D8C24 8C000000          | lea ecx,dword ptr ss:[esp+8C]                |
6F5C2774 | 51                       | push ecx                                     |
6F5C2775 | 66:896C24 74             | mov word ptr ss:[esp+74],bp                  |
6F5C277A | 899424 8C000000          | mov dword ptr ss:[esp+8C],edx                |
6F5C2781 | 894424 78                | mov dword ptr ss:[esp+78],eax                |
6F5C2785 | E8 3A8E1200              | call <JMP.&Ordinal#501>                      |
6F5C278A | 8B7424 18                | mov esi,dword ptr ss:[esp+18]                |
6F5C278E | 6A 10                    | push 10                                      |
6F5C2790 | 56                       | push esi                                     |
6F5C2791 | 8D9424 C2000000          | lea edx,dword ptr ss:[esp+C2]                |
6F5C2798 | 52                       | push edx                                     |
6F5C2799 | E8 268E1200              | call <JMP.&Ordinal#501>                      |
6F5C279E | 399C24 94390000          | cmp dword ptr ss:[esp+3994],ebx              |
6F5C27A5 | 8B8C24 98390000          | mov ecx,dword ptr ss:[esp+3998]              |
6F5C27AC | 8B9424 7C390000          | mov edx,dword ptr ss:[esp+397C]              |
6F5C27B3 | 0F95C0                   | setne al                                     |
6F5C27B6 | F7D9                     | neg ecx                                      |
6F5C27B8 | 1AC9                     | sbb cl,cl                                    |
6F5C27BA | 89BC24 D0000000          | mov dword ptr ss:[esp+D0],edi                |
6F5C27C1 | 8B7C24 1C                | mov edi,dword ptr ss:[esp+1C]                |
6F5C27C5 | 899424 CC000000          | mov dword ptr ss:[esp+CC],edx                |
6F5C27CC | 8D5424 30                | lea edx,dword ptr ss:[esp+30]                |
6F5C27D0 | 83E1 02                  | and ecx,2                                    |
6F5C27D3 | 0AC1                     | or al,cl                                     |
6F5C27D5 | 8BCF                     | mov ecx,edi                                  |
6F5C27D7 | 884424 60                | mov byte ptr ss:[esp+60],al                  |
6F5C27DB | E8 00AAFFFF              | call game.6F5BD1E0                           |
6F5C27E0 | 85C0                     | test eax,eax                                 |
6F5C27E2 | 5D                       | pop ebp                                      |
6F5C27E3 | 5B                       | pop ebx                                      |
6F5C27E4 | 74 3B                    | je game.6F5C2821                             |
6F5C27E6 | F787 B4000000 00000080   | test dword ptr ds:[edi+B4],80000000          |
6F5C27F0 | 74 0F                    | je game.6F5C2801                             |
6F5C27F2 | BA 03000000              | mov edx,3                                    |
6F5C27F7 | B9 F09A966F              | mov ecx,game.6F969AF0                        |
6F5C27FC | E8 5F97F0FF              | call game.6F4CBF60                           |
6F5C2801 | 8B4424 18                | mov eax,dword ptr ss:[esp+18]                |
6F5C2805 | 8A8C24 98390000          | mov cl,byte ptr ss:[esp+3998]                |
6F5C280C | 6A 10                    | push 10                                      |
6F5C280E | 56                       | push esi                                     |
6F5C280F | 50                       | push eax                                     |
6F5C2810 | C640 10 01               | mov byte ptr ds:[eax+10],1                   |
6F5C2814 | 8848 11                  | mov byte ptr ds:[eax+11],cl                  |
6F5C2817 | E8 A88D1200              | call <JMP.&Ordinal#501>                      |
6F5C281C | B8 01000000              | mov eax,1                                    |
6F5C2821 | 8B8C24 60390000          | mov ecx,dword ptr ss:[esp+3960]              |
6F5C2828 | 5F                       | pop edi                                      |
6F5C2829 | 5E                       | pop esi                                      |
6F5C282A | 33CC                     | xor ecx,esp                                  |
6F5C282C | E8 28E82100              | call game.6F7E1059                           |
6F5C2831 | 81C4 5C390000            | add esp,395C                                 |
6F5C2837 | C2 3400                  | ret 34                                       |
```
</details>

<details open>
<summary><strong>[展开汇编] B. 本地加载检查 (Local Map Check) - <code>Game.dll + 3AEF80</code></strong></summary>

功能：本地选择地图或加入房间时的校验。确认 MPQ 完整性并计算 CRC 用于比对。
```assembly
6F3AEF80 | B8 A4390000              | mov eax,39A4                                 |
6F3AEF85 | E8 06224300              | call game.6F7E1190                           |
6F3AEF8A | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]              |
6F3AEF8F | 33C4                     | xor eax,esp                                  |
6F3AEF91 | 898424 A0390000          | mov dword ptr ss:[esp+39A0],eax              |
6F3AEF98 | 53                       | push ebx                                     |
6F3AEF99 | 8B9C24 B8390000          | mov ebx,dword ptr ss:[esp+39B8]              |
6F3AEFA0 | 55                       | push ebp                                     |
6F3AEFA1 | 56                       | push esi                                     |
6F3AEFA2 | 57                       | push edi                                     |
6F3AEFA3 | 8BBC24 B8390000          | mov edi,dword ptr ss:[esp+39B8]              |
6F3AEFAA | 8BF1                     | mov esi,ecx                                  |
6F3AEFAC | E8 6FFBFFFF              | call game.6F3AEB20                           |
6F3AEFB1 | 8B46 30                  | mov eax,dword ptr ds:[esi+30]                |
6F3AEFB4 | 8D48 24                  | lea ecx,dword ptr ds:[eax+24]                |
6F3AEFB7 | E8 74561100              | call game.6F4C4630                           |
6F3AEFBC | 837E 04 00               | cmp dword ptr ds:[esi+4],0                   |
6F3AEFC0 | 74 18                    | je game.6F3AEFDA                             |
6F3AEFC2 | 85C0                     | test eax,eax                                 |
6F3AEFC4 | 74 14                    | je game.6F3AEFDA                             |
6F3AEFC6 | 68 FFFFFF7F              | push 7FFFFFFF                                |
6F3AEFCB | 50                       | push eax                                     |
6F3AEFCC | 57                       | push edi                                     |
6F3AEFCD | E8 B8C63300              | call <JMP.&Ordinal#508>                      |
6F3AEFD2 | 85C0                     | test eax,eax                                 |
6F3AEFD4 | 0F84 36010000            | je game.6F3AF110                             |
6F3AEFDA | 68 04010000              | push 104                                     |
6F3AEFDF | 8D9424 B0380000          | lea edx,dword ptr ss:[esp+38B0]              |
6F3AEFE6 | 8BCF                     | mov ecx,edi                                  |
6F3AEFE8 | C74424 14 00000000       | mov dword ptr ss:[esp+14],0                  |
6F3AEFF0 | E8 7B2DC6FF              | call game.6F011D70                           | <--- 获取游戏目录
6F3AEFF5 | 85C0                     | test eax,eax                                 |
6F3AEFF7 | 0F84 21010000            | je game.6F3AF11E                             |
6F3AEFFD | 6A 00                    | push 0                                       |
6F3AEFFF | 6A 01                    | push 1                                       |
6F3AF001 | 8D5424 18                | lea edx,dword ptr ss:[esp+18]                |
6F3AF005 | 8BCF                     | mov ecx,edi                                  |
6F3AF007 | E8 64FDFEFF              | call game.6F39ED70                           | <--- 进入版本检查与CRC计算
6F3AF00C | 6A 00                    | push 0                                       |
6F3AF00E | 6A 01                    | push 1                                       |
6F3AF010 | 8D9424 A0380000          | lea edx,dword ptr ss:[esp+38A0]              |
6F3AF017 | 8BCF                     | mov ecx,edi                                  |
6F3AF019 | E8 F2FDFEFF              | call game.6F39EE10                           |
6F3AF01E | 68 CC50876F              | push game.6F8750CC                           |
6F3AF023 | 8BCE                     | mov ecx,esi                                  |
6F3AF025 | E8 7606FFFF              | call game.6F39F6A0                           |
6F3AF02A | 8B9424 C0390000          | mov edx,dword ptr ss:[esp+39C0]              |
6F3AF031 | 8B4C24 10                | mov ecx,dword ptr ss:[esp+10]                |
6F3AF035 | E8 76FEFEFF              | call game.6F39EEB0                           |
6F3AF03A | 85C0                     | test eax,eax                                 |
6F3AF03C | 0F84 D5000000            | je game.6F3AF117                             |
6F3AF042 | 8BD3                     | mov edx,ebx                                  |
6F3AF044 | 8D8C24 98380000          | lea ecx,dword ptr ss:[esp+3898]              |
6F3AF04B | E8 80FEFEFF              | call game.6F39EED0                           |
6F3AF050 | 85C0                     | test eax,eax                                 |
6F3AF052 | 0F84 BF000000            | je game.6F3AF117                             |
6F3AF058 | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                |
6F3AF05C | E8 BF2DC6FF              | call game.6F011E20                           |
6F3AF061 | 8D4424 14                | lea eax,dword ptr ss:[esp+14]                |
6F3AF065 | 50                       | push eax                                     |
6F3AF066 | 6A 00                    | push 0                                       |
6F3AF068 | 6A 00                    | push 0                                       |
6F3AF06A | 8D5424 24                | lea edx,dword ptr ss:[esp+24]                |
6F3AF06E | 8BCF                     | mov ecx,edi                                  |
6F3AF070 | E8 2BE9C6FF              | call game.6F01D9A0                           |
6F3AF075 | BB 01000000              | mov ebx,1                                    |
6F3AF07A | 395C24 14                | cmp dword ptr ss:[esp+14],ebx                |
6F3AF07E | 8BE8                     | mov ebp,eax                                  |
6F3AF080 | 74 1D                    | je game.6F3AF09F                             |
6F3AF082 | 8BCF                     | mov ecx,edi                                  |
6F3AF084 | E8 47F2C5FF              | call game.6F00E2D0                           |
6F3AF089 | 85C0                     | test eax,eax                                 |
6F3AF08B | 75 10                    | jne game.6F3AF09D                            |
6F3AF08D | 8D8C24 98380000          | lea ecx,dword ptr ss:[esp+3898]              |
6F3AF094 | E8 17270000              | call game.6F3B17B0                           |
6F3AF099 | 85C0                     | test eax,eax                                 |
6F3AF09B | 74 02                    | je game.6F3AF09F                             |
6F3AF09D | 33DB                     | xor ebx,ebx                                  |
6F3AF09F | 8BD3                     | mov edx,ebx                                  |
6F3AF0A1 | 8D8C24 AC380000          | lea ecx,dword ptr ss:[esp+38AC]              |
6F3AF0A8 | E8 A3630000              | call game.6F3B5450                           |
6F3AF0AD | 85C0                     | test eax,eax                                 |
6F3AF0AF | 8946 04                  | mov dword ptr ds:[esi+4],eax                 |
6F3AF0B2 | 74 63                    | je game.6F3AF117                             |
6F3AF0B4 | 33C0                     | xor eax,eax                                  |
6F3AF0B6 | 85ED                     | test ebp,ebp                                 |
6F3AF0B8 | 74 18                    | je game.6F3AF0D2                             |
6F3AF0BA | 8B8C24 58040000          | mov ecx,dword ptr ss:[esp+458]               |
6F3AF0C1 | 8BC1                     | mov eax,ecx                                  |
6F3AF0C3 | 83E0 20                  | and eax,20                                   |
6F3AF0C6 | 03C0                     | add eax,eax                                  |
6F3AF0C8 | 03C0                     | add eax,eax                                  |
6F3AF0CA | 83E1 40                  | and ecx,40                                   |
6F3AF0CD | 0BC1                     | or eax,ecx                                   |
6F3AF0CF | C1E0 0E                  | shl eax,E                                    |
6F3AF0D2 | 8B4E 30                  | mov ecx,dword ptr ds:[esi+30]                |
6F3AF0D5 | 0941 38                  | or dword ptr ds:[ecx+38],eax                 |
6F3AF0D8 | 8B4E 30                  | mov ecx,dword ptr ds:[esi+30]                |
6F3AF0DB | 8B9424 BC390000          | mov edx,dword ptr ss:[esp+39BC]              |
6F3AF0E2 | 8951 34                  | mov dword ptr ds:[ecx+34],edx                |
6F3AF0E5 | 8B4E 04                  | mov ecx,dword ptr ds:[esi+4]                 |
6F3AF0E8 | E8 83630000              | call game.6F3B5470                           |
6F3AF0ED | 8B4E 30                  | mov ecx,dword ptr ds:[esi+30]                |
6F3AF0F0 | 57                       | push edi                                     |
6F3AF0F1 | 83C1 24                  | add ecx,24                                   |
6F3AF0F4 | 8946 08                  | mov dword ptr ds:[esi+8],eax                 |
6F3AF0F7 | E8 F46B1100              | call game.6F4C5CF0                           |
6F3AF0FC | 8B46 30                  | mov eax,dword ptr ds:[esi+30]                |
6F3AF0FF | 8B4C24 10                | mov ecx,dword ptr ss:[esp+10]                |
6F3AF103 | 8948 1C                  | mov dword ptr ds:[eax+1C],ecx                |
6F3AF106 | 8B56 30                  | mov edx,dword ptr ds:[esi+30]                |
6F3AF109 | C742 20 01000000         | mov dword ptr ds:[edx+20],1                  |
6F3AF110 | B8 01000000              | mov eax,1                                    |
6F3AF115 | EB 09                    | jmp game.6F3AF120                            |
6F3AF117 | 8BCE                     | mov ecx,esi                                  |
6F3AF119 | E8 02FAFFFF              | call game.6F3AEB20                           |
6F3AF11E | 33C0                     | xor eax,eax                                  |
6F3AF120 | 8B8C24 B0390000          | mov ecx,dword ptr ss:[esp+39B0]              |
6F3AF127 | 5F                       | pop edi                                      |
6F3AF128 | 5E                       | pop esi                                      |
6F3AF129 | 5D                       | pop ebp                                      |
6F3AF12A | 5B                       | pop ebx                                      |
6F3AF12B | 33CC                     | xor ecx,esp                                  |
6F3AF12D | E8 271F4300              | call game.6F7E1059                           |
6F3AF132 | 81C4 A4390000            | add esp,39A4                                 |
6F3AF138 | C2 1000                  | ret 10                                       |
```
</details>

### 3.2 中间层：版本分发
**函数地址**：`Game.dll + 39ED70`
**功能**：检查游戏 Build 版本。
*   如果版本号小于 6000 (1.21 及以前)，使用旧算法。
*   如果版本号大于等于 6000 (1.22+)，使用新算法（本报告分析的逻辑）。
*   1.26 的内部版本号为 `6059` (0x17AB)，因此走新逻辑。

<details open>
<summary><strong>[展开汇编] C. 版本检查逻辑</strong></summary>
```assembly
6F39ED70 | 81EC 08010000            | sub esp,108                                  |
6F39ED76 | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]              |
6F39ED7B | 33C4                     | xor eax,esp                                  |
6F39ED7D | 898424 04010000          | mov dword ptr ss:[esp+104],eax               |
6F39ED84 | 56                       | push esi                                     |
6F39ED85 | 8BF2                     | mov esi,edx                                  |
6F39ED87 | 68 04010000              | push 104                                     | <--- 入参 0x104 = 260 (即 Windows 的 MAX_PATH 常量)
6F39ED8C | 8D5424 08                | lea edx,dword ptr ss:[esp+8]                 | <--- 获取栈上的缓冲区地址
6F39ED90 | E8 DB2FC7FF              | call game.6F011D70                           | <--- 调用函数：获取游戏目录/路径
6F39ED95 | 85C0                     | test eax,eax                                 | <--- 检查函数返回值：获取路径成功了吗？
6F39ED97 | 75 18                    | jne game.6F39EDB1                            | <--- 成功(非0)则跳转继续，失败(0)则退出
6F39ED99 | 5E                       | pop esi                                      |
6F39ED9A | 8B8C24 04010000          | mov ecx,dword ptr ss:[esp+104]               |
6F39EDA1 | 33CC                     | xor ecx,esp                                  |
6F39EDA3 | E8 B1224400              | call game.6F7E1059                           |
6F39EDA8 | 81C4 08010000            | add esp,108                                  |
6F39EDAE | C2 0800                  | ret 8                                        |
6F39EDB1 | 57                       | push edi                                     |
6F39EDB2 | 8BBC24 18010000          | mov edi,dword ptr ss:[esp+118]               | <--- 获取参数2
6F39EDB9 | 85FF                     | test edi,edi                                 | <--- 检查参数2是否为 0
6F39EDBB | 75 05                    | jne game.6F39EDC2                            | <--- 如果不是0，继续
6F39EDBD | BF AB170000              | mov edi,17AB                                 | <--- 如果是0，赋予默认值 0x17AB (十进制 6059)
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
</details>

### 3.3 参数封装层 (Wrapper Layer)
**函数地址**：`Game.dll + 3B1BB0`
**功能**：这是核心计算逻辑的“外壳”。它负责准备环境，将 `common.j`、`blizzard.j` 和 `war3map.j` 的文件名及内存指针压入堆栈，调用核心计算函数，最后**负责清理内存**（即处理 `test ecx, ecx` 的逻辑）。

<details open>
<summary><strong>[展开汇编] D. Wrapper 层实现</strong></summary>

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
6F3B1BE8 | 85C9                     | test ecx,ecx                                 | <--- 检查指针是否为空
6F3B1BEA | 74 0A                    | je game.6F3B1BF6                             |
6F3B1BEC | BA 01000000              | mov edx,1                                    |
6F3B1BF1 | E8 BAAA1000              | call game.6F4BC6B0                           | <--- FreeMemory()
6F3B1BF6 | 8B4C24 0C                | mov ecx,dword ptr ss:[esp+C]                 |
6F3B1BFA | 85C9                     | test ecx,ecx                                 | <--- 检查指针是否为空
6F3B1BFC | 74 0A                    | je game.6F3B1C08                             |
6F3B1BFE | BA 01000000              | mov edx,1                                    |
6F3B1C03 | E8 A8AA1000              | call game.6F4BC6B0                           | <--- FreeMemory()
6F3B1C08 | 8B4C24 04                | mov ecx,dword ptr ss:[esp+4]                 |
6F3B1C0C | 85C9                     | test ecx,ecx                                 | <--- 检查指针是否为空
6F3B1C0E | 74 0A                    | je game.6F3B1C1A                             |
6F3B1C10 | BA 01000000              | mov edx,1                                    |
6F3B1C15 | E8 96AA1000              | call game.6F4BC6B0                           | <--- FreeMemory()
6F3B1C1A | 8B0424                   | mov eax,dword ptr ss:[esp]                   |
6F3B1C1D | 83C4 08                  | add esp,8                                    |
6F3B1C20 | C2 0800                  | ret 8                                        |
```
</details>

### 3.4 核心层：逻辑混合 (Core Calculation)
**函数地址**：`Game.dll + 3B1A20`
**功能**：执行具体的位运算混合逻辑。

此函数按顺序调用哈希函数计算三个文件的 Hash，然后按照第 2 节的数学公式进行混合。

<details open>
<summary><strong>[展开汇编] D. 核心混合逻辑</strong></summary>
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
; --- 1. 计算 Common.j Hash ---
6F3B1AA0 | E8 1BCBFEFF              | call game.6F39E5C0             			   | GetHash(common.j)
6F3B1AA5 | 8BE8                     | mov ebp,eax                                  | EBP = Hash_Com
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
; --- 2. 计算 Blizzard.j Hash ---
6F3B1ADC | E8 DFCAFEFF              | call game.6F39E5C0                           | GetHash(blizzard.j)
6F3B1AE1 | 8BD8                     | mov ebx,eax                                  | EBX = Hash_Bliz
6F3B1AE3 | 8B5424 30                | mov edx,dword ptr ss:[esp+30]                |
6F3B1AE7 | 8B7424 20                | mov esi,dword ptr ss:[esp+20]                |
; --- 3. 混合环境 (Environment Mix) ---
6F3B1AEB | 33DD                     | xor ebx,ebp                                  | EBX = Hash_Bliz ^ Hash_Com
6F3B1AED | C1C3 03                  | rol ebx,3                                    | EBX = ROL(EBX, 3)
6F3B1AF0 | 81F3 9E37F103            | xor ebx,3F1379E                              | EBX = EBX ^ MagicNumber
6F3B1AF6 | C1C3 03                  | rol ebx,3                                    | EBX = ROL(EBX, 3)
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
; --- 4. 计算 War3map.j Hash ---
6F3B1B1C | E8 9FCAFEFF              | call game.6F39E5C0             		       | GetHash(war3map.j)
6F3B1B21 | 8BD0                     | mov edx,eax                    		       | EDX = Hash_Map
6F3B1B23 | 8B4424 30                | mov eax,dword ptr ss:[esp+30]                |
; --- 5. 最终混合 (Final Mix) ---
6F3B1B27 | 3310                     | xor edx,dword ptr ds:[eax]     		       | EDX = Hash_Map ^ EnvironmentHash
6F3B1B29 | 5F                       | pop edi                                      |
6F3B1B2A | 5E                       | pop esi                                      |
6F3B1B2B | C1C2 03                  | rol edx,3                      			   | EDX = ROL(EDX, 3)
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
</details>

### 3.5 基础算法层 (Blizzard Hash Algorithm)
**函数地址**：`Game.dll + 39E5C0`
**功能**：暴雪自定义的哈希计算函数。
**逻辑**：它不是 CRC32，也不是 MD5。它是一个简单的 **Rotate-XOR** 算法。它遍历缓冲区，每 4 字节进行一次异或并左移 3 位。

<details open>
<summary><strong>[展开汇编] F. 自定义 Hash 算法实现</strong></summary>

```assembly
6F39E5C0 | 56                        | push esi                                     |
6F39E5C1 | 8BF2                      | mov esi,edx                                  | len
6F39E5C3 | C1EE 02                   | shr esi,2                                    | len / 4 (按 DWORD 处理)
6F39E5C6 | 33C0                      | xor eax,eax                                  | Init Hash = 0
6F39E5C8 | 83E2 03                   | and edx,3                                    | 计算剩余字节数 (len % 4)
6F39E5CB | 85F6                      | test esi,esi                                 |
6F39E5CD | 74 15                     | je game.6F39E5E4                             |
6F39E5CF | 57                        | push edi                                     |
; --- 主循环 (4字节一组) ---
6F39E5D0 | 8B39                      | mov edi,dword ptr ds:[ecx]                   | 读取 4 Bytes
6F39E5D2 | 33F8                      | xor edi,eax                                  | XOR CurrentHash
6F39E5D4 | 83EE 01                   | sub esi,1                                    |
6F39E5D7 | C1C7 03                   | rol edi,3                                    | ROL 3
6F39E5DA | 83C1 04                   | add ecx,4                                    | 指针 +4
6F39E5DD | 85F6                      | test esi,esi                                 |
6F39E5DF | 8BC7                      | mov eax,edi                                  | 更新 Hash
6F39E5E1 | 75 ED                     | jne game.6F39E5D0                            | 循环
6F39E5E3 | 5F                        | pop edi                                      |
6F39E5E4 | 85D2                      | test edx,edx                                 |
6F39E5E6 | 74 14                     | je game.6F39E5FC                             |
; --- 尾部处理 (剩余 1-3 字节) ---
6F39E5E8 | 0FB631                    | movzx esi,byte ptr ds:[ecx]                  | 读取 1 Byte
6F39E5EB | 33F0                      | xor esi,eax                                  | XOR
6F39E5ED | 83EA 01                   | sub edx,1                                    |
6F39E5F0 | C1C6 03                   | rol esi,3                                    | ROL 3
6F39E5F3 | 83C1 01                   | add ecx,1                                    | 指针 +1
6F39E5F6 | 85D2                      | test edx,edx                                 |
6F39E5F8 | 8BC6                      | mov eax,esi                                  |
6F39E5FA | 75 EC                     | jne game.6F39E5E8                            | 循环
6F39E5FC | 5E                        | pop esi                                      |
6F39E5FD | C3                        | ret                                          |
```
</details>

## 4. 关键机制详解

### 4.1 魔法数 (Magic Number)
*   **数值**: `0x03F1379E`
*   **作用**: **环境指纹盐值**。
    *   它强制校验和必须包含 `common.j` 和 `blizzard.j` 的信息。
    *   防止攻击者仅通过修改地图脚本来伪造一个合法的校验和。
    *   如果玩家修改了本地的 `common.j`（例如移除迷雾限制），会导致中间计算结果 `EBX` 改变，最终 Checksum 不匹配，从而引发掉线（Desync）。

### 4.2 暴雪自定义哈希函数 (Blizzard Hash)
**函数地址**：`Game.dll + 39E5C0`
**算法**：`Rotate-XOR Hash`
**注意**：这不是 CRC32，不要混淆。

<details open>
<summary><strong>[展开汇编] E. 自定义哈希函数实现</strong></summary>

```assembly
; 核心循环 (处理 4 字节块)
6F39E5C3 | C1EE 02        | shr esi,2            | 长度 / 4
...
6F39E5D0 | 8B39           | mov edi,[ecx]        | 读取 4 字节
6F39E5D2 | 33F8           | xor edi,eax          | XOR 上次结果
6F39E5D7 | C1C7 03        | rol edi,3            | ROL 3
6F39E5DF | 8BC7           | mov eax,edi          | 保存结果
...
; 尾部处理 (处理剩余 1-3 字节)
6F39E5E8 | 0FB631         | movzx esi,byte...    | 读取 1 字节
6F39E5EB | 33F0           | xor esi,eax          | XOR
6F39E5F0 | C1C6 03        | rol esi,3            | ROL 3
```
</details>

---

## 5. 代码还原 (C++ Implementation)

以下代码可用于编写版本伪装器、改图工具或私有服务器的校验逻辑。

```cpp
#include <cstdint>
#include <vector>

// 循环左移 (对应汇编 ROL)
inline uint32_t RotateLeft(uint32_t value, int shift) {
    return (value << shift) | (value >> (32 - shift));
}

// 暴雪自定义哈希算法 (Game.dll + 39E5C0)
uint32_t BlizzardHash(const uint8_t* data, size_t length) {
    uint32_t hash = 0;
    const uint8_t* ptr = data;
    
    // 处理 4 字节块
    size_t dwords = length / 4;
    for (size_t i = 0; i < dwords; ++i) {
        uint32_t chunk = *reinterpret_cast<const uint32_t*>(ptr);
        hash = hash ^ chunk;
        hash = RotateLeft(hash, 3);
        ptr += 4;
        length -= 4;
    }
    
    // 处理剩余字节
    while (length > 0) {
        uint32_t byteVal = *ptr;
        hash = hash ^ byteVal;
        hash = RotateLeft(hash, 3);
        ptr++;
        length--;
    }
    return hash;
}

/**
 * 计算 War3 1.26 地图校验和 (Game.dll + 3B1A20)
 * @param h_common   common.j 的 BlizzardHash
 * @param h_blizzard blizzard.j 的 BlizzardHash
 * @param h_map      war3map.j 的 BlizzardHash
 * @return 最终的 Checksum
 */
uint32_t MixCheckSum(uint32_t h_common, uint32_t h_blizzard, uint32_t h_map) {
    // 1. 混合 Common 和 Blizzard
    uint32_t val = h_blizzard ^ h_common;
    
    // 2. 变换 1
    val = RotateLeft(val, 3);
    
    // 3. 异或魔数 (特征码 0x03F1379E)
    val = val ^ 0x03F1379E;
    
    // 4. 变换 2
    val = RotateLeft(val, 3);
    
    // 5. 混合地图脚本
    val = h_map ^ val;
    
    // 6. 最终变换
    val = RotateLeft(val, 3);
    
    return val;
}
```

---

## 6. 利用与绕过 (Exploitation)

对于外挂开发或改图过检测，主要有两种思路：

1.  **底层强杀 (Memory Hook)** - **推荐**
    *   **挂钩点**: `Game.dll + 3B1B2F` (CoreCalc 函数末尾)。
    *   **操作**: 在指令 `mov [eax], edx` 执行前，将寄存器 `EDX` 的值强行修改为**正版地图的 Checksum**。
    *   **优点**: 一劳永逸。无论是自己建房（HostGame）还是加入别人的房间（LocalCheck），底层计算都会被劫持，完美通过校验。

2.  **文件欺骗 (Storm Hook)**
    *   **挂钩点**: `Storm.dll` 的 `SFileReadFile` 或 Ordinal #279。
    *   **操作**: 当游戏引擎请求读取 `war3map.j` 时，重定向到原始的正版脚本，或者在计算 Hash 时返回预先计算好的正版 Hash。
    *   **缺点**: 需要处理文件指针和缓冲区，逻辑较复杂。
	

## 7. 进阶分析：第三阶段 - 地图组件校验
**函数地址**：`Game.dll + 6F39EC00`

---

### 7.1 逻辑概述
`Final Checksum : DD81EDDA` 仅仅是 **脚本层面的校验和** (Script Checksum)。
游戏为了防止玩家修改地形、穿墙、修改单位属性等数据，还会对 MPQ 包内的二进制数据文件进行校验。

最终的 **Map Checksum** (`3AEBCEF3`) 计算公式为：

$$Val_{final} = \text{ROL}( Val_{script} \oplus H_{component}, 3 )$$

该公式会对下列文件按**固定顺序**递归执行。如果 MPQ 中不存在某个文件，则直接跳过（不混入 0）。

### 7.2 校验文件列表 (按顺序)
1.  `war3map.w3e` (Environment / 地形高度、纹理)
2.  `war3map.wpm` (Pathing / 路径阻挡)
3.  `war3map.doo` (Doodads / 树木、装饰物布局)
4.  `war3map.w3u` (Units / 自定义单位数据)
5.  `war3map.w3b` (Destructables / 可破坏物布局)
6.  `war3map.w3d` (Doodad Defs / 装饰物定义)
7.  `war3map.w3a` (Abilities / 自定义技能数据)
8.  `war3map.w3q` (Upgrades / 自定义科技数据)

### 7.3 关键汇编分析

#### A. 组件遍历驱动 (6F39EC00)

<details open>
<summary><strong>[展开汇编] A. 负责按顺序加载文件，如果存在则调用混合函数。</strong></summary>

```assembly
6F39EC00 | 56                        | push esi                                     |
6F39EC01 | 8BF1                      | mov esi,ecx                                  | <--- ESI = 当前的总 Checksum 指针
6F39EC03 | 57                        | push edi                                     |
6F39EC04 | 8BFA                      | mov edi,edx                                  |
6F39EC06 | 8BD6                      | mov edx,esi                                  |
6F39EC08 | B9 2C6A876F               | mov ecx,game.6F876A2C                        | <--- ".w3e"
6F39EC0D | E8 5EFFFFFF               | call game.6F39EB70                           |
6F39EC12 | 8BD6                      | mov edx,esi                                  |
6F39EC14 | B9 246A876F               | mov ecx,game.6F876A24                        | <--- ".wpm"
6F39EC19 | E8 52FFFFFF               | call game.6F39EB70                           |
6F39EC1E | 8BD6                      | mov edx,esi                                  |
6F39EC20 | B9 2819946F               | mov ecx,game.6F941928                        | <--- ".doo"
6F39EC25 | E8 46FFFFFF               | call game.6F39EB70                           |
6F39EC2A | 8BD6                      | mov edx,esi                                  |
6F39EC2C | B9 60C5926F               | mov ecx,game.6F92C560                        | <--- ".w3u"
6F39EC31 | E8 3AFFFFFF               | call game.6F39EB70                           |
6F39EC36 | 85FF                      | test edi,edi                                 |
6F39EC38 | 74 32                     | je game.6F39EC6C                             |
6F39EC3A | 8BD6                      | mov edx,esi                                  |
6F39EC3C | B9 50C6926F               | mov ecx,game.6F92C650                        | <--- ".w3b"
6F39EC41 | E8 2AFFFFFF               | call game.6F39EB70                           |
6F39EC46 | 8BD6                      | mov edx,esi                                  |
6F39EC48 | B9 80C6926F               | mov ecx,game.6F92C680                        | <--- ".w3d"
6F39EC4D | E8 1EFFFFFF               | call game.6F39EB70                           |
6F39EC52 | 8BD6                      | mov edx,esi                                  |
6F39EC54 | B9 C0C5926F               | mov ecx,game.6F92C5C0                        | <--- ".w3a"
6F39EC59 | E8 12FFFFFF               | call game.6F39EB70                           |
6F39EC5E | 5F                        | pop edi                                      |
6F39EC5F | 8BD6                      | mov edx,esi                                  
6F39EC61 | B9 20C6926F               | mov ecx,game.6F92C620                        | <--- ".w3q"
6F39EC66 | 5E                        | pop esi                                      |
6F39EC67 | E9 04FFFFFF               | jmp game.6F39EB70                            |
6F39EC6C | 5F                        | pop edi                                      |
6F39EC6D | 5E                        | pop esi                                      |
6F39EC6E | C3                        | ret                                          |
```
</details>

#### B. 组件混合逻辑 (6F39EB70)

<details open>
<summary><strong>[展开汇编] B. 负责提取文件、计算 Hash 并混入总值。</strong></summary>

```assembly
6F39EB70 | 81EC 10010000             | sub esp,110                                  |
6F39EB76 | A1 40E1AA6F               | mov eax,dword ptr ds:[6FAAE140]              |
6F39EB7B | 33C4                      | xor eax,esp                                  |
6F39EB7D | 898424 0C010000           | mov dword ptr ss:[esp+10C],eax               |
6F39EB84 | 56                        | push esi                                     |
6F39EB85 | 51                        | push ecx                                     |
6F39EB86 | 68 CC50876F               | push game.6F8750CC                           |
6F39EB8B | 68 F0C6926F               | push game.6F92C6F0                           |
6F39EB90 | 8D4424 18                 | lea eax,dword ptr ss:[esp+18]                |
6F39EB94 | 68 04010000               | push 104                                     |
6F39EB99 | 50                        | push eax                                     |
6F39EB9A | 8BF2                      | mov esi,edx                                  |
6F39EB9C | E8 05CA3400               | call <JMP.&Ordinal#578>                      |
6F39EBA1 | 83C4 14                   | add esp,14                                   |
6F39EBA4 | 6A 01                     | push 1                                       |
6F39EBA6 | 8D4C24 0C                 | lea ecx,dword ptr ss:[esp+C]                 |
6F39EBAA | 51                        | push ecx                                     |
6F39EBAB | 8D5424 0C                 | lea edx,dword ptr ss:[esp+C]                 |
6F39EBAF | 8D4C24 14                 | lea ecx,dword ptr ss:[esp+14]                |
6F39EBB3 | C74424 0C 00000000        | mov dword ptr ss:[esp+C],0                   |
6F39EBBB | E8 90291200               | call game.6F4C1550                           |
6F39EBC0 | 85C0                      | test eax,eax                                 | <--- 检查文件是否存在/大小是否为0
6F39EBC2 | 74 24                     | je game.6F39EBE8                             | <--- 不存在则直接返回，不改变 Checksum
6F39EBC4 | 8B5424 08                 | mov edx,dword ptr ss:[esp+8]                 |
6F39EBC8 | 57                        | push edi                                     |
6F39EBC9 | 8B7C24 08                 | mov edi,dword ptr ss:[esp+8]                 |
6F39EBCD | 8BCF                      | mov ecx,edi                                  |
6F39EBCF | E8 ECF9FFFF               | call game.6F39E5C0                           | <--- Calculate BlizzardHash(FileContent)
6F39EBD4 | 3306                      | xor eax,dword ptr ds:[esi]                   | <--- EAX = FileHash ^ CurrentChecksum
6F39EBD6 | BA 01000000               | mov edx,1                                    |
6F39EBDB | C1C0 03                   | rol eax,3                                    | <--- EAX = ROL(EAX, 3)
6F39EBDE | 8BCF                      | mov ecx,edi                                  |
6F39EBE0 | 8906                      | mov dword ptr ds:[esi],eax                   | <--- 更新 Checksum
6F39EBE2 | E8 C9DA1100               | call game.6F4BC6B0                           |
6F39EBE7 | 5F                        | pop edi                                      |
6F39EBE8 | 8B8C24 10010000           | mov ecx,dword ptr ss:[esp+110]               |
6F39EBEF | 5E                        | pop esi                                      |
6F39EBF0 | 33CC                      | xor ecx,esp                                  |
6F39EBF2 | E8 62244400               | call game.6F7E1059                           |
6F39EBF7 | 81C4 10010000             | add esp,110                                  |
6F39EBFD | C3                        | ret                                          |
```
</details>

---

## 8. 完整的 C++ 算法实现 (最终版)

这是包含脚本校验和组件校验的完整逻辑。

```cpp
#include <vector>
#include <string>
#include <iostream>

// 循环左移 (ROL)
inline uint32_t RotateLeft(uint32_t value, int shift) {
    return (value << shift) | (value >> (32 - shift));
}

// 暴雪自定义哈希 (6F39E5C0)
uint32_t BlizzardHash(const std::vector<uint8_t>& data) {
    uint32_t hash = 0;
    const uint8_t* ptr = data.data();
    size_t length = data.size();
    
    size_t dwords = length / 4;
    for (size_t i = 0; i < dwords; ++i) {
        uint32_t chunk = *reinterpret_cast<const uint32_t*>(ptr);
        hash = hash ^ chunk;
        hash = RotateLeft(hash, 3);
        ptr += 4;
        length -= 4;
    }
    while (length > 0) {
        uint32_t byteVal = *ptr;
        hash = hash ^ byteVal;
        hash = RotateLeft(hash, 3);
        ptr++;
        length--;
    }
    return hash;
}

// 模拟从 MPQ 读取文件 (需自行实现 MPQ 库调用)
std::vector<uint8_t> GetFileFromMPQ(const std::string& filename) {
    // TODO: 调用 StormLib SFileOpenFileEx / SFileReadFile
    // 如果文件不存在，返回空 vector
    return {}; 
}

/**
 * 计算 War3 1.26 最终地图校验和
 */
uint32_t CalculateFinalChecksum() {
    // --- 第一阶段：脚本校验 ---
    std::vector<uint8_t> common = GetFileFromMPQ("common.j");
    std::vector<uint8_t> blizzard = GetFileFromMPQ("blizzard.j");
    std::vector<uint8_t> war3map = GetFileFromMPQ("war3map.j"); // 或 war3map.lua

    uint32_t h_com = BlizzardHash(common);
    uint32_t h_bliz = BlizzardHash(blizzard);
    uint32_t h_map = BlizzardHash(war3map);

    // Mix Environment
    uint32_t val = h_bliz ^ h_com;
    val = RotateLeft(val, 3);
    val = val ^ 0x03F1379E; // Magic Salt
    val = RotateLeft(val, 3);

    // Mix Script
    val = h_map ^ val;
    val = RotateLeft(val, 3);
    
    // 此时 val = Script Checksum (例如 DD81EDDA)

    // --- 第二阶段：组件校验 (6F39EC00) ---
    const char* componentFiles[] = {
        "war3map.w3e", "war3map.wpm", "war3map.doo", "war3map.w3u",
        "war3map.w3b", "war3map.w3d", "war3map.w3a", "war3map.w3q"
    };

    for (const char* filename : componentFiles) {
        std::vector<uint8_t> fileData = GetFileFromMPQ(filename);
        
        // [关键] 如果文件不存在或为空，直接跳过 (je 6F39EBE8)
        if (fileData.empty()) continue;

        uint32_t fileHash = BlizzardHash(fileData);

        // Mix Component
        // 6F39EBD4 | xor eax, [esi]
        val = val ^ fileHash; 
        
        // 6F39EBDB | rol eax, 3
        val = RotateLeft(val, 3);
    }

    // 此时 val = Final Map Checksum (例如 3AEBCEF3)
    return val;
}
```
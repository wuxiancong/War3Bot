# War3 Game StatString 解析逻辑逆向分析 (Game.dll)

**环境 (Environment):**
- **客户端:** Warcraft III (v1.26.0.6401 / Game.dll)
- **服务端:** PVPGN
- **调试工具:** x64dbg
- **基地址:** 6F000000

**目标**: 分析 `Game.dll` 如何解析房间的 StatString，并找出机器人房间 (Bot) 与真人房间 (Test) 在解析流程中的差异。
**结论**: 机器人房间虽然通过了初步的解码函数，但在**数据流完整性校验 (Stream Integrity Check)** 阶段失败，导致整个房间信息被判定为无效。

这是一个关于 WarCraft III (WAR3/W3XP) **Game StatString**（游戏统计字符串）字段结构的中文技术文档。

该字段包含了针对每个游戏产品非常具体的信息。

# WarCraft III (WAR3/W3XP) Game StatString 结构

该字段的数据通过特殊编码以符合 **以 Null (0x00) 结尾的字符串** 格式，这意味着原始数据中即便包含 0x00，经过编码后也不会在字符串中间出现空字符。

## 数据结构概览

整个字符串主要分为两部分：
1.  **头部（未编码）**：前 9 个字节。
2.  **主体（已编码）**：剩余数据经过特殊算法编码（解码后包含原始的二进制数据）。

---

## 头部数据 (未编码)
这部分直接由 ASCII 字符组成，表示十六进制整数（0-9, a-f）。

*   **(CHAR)** **空闲槽位数量**
    *   例如：字符 `7` 表示有 7 个空位。
*   **(CHAR) [8]** **主机计数器 (Host Counter)**
    *   反转的十六进制整数。
    *   例如：`20000000` (反转后为 `00000002`) 表示这是该主机在本次会话中创建的第 2 个游戏。

---

## 编码数据 (Encoded Data)
这部分数据经过解码算法还原后，包含以下字段结构：

### A. 地图标志位 (Map Flags) - (UINT32)
这是一个 32 位整数，由多个设置掩码组合而成：

**游戏速度 (Game Speed)**
*掩码: `0x00000003` (互斥，只能选其一)*
*   `0x00000000`: **慢速 (Slow)**
*   `0x00000001`: **普通 (Normal)**
*   `0x00000002`: **快速 (Fast)**

**可见度设置 (Visibility)**
*掩码: `0x00000F00` (互斥)*
*   `0x00000100`: **隐藏地形 (Hide Terrain)**
*   `0x00000200`: **地图已探索 (Map Explored)**
*   `0x00000400`: **总是可见 (Always Visible)**
*   `0x00000800`: **默认 (Default)**

**观察者设置 (Observers)**
*掩码: `0x40003000` (互斥)*
*   `0x00000000`: **无观察者 (No Observers)**
*   `0x00002000`: **失败后旁观 (Observers on Defeat)**
*   `0x00003000`: **允许完整观察者 (Full Observers)**
*   `0x40000000`: **裁判 (Referees)**

**高级主机设置 (Advanced Options)**
*掩码: `0x07064000` (可组合)*
*   `0x00004000`: **队伍在一起 (Teams Together)** - 队员出生点相邻
*   `0x00060000`: **锁定队伍 (Lock Teams)**
*   `0x01000000`: **共享单位控制 (Full Shared Unit Control)**
*   `0x02000000`: **随机英雄 (Random Hero)**
*   `0x04000000`: **随机种族 (Random Races)**

### B. 地图信息
*   **(UINT8)** **Map null 1** (填充字节 0)
*   **(UINT8)** **地图宽度** (可玩区域)
*   **(UINT8)** **Map null 2** (填充字节 0)
*   **(UINT8)** **地图高度** (可玩区域)
*   **(UINT8)** **Map null 3** (填充字节 0)
*   **(UINT32)** **地图 CRC** (校验和)
*   **(STRING)** **地图路径** (Map path)
*   **(STRING)** **游戏房主名称** (Game host name)
*   **(UINT8)** **地图未知字节** (可能是一个仅包含结束符的空字符串)
*   **(UINT8) [20]** **未知数据** (通常是 **SHA1 哈希值**)

---

## 1. 入口函数: 校验与分发 (0x6F5BE710)

此函数是上层 UI 逻辑调用解析器的入口。它调用核心解析函数，并根据结果决定是否继续比较缓存的数据。

```assembly
6F5BE710 | 56                       | push esi                                                 | <--- 保存 ESI (指向 BattleNetCustomJoinPanel)
6F5BE711 | 8BF1                     | mov esi,ecx                                              | <--- ESI = this 指针
6F5BE713 | E8 F85D0900              | call game.6F654510                                       | <--- 核心调用 ParseStatString (解析函数)
6F5BE718 | 85C0                     | test eax,eax                                             | <--- 检查解析结果 (1=成功, 0=失败)
6F5BE71A | 75 04                    | jne game.6F5BE720                                        | <--- 若成功(1)，跳转继续比较数据
6F5BE71C | 33C0                     | xor eax,eax                                              | <--- 若失败(0)，返回 0
6F5BE71E | 5E                       | pop esi                                                  | <--- 恢复 ESI
6F5BE71F | C3                       | ret                                                      | <--- 返回
; --- 以下是解析成功后的缓存比较逻辑 ---
6F5BE720 | 807E 31 02               | cmp byte ptr ds:[esi+31],2                               | <--- 比较标志位 (Memory Dump 2)
6F5BE724 | 77 F6                    | ja game.6F5BE71C                                         |
6F5BE726 | 33C0                     | xor eax,eax                                              |
6F5BE728 | 3846 54                  | cmp byte ptr ds:[esi+54],al                              | <--- 比较地图路径首字符 'M' (Memory Dump: 0019F8DC)
6F5BE72B | 74 09                    | je game.6F5BE736                                         |
6F5BE72D | 3886 8A000000            | cmp byte ptr ds:[esi+8A],al                              | <--- 比较创建者首字符 't' (Memory Dump: 0019F912)
6F5BE733 | 0F95C0                   | setne al                                                 | <--- 设置返回值
6F5BE736 | 5E                       | pop esi                                                  |
6F5BE737 | C3                       | ret                                                      |
```

---

## 2. 核心解析函数 (0x6F654510)

此函数负责解密 StatString，并从中提取 MapPath, CreatorName, GameSettings 等信息。

**关键发现**: bot 房间的死因在于函数末尾的 **数据长度校验**。

```assembly
; --- 函数序言与栈帧建立 ---
6F654510 | 6A FF                    | push FFFFFFFF                                            |
6F654512 | 68 DB12846F              | push game.6F8412DB                                       | <--- 注册 SEH 异常处理
6F654517 | 64:A1 00000000           | mov eax,dword ptr fs:[0]                                 |
6F65451D | 50                       | push eax                                                 |
6F65451E | 81EC 8C000000            | sub esp,8C                                               | <--- 分配栈空间
6F654524 | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]                          | <--- Stack Cookie
6F654529 | 33C4                     | xor eax,esp                                              |
6F65452B | 898424 88000000          | mov dword ptr ss:[esp+88],eax                            |
6F654532 | 53                       | push ebx                                                 | <--- ebx: 指向房间名字符串 (如 "bot" 或 "test")
6F654533 | 55                       | push ebp                                                 | <--- ebp: 指向输出结构体 (0019FA08)
6F654534 | 56                       | push esi                                                 |
6F654535 | 57                       | push edi                                                 |
6F654536 | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]                          |
6F65453B | 33C4                     | xor eax,esp                                              |
6F65453D | 50                       | push eax                                                 |
6F65453E | 8D8424 A0000000          | lea eax,dword ptr ss:[esp+A0]                            |
6F654545 | 64:A3 00000000           | mov dword ptr fs:[0],eax                                 |
6F65454B | 8BE9                     | mov ebp,ecx                                              |
6F65454D | 8BDA                     | mov ebx,edx                                              | <--- EBX = 房间名指针
6F65454F | B9 08000000              | mov ecx,8                                                |
6F654554 | 8BF3                     | mov esi,ebx                                              |
6F654556 | 8BFD                     | mov edi,ebp                                              |
6F654558 | F3:A5                    | rep movsd                                                | <--- 复制部分数据初始化
6F65455A | 807D 00 00               | cmp byte ptr ss:[ebp],0                                  | <--- 检查名字是否为空
6F65455E | 0F84 79010000            | je game.6F6546DD                                         |
6F654564 | 6A 68                    | push 68                                                  |
6F654566 | 8D4424 38                | lea eax,dword ptr ss:[esp+38]                            |
6F65456A | 50                       | push eax                                                 |
6F65456B | 8D4C24 1C                | lea ecx,dword ptr ss:[esp+1C]                            |
6F65456F | E8 1CFCFFFF              | call game.6F654190                                       | <--- 初始化解密缓冲区
6F654574 | C74424 14 800E976F       | mov dword ptr ss:[esp+14],game.6F970E80                  |

; --- 解码 StatString (Decode) ---
6F65457C | 8D4B 30                  | lea ecx,dword ptr ds:[ebx+30]                            | <--- ECX = StatString 起始地址 (Memory Dump: 15E201D4)
6F65457F | 51                       | push ecx                                                 |
6F654580 | 33F6                     | xor esi,esi                                              |
6F654582 | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                            |
6F654586 | 89B424 AC000000          | mov dword ptr ss:[esp+AC],esi                            |
6F65458D | E8 8EC2FFFF              | call game.6F650820                                       | <--- DecodeStatString (解码)
6F654592 | 85C0                     | test eax,eax                                             | <--- 检查解码头部校验和是否正确
6F654594 | 8D4C24 14                | lea ecx,dword ptr ss:[esp+14]                            |
6F654598 | 0F84 2F010000            | je game.6F6546CD                                         | <--- 解码失败则跳转 (Bot 未在此跳转)

; --- 逐字段读取解析 (Parse Fields) ---
6F65459E | 8D55 31                  | lea edx,dword ptr ss:[ebp+31]                            | 
6F6545A1 | 52                       | push edx                                                 |
6F6545A2 | 897424 2C                | mov dword ptr ss:[esp+2C],esi                            |
6F6545A6 | E8 65E6E6FF              | call game.6F4C2C10                                       | <--- 读取 Flags
6F6545AB | 8D45 34                  | lea eax,dword ptr ss:[ebp+34]                            |
6F6545AE | 50                       | push eax                                                 |
6F6545AF | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                            |
6F6545B3 | E8 78E7E6FF              | call game.6F4C2D30                                       | <--- 读取 Map Path
6F6545B8 | 8D4D 38                  | lea ecx,dword ptr ss:[ebp+38]                            |
6F6545BB | 51                       | push ecx                                                 |
6F6545BC | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                            |
6F6545C0 | E8 ABE6E6FF              | call game.6F4C2C70                                       | <--- 读取数据
6F6545C5 | 8D55 3A                  | lea edx,dword ptr ss:[ebp+3A]                            |
6F6545C8 | 52                       | push edx                                                 |
6F6545C9 | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                            |
6F6545CD | E8 9EE6E6FF              | call game.6F4C2C70                                       | <--- 读取数据
6F6545D2 | 8D45 3C                  | lea eax,dword ptr ss:[ebp+3C]                            |
6F6545D5 | 50                       | push eax                                                 |
6F6545D6 | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                            |
6F6545DA | E8 51E7E6FF              | call game.6F4C2D30                                       | <--- 读取 Creator Name
6F6545DF | 6A 36                    | push 36                                                  |
6F6545E1 | 8D75 54                  | lea esi,dword ptr ss:[ebp+54]                            |
6F6545E4 | 56                       | push esi                                                 |
6F6545E5 | 8D4C24 1C                | lea ecx,dword ptr ss:[esp+1C]                            |
6F6545E9 | E8 22E8E6FF              | call game.6F4C2E10                                       | <--- 处理字符串
6F6545EE | 8B4C24 28                | mov ecx,dword ptr ss:[esp+28]                            |
6F6545F2 | 3B4C24 24                | cmp ecx,dword ptr ss:[esp+24]                            |
6F6545F6 | 76 03                    | jbe game.6F6545FB                                        |
6F6545F8 | C606 00                  | mov byte ptr ds:[esi],0                                  |
6F6545FB | 6A 10                    | push 10                                                  |
6F6545FD | 8DB5 8A000000            | lea esi,dword ptr ss:[ebp+8A]                            |
6F654603 | 56                       | push esi                                                 |
6F654604 | 8D4C24 1C                | lea ecx,dword ptr ss:[esp+1C]                            |
6F654608 | E8 03E8E6FF              | call game.6F4C2E10                                       |
6F65460D | 8B5424 28                | mov edx,dword ptr ss:[esp+28]                            |
6F654611 | 3B5424 24                | cmp edx,dword ptr ss:[esp+24]                            |
6F654615 | 76 03                    | jbe game.6F65461A                                        |
6F654617 | C606 00                  | mov byte ptr ds:[esi],0                                  |
6F65461A | 8D45 30                  | lea eax,dword ptr ss:[ebp+30]                            |
6F65461D | 50                       | push eax                                                 |
6F65461E | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                            |
6F654622 | E8 E9E5E6FF              | call game.6F4C2C10                                       |
6F654627 | 8B4C24 28                | mov ecx,dword ptr ss:[esp+28]                            |
6F65462B | 3B4C24 24                | cmp ecx,dword ptr ss:[esp+24]                            |
6F65462F | 74 0E                    | je game.6F65463F                                         |
6F654631 | 8D55 40                  | lea edx,dword ptr ss:[ebp+40]                            |
6F654634 | 8D4C24 14                | lea ecx,dword ptr ss:[esp+14]                            |
6F654638 | E8 33C1FFFF              | call game.6F650770                                       |
6F65463D | EB 2B                    | jmp game.6F65466A                                        |
6F65463F | 8B15 380E976F            | mov edx,dword ptr ds:[6F970E38]                          |
6F654645 | 8955 40                  | mov dword ptr ss:[ebp+40],edx                            |
6F654648 | A1 3C0E976F              | mov eax,dword ptr ds:[6F970E3C]                          |
6F65464D | 8945 44                  | mov dword ptr ss:[ebp+44],eax                            |
6F654650 | 8B0D 400E976F            | mov ecx,dword ptr ds:[6F970E40]                          |
6F654656 | 894D 48                  | mov dword ptr ss:[ebp+48],ecx                            |
6F654659 | 8B15 440E976F            | mov edx,dword ptr ds:[6F970E44]                          |
6F65465F | 8955 4C                  | mov dword ptr ss:[ebp+4C],edx                            |
6F654662 | A1 480E976F              | mov eax,dword ptr ds:[6F970E48]                          |
6F654667 | 8945 50                  | mov dword ptr ss:[ebp+50],eax                            |

; ========================================================================
; 关键差异点：数据流完整性校验 (Check Stream End)
; ========================================================================
6F65466A | 8B4C24 24                | mov ecx,dword ptr ss:[esp+24]                            | <--- ECX = 缓冲区结束位置 (End Ptr)
6F65466E | 394C24 28                | cmp dword ptr ss:[esp+28],ecx                            | <--- 比较: 当前读取位置 (Current Ptr) vs 结束位置
6F654672 | 75 55                    | jne game.6F6546C9                                        | <--- Bot 死因 (校验失败，跳转至清理代码)

; --- 解析成功后续处理 ---
6F654674 | 8B53 20                  | mov edx,dword ptr ds:[ebx+20]                            | <--- 保存解析出的数据到 GameInfo
6F654677 | 8955 20                  | mov dword ptr ss:[ebp+20],edx                            |
; ... (填充结构体) ...
6F6546BD | E8 AEFBFFFF              | call game.6F654270                                       |
6F6546C2 | B8 01000000              | mov eax,1                                                | <--- 设置返回值 EAX = 1 (成功)
6F6546C7 | EB 16                    | jmp game.6F6546DF                                        | <--- 跳转到退出

; --- 错误处理分支 (Bot 跳转到了这里) ---
6F6546C9 | 8D4C24 14                | lea ecx,dword ptr ss:[esp+14]                            |
6F6546CD | C78424 A8000000 FFFFFFFF | mov dword ptr ss:[esp+A8],FFFFFFFF                       |
6F6546D8 | E8 93FBFFFF              | call game.6F654270                                       | <--- 清理资源
6F6546DD | 33C0                     | xor eax,eax                                              | <--- 设置返回值 EAX = 0 (失败)

; --- 函数退出 ---
6F6546DF | 8B8C24 A0000000          | mov ecx,dword ptr ss:[esp+A0]                            |
6F6546E6 | 64:890D 00000000         | mov dword ptr fs:[0],ecx                                 |
6F6546ED | 59                       | pop ecx                                                  |
6F6546EE | 5F                       | pop edi                                                  |
6F6546EF | 5E                       | pop esi                                                  |
6F6546F0 | 5D                       | pop ebp                                                  |
6F6546F1 | 5B                       | pop ebx                                                  |
6F6546F2 | 8B8C24 88000000          | mov ecx,dword ptr ss:[esp+88]                            |
6F6546F9 | 33CC                     | xor ecx,esp                                              |
6F6546FB | E8 59C91800              | call game.6F7E1059                                       |
6F654700 | 81C4 98000000            | add esp,98                                               |
6F654706 | C3                       | ret                                                      |
```

---

## 3. SEH 异常处理函数 (0x6F8412DB)

这是一个标准的 C++ 异常处理存根，用于处理解析过程中可能发生的内存访问违规等异常。

```assembly
6F8412D0 | 8D8D 68FFFFFF            | lea ecx,dword ptr ss:[ebp-98]           |
6F8412D6 | E9 7530E1FF              | jmp game.6F654350                       |
6F8412DB | 8B5424 08                | mov edx,dword ptr ss:[esp+8]            |
6F8412DF | 8D82 64FFFFFF            | lea eax,dword ptr ds:[edx-9C]           |
6F8412E5 | 8B8A 60FFFFFF            | mov ecx,dword ptr ds:[edx-A0]           |
6F8412EB | 33C8                     | xor ecx,eax                             |
6F8412ED | E8 67FDF9FF              | call game.6F7E1059                      |
6F8412F2 | 83C0 10                  | add eax,10                              |
6F8412F5 | 8B4A FC                  | mov ecx,dword ptr ds:[edx-4]            |
6F8412F8 | 33C8                     | xor ecx,eax                             |
6F8412FA | E8 5AFDF9FF              | call game.6F7E1059                      |
6F8412FF | B8 C40CA46F              | mov eax,game.6FA40CC4                   |
6F841304 | E9 BFFDF9FF              | jmp <JMP.&__CxxFrameHandler3>           |
```
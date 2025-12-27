# War3 创建房间后点击列表选项弹出找不到游戏的对话框调试日志

![x64dbg调试截图](https://github.com/wuxiancong/War3Bot/raw/main/debug/images/War3Dialog_NETERROR_JOINGAMENOTFOUND.PNG)


## 问题描述 (Issue Description)

**现象:**
在使用自定义 C++ 编写的 War3Bot 创建房间后：
1.  **列表可见**: 房间可以正常显示在魔兽争霸 III 的局域网游戏列表中（说明 UDP 广播正常）。
2.  **加入失败**: 当点击列表中的游戏尝试加入时，客户端立刻弹出错误提示。
3.  **错误文本**: 尝试点击“加入游戏”时，客户端弹出错误提示：**"The game you attempted to join could not be found./n/nYou may have entered the name incorrectly or the game creator may have canceled the game."** (您尝试加入的游戏未找到。/n/您输入的名称可能不正确，或者游戏创建者可能已取消了该游戏。)。

**环境 (Environment):**
- **客户端:** Warcraft III (1.26a / Game.dll)
- **服务端:** PVPGN (C++)
- **调试工具:** x64dbg
- **基地址:** 6F000000
- **关键函数地址:** 
    - 字符处理逻辑: `Game.dll + 5C9650`
    - 错误分发逻辑: `Game.dll + 5B51D0`
    - 列表选中逻辑: `Game.dll + 5B51D0`
    - 状态解析逻辑: `Game.dll + 5BE710`

---

## 2. 逆向分析过程 (Reverse Engineering)

### 2.1 字符处理：追踪处理字符的函数
通过搜索字符串 "The game you attempted to join could not be found"，定位到 `Storm.dll` 中的字符处理函数。

**地址**: `6F5C9650` (字符处理逻辑)
**偏移**: `game.dll + 5C9650`
**关键发现**: 命中 Storm.dll Ordinal#501 导入表序号，由此追踪上一级函数。


```assembly
6F5C9650      | 53                       | push ebx                              |
6F5C9651      | 56                       | push esi                              |
6F5C9652      | 8BF1                     | mov esi,ecx                           |
6F5C9654      | 85F6                     | test esi,esi                          |
6F5C9656      | 57                       | push edi                              |
6F5C9657      | 8BFA                     | mov edi,edx                           |
6F5C9659      | 74 52                    | je game.6F5C96AD                      |
6F5C965B      | 803E 00                  | cmp byte ptr ds:[esi],0               |
6F5C965E      | 74 4D                    | je game.6F5C96AD                      |
6F5C9660      | 85FF                     | test edi,edi                          |
6F5C9662      | 74 49                    | je game.6F5C96AD                      |
6F5C9664      | 8B5C24 10                | mov ebx,dword ptr ss:[esp+10]         |
6F5C9668      | 85DB                     | test ebx,ebx                          |
6F5C966A      | 74 41                    | je game.6F5C96AD                      |
6F5C966C      | 56                       | push esi                              |
6F5C966D      | B9 3CD2AC6F              | mov ecx,game.6FACD23C                 |
6F5C9672      | C607 00                  | mov byte ptr ds:[edi],0               |
6F5C9675      | E8 56F8FFFF              | call game.6F5C8ED0                    |
6F5C967A      | 85C0                     | test eax,eax                          |
6F5C967C      | 74 06                    | je game.6F5C9684                      |
6F5C967E      | 8378 1C 00               | cmp dword ptr ds:[eax+1C],0           |
6F5C9682      | 75 0B                    | jne game.6F5C968F                     |
6F5C9684      | 56                       | push esi                              |
6F5C9685      | B9 14D2AC6F              | mov ecx,game.6FACD214                 |
6F5C968A      | E8 41F8FFFF              | call game.6F5C8ED0                    |
6F5C968F      | 85C0                     | test eax,eax                          |
6F5C9691      | 74 1A                    | je game.6F5C96AD                      |
6F5C9693      | 8B40 1C                  | mov eax,dword ptr ds:[eax+1C]         |
6F5C9696      | 85C0                     | test eax,eax                          |
6F5C9698      | 74 13                    | je game.6F5C96AD                      |
6F5C969A      | 53                       | push ebx                              |
6F5C969B      | 50                       | push eax                              |
6F5C969C      | 57                       | push edi                              |
6F5C969D      | E8 221F1200              | call <JMP.&Ordinal#501>               |
6F5C96A2      | 5F                       | pop edi                               |
6F5C96A3      | 5E                       | pop esi                               |
6F5C96A4      | B8 01000000              | mov eax,1                             |
6F5C96A9      | 5B                       | pop ebx                               |
6F5C96AA      | C2 0400                  | ret 4                                 |
6F5C96AD      | 5F                       | pop edi                               |
6F5C96AE      | 5E                       | pop esi                               |
6F5C96AF      | 33C0                     | xor eax,eax                           |
6F5C96B1      | 5B                       | pop ebx                               |
6F5C96B2      | C2 0400                  | ret 4                                 |
```

### 2.2 错误分发：定位错误弹窗来源

**地址**: `6F5B51D0` (错误分发逻辑)
**偏移**: `game.dll + 5B51D0`
**关键发现**: 程序根据错误代码（Error ID）查表跳转。本案中的错误码导致跳转到 `NETERROR_JOINGAMENOTFOUND`。

```assembly
6F5B51D0      | 81EC 04020000            | sub esp,204                           |
6F5B51D6      | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]       |
6F5B51DB      | 33C4                     | xor eax,esp                           |
6F5B51DD      | 898424 00020000          | mov dword ptr ss:[esp+200],eax        |
6F5B51E4      | 56                       | push esi                              |
6F5B51E5      | 8BF1                     | mov esi,ecx                           |
6F5B51E7      | 57                       | push edi                              |
6F5B51E8      | 6A 00                    | push 0                                |
6F5B51EA      | 8BD6                     | mov edx,esi                           |
6F5B51EC      | B9 78000940              | mov ecx,40090078                      |
6F5B51F1      | C786 70010000 00000000   | mov dword ptr ds:[esi+170],0          |
6F5B51FB      | E8 90A1F8FF              | call game.6F53F390                    |
6F5B5200      | 6A 01                    | push 1                                |
6F5B5202      | 8BD6                     | mov edx,esi                           |
6F5B5204      | B9 78000940              | mov ecx,40090078                      |
6F5B5209      | E8 82A1F8FF              | call game.6F53F390                    |
6F5B520E      | 8B8424 10020000          | mov eax,dword ptr ss:[esp+210]        | <--- 在此处断点 跟踪栈
6F5B5215      | 8B78 14                  | mov edi,dword ptr ds:[eax+14]         | <--- 在此处断点 跟踪栈 到返回地址上面（game.dll + 546996）
6F5B5218      | 8D47 FF                  | lea eax,dword ptr ds:[edi-1]          | <--- 在函数头 game.dll + 5468A0 处断点，等待断点触发 F8 步进并观察[eax+14]处的内存值何时变为C
;================================================================================| <--- 最终在 game.dll + 546941 处找到了返回这个 C 的函数（game.dll + 652710）
;================================================================================| <--- 我们只需要追踪谁在写入栈指针 0019F770
6F5B521B      | 83F8 2A                  | cmp eax,2A                            | <--- 在此处断点可以找到突破口（正常情况为eax=0，异常情况为B，C-1=B）
6F5B521E      | 77 75                    | ja game.6F5B5295                      | <--- 为 B 会跳转到 NETERROR_JOINGAMENOTFOUND 分支
6F5B5220      | 0FB688 34535B6F          | movzx ecx,byte ptr ds:[eax+6F5B5334]  |
6F5B5227      | FF248D 08535B6F          | jmp dword ptr ds:[ecx*4+6F5B5308]     |
6F5B522E      | E8 8D62FFFF              | call game.6F5AB4C0                    |
6F5B5233      | B9 01000000              | mov ecx,1                             |
6F5B5238      | C740 18 00000000         | mov dword ptr ds:[eax+18],0           |
6F5B523F      | C740 14 01000000         | mov dword ptr ds:[eax+14],1           |
6F5B5246      | E8 F59FF8FF              | call game.6F53F240                    |
6F5B524B      | 6A FE                    | push FFFFFFFE                         |
6F5B524D      | 8BCE                     | mov ecx,esi                           |
6F5B524F      | E8 4CCDFDFF              | call game.6F591FA0                    |
6F5B5254      | EB 52                    | jmp game.6F5B52A8                     |
6F5B5256      | B9 6491966F              | mov ecx,game.6F969164                 | <--- "NETERROR_JOINGAMENOTFOUND"
6F5B525B      | EB 3D                    | jmp game.6F5B529A                     | <--- 
6F5B525D      | B9 188B966F              | mov ecx,game.6F968B18                 | <--- "ERROR_ID_GAMEPORTINUSE"
6F5B5262      | EB 36                    | jmp game.6F5B529A                     | <--- 
6F5B5264      | B9 7C05966F              | mov ecx,game.6F96057C                 | <--- "ERROR_ID_AUTOCANCEL"
6F5B5269      | EB 2F                    | jmp game.6F5B529A                     | <--- 
6F5B526B      | B9 381A966F              | mov ecx,game.6F961A38                 | <--- "ERROR_ID_BADPASSWORD"
6F5B5270      | EB 28                    | jmp game.6F5B529A                     | <--- 
6F5B5272      | B9 3491966F              | mov ecx,game.6F969134                 | <--- "ERROR_ID_GAMEUNJOINABLE"
6F5B5277      | EB 21                    | jmp game.6F5B529A                     | <--- 
6F5B5279      | B9 8C61966F              | mov ecx,game.6F96618C                 | <--- "ERROR_ID_GAMEFULL"
6F5B527E      | EB 1A                    | jmp game.6F5B529A                     | <--- 
6F5B5280      | B9 048B966F              | mov ecx,game.6F968B04                 | <--- "ERROR_ID_GAMECLOSED"
6F5B5285      | EB 13                    | jmp game.6F5B529A                     | <--- 
6F5B5287      | B9 8091966F              | mov ecx,game.6F969180                 | <--- "ERROR_ID_BADCDKEY"
6F5B528C      | EB 0C                    | jmp game.6F5B529A                     | <--- 
6F5B528E      | B9 4C91966F              | mov ecx,game.6F96914C                 | <--- "ERROR_ID_REQUESTDENIED"
6F5B5293      | EB 05                    | jmp game.6F5B529A                     | <--- 
6F5B5295      | B9 7461966F              | mov ecx,game.6F966174                 | <--- "NETERROR_JOINGAMEFAILED"
6F5B529A      | 68 00020000              | push 200                              |
6F5B529F      | 8D5424 0C                | lea edx,dword ptr ss:[esp+C]          |
6F5B52A3      | E8 A8430100              | call game.6F5C9650                    |
6F5B52A8      | 83FF 01                  | cmp edi,1                             |
6F5B52AB      | 74 3C                    | je game.6F5B52E9                      |
6F5B52AD      | 6A 03                    | push 3                                |
6F5B52AF      | 8D8E 74010000            | lea ecx,dword ptr ds:[esi+174]        |
6F5B52B5      | E8 96230600              | call game.6F617650                    |
6F5B52BA      | 6A 01                    | push 1                                |
6F5B52BC      | 6A 04                    | push 4                                |
6F5B52BE      | 6A 00                    | push 0                                |
6F5B52C0      | 56                       | push esi                              |
6F5B52C1      | 6A 09                    | push 9                                |
6F5B52C3      | 8D5424 1C                | lea edx,dword ptr ss:[esp+1C]         |
6F5B52C7      | B9 01000000              | mov ecx,1                             |
6F5B52CC      | E8 DF7BFAFF              | call game.6F55CEB0                    |
6F5B52D1      | 8B96 04020000            | mov edx,dword ptr ds:[esi+204]        |
6F5B52D7      | 8996 FC010000            | mov dword ptr ds:[esi+1FC],edx        |
6F5B52DD      | 33D2                     | xor edx,edx                           |
6F5B52DF      | 6A 00                    | push 0                                |
6F5B52E1      | 8D4A 07                  | lea ecx,dword ptr ds:[edx+7]          |
6F5B52E4      | E8 F774F9FF              | call game.6F54C7E0                    |
6F5B52E9      | 8B8C24 08020000          | mov ecx,dword ptr ss:[esp+208]        |
6F5B52F0      | 5F                       | pop edi                               |
6F5B52F1      | 5E                       | pop esi                               |
6F5B52F2      | 33CC                     | xor ecx,esp                           |
6F5B52F4      | B8 01000000              | mov eax,1                             |
6F5B52F9      | E8 5BBD2200              | call game.6F7E1059                    |
6F5B52FE      | 81C4 04020000            | add esp,204                           |
6F5B5304      | C2 0400                  | ret 4                                 |
6F5B5307      | 90                       | nop                                   |
6F5B5308      | 2E:52                    | push edx                              |
6F5B530A      | 5B                       | pop ebx                               |
6F5B530B      | 6F                       | outsd                                 |
6F5B530C      | 64:52                    | push edx                              |
6F5B530E      | 5B                       | pop ebx                               |
6F5B530F      | 6F                       | outsd                                 |
6F5B5310      | 56                       | push esi                              |
6F5B5311      | 52                       | push edx                              |
6F5B5312      | 5B                       | pop ebx                               |
6F5B5313      | 6F                       | outsd                                 |
6F5B5314      | 72 52                    | jb game.6F5B5368                      |
6F5B5316      | 5B                       | pop ebx                               |
6F5B5317      | 6F                       | outsd                                 |
6F5B5318      | 79 52                    | jns game.6F5B536C                     |
6F5B531A      | 5B                       | pop ebx                               |
6F5B531B      | 6F                       | outsd                                 |
6F5B531C      | 8052 5B 6F               | adc byte ptr ds:[edx+5B],6F           |
6F5B5320      | 5D                       | pop ebp                               |
6F5B5321      | 52                       | push edx                              |
6F5B5322      | 5B                       | pop ebx                               |
6F5B5323      | 6F                       | outsd                                 |
6F5B5324      | 6B52 5B 6F               | imul edx,dword ptr ds:[edx+5B],6F     |
6F5B5328      | 8752 5B                  | xchg dword ptr ds:[edx+5B],edx        |
6F5B532B      | 6F                       | outsd                                 |
6F5B532C      | 8E52 5B                  | mov ss,word ptr ds:[edx+5B]           |
6F5B532F      | 6F                       | outsd                                 |
6F5B5330      | 95                       | xchg ebp,eax                          |
6F5B5331      | 52                       | push edx                              |
6F5B5332      | 5B                       | pop ebx                               |
6F5B5333      | 6F                       | outsd                                 |
6F5B5334      | 0001                     | add byte ptr ds:[ecx],al              |
6F5B5336      | 0A0A                     | or cl,byte ptr ds:[edx]               |
6F5B5338      | 0A0A                     | or cl,byte ptr ds:[edx]               |
6F5B533A      | 0203                     | add al,byte ptr ds:[ebx]              |
6F5B533C      | 04 05                    | add al,5                              |
6F5B533E      | 0202                     | add al,byte ptr ds:[edx]              |
6F5B5340      | 06                       | push es                               |
6F5B5341      | 0A0A                     | or cl,byte ptr ds:[edx]               |
6F5B5343      | 0A0A                     | or cl,byte ptr ds:[edx]               |
6F5B5345      | 0A0A                     | or cl,byte ptr ds:[edx]               |
6F5B5347      | 0A0A                     | or cl,byte ptr ds:[edx]               |
6F5B5349      | 0A0A                     | or cl,byte ptr ds:[edx]               |
6F5B534B      | 0A0A                     | or cl,byte ptr ds:[edx]               |
6F5B534D      | 0A07                     | or al,byte ptr ds:[edi]               |
6F5B534F      | 080A                     | or byte ptr ds:[edx],cl               |
6F5B5351      | 0A0A                     | or cl,byte ptr ds:[edx]               |
6F5B5353      | 0A0A                     | or cl,byte ptr ds:[edx]               |
6F5B5355      | 0A0A                     | or cl,byte ptr ds:[edx]               |
6F5B5357      | 0A0A                     | or cl,byte ptr ds:[edx]               |
6F5B5359      | 0A0A                     | or cl,byte ptr ds:[edx]               |
6F5B535B      | 0A0A                     | or cl,byte ptr ds:[edx]               |
6F5B535D      | 0A09                     | or cl,byte ptr ds:[ecx]               |
```

### 核心逻辑流程
1.  **输入计算**：`eax = 原始返回值 (edi) - 1`
2.  **查索引表**：`ecx = Map[eax]` (地址 `02C75334`)
3.  **跳转执行**：`Target = JumpTable[ecx]` (地址 `02C75308`)

---

### a. 跳转目标定义 (Jump Table Targets)
`ecx` 的值决定了最终跳转到哪个错误处理分支。

| 索引 (ecx) | 错误标识符 (Symbol) | 中文含义 |
| :---: | :--- | :--- |
| **00** | `NETERROR_JOINGAMEFAILED` | 加入失败 (未知原因) |
| **01** | `NETERROR_JOINGAMENOTFOUND` | **找不到游戏** |
| **02** | `ERROR_ID_GAMEPORTINUSE` | **游戏端口被占用** |
| **03** | `ERROR_ID_AUTOCANCEL` | 自动取消 |
| **04** | `ERROR_ID_BADPASSWORD` | 密码错误 |
| **05** | `ERROR_ID_GAMEUNJOINABLE` | 无法加入 (通用) |
| **06** | `ERROR_ID_GAMEFULL` | **游戏已满** |
| **07** | `ERROR_ID_GAMECLOSED` | 游戏已关闭/开始 |
| **08** | `ERROR_ID_BADCDKEY` | CDKey 无效/占用 |
| **09** | `ERROR_ID_REQUESTDENIED` | 请求被拒绝 (被踢) |
| **0A** | `NETERROR_JOINGAMEFAILED` | 加入失败 (默认兜底) |

---

### b. 完整映射表 (Index Map)
根据 `eax` 的值查找对应的 `ecx` 索引。

| eax (Hex) | 原始返回值 | 映射索引 (ecx) | 最终错误结果 | 备注 |
| :---: | :---: | :---: | :--- | :--- |
| **00** | 1 | **00** | 加入失败 | 正常应跳转成功逻辑，此处为异常分支 |
| **01** | 2 | **01** | **找不到游戏** | **常见错误: 错误的 EntryKey** |
| **02** | 3 | **0A** | 加入失败 | |
| **03** | 4 | **0A** | 加入失败 | |
| **04** | 5 | **0A** | 加入失败 | |
| **05** | 6 | **0A** | 加入失败 | |
| **06** | 7 | **02** | 端口被占用 | |
| **07** | 8 | **03** | 自动取消 | |
| **08** | 9 | **04** | 密码错误 | |
| **09** | 10 | **05** | 无法加入 | |
| **0A** | 11 | **02** | **端口被占用** | |
| **0B** | 12 | **02** | **端口被占用** | **你最初提到的位置 (B)** |
| **0C** | 13 | **06** | **游戏已满** | **如果读到槽位 12 的偏移结果** |
| ... | ... | ... | ... | ... |
| **2A** | 43 | **09** | 请求被拒绝 | 最大边界值 |

---

### c. 关键值速查

根据你的内存错位分析，如果你的读取函数 `6F4C2D30` 错误地读取了槽位数量：

*   **如果读到了 11 (0xB)**:
    *   `eax = A`
    *   结果：**游戏端口被占用** (`ERROR_ID_GAMEPORTINUSE`)

*   **如果读到了 12 (0xC)**:
    *   `eax = B`
    *   结果：**游戏端口被占用** (`ERROR_ID_GAMEPORTINUSE`)
    *   *(注意：在你的第一版提问中可能混淆了 B 和 C 的映射，根据字节流 `02 02 06`，B 确实映射到 02)*

*   **如果读到了 13 (0xD)**:
    *   `eax = C`
    *   结果：**游戏已满** (`ERROR_ID_GAMEFULL`)

*   **如果读到了 2 (0x2)**:
    *   `eax = 1`
    *   结果：**找不到游戏** (`NETERROR_JOINGAMENOTFOUND`)
    *   *(这是最符合你目前遇到的“找不到游戏”错误的数值)*

通过对上面反汇编的分析，得到生成 错误码 `C` 的函数在 `game.dll + 652710` ，现在我们分析此函数。
	
```assembly
6F652710 | 56                       	 | push esi                              |
6F652711 | 57                       	 | push edi                              |
6F652712 | 8BFA                     	 | mov edi,edx                           |
6F652714 | 57                       	 | push edi                              |
6F652715 | 8BF1                     	 | mov esi,ecx                           | <--- eax = CNetEventGameJoin
6F652717 | E8 1406E7FF              	 | call game.6F4C2D30                    | <--- game.dll + 4C2D30 关键函数：栈指针 0019F770 生成错误码 C
6F65271C | 8D47 04                  	 | lea eax,dword ptr ds:[edi+4]          |
6F65271F | 50                       	 | push eax                              |
6F652720 | 8BCE                     	 | mov ecx,esi                           |
6F652722 | E8 E904E7FF              	 | call game.6F4C2C10                    | <--- game.dll + 4C2C10
6F652727 | 8D57 05                  	 | lea edx,dword ptr ds:[edi+5]          |
6F65272A | 8BCE                     	 | mov ecx,esi                           |
6F65272C | E8 6FFBFFFF              	 | call game.6F6522A0                    |
6F652731 | 8D57 20                  	 | lea edx,dword ptr ds:[edi+20]         | <--- game.dll + 6522A0
6F652734 | 8BCE                     	 | mov ecx,esi                           |
6F652736 | E8 75FAFFFF              	 | call game.6F6521B0                    | <--- game.dll + 6521B0 创建房间的房主名称有关
6F65273B | 81C7 D8000000            	 | add edi,D8                            |
6F652741 | 57                       	 | push edi                              |
6F652742 | 8BCE                     	 | mov ecx,esi                           |
6F652744 | E8 E705E7FF              	 | call game.6F4C2D30                    | <--- game.dll + 4C2D30
6F652749 | 5F                       	 | pop edi                               |
6F65274A | 8BC6                     	 | mov eax,esi                           |
6F65274C | 5E                       	 | pop esi                               |
6F65274D | C3                       	 | ret                                   |
```

现在分析下面的函数，是玩家加入有关的逻辑（CNetEventGameJoin）

```assembly
6F4C2D30 | 56                       	 | push esi                              |
6F4C2D31 | 8BF1                     	 | mov esi,ecx                           |
6F4C2D33 | 8B46 14                  	 | mov eax,dword ptr ds:[esi+14]         |
6F4C2D36 | 6A 04                    	 | push 4                                |
6F4C2D38 | 50                       	 | push eax                              |
6F4C2D39 | E8 F2FDFFFF              	 | call game.6F4C2B30                    | <--- game.dll + 4C2B30
6F4C2D3E | 85C0                     	 | test eax,eax                          |
6F4C2D40 | 74 16                    	 | je game.29C2D58                       |
6F4C2D42 | 8B4E 04                  	 | mov ecx,dword ptr ds:[esi+4]          |
6F4C2D45 | 2B4E 08                  	 | sub ecx,dword ptr ds:[esi+8]          |
6F4C2D48 | 8B56 14                  	 | mov edx,dword ptr ds:[esi+14]         |
6F4C2D4B | 8B0411                   	 | mov eax,dword ptr ds:[ecx+edx]        |
6F4C2D4E | 8B4C24 08                	 | mov ecx,dword ptr ss:[esp+8]          |
6F4C2D52 | 8901                     	 | mov dword ptr ds:[ecx],eax            | <--- 暂停条件(ecx==0019F770)
6F4C2D54 | 8346 14 04               	 | add dword ptr ds:[esi+14],4           |
6F4C2D58 | 8BC6                     	 | mov eax,esi                           |
6F4C2D5A | 5E                       	 | pop esi                               |
6F4C2D5B | C2 0400                  	 | ret 4                                 |
```

经过 x64dbg 观察，错误码 C 的值是由 6F4C2D52（game.dll + 4C2D52）处赋值的，ecx 是一个指针，在不断变化，明显这个函数是循环执行的！

我截取了一段内存观察，读取到了 03A002A8 处的 C 这个值 - 1 刚好是B ,这个值在槽位信息中是什么？

观察别的平台的值此处不是C而是1，所以只要把C变为1，问题就可以解决！

```
03A00068  FFFFFFFD  ýÿÿÿ
03A0006C  69676E45  Engi
03A00070  654E656E  neNe
03A00074  65522374  t#Re
03A00078  6C637963  cycl
03A0007C  45236465  ed#E
03A00080  746E6576  vent
03A00084  74614423  #Dat
03A00088  00000061  a...
03A0008C  00000000  ....
03A00090  00000108  ....
03A00094  6F6D03A0   .mo
03A00098  00000004  ....
03A0009C  00000001  ....
03A000A0  0000000A  ....
03A000A4  694F53AD  .SOi
03A000A8  00746F62  bot.
03A000AC  49030100  ...I
03A000B0  77010107  ...w
03A000B4  0179A901  .©y.
03A000B8  3BEBCFF3  óÏë;
03A000BC  7161CB4D  MËaq
03A000C0  6F455D73  s]Eo
03A000C4  6D6F1977  w.om
03A000C8  5D65616F  oae]
03A000CC  756F0B45  E.ou
03A000D0  37772141  A!w7
03A000D4  3339652F  /e93
03A000D8  33772F65  e/w3
03A000DC  63010979  y..c
03A000E0  0101756F  ou..
03A000E4  A52FF72F  /÷/¥
03A000E8  FF9B370B  .7.ÿ
03A000EC  5753F5E3  ãõSW
03A000F0  F545E74D  MçEõ
03A000F4  1BA72BB9  ¹+§.
03A000F8  005FA5DF  ß¥_.
03A000FC  0000000A  ....
03A00100  00000000  ....
03A00104  00000000  ....
03A00108  00000000  ....
03A0010C  00000000  ....
03A00110  00000000  ....
03A00114  00000000  ....
03A00118  00000000  ....
03A0011C  00000000  ....
03A00120  00000000  ....
03A00124  00000000  ....
03A00128  00000000  ....
03A0012C  00000000  ....
03A00130  00000000  ....
03A00134  00000000  ....
03A00138  00000000  ....
03A0013C  00000000  ....
03A00140  00000000  ....
03A00144  00000000  ....
03A00148  00000000  ....
03A0014C  00000000  ....
03A00150  00000000  ....
03A00154  00000000  ....
03A00158  00000000  ....
03A0015C  00000000  ....
03A00160  00000000  ....
03A00164  00000000  ....
03A00168  00000000  ....
03A0016C  00000000  ....
03A00170  00000000  ....
03A00174  00000000  ....
03A00178  00000000  ....
03A0017C  00000000  ....
03A00180  00000000  ....
03A00184  00000000  ....
03A00188  00000000  ....
03A0018C  00000000  ....
03A00190  00000000  ....
03A00194  00000000  ....
03A00198  00000108  ....
03A0019C  6F6D03A0   .mo
03A001A0  0000000A  ....
03A001A4  6D646100  .adm
03A001A8  01006E69  in..
03A001AC  746F6200  .bot
03A001B0  03010000  ....
03A001B4  01010749  I...
03A001B8  79A90177  w.©y
03A001BC  EBCFF301  .óÏë
03A001C0  61CB4D3B  ;MËa
03A001C4  455D7371  qs]E
03A001C8  6F19776F  ow.o
03A001CC  65616F6D  moae
03A001D0  6F0B455D  ]E.o
03A001D4  77214175  uA!w
03A001D8  39652F37  7/e9
03A001DC  772F6533  3e/w
03A001E0  01097933  3y..
03A001E4  01756F63  cou.
03A001E8  2FF72F01  ./÷/
03A001EC  9B370BA5  ¥.7.
03A001F0  53F5E3FF  ÿãõS
03A001F4  45E74D57  WMçE
03A001F8  A72BB9F5  õ¹+§
03A001FC  5FA5DF1B  .ß¥_
03A00200  00000A00  ....
03A00204  00000000  ....
03A00208  00000500  ....
03A0020C  00000000  ....
03A00210  00000000  ....
03A00214  00000000  ....
03A00218  00000000  ....
03A0021C  00000000  ....
03A00220  00000000  ....
03A00224  00000000  ....
03A00228  00000000  ....
03A0022C  00000000  ....
03A00230  00000000  ....
03A00234  00000000  ....
03A00238  00000000  ....
03A0023C  00000000  ....
03A00240  00000000  ....
03A00244  00000000  ....
03A00248  00000000  ....
03A0024C  00000000  ....
03A00250  00000000  ....
03A00254  00000000  ....
03A00258  00000000  ....
03A0025C  00000000  ....
03A00260  00000000  ....
03A00264  00000000  ....
03A00268  00000000  ....
03A0026C  00000000  ....
03A00270  00000000  ....
03A00274  00000000  ....
03A00278  00000000  ....
03A0027C  00000000  ....
03A00280  00000000  ....
03A00284  00000000  ....
03A00288  00000000  ....
03A0028C  00000000  ....
03A00290  00000000  ....
03A00294  00000000  ....
03A00298  00000000  ....
03A0029C  00000000  ....
03A002A0  00000108  ....
03A002A4  6F6D03A0   .mo
03A002A8  0000000C  .... <--- 错误码来源（C-1=B）
03A002AC  6D646100  .adm
03A002B0  01006E69  in..
03A002B4  746F6200  .bot
03A002B8  03010000  ....
03A002BC  01010749  I...
03A002C0  79A90177  w.©y
03A002C4  EBCFF301  .óÏë
03A002C8  61CB4D3B  ;MËa
03A002CC  455D7371  qs]E
03A002D0  6F19776F  ow.o
03A002D4  65616F6D  moae
03A002D8  6F0B455D  ]E.o
03A002DC  77214175  uA!w
03A002E0  39652F37  7/e9
03A002E4  772F6533  3e/w
03A002E8  01097933  3y..
03A002EC  01756F63  cou.
03A002F0  2FF72F01  ./÷/
03A002F4  9B370BA5  ¥.7.
03A002F8  53F5E3FF  ÿãõS
03A002FC  45E74D57  WMçE
03A00300  A72BB9F5  õ¹+§
03A00304  5FA5DF1B  .ß¥_
03A00308  00000A00  ....
03A0030C  00000000  ....
03A00310  00000800  ....
03A00314  00000000  ....
03A00318  00000000  ....

```

### 2.3 逻辑层：追溯加入流程
通过分析点击“加入”按钮后的调用栈，找到发起连接的入口。

**地址**: `6F5BABF0` (UI 事件处理)
**偏移**: `game.dll + 5BABF0`
这里处理了用户的键盘输入、创建游戏、刷新列表以及**加入游戏**的请求。

```assembly
6F5BABF0      | 8B4424 04                | mov eax,dword ptr ss:[esp+4]          |
6F5BABF4      | 56                       | push esi                              |
6F5BABF5      | 8BF1                     | mov esi,ecx                           |
6F5BABF7      | 8B48 08                  | mov ecx,dword ptr ds:[eax+8]          |
6F5BABFA      | 83F9 17                  | cmp ecx,17                            |
6F5BABFD      | 0F87 B2010000            | ja game.6F5BADB5                      |
6F5BAC03      | FF248D BCAD5B6F          | jmp dword ptr ds:[ecx*4+6F5BADBC]     | <--- 处理分支
6F5BAC0A      | 8BCE                     | mov ecx,esi                           |
6F5BAC0C      | E8 8F70FDFF              | call game.6F591CA0                    | <--- 选择列表
6F5BAC11      | 5E                       | pop esi                               |
6F5BAC12      | C2 0400                  | ret 4                                 |
6F5BAC15      | 8BCE                     | mov ecx,esi                           | <--- 键盘输入
6F5BAC17      | E8 A472FDFF              | call game.6F591EC0                    |
6F5BAC1C      | 5E                       | pop esi                               |
6F5BAC1D      | C2 0400                  | ret 4                                 |
6F5BAC20      | 8BCE                     | mov ecx,esi                           | <--- 创建游戏
6F5BAC22      | E8 19D0FDFF              | call game.6F597C40                    |
6F5BAC27      | 5E                       | pop esi                               |
6F5BAC28      | C2 0400                  | ret 4                                 |
6F5BAC2B      | 8BCE                     | mov ecx,esi                           | <--- 装载游戏
6F5BAC2D      | E8 FECFFDFF              | call game.6F597C30                    |
6F5BAC32      | 5E                       | pop esi                               |
6F5BAC33      | C2 0400                  | ret 4                                 |
6F5BAC36      | 6A 00                    | push 0                                | <--- 加入游戏
6F5BAC38      | 8BCE                     | mov ecx,esi                           |
6F5BAC3A      | E8 71A3FFFF              | call game.6F5B4FB0                    |
6F5BAC3F      | 5E                       | pop esi                               |
6F5BAC40      | C2 0400                  | ret 4                                 |
6F5BAC43      | 8BCE                     | mov ecx,esi                           | <--- 返回大厅
6F5BAC45      | E8 06D0FDFF              | call game.6F597C50                    |
6F5BAC4A      | 5E                       | pop esi                               |
6F5BAC4B      | C2 0400                  | ret 4                                 |
6F5BAC4E      | 8BCE                     | mov ecx,esi                           |
6F5BAC50      | E8 9B40FEFF              | call game.6F59ECF0                    |
6F5BAC55      | 5E                       | pop esi                               |
6F5BAC56      | C2 0400                  | ret 4                                 |
6F5BAC59      | 8BCE                     | mov ecx,esi                           | <--- 确定关闭对话框
6F5BAC5B      | E8 30B0FBFF              | call game.6F575C90                    |
6F5BAC60      | 5E                       | pop esi                               |
6F5BAC61      | C2 0400                  | ret 4                                 |
6F5BAC64      | 8BCE                     | mov ecx,esi                           | <--- 设置过滤
6F5BAC66      | E8 35B5FCFF              | call game.6F5861A0                    |
6F5BAC6B      | 5E                       | pop esi                               |
6F5BAC6C      | C2 0400                  | ret 4                                 |
6F5BAC6F <gam | 8BCE                     | mov ecx,esi                           | <--- 刷新列表开始
6F5BAC71      | E8 0A70FDFF              | call game.6F591C80                    |
6F5BAC76      | 5E                       | pop esi                               |
6F5BAC77      | C2 0400                  | ret 4                                 |
6F5BAC7A      | 8BCE                     | mov ecx,esi                           | <--- 设置过滤确认
6F5BAC7C      | E8 DF6FFDFF              | call game.6F591C60                    |
6F5BAC81      | 5E                       | pop esi                               |
6F5BAC82      | C2 0400                  | ret 4                                 |
6F5BAC85      | 8BCE                     | mov ecx,esi                           | <--- 设置过滤取消
6F5BAC87      | E8 B4B4FCFF              | call game.6F586140                    |
6F5BAC8C      | 5E                       | pop esi                               |
6F5BAC8D      | C2 0400                  | ret 4                                 |
6F5BAC90 <gam | 8BCE                     | mov ecx,esi                           | <--- 搜索游戏名
6F5BAC92      | E8 B9AFFBFF              | call game.6F575C50                    |
6F5BAC97      | 5E                       | pop esi                               |
6F5BAC98      | C2 0400                  | ret 4                                 |
6F5BAC9B      | 50                       | push eax                              |
6F5BAC9C      | 8BCE                     | mov ecx,esi                           |
6F5BAC9E      | E8 7DE8FFFF              | call game.6F5B9520                    |
6F5BACA3      | 5E                       | pop esi                               |
6F5BACA4      | C2 0400                  | ret 4                                 |
6F5BACA7      | 50                       | push eax                              | <--- 获取列表结束 循环填入数据
6F5BACA8      | 8BCE                     | mov ecx,esi                           |
6F5BACAA      | E8 01B5FCFF              | call game.6F5861B0                    |
6F5BACAF      | 5E                       | pop esi                               |
6F5BACB0      | C2 0400                  | ret 4                                 |
6F5BACB3      | 50                       | push eax                              | <--- 刷新列表完毕
6F5BACB4      | 8BCE                     | mov ecx,esi                           |
6F5BACB6      | E8 85CEFDFF              | call game.6F597B40                    |
6F5BACBB      | 5E                       | pop esi                               |
6F5BACBC      | C2 0400                  | ret 4                                 |
6F5BACBF      | 50                       | push eax                              |
6F5BACC0      | 8BCE                     | mov ecx,esi                           |
6F5BACC2      | E8 3972FDFF              | call game.6F591F00                    |
6F5BACC7      | 5E                       | pop esi                               |
6F5BACC8      | C2 0400                  | ret 4                                 |
; ====================================== E =======================================
6F5BACCB      | 50                       | push eax                              | <--- 弹出对话框 游戏找不到
6F5BACCC      | 8BCE                     | mov ecx,esi                           |
6F5BACCE      | E8 FDA4FFFF              | call game.6F5B51D0                    |
6F5BACD3      | 5E                       | pop esi                               |
6F5BACD4      | C2 0400                  | ret 4                                 |
6F5BACD7      | 50                       | push eax                              | <--- 搜索游戏不存在
6F5BACD8      | 8BCE                     | mov ecx,esi                           |
6F5BACDA      | E8 61A0FFFF              | call game.6F5B4D40                    |
6F5BACDF      | 5E                       | pop esi                               |
6F5BACE0      | C2 0400                  | ret 4                                 |
6F5BACE3      | 8B06                     | mov eax,dword ptr ds:[esi]            | <--- 进入自定义游戏
6F5BACE5      | 8B90 D4000000            | mov edx,dword ptr ds:[eax+D4]         |
6F5BACEB      | 8BCE                     | mov ecx,esi                           |
6F5BACED      | FFD2                     | call edx                              |
6F5BACEF      | 6A 16                    | push 16                               |
6F5BACF1      | 56                       | push esi                              |
6F5BACF2      | 6A 00                    | push 0                                |
6F5BACF4      | BA 08A2956F              | mov edx,game.6F95A208                 |
6F5BACF9      | B9 00A2956F              | mov ecx,game.6F95A200                 |
6F5BACFE      | E8 2DF1A4FF              | call game.6F009E30                    |
6F5BAD03      | 51                       | push ecx                              |
6F5BAD04      | D91C24                   | fstp dword ptr ss:[esp]               |
6F5BAD07      | 56                       | push esi                              |
6F5BAD08      | 8D8E B8010000            | lea ecx,dword ptr ds:[esi+1B8]        |
6F5BAD0E      | E8 BD9AD5FF              | call game.6F3147D0                    |
6F5BAD13      | B8 01000000              | mov eax,1                             |
6F5BAD18      | 5E                       | pop esi                               |
6F5BAD19      | C2 0400                  | ret 4                                 |
6F5BAD1C      | 6A 01                    | push 1                                | <--- 获取列表开始 发起网络请求
6F5BAD1E      | 8BCE                     | mov ecx,esi                           |
6F5BAD20      | E8 4B6EFDFF              | call game.6F591B70                    |
6F5BAD25      | 8B8E 04020000            | mov ecx,dword ptr ds:[esi+204]        |
6F5BAD2B      | 6A 00                    | push 0                                |
6F5BAD2D      | 6A 00                    | push 0                                |
6F5BAD2F      | 6A 01                    | push 1                                |
6F5BAD31      | E8 DA010400              | call game.6F5FAF10                    |
6F5BAD36      | 830D 18A4A96F 01         | or dword ptr ds:[6FA9A418],1          |
6F5BAD3D      | B8 01000000              | mov eax,1                             |
6F5BAD42      | 5E                       | pop esi                               |
6F5BAD43      | C2 0400                  | ret 4                                 |
6F5BAD46      | 8B06                     | mov eax,dword ptr ds:[esi]            | <--- 画面切换开始
6F5BAD48      | 8B90 D0000000            | mov edx,dword ptr ds:[eax+D0]         |
6F5BAD4E      | 8BCE                     | mov ecx,esi                           |
6F5BAD50      | FFD2                     | call edx                              |
6F5BAD52      | 8BCE                     | mov ecx,esi                           |
6F5BAD54      | E8 A7CFFDFF              | call game.6F597D00                    |
6F5BAD59      | B8 01000000              | mov eax,1                             |
6F5BAD5E      | 5E                       | pop esi                               |
6F5BAD5F      | C2 0400                  | ret 4                                 |
6F5BAD62      | 830D 18A4A96F 01         | or dword ptr ds:[6FA9A418],1          | <--- 画面切换完毕
6F5BAD69      | 8B86 44020000            | mov eax,dword ptr ds:[esi+244]        |
6F5BAD6F      | 83F8 FE                  | cmp eax,FFFFFFFE                      |
6F5BAD72      | 8B8E 68010000            | mov ecx,dword ptr ds:[esi+168]        |
6F5BAD78      | 74 2B                    | je game.6F5BADA5                      |
6F5BAD7A      | 83F8 FF                  | cmp eax,FFFFFFFF                      |
6F5BAD7D      | 74 16                    | je game.6F5BAD95                      |
6F5BAD7F      | 8B0485 18FB956F          | mov eax,dword ptr ds:[eax*4+6F95FB18] |
6F5BAD86      | 50                       | push eax                              |
6F5BAD87      | E8 9482FBFF              | call game.6F573020                    |
6F5BAD8C      | B8 01000000              | mov eax,1                             |
6F5BAD91      | 5E                       | pop esi                               |
6F5BAD92      | C2 0400                  | ret 4                                 |
6F5BAD95      | 6A 02                    | push 2                                |
6F5BAD97      | E8 4483FEFF              | call game.6F5A30E0                    |
6F5BAD9C      | B8 01000000              | mov eax,1                             |
6F5BADA1      | 5E                       | pop esi                               |
6F5BADA2      | C2 0400                  | ret 4                                 |
6F5BADA5      | 6A 0B                    | push B                                |
6F5BADA7      | E8 3483FEFF              | call game.6F5A30E0                    |
6F5BADAC      | B8 01000000              | mov eax,1                             |
6F5BADB1      | 5E                       | pop esi                               |
6F5BADB2      | C2 0400                  | ret 4                                 |
6F5BADB5      | 33C0                     | xor eax,eax                           |
6F5BADB7      | 5E                       | pop esi                               |
6F5BADB8      | C2 0400                  | ret 4                                 |
6F5BADBB      | 90                       | nop                                   |
6F5BADBC      | 0AAC5B 6F15AC5B          | or ch,byte ptr ds:[ebx+ebx*2+5BAC156F]|
6F5BADC3      | 6F                       | outsd                                 |
6F5BADC4      | 20AC5B 6F2BAC5B          | and byte ptr ds:[ebx+ebx*2+5BAC2B6F],c|
6F5BADCB      | 6F                       | outsd                                 |
6F5BADCC      | 36:AC                    | lodsb                                 |
6F5BADCE      | 5B                       | pop ebx                               |
6F5BADCF      | 6F                       | outsd                                 |
6F5BADD0      | 64:AC                    | lodsb                                 |
6F5BADD2      | 5B                       | pop ebx                               |
6F5BADD3      | 6F                       | outsd                                 |
6F5BADD4 <gam | 6F                       | outsd                                 |
6F5BADD5      | AC                       | lodsb                                 |
6F5BADD6      | 5B                       | pop ebx                               |
6F5BADD7      | 6F                       | outsd                                 |
6F5BADD8      | 43                       | inc ebx                               |
6F5BADD9      | AC                       | lodsb                                 |
6F5BADDA      | 5B                       | pop ebx                               |
6F5BADDB      | 6F                       | outsd                                 |
6F5BADDC      | 4E                       | dec esi                               |
6F5BADDD      | AC                       | lodsb                                 |
6F5BADDE      | 5B                       | pop ebx                               |
6F5BADDF      | 6F                       | outsd                                 |
6F5BADE0      | 59                       | pop ecx                               |
6F5BADE1      | AC                       | lodsb                                 |
6F5BADE2      | 5B                       | pop ebx                               |
6F5BADE3      | 6F                       | outsd                                 |
6F5BADE4      | 7A AC                    | jp game.6F5BAD92                      |
6F5BADE6      | 5B                       | pop ebx                               |
6F5BADE7      | 6F                       | outsd                                 |
6F5BADE8      | 85AC5B 6F90AC5B          | test dword ptr ds:[ebx+ebx*2+5BAC906F]|
6F5BADEF      | 6F                       | outsd                                 |
6F5BADF0      | 9B                       | fwait                                 |
6F5BADF1      | AC                       | lodsb                                 |
6F5BADF2      | 5B                       | pop ebx                               |
6F5BADF3      | 6F                       | outsd                                 |
6F5BADF4      | CB                       | ret far                               |
6F5BADF5      | AC                       | lodsb                                 |
6F5BADF6      | 5B                       | pop ebx                               |
6F5BADF7      | 6F                       | outsd                                 |
6F5BADF8      | B5 AD                    | mov ch,AD                             |
6F5BADFA      | 5B                       | pop ebx                               |
6F5BADFB      | 6F                       | outsd                                 |
6F5BADFC      | A7                       | cmpsd                                 |
6F5BADFD      | AC                       | lodsb                                 |
6F5BADFE      | 5B                       | pop ebx                               |
6F5BADFF      | 6F                       | outsd                                 |
6F5BAE00      | B3 AC                    | mov bl,AC                             |
6F5BAE02      | 5B                       | pop ebx                               |
6F5BAE03      | 6F                       | outsd                                 |
6F5BAE04      | BF AC5B6FD7              | mov edi,D76F5BAC                      |
6F5BAE09      | AC                       | lodsb                                 |
6F5BAE0A      | 5B                       | pop ebx                               |
6F5BAE0B      | 6F                       | outsd                                 |
6F5BAE0C      | E3 AC                    | jecxz game.6F5BADBA                   |
6F5BAE0E      | 5B                       | pop ebx                               |
6F5BAE0F      | 6F                       | outsd                                 |
6F5BAE10      | 62AD 5B6F1CAD            | bound ebp,qword ptr ss:[ebp-52E390A5] |
6F5BAE16      | 5B                       | pop ebx                               |
6F5BAE17      | 6F                       | outsd                                 |
6F5BAE18      | 46                       | inc esi                               |
6F5BAE19      | AD                       | lodsd                                 |
6F5BAE1A      | 5B                       | pop ebx                               |
6F5BAE1B      | 6F                       | outsd                                 |
```

下面的函数 02B7A6FB 在调用上面子函数，此父函数处于事件循环中，所以不在这里断点。
**地址**: `6F5BABF0` (UI 事件处理)
**偏移**: `game.dll + 62A5D0`
**断点**: `game.dll + 62A6FB`
**条件**: `edx == game.dll + 5BABF0`

```assembly
6F62A5D0      | 6A FF                    | push FFFFFFFF                         |
6F62A5D2      | 68 880A846F              | push game.6F840A88                    |
6F62A5D7      | 64:A1 00000000           | mov eax,dword ptr fs:[0]              |
6F62A5DD      | 50                       | push eax                              |
6F62A5DE      | 83EC 24                  | sub esp,24                            |
6F62A5E1      | 53                       | push ebx                              |
6F62A5E2      | 55                       | push ebp                              |
6F62A5E3      | 56                       | push esi                              |
6F62A5E4      | 57                       | push edi                              |
6F62A5E5      | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]       |
6F62A5EA      | 33C4                     | xor eax,esp                           |
6F62A5EC      | 50                       | push eax                              |
6F62A5ED      | 8D4424 38                | lea eax,dword ptr ss:[esp+38]         |
6F62A5F1      | 64:A3 00000000           | mov dword ptr fs:[0],eax              |
6F62A5F7      | 8BD9                     | mov ebx,ecx                           |
6F62A5F9      | 33D2                     | xor edx,edx                           |
6F62A5FB      | 895424 14                | mov dword ptr ss:[esp+14],edx         |
6F62A5FF      | E8 2CF1FFFF              | call game.6F629730                    |
6F62A604      | 0FB743 02                | movzx eax,word ptr ds:[ebx+2]         |
6F62A608      | 8B4B 04                  | mov ecx,dword ptr ds:[ebx+4]          |
6F62A60B      | 894424 18                | mov dword ptr ss:[esp+18],eax         |
6F62A60F      | 0FB643 01                | movzx eax,byte ptr ds:[ebx+1]         |
6F62A613      | 83E8 01                  | sub eax,1                             |
6F62A616      | 234424 48                | and eax,dword ptr ss:[esp+48]         |
6F62A61A      | 895424 28                | mov dword ptr ss:[esp+28],edx         |
6F62A61E      | 8D3C81                   | lea edi,dword ptr ds:[ecx+eax*4]      |
6F62A621      | 895424 30                | mov dword ptr ss:[esp+30],edx         |
6F62A625      | 8B07                     | mov eax,dword ptr ds:[edi]            |
6F62A627      | 3BC2                     | cmp eax,edx                           |
6F62A629      | 895424 40                | mov dword ptr ss:[esp+40],edx         |
6F62A62D      | 74 10                    | je game.6F62A63F                      |
6F62A62F      | 8B00                     | mov eax,dword ptr ds:[eax]            |
6F62A631      | 894424 28                | mov dword ptr ss:[esp+28],eax         |
6F62A635      | 8B0F                     | mov ecx,dword ptr ds:[edi]            |
6F62A637      | 8D4424 28                | lea eax,dword ptr ss:[esp+28]         |
6F62A63B      | 8901                     | mov dword ptr ds:[ecx],eax            |
6F62A63D      | EB 08                    | jmp game.6F62A647                     |
6F62A63F      | 8D4C24 28                | lea ecx,dword ptr ss:[esp+28]         |
6F62A643      | 894C24 28                | mov dword ptr ss:[esp+28],ecx         |
6F62A647      | 8D4424 28                | lea eax,dword ptr ss:[esp+28]         |
6F62A64B      | 8907                     | mov dword ptr ds:[edi],eax            |
6F62A64D      | 8B7424 28                | mov esi,dword ptr ss:[esp+28]         |
6F62A651      | 3BF0                     | cmp esi,eax                           |
6F62A653      | 8BC8                     | mov ecx,eax                           |
6F62A655      | 897C24 1C                | mov dword ptr ss:[esp+1C],edi         |
6F62A659      | 894C24 20                | mov dword ptr ss:[esp+20],ecx         |
6F62A65D      | 897424 24                | mov dword ptr ss:[esp+24],esi         |
6F62A661      | 8BEE                     | mov ebp,esi                           |
6F62A663      | 0F84 C4000000            | je game.6F62A72D                      |
6F62A669      | EB 07                    | jmp game.6F62A672                     |
6F62A66B      | EB 03                    | jmp game.6F62A670                     |
6F62A66D      | 8D49 00                  | lea ecx,dword ptr ds:[ecx]            |
6F62A670      | 33D2                     | xor edx,edx                           |
6F62A672      | 8B4D 08                  | mov ecx,dword ptr ss:[ebp+8]          |
6F62A675      | 3BCA                     | cmp ecx,edx                           |
6F62A677      | 75 69                    | jne game.6F62A6E2                     |
6F62A679      | 803B 01                  | cmp byte ptr ds:[ebx],1               |
6F62A67C      | 0F85 87000000            | jne game.6F62A709                     |
6F62A682      | 85F6                     | test esi,esi                          |
6F62A684      | 8BD6                     | mov edx,esi                           |
6F62A686      | 74 36                    | je game.6F62A6BE                      |
6F62A688      | 8B4C24 20                | mov ecx,dword ptr ss:[esp+20]         |
6F62A68C      | 8B06                     | mov eax,dword ptr ds:[esi]            |
6F62A68E      | 8901                     | mov dword ptr ds:[ecx],eax            |
6F62A690      | 8B0F                     | mov ecx,dword ptr ds:[edi]            |
6F62A692      | 3BF1                     | cmp esi,ecx                           |
6F62A694      | 75 20                    | jne game.6F62A6B6                     |
6F62A696      | 8B4424 20                | mov eax,dword ptr ss:[esp+20]         |
6F62A69A      | 3BC1                     | cmp eax,ecx                           |
6F62A69C      | 75 12                    | jne game.6F62A6B0                     |
6F62A69E      | C74424 20 00000000       | mov dword ptr ss:[esp+20],0           |
6F62A6A6      | C707 00000000            | mov dword ptr ds:[edi],0              |
6F62A6AC      | 33F6                     | xor esi,esi                           |
6F62A6AE      | EB 08                    | jmp game.6F62A6B8                     |
6F62A6B0      | 8907                     | mov dword ptr ds:[edi],eax            |
6F62A6B2      | 33F6                     | xor esi,esi                           |
6F62A6B4      | EB 02                    | jmp game.6F62A6B8                     |
6F62A6B6      | 8BF0                     | mov esi,eax                           |
6F62A6B8      | C702 00000000            | mov dword ptr ds:[edx],0              |
6F62A6BE      | 8B4D 08                  | mov ecx,dword ptr ss:[ebp+8]          |
6F62A6C1      | 85C9                     | test ecx,ecx                          |
6F62A6C3      | 74 0C                    | je game.6F62A6D1                      |
6F62A6C5      | 8341 04 FF               | add dword ptr ds:[ecx+4],FFFFFFFF     |
6F62A6C9      | 75 06                    | jne game.6F62A6D1                     |
6F62A6CB      | 8B11                     | mov edx,dword ptr ds:[ecx]            |
6F62A6CD      | 8B02                     | mov eax,dword ptr ds:[edx]            |
6F62A6CF      | FFD0                     | call eax                              |
6F62A6D1      | 6A 00                    | push 0                                |
6F62A6D3      | 6A 00                    | push 0                                |
6F62A6D5      | 55                       | push ebp                              |
6F62A6D6      | B9 ECE9AC6F              | mov ecx,game.6FACE9EC                 |
6F62A6DB      | E8 7074E9FF              | call game.6F4C1B50                    |
6F62A6E0      | EB 39                    | jmp game.6F62A71B                     |
6F62A6E2      | 8B5424 48                | mov edx,dword ptr ss:[esp+48]         |
6F62A6E6      | 3955 04                  | cmp dword ptr ss:[ebp+4],edx          |
6F62A6E9      | 75 1E                    | jne game.6F62A709                     |
6F62A6EB      | 8B45 0C                  | mov eax,dword ptr ss:[ebp+C]          | <--- 读取错误码E
6F62A6EE      | 8B5424 4C                | mov edx,dword ptr ss:[esp+4C]         |
6F62A6F2      | 8942 08                  | mov dword ptr ds:[edx+8],eax          |
6F62A6F5      | 8B01                     | mov eax,dword ptr ds:[ecx]            | <--- 取出事件对象
6F62A6F7      | 52                       | push edx                              | <--- 压入参数
6F62A6F8      | 8B50 0C                  | mov edx,dword ptr ds:[eax+C]          | <--- 取出回调函数地址
6F62A6FB      | FFD2                     | call edx                              | <--- 执行回调
6F62A6FD      | 85C0                     | test eax,eax                          |
6F62A6FF      | 74 08                    | je game.6F62A709                      |
6F62A701      | C74424 14 01000000       | mov dword ptr ss:[esp+14],1           |
6F62A709      | 85F6                     | test esi,esi                          |
6F62A70B      | 74 0E                    | je game.6F62A71B                      |
6F62A70D      | 3B37                     | cmp esi,dword ptr ds:[edi]            |
6F62A70F      | 897424 20                | mov dword ptr ss:[esp+20],esi         |
6F62A713      | 75 04                    | jne game.6F62A719                     |
6F62A715      | 33F6                     | xor esi,esi                           |
6F62A717      | EB 02                    | jmp game.6F62A71B                     |
6F62A719      | 8B36                     | mov esi,dword ptr ds:[esi]            |
6F62A71B      | 8D4424 28                | lea eax,dword ptr ss:[esp+28]         |
6F62A71F      | 3BF0                     | cmp esi,eax                           |
6F62A721      | 8BEE                     | mov ebp,esi                           |
6F62A723      | 0F85 47FFFFFF            | jne game.6F62A670                     |
6F62A729      | 897424 24                | mov dword ptr ss:[esp+24],esi         |
6F62A72D      | 8D4C24 1C                | lea ecx,dword ptr ss:[esp+1C]         |
6F62A731      | 51                       | push ecx                              |
6F62A732      | 8BCF                     | mov ecx,edi                           |
6F62A734      | E8 F7F0FFFF              | call game.6F629830                    |
6F62A739      | 8BCB                     | mov ecx,ebx                           |
6F62A73B      | E8 00F0FFFF              | call game.6F629740                    |
6F62A740      | 803B 00                  | cmp byte ptr ds:[ebx],0               |
6F62A743      | 75 10                    | jne game.6F62A755                     |
6F62A745      | 66:8B5424 18             | mov dx,word ptr ss:[esp+18]           |
6F62A74A      | 66:3953 02               | cmp word ptr ds:[ebx+2],dx            |
6F62A74E      | 76 05                    | jbe game.6F62A755                     |
6F62A750      | E8 1BFDFFFF              | call game.6F62A470                    |
6F62A755      | 8B4424 30                | mov eax,dword ptr ss:[esp+30]         |
6F62A759      | 85C0                     | test eax,eax                          |
6F62A75B      | C74424 40 FFFFFFFF       | mov dword ptr ss:[esp+40],FFFFFFFF    |
6F62A763      | 74 14                    | je game.6F62A779                      |
6F62A765      | 8340 04 FF               | add dword ptr ds:[eax+4],FFFFFFFF     |
6F62A769      | 8BC8                     | mov ecx,eax                           |
6F62A76B      | 83C0 04                  | add eax,4                             |
6F62A76E      | 8338 00                  | cmp dword ptr ds:[eax],0              |
6F62A771      | 75 06                    | jne game.6F62A779                     |
6F62A773      | 8B01                     | mov eax,dword ptr ds:[ecx]            |
6F62A775      | 8B10                     | mov edx,dword ptr ds:[eax]            |
6F62A777      | FFD2                     | call edx                              |
6F62A779      | 8B4424 14                | mov eax,dword ptr ss:[esp+14]         |
6F62A77D      | 8B4C24 38                | mov ecx,dword ptr ss:[esp+38]         |
6F62A781      | 64:890D 00000000         | mov dword ptr fs:[0],ecx              |
6F62A788      | 59                       | pop ecx                               |
6F62A789      | 5F                       | pop edi                               |
6F62A78A      | 5E                       | pop esi                               |
6F62A78B      | 5D                       | pop ebp                               |
6F62A78C      | 5B                       | pop ebx                               |
6F62A78D      | 83C4 30                  | add esp,30                            |
6F62A790      | C2 0800                  | ret 8                                 |
```

- 技巧：在 `6F62A820` 处硬件断点写入，定位到下面的反汇编：
- 地址：`6F62A820` 
- 偏移: `game.dll + 62A820`
- 断点：`game.dll + 62A954`

```assembly
6F62A820      | 83EC 10                  | sub esp,10                            |
6F62A823      | 53                       | push ebx                              |
6F62A824      | 55                       | push ebp                              |
6F62A825      | 8BE9                     | mov ebp,ecx                           |
6F62A827      | 0FB645 01                | movzx eax,byte ptr ss:[ebp+1]         |
6F62A82B      | 8B4D 04                  | mov ecx,dword ptr ss:[ebp+4]          |
6F62A82E      | 83E8 01                  | sub eax,1                             |
6F62A831      | 234424 1C                | and eax,dword ptr ss:[esp+1C]         |
6F62A835      | 56                       | push esi                              |
6F62A836      | 8D1C81                   | lea ebx,dword ptr ds:[ecx+eax*4]      |
6F62A839      | 33C0                     | xor eax,eax                           |
6F62A83B      | 57                       | push edi                              |
6F62A83C      | 8B3B                     | mov edi,dword ptr ds:[ebx]            |
6F62A83E      | 3BF8                     | cmp edi,eax                           |
6F62A840      | 896C24 14                | mov dword ptr ss:[esp+14],ebp         |
6F62A844      | 894424 1C                | mov dword ptr ss:[esp+1C],eax         |
6F62A848      | 894424 18                | mov dword ptr ss:[esp+18],eax         |
6F62A84C      | 74 04                    | je game.6F62A852                      |
6F62A84E      | 8B37                     | mov esi,dword ptr ds:[edi]            |
6F62A850      | EB 02                    | jmp game.6F62A854                     |
6F62A852      | 33F6                     | xor esi,esi                           |
6F62A854      | 3BF0                     | cmp esi,eax                           |
6F62A856      | 897424 10                | mov dword ptr ss:[esp+10],esi         |
6F62A85A      | 0F84 96000000            | je game.6F62A8F6                      |
6F62A860      | 8B46 08                  | mov eax,dword ptr ds:[esi+8]          |
6F62A863      | 85C0                     | test eax,eax                          |
6F62A865      | 8D6E 08                  | lea ebp,dword ptr ds:[esi+8]          |
6F62A868      | 75 56                    | jne game.6F62A8C0                     |
6F62A86A      | 8B5424 14                | mov edx,dword ptr ss:[esp+14]         |
6F62A86E      | 3802                     | cmp byte ptr ds:[edx],al              |
6F62A870      | 75 68                    | jne game.6F62A8DA                     |
6F62A872      | 8B06                     | mov eax,dword ptr ds:[esi]            |
6F62A874      | 8907                     | mov dword ptr ds:[edi],eax            |
6F62A876      | 8B0B                     | mov ecx,dword ptr ds:[ebx]            |
6F62A878      | 3BF1                     | cmp esi,ecx                           |
6F62A87A      | 8BD6                     | mov edx,esi                           |
6F62A87C      | 75 12                    | jne game.6F62A890                     |
6F62A87E      | 3BF9                     | cmp edi,ecx                           |
6F62A880      | 75 08                    | jne game.6F62A88A                     |
6F62A882      | 33FF                     | xor edi,edi                           |
6F62A884      | 893B                     | mov dword ptr ds:[ebx],edi            |
6F62A886      | 33F6                     | xor esi,esi                           |
6F62A888      | EB 08                    | jmp game.6F62A892                     |
6F62A88A      | 893B                     | mov dword ptr ds:[ebx],edi            |
6F62A88C      | 33F6                     | xor esi,esi                           |
6F62A88E      | EB 02                    | jmp game.6F62A892                     |
6F62A890      | 8BF0                     | mov esi,eax                           |
6F62A892      | C702 00000000            | mov dword ptr ds:[edx],0              |
6F62A898      | 8B4D 00                  | mov ecx,dword ptr ss:[ebp]            |
6F62A89B      | 85C9                     | test ecx,ecx                          |
6F62A89D      | 74 0C                    | je game.6F62A8AB                      |
6F62A89F      | 8341 04 FF               | add dword ptr ds:[ecx+4],FFFFFFFF     |
6F62A8A3      | 75 06                    | jne game.6F62A8AB                     |
6F62A8A5      | 8B01                     | mov eax,dword ptr ds:[ecx]            |
6F62A8A7      | 8B10                     | mov edx,dword ptr ds:[eax]            |
6F62A8A9      | FFD2                     | call edx                              |
6F62A8AB      | 8B4424 10                | mov eax,dword ptr ss:[esp+10]         |
6F62A8AF      | 6A 00                    | push 0                                |
6F62A8B1      | 6A 00                    | push 0                                |
6F62A8B3      | 50                       | push eax                              |
6F62A8B4      | B9 ECE9AC6F              | mov ecx,game.6FACE9EC                 |
6F62A8B9      | E8 9272E9FF              | call game.6F4C1B50                    |
6F62A8BE      | EB 26                    | jmp game.6F62A8E6                     |
6F62A8C0      | 8B4C24 24                | mov ecx,dword ptr ss:[esp+24]         |
6F62A8C4      | 394E 04                  | cmp dword ptr ds:[esi+4],ecx          |
6F62A8C7      | 75 11                    | jne game.6F62A8DA                     |
6F62A8C9      | 8B7C24 2C                | mov edi,dword ptr ss:[esp+2C]         |
6F62A8CD      | 3BC7                     | cmp eax,edi                           |
6F62A8CF      | B9 01000000              | mov ecx,1                             |
6F62A8D4      | 894C24 1C                | mov dword ptr ss:[esp+1C],ecx         |
6F62A8D8      | 74 54                    | je game.6F62A92E                      |
6F62A8DA      | 3B33                     | cmp esi,dword ptr ds:[ebx]            |
6F62A8DC      | 8BFE                     | mov edi,esi                           |
6F62A8DE      | 75 04                    | jne game.6F62A8E4                     |
6F62A8E0      | 33F6                     | xor esi,esi                           |
6F62A8E2      | EB 02                    | jmp game.6F62A8E6                     |
6F62A8E4      | 8B36                     | mov esi,dword ptr ds:[esi]            |
6F62A8E6      | 85F6                     | test esi,esi                          |
6F62A8E8      | 897424 10                | mov dword ptr ss:[esp+10],esi         |
6F62A8EC      | 0F85 6EFFFFFF            | jne game.6F62A860                     |
6F62A8F2      | 8B6C24 14                | mov ebp,dword ptr ss:[esp+14]         |
6F62A8F6      | 6A FE                    | push FFFFFFFE                         |
6F62A8F8      | 68 70B3A96F              | push game.6FA9B370                    |
6F62A8FD      | 6A 00                    | push 0                                |
6F62A8FF      | B9 ECE9AC6F              | mov ecx,game.6FACE9EC                 |
6F62A904      | E8 A771E9FF              | call game.6F4C1AB0                    |
6F62A909      | 85C0                     | test eax,eax                          |
6F62A90B      | 74 0D                    | je game.6F62A91A                      |
6F62A90D      | C700 00000000            | mov dword ptr ds:[eax],0              |
6F62A913      | C740 08 00000000         | mov dword ptr ds:[eax+8],0            |
6F62A91A      | 8B0B                     | mov ecx,dword ptr ds:[ebx]            |
6F62A91C      | 85C9                     | test ecx,ecx                          |
6F62A91E      | 894424 10                | mov dword ptr ss:[esp+10],eax         |
6F62A922      | 74 14                    | je game.6F62A938                      |
6F62A924      | 8B11                     | mov edx,dword ptr ds:[ecx]            |
6F62A926      | 8910                     | mov dword ptr ds:[eax],edx            |
6F62A928      | 8B0B                     | mov ecx,dword ptr ds:[ebx]            |
6F62A92A      | 8901                     | mov dword ptr ds:[ecx],eax            |
6F62A92C      | EB 0C                    | jmp game.6F62A93A                     |
6F62A92E      | 8B6C24 14                | mov ebp,dword ptr ss:[esp+14]         |
6F62A932      | 894C24 18                | mov dword ptr ss:[esp+18],ecx         |
6F62A936      | EB 0F                    | jmp game.6F62A947                     |
6F62A938      | 8900                     | mov dword ptr ds:[eax],eax            |
6F62A93A      | 8B5424 24                | mov edx,dword ptr ss:[esp+24]         |
6F62A93E      | 8B7C24 2C                | mov edi,dword ptr ss:[esp+2C]         |
6F62A942      | 8903                     | mov dword ptr ds:[ebx],eax            |
6F62A944      | 8950 04                  | mov dword ptr ds:[eax+4],edx          |
6F62A947      | 85FF                     | test edi,edi                          |
6F62A949      | 8B4C24 28                | mov ecx,dword ptr ss:[esp+28]         |
6F62A94D      | 8B4424 10                | mov eax,dword ptr ss:[esp+10]         |
6F62A951      | 8948 0C                  | mov dword ptr ds:[eax+C],ecx          | <--- 这里写入了错误码E
6F62A954      | 74 04                    | je game.6F62A95A                      | <--- 在此处硬件端点写入
6F62A956      | 8347 04 01               | add dword ptr ds:[edi+4],1            |
6F62A95A      | 8B48 08                  | mov ecx,dword ptr ds:[eax+8]          |
6F62A95D      | 85C9                     | test ecx,ecx                          |
6F62A95F      | 74 0C                    | je game.6F62A96D                      |
6F62A961      | 8341 04 FF               | add dword ptr ds:[ecx+4],FFFFFFFF     |
6F62A965      | 75 06                    | jne game.6F62A96D                     |
6F62A967      | 8B11                     | mov edx,dword ptr ds:[ecx]            |
6F62A969      | 8B02                     | mov eax,dword ptr ds:[edx]            |
6F62A96B      | FFD0                     | call eax                              |
6F62A96D      | 837C24 18 00             | cmp dword ptr ss:[esp+18],0           |
6F62A972      | 8B4C24 10                | mov ecx,dword ptr ss:[esp+10]         |
6F62A976      | 8979 08                  | mov dword ptr ds:[ecx+8],edi          |
6F62A979      | 75 0E                    | jne game.6F62A989                     |
6F62A97B      | 837C24 1C 00             | cmp dword ptr ss:[esp+1C],0           |
6F62A980      | 75 07                    | jne game.6F62A989                     |
6F62A982      | 8BCD                     | mov ecx,ebp                           |
6F62A984      | E8 17FEFFFF              | call game.6F62A7A0                    |
6F62A989      | 5F                       | pop edi                               |
6F62A98A      | 5E                       | pop esi                               |
6F62A98B      | 5D                       | pop ebp                               |
6F62A98C      | 5B                       | pop ebx                               |
6F62A98D      | 83C4 10                  | add esp,10                            |
6F62A990      | C2 0C00                  | ret C                                 |
```

找到了一些上层函数！！
调用顺序如下：（`6F53F36B | 69C0 04030000            | imul eax,eax,304          |）这里的代码貌似在分配槽位数据的内存！

```assembly
6F5B4CF0 | 56                       | push esi                                   |
6F5B4CF1 | 8BF1                     | mov esi,ecx                                |
6F5B4CF3 | E8 C867FFFF              | call game.6F5AB4C0                         |
6F5B4CF8 | 8B4C24 10                | mov ecx,dword ptr ss:[esp+10]              |
6F5B4CFC | 51                       | push ecx                                   |
6F5B4CFD | 8B4C24 0C                | mov ecx,dword ptr ss:[esp+C]               |
6F5B4D01 | 05 1C020000              | add eax,21C                                |
6F5B4D06 | 50                       | push eax                                   |
6F5B4D07 | 68 9C52876F              | push game.6F87529C                         |
6F5B4D0C | BA 9C52876F              | mov edx,game.6F87529C                      |
6F5B4D11 | E8 1A040100              | call game.6F5C5130                         |
6F5B4D16 | 6A 00                    | push 0                                     |
6F5B4D18 | 56                       | push esi                                   |
6F5B4D19 | BA 0E000000              | mov edx,E                                  |
6F5B4D1E | B9 78000940              | mov ecx,40090078                           |
6F5B4D23 | E8 28A6F8FF              | call game.6F53F350                         |
6F5B4D28 | 6A 01                    | push 1                                     |
6F5B4D2A | 56                       | push esi                                   |
6F5B4D2B | BA 0E000000              | mov edx,E                                  |
6F5B4D30 | B9 78000940              | mov ecx,40090078                           |
6F5B4D35 | E8 16A6F8FF              | call game.6F53F350                         |
6F5B4D3A | 5E                       | pop esi                                    |
6F5B4D3B | C2 0C00                  | ret C                                      |
```
```assembly
6F53F350 | 56                       | push esi                                   |
6F53F351 | 57                       | push edi                                   |
6F53F352 | 8BF9                     | mov edi,ecx                                |
6F53F354 | B9 0D000000              | mov ecx,D                                  |
6F53F359 | 8BF2                     | mov esi,edx                                |
6F53F35B | E8 7041F8FF              | call game.6F4C34D0                         |
6F53F360 | 8B50 10                  | mov edx,dword ptr ds:[eax+10]              |
6F53F363 | 8B4424 10                | mov eax,dword ptr ss:[esp+10]              |
6F53F367 | 8B4C24 0C                | mov ecx,dword ptr ss:[esp+C]               |
6F53F36B | 69C0 04030000            | imul eax,eax,304                           |
6F53F371 | 51                       | push ecx                                   |
6F53F372 | 8B4A 08                  | mov ecx,dword ptr ds:[edx+8]               |
6F53F375 | 56                       | push esi                                   |
6F53F376 | 57                       | push edi                                   |
6F53F377 | 8D4C01 08                | lea ecx,dword ptr ds:[ecx+eax+8]           |
6F53F37B | E8 4085FFFF              | call game.6F5378C0                         |
6F53F380 | 5F                       | pop edi                                    |
6F53F381 | 5E                       | pop esi                                    |
6F53F382 | C2 0800                  | ret 8                                      |
```
```assembly
6F5378C0 | 8B41 10                  | mov eax,dword ptr ds:[ecx+10]              |
6F5378C3 | 8B40 08                  | mov eax,dword ptr ds:[eax+8]               |
6F5378C6 | 83C1 10                  | add ecx,10                                 |
6F5378C9 | FFE0                     | jmp eax                                    |
```

eax=game.6F62A9A0

```assembly

6F62A9A0 | 6A 01                    | push 1                                     |
6F62A9A2 | E8 C9F5FFFF              | call game.6F629F70                         |
6F62A9A7 | 8BC8                     | mov ecx,eax                                |
6F62A9A9 | E9 72FEFFFF              | jmp game.6F62A820                          |
```
刚好跳转到上面的 `6F62A820`

### 2.3 网络层：定位连接失败原因
深入底层网络调用，发现 `WSARecv` 返回异常，或在连接建立后未能读取到握手数据。

**地址**: `6F6DA990` (网络接收封装函数)
**地址**: `game.dll + 6DA990`
**关键发现**: `WSARecv` 被调用，但没有收到有效数据，导致上层判定连接无效。

```assembly
6F6DA990      | 83EC 10                  | sub esp,10                            |
6F6DA993      | 56                       | push esi                              |
6F6DA994      | 8BF1                     | mov esi,ecx                           |
6F6DA996      | 8B06                     | mov eax,dword ptr ds:[esi]            |
6F6DA998      | 8B50 14                  | mov edx,dword ptr ds:[eax+14]         |
6F6DA99B      | 57                       | push edi                              |
6F6DA99C      | FFD2                     | call edx                              |
6F6DA99E      | 33C9                     | xor ecx,ecx                           |
6F6DA9A0      | 8D86 34060000            | lea eax,dword ptr ds:[esi+634]        |
6F6DA9A6      | 8908                     | mov dword ptr ds:[eax],ecx            |
6F6DA9A8      | 8948 04                  | mov dword ptr ds:[eax+4],ecx          |
6F6DA9AB      | 8948 08                  | mov dword ptr ds:[eax+8],ecx          |
6F6DA9AE      | 8948 0C                  | mov dword ptr ds:[eax+C],ecx          |
6F6DA9B1      | 8948 10                  | mov dword ptr ds:[eax+10],ecx         |
6F6DA9B4      | 8948 14                  | mov dword ptr ds:[eax+14],ecx         |
6F6DA9B7      | 8B3D 24A6AD6F            | mov edi,dword ptr ds:[<&WSARecv>]     |
6F6DA9BD      | 3BF9                     | cmp edi,ecx                           |
6F6DA9BF      | 8B56 74                  | mov edx,dword ptr ds:[esi+74]         |
6F6DA9C2      | 74 4E                    | je game.6F6DAA12                      |
6F6DA9C4      | 53                       | push ebx                              |
6F6DA9C5      | 51                       | push ecx                              |
6F6DA9C6      | 50                       | push eax                              |
6F6DA9C7      | 8D4424 14                | lea eax,dword ptr ss:[esp+14]         |
6F6DA9CB      | 50                       | push eax                              |
6F6DA9CC      | 8B46 04                  | mov eax,dword ptr ds:[esi+4]          |
6F6DA9CF      | 894C24 1C                | mov dword ptr ss:[esp+1C],ecx         |
6F6DA9D3      | 894C24 18                | mov dword ptr ss:[esp+18],ecx         |
6F6DA9D7      | BB B4050000              | mov ebx,5B4                           |
6F6DA9DC      | 2BDA                     | sub ebx,edx                           |
6F6DA9DE      | 8D5432 78                | lea edx,dword ptr ds:[edx+esi+78]     |
6F6DA9E2      | 8D4C24 1C                | lea ecx,dword ptr ss:[esp+1C]         |
6F6DA9E6      | 51                       | push ecx                              |
6F6DA9E7      | 895424 28                | mov dword ptr ss:[esp+28],edx         |
6F6DA9EB      | 6A 01                    | push 1                                |
6F6DA9ED      | 8D5424 28                | lea edx,dword ptr ss:[esp+28]         |
6F6DA9F1      | 52                       | push edx                              |
6F6DA9F2      | 50                       | push eax                              |
6F6DA9F3      | 895C24 30                | mov dword ptr ss:[esp+30],ebx         |
6F6DA9F7      | FFD7                     | call edi                              | <--- 调用 WSARecv 根据实际情况设置暂停条件([0EE9FEA0]!=1&&[0F11FEA0]!=1&&[144FFEA0]!=1&&[1796FEA0]!=1&&[1782FEA0]!=1)
6F6DA9F9      | 83F8 FF                  | cmp eax,FFFFFFFF                      | <--- 检查返回值 (-1 表示 SOCKET_ERROR)
6F6DA9FC      | 5B                       | pop ebx                               |
6F6DA9FD      | 75 0D                    | jne game.6F6DAA0C                     |
6F6DA9FF      | FF15 9CD8866F            | call dword ptr ds:[<Ordinal#111>]     | <--- 获取 WSAGetLastError
6F6DAA05      | 3D E5030000              | cmp eax,3E5                           | <--- 检查是否为 WSA_IO_PENDING (997)
6F6DAA0A      | 75 30                    | jne game.6F6DAA3C                     |
6F6DAA0C      | 5F                       | pop edi                               |
6F6DAA0D      | 5E                       | pop esi                               |
6F6DAA0E      | 83C4 10                  | add esp,10                            |
6F6DAA11      | C3                       | ret                                   |
6F6DAA12      | 50                       | push eax                              |
6F6DAA13      | 8B46 04                  | mov eax,dword ptr ds:[esi+4]          |
6F6DAA16      | 51                       | push ecx                              |
6F6DAA17      | B9 B4050000              | mov ecx,5B4                           |
6F6DAA1C      | 2BCA                     | sub ecx,edx                           |
6F6DAA1E      | 51                       | push ecx                              |
6F6DAA1F      | 8D5432 78                | lea edx,dword ptr ds:[edx+esi+78]     |
6F6DAA23      | 52                       | push edx                              |
6F6DAA24      | 50                       | push eax                              |
6F6DAA25      | FF15 C8D1866F            | call dword ptr ds:[<ReadFile>]        |
6F6DAA2B      | 85C0                     | test eax,eax                          |
6F6DAA2D      | 75 DD                    | jne game.6F6DAA0C                     |
6F6DAA2F      | FF15 E4D1866F            | call dword ptr ds:[<GetLastError>]    |
6F6DAA35      | 3D E5030000              | cmp eax,3E5                           |
6F6DAA3A      | 74 D0                    | je game.6F6DAA0C                      |
6F6DAA3C      | 8B16                     | mov edx,dword ptr ds:[esi]            |
6F6DAA3E      | 8B42 24                  | mov eax,dword ptr ds:[edx+24]         |
6F6DAA41      | 8BCE                     | mov ecx,esi                           |
6F6DAA43      | FFD0                     | call eax                              |
6F6DAA45      | 8B16                     | mov edx,dword ptr ds:[esi]            |
6F6DAA47      | 8B42 18                  | mov eax,dword ptr ds:[edx+18]         |
6F6DAA4A      | 5F                       | pop edi                               |
6F6DAA4B      | 8BCE                     | mov ecx,esi                           |
6F6DAA4D      | 5E                       | pop esi                               |
6F6DAA4E      | 83C4 10                  | add esp,10                            |
6F6DAA51      | FFE0                     | jmp eax                               |
```

**调试结论**:
1.  客户端发起了 `connect`。
2.  客户端调用 `WSARecv` 等待服务端的握手包（协议头 `F7`）。
3.  **结果**: 接收超时或连接被立即重置（RST），导致进入 `NETERROR_JOINGAMENOTFOUND` 分支。

---

## 3. 根因分析 (Root Cause)

经过对 Bot 源代码的审查，发现**服务端（Bot）代码缺失了 TCP 监听逻辑**。

**错误代码逻辑 (Client.cpp):**
```cpp
// 只有 UDP 用于广播，没有 TCP Server
m_udpSocket->bind(QHostAddress::AnyIPv4, port); 
// 告诉魔兽我的游戏端口是 port，但实际上这个端口只绑定了 UDP，没绑定 TCP
sendPacket(SID_NETGAMEPORT, port); 
```

**问题链:**
1.  **UDP 广播正常**: 魔兽客户端收到了 UDP 广播，解析出 IP 和端口，将房间显示在列表里。
2.  **TCP 连接失败**: 当玩家点击“加入”时，魔兽客户端向该端口发起 **TCP 连接**。
3.  **操作系统拒绝**: 由于 Bot 此时只在端口上运行了 `QUdpSocket`，没有 `QTcpServer` 进行 `listen`，操作系统内核直接回复 **RST (Reset)** 包拒绝连接。
4.  **客户端报错**: 魔兽客户端收到连接拒绝或超时，判定游戏不存在。

---

## 4. 解决方案 (Solution)

在 Bot 代码中引入 `QTcpServer`，并确保与 UDP 绑定在同一端口。

**修复后的代码片段:**

```cpp
// 1. 在头文件添加 TCP Server
QTcpServer *m_gameServer;

// 2. 在绑定端口时同时启动 UDP 和 TCP
bool Client::bindToRandomPort() {
    // ...
    // 绑定 UDP
    m_udpSocket->bind(QHostAddress::AnyIPv4, port);
    
    // [修复] 同时监听 TCP
    m_gameServer = new QTcpServer(this);
    if (!m_gameServer->listen(QHostAddress::AnyIPv4, port)) {
        return false; // 如果 TCP 监听失败，则该端口不可用
    }
    // ...
}

// 3. 处理玩家进入 (W3GS 握手)
connect(m_gameServer, &QTcpServer::newConnection, this, &Client::onNewConnection);
```

**验证结果:**
修复代码后，Bot 同时监听 TCP/UDP 6112 端口。客户端点击加入时，TCP 握手成功，Bot 回复 `0xF7 0x1F` (Request Accepted)，成功进入房间。
```
# Game.dll + 591CA0 - 房间列表项选中处理函数 (Select Room List Item)

![x64dbg调试截图](https://github.com/wuxiancong/War3Bot/blob/main/details/images/BattleNetCustomJoinPanel.PNG)

**功能**: 当用户在自定义游戏列表（ListBox）中点击选中某个房间时触发。该函数负责读取选中项的关联数据，解析 StatString，并刷新右侧的详细信息面板（地图预览、创建者、时长等）。

**环境 (Environment):**
- **客户端:** Warcraft III (1.26a / Game.dll)
- **服务端:** PVPGN
- **调试工具:** x64dbg
- **基地址:** 6F000000

## 1. 核心逻辑流程

- 1.  **获取数据**: 从 UI 列表选中项中获取 `GameInfo` 结构体指针。
- 2.  **解析数据**: 调用 `ParseStatString` 解析地图信息。
- 3.	**关键点**: 如果解析失败（例如数据格式对不上），直接跳过后续所有 UI 更新。
- 4.  **UI 更新**:
    - *   更新游戏名称输入框 (JoinGameNameEditBox)。
    - *   更新“创建者”标签 (GameCreatorLabel)。
    - *   更新“游戏时长”标签 (TimeLabel)。
    - *   加载并显示地图预览图 (MapInfoPane)。

---

## 2. 详细反汇编分析

**地址**: `Game.dll + 0x591CA0`

```assembly
; ========================================================================
; 函数入口: 选中房间列表项
; ========================================================================
6F591CA0 | 81EC 60010000            | sub esp,160                                              | <--- 建立栈帧
6F591CA6 | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]                          | <--- Stack Cookie 保护
6F591CAB | 33C4                     | xor eax,esp                                              |
6F591CAD | 898424 5C010000          | mov dword ptr ss:[esp+15C],eax                           |
6F591CB4 | 53                       | push ebx                                                 |
6F591CB5 | 56                       | push esi                                                 |
6F591CB6 | 57                       | push edi                                                 |
6F591CB7 | 33DB                     | xor ebx,ebx                                              | <--- EBX = 0
6F591CB9 | 8BF1                     | mov esi,ecx                                              | <--- ESI = BattleNetCustomJoinPanel 对象指针

; --- 获取 UI 控件对象 ---
6F591CBB | 8B8E 04020000            | mov ecx,dword ptr ds:[esi+204]                           | <--- ECX = JoinGameNameEditBox 指针
6F591CC1 | 53                       | push ebx                                                 |
6F591CC2 | 68 9C52876F              | push game.6F87529C                                       |
6F591CC7 | E8 843E0800              | call game.6F615B50                                       |
6F591CCC | 8BCE                     | mov ecx,esi                                              |
6F591CCE | E8 9D45FFFF              | call game.6F586270                                       |

; ========================================================================
; 步骤 1: 获取选中项的数据结构 (GameInfo)
; ========================================================================
6F591CD3 | 8B8E 08020000            | mov ecx,dword ptr ds:[esi+208]                           | <--- ECX = CListBoxWar3 指针
6F591CD9 | E8 22C0FCFF              | call game.6F55DD00                                       | <--- call GetItemData() -> 获取选中项关联数据
6F591CDE | 8BF8                     | mov edi,eax                                              | <--- EDI = GameInfo 结构体指针
6F591CE0 | 3BFB                     | cmp edi,ebx                                              | <--- 检查指针是否为 NULL
6F591CE2 | 0F84 B1010000            | je game.6F591E99                                         | <---  如果为空，直接跳到函数末尾 (不刷新UI)

; ========================================================================
; 步骤 2: 解析 StatString (核心判决点)
; ========================================================================
6F591CE8 | 8D57 0C                  | lea edx,dword ptr ds:[edi+C]                             | <--- EDX = 房间名字符串 ("bot" 或 "test")
6F591CEB | 8D4C24 24                | lea ecx,dword ptr ss:[esp+24]                            | <--- ECX = 栈上的临时缓冲区
6F591CEF | 885C24 24                | mov byte ptr ss:[esp+24],bl                              | <--- 初始化缓冲区
6F591CF3 | 885C24 44                | mov byte ptr ss:[esp+44],bl                              |
6F591CF7 | 885C24 78                | mov byte ptr ss:[esp+78],bl                              |
6F591CFB | 889C24 AE000000          | mov byte ptr ss:[esp+AE],bl                              |
6F591D02 | E8 09CA0200              | call game.6F5BE710                                       | <--- call ParseStatString() -> 尝试解析地图/主机信息
6F591D07 | 85C0                     | test eax,eax                                             | <--- 检查解析结果 (1=成功, 0=失败)
; ------------------------------------------------------------------------
; [异常点] 如果 Bot 发送了 ASCII 格式，解析失败(0)，此处跳转触发
; 导致下方所有 UI 更新代码被跳过，右侧面板变白。
; ------------------------------------------------------------------------
6F591D09 | 0F84 8A010000            | je game.6F591E99                                         | <--- 解析失败则退出

; ========================================================================
; 步骤 3: 更新 UI - 游戏名称 (Game Name)
; ========================================================================
6F591D0F | 8B8E 04020000            | mov ecx,dword ptr ds:[esi+204]                           | <--- ECX = JoinGameNameEditBox
6F591D15 | 53                       | push ebx                                                 |
6F591D16 | 8D4424 28                | lea eax,dword ptr ss:[esp+28]                            | <--- 指向解析出的游戏名
6F591D1A | 50                       | push eax                                                 |
6F591D1B | E8 303E0800              | call game.6F615B50                                       | <---  [UI] 更新游戏名编辑框 (SetText)

; ========================================================================
; 步骤 4: 更新 UI - 创建者 (Creator)
; ========================================================================
6F591D20 | 8B86 28020000            | mov eax,dword ptr ds:[esi+228]                           | <--- EAX = GameCreatorValue 控件
6F591D26 | 8B90 B4000000            | mov edx,dword ptr ds:[eax+B4]                            | <--- 获取虚表
6F591D2C | 8D88 B4000000            | lea ecx,dword ptr ds:[eax+B4]                            |
6F591D32 | 8B42 18                  | mov eax,dword ptr ds:[edx+18]                            | <--- 获取 SetText 虚函数
6F591D35 | FFD0                     | call eax                                                 | <---  [UI] 更新“创建者”标签
6F591D37 | D95C24 0C                | fstp dword ptr ss:[esp+C]                                | <--- 浮点数处理...
6F591D3B | 53                       | push ebx                                                 |
6F591D3C | 68 80000000              | push 80                                                  |
6F591D41 | 8D8C24 F0000000          | lea ecx,dword ptr ss:[esp+F0]                            |
6F591D48 | 51                       | push ecx                                                 |
6F591D49 | 8B8E 20020000            | mov ecx,dword ptr ds:[esi+220]                           |
6F591D4F | E8 9CF50700              | call game.6F6112F0                                       |
6F591D54 | D9EE                     | fldz                                                     |
6F591D56 | 53                       | push ebx                                                 |
6F591D57 | 51                       | push ecx                                                 |
6F591D58 | 8B8E 20020000            | mov ecx,dword ptr ds:[esi+220]                           |
6F591D5E | D91C24                   | fstp dword ptr ss:[esp]                                  |
6F591D61 | 8D5424 18                | lea edx,dword ptr ss:[esp+18]                            |
6F591D65 | 52                       | push edx                                                 |
6F591D66 | E8 559C0600              | call game.6F5FB9C0                                       |
6F591D6B | 51                       | push ecx                                                 |
6F591D6C | 8D8424 F8000000          | lea eax,dword ptr ss:[esp+F8]                            |
6F591D73 | D91C24                   | fstp dword ptr ss:[esp]                                  |
6F591D76 | 50                       | push eax                                                 |
6F591D77 | E8 EA981500              | call <JMP.&Ordinal#506>                                  | <--- String Length
6F591D7C | 8B8E 20020000            | mov ecx,dword ptr ds:[esi+220]                           |
6F591D82 | 50                       | push eax                                                 |
6F591D83 | E8 189C0600              | call game.6F5FB9A0                                       |
6F591D88 | 8D9424 FC000000          | lea edx,dword ptr ss:[esp+FC]                            |
6F591D8F | 8BC8                     | mov ecx,eax                                              |
6F591D91 | E8 2AA7F3FF              | call game.6F4CC4C0                                       |
6F591D96 | D94424 0C                | fld dword ptr ss:[esp+C]                                 |
6F591D9A | D86424 10                | fsub dword ptr ss:[esp+10]                               |
6F591D9E | 6A 10                    | push 10                                                  |
6F591DA0 | 8D8C24 CC000000          | lea ecx,dword ptr ss:[esp+CC]                            |
6F591DA7 | 51                       | push ecx                                                 |
6F591DA8 | 8B8E 28020000            | mov ecx,dword ptr ds:[esi+228]                           |
6F591DAE | D95C24 14                | fstp dword ptr ss:[esp+14]                               |
6F591DB2 | E8 099C0600              | call game.6F5FB9C0                                       |
6F591DB7 | 51                       | push ecx                                                 |
6F591DB8 | 8B8E 28020000            | mov ecx,dword ptr ds:[esi+228]                           |
6F591DBE | D91C24                   | fstp dword ptr ss:[esp]                                  |
6F591DC1 | E8 DA9B0600              | call game.6F5FB9A0                                       |
6F591DC6 | D94424 18                | fld dword ptr ss:[esp+18]                                |
6F591DCA | 51                       | push ecx                                                 |
6F591DCB | 8BD0                     | mov edx,eax                                              |
6F591DCD | D91C24                   | fstp dword ptr ss:[esp]                                  |
6F591DD0 | 8D8C24 BE000000          | lea ecx,dword ptr ss:[esp+BE]                            |
6F591DD7 | E8 14B10200              | call game.6F5BCEF0                                       |
6F591DDC | 8B8E 28020000            | mov ecx,dword ptr ds:[esi+228]                           |
6F591DE2 | 8D9424 C8000000          | lea edx,dword ptr ss:[esp+C8]                            |
6F591DE9 | 52                       | push edx                                                 |
6F591DEA | E8 51FF0700              | call game.6F611D40                                       |
6F591DEF | 8B4424 55                | mov eax,dword ptr ss:[esp+55]                            |
6F591DF3 | 50                       | push eax                                                 |
6F591DF4 | 8BCE                     | mov ecx,esi                                              |
6F591DF6 | E8 1540FEFF              | call game.6F575E10                                       |
6F591DFB | 8B8F C8000000            | mov ecx,dword ptr ds:[edi+C8]                            |
6F591E01 | 8D5424 14                | lea edx,dword ptr ss:[esp+14]                            |
6F591E05 | E8 76321300              | call game.6F6C5080                                       |
6F591E0A | 66:8B4424 1E             | mov ax,word ptr ss:[esp+1E]                              |
6F591E0F | 66:3D 0A00               | cmp ax,A                                                 |
6F591E13 | B9 885B966F              | mov ecx,game.6F965B88                                    | <--- 格式化字符串: "%d:0%d"
6F591E18 | 72 05                    | jb game.6F591E1F                                         |
6F591E1A | B9 805B966F              | mov ecx,game.6F965B80                                    | <--- 格式化字符串: "%d:%d"
6F591E1F | 0FB7D0                   | movzx edx,ax                                             |
6F591E22 | 0FB74424 1C              | movzx eax,word ptr ss:[esp+1C]                           |
6F591E27 | 52                       | push edx                                                 |
6F591E28 | 50                       | push eax                                                 |
6F591E29 | 51                       | push ecx                                                 |
6F591E2A | 8D8C24 E4000000          | lea ecx,dword ptr ss:[esp+E4]                            |
6F591E31 | 6A 10                    | push 10                                                  |
6F591E33 | 51                       | push ecx                                                 |
6F591E34 | E8 6D971500              | call <JMP.&Ordinal#578>                                  | <--- sprintf() 格式化时间字符串

; ========================================================================
; 步骤 5: 更新 UI - 游戏时长 (Game Duration)
; ========================================================================
6F591E39 | 8B8E 30020000            | mov ecx,dword ptr ds:[esi+230]                           | <--- ECX = GameCreationTimeValue 指针
6F591E3F | 83C4 14                  | add esp,14                                               |
6F591E42 | 8D9424 D8000000          | lea edx,dword ptr ss:[esp+D8]                            | <--- EDX = 格式化后的时间字符串 (如 "6:39")
6F591E49 | 52                       | push edx                                                 |
6F591E4A | E8 F1FE0700              | call game.6F611D40                                       | <--- [UI] 更新“游戏时长”标签 (SetText)

; ========================================================================
; 步骤 6: 加载地图预览 (Map Preview)
; ========================================================================
6F591E4F | 33D2                     | xor edx,edx                                              |
6F591E51 | 8D4C24 78                | lea ecx,dword ptr ss:[esp+78]                            | <--- ECX = 地图路径字符串 "Maps\..."
6F591E55 | E8 A6631900              | call game.6F728200                                       | <--- [File] 检查地图文件是否存在
6F591E5A | 85C0                     | test eax,eax                                             |
6F591E5C | 74 2B                    | je game.6F591E89                                         |
6F591E5E | 8B8E 18020000            | mov ecx,dword ptr ds:[esi+218]                           | <--- ECX = MapInfoPane 指针
6F591E64 | 8D4424 78                | lea eax,dword ptr ss:[esp+78]                            |
6F591E68 | 50                       | push eax                                                 |
6F591E69 | E8 D2C5FEFF              | call game.6F57E440                                       | <--- [UI] 加载地图预览图 (LoadMapImage)
6F591E6E | F68424 C4000000 08       | test byte ptr ss:[esp+C4],8                              |
6F591E76 | 8B8E 18020000            | mov ecx,dword ptr ds:[esi+218]                           |
6F591E7C | 74 04                    | je game.6F591E82                                         |
6F591E7E | 6A 01                    | push 1                                                   |
6F591E80 | EB 02                    | jmp game.6F591E84                                        |
6F591E82 | 6A 02                    | push 2                                                   |
6F591E84 | E8 37C6FEFF              | call game.6F57E4C0                                       |

; --- 尾部处理 ---
6F591E89 | 8B4C24 58                | mov ecx,dword ptr ss:[esp+58]                            |
6F591E8D | 51                       | push ecx                                                 |
6F591E8E | 8B8E 34020000            | mov ecx,dword ptr ds:[esi+234]                           |
6F591E94 | E8 07E7FDFF              | call game.6F5705A0                                       |
6F591E99 | 8B8C24 68010000          | mov ecx,dword ptr ss:[esp+168]                           |
6F591EA0 | 5F                       | pop edi                                                  |
6F591EA1 | 5E                       | pop esi                                                  |
6F591EA2 | 5B                       | pop ebx                                                  |
6F591EA3 | 33CC                     | xor ecx,esp                                              |
6F591EA5 | B8 01000000              | mov eax,1                                                |
6F591EAA | E8 AAF12400              | call game.6F7E1059                                       |
6F591EAF | 81C4 60010000            | add esp,160                                              |
6F591EB5 | C3                       | ret                                                      |
```
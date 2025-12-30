# War3 机器人主机创建房间后地图信息空白调试日志

![x64dbg调试截图](https://github.com/wuxiancong/War3Bot/raw/main/debug/images/War3Dialog_NETERROR_NOGAMESPECIFIED.PNG)


## 问题描述 (Issue Description)

在使用自定义机器人（或通过 Bot 代理的真人玩家）在 PVPGN 服务端创建房间时：
1.  房间可以正常显示在游戏列表中。
2.  **右侧面板（地图信息/Mappanel）显示为空白。**
3.  尝试点击“加入游戏”时，客户端弹出错误提示：**"You must specify a game to join."** (你必须指定一个要加入的游戏)。

**环境 (Environment):**
- **客户端:** Warcraft III (v1.26.0.6401 / Game.dll)
- **服务端:** PVPGN (C++)
- **调试工具:** x64dbg
- **基地址:** 6F000000
- **关键函数地址:** 
    - 加入按钮逻辑: `Game.dll + 575E80`
    - 列表选中逻辑: `Game.dll + 591CA0`
    - 状态解析逻辑: `Game.dll + 5BE710`

---

## 1. 逆向分析：为何点击“加入游戏”报错？

**断点位置:** `6F575E80` (弹出对话框)

当我们点击加入按钮时，触发了错误弹窗 "You must specify a game to join."。通过反汇编分析，发现是因为获取到的**房间名指针指向了空数据**。

```assembly
6F575E80 | 81EC 04020000            | sub esp,204                                              |
6F575E86 | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]                          |
6F575E8B | 33C4                     | xor eax,esp                                              |
6F575E8D | 898424 00020000          | mov dword ptr ss:[esp+200],eax                           |
6F575E94 | 56                       | push esi                                                 |
6F575E95 | 8BF1                     | mov esi,ecx                                              |
6F575E97 | 8B8E 04020000            | mov ecx,dword ptr ds:[esi+204]                           |
6F575E9D | E8 FEDF0900              | call game.6F613EA0                                       | <--- GetSelectedGameName() - 尝试获取选中房间的信息
6F575EA2 | 85C0                     | test eax,eax                                             | <--- 检查返回的指针是否为 NULL
6F575EA4 | 74 20                    | je game.6F575EC6                                         | <--- 如果为 NULL，跳转到错误处理
6F575EA6 | 8038 00                  | cmp byte ptr ds:[eax],0                                  | <--- 检查字符串长度。此处 EAX 指向 00 (空字符串)
6F575EA9 | 74 1B                    | je game.6F575EC6                                         | <--- 判定为空，跳转到报错逻辑
6F575EAB | B8 01000000              | mov eax,1                                                |
6F575EB0 | 5E                       | pop esi                                                  |
6F575EB1 | 8B8C24 00020000          | mov ecx,dword ptr ss:[esp+200]                           |
6F575EB8 | 33CC                     | xor ecx,esp                                              |
6F575EBA | E8 9AB12600              | call game.6F7E1059                                       |
6F575EBF | 81C4 04020000            | add esp,204                                              |
6F575EC5 | C3                       | ret                                                      |
6F575EC6 | 68 00020000              | push 200                                                 |
6F575ECB | 8D5424 08                | lea edx,dword ptr ss:[esp+8]                             |
6F575ECF | B9 2814966F              | mov ecx,game.6F961428                                    | <--- 加载错误代码 "NETERROR_NOGAMESPECIFIED"
6F575ED4 | E8 77370500              | call game.6F5C9650                                       | <--- 转换为文本 "You must specify a game to join"
6F575ED9 | 6A 01                    | push 1                                                   |
6F575EDB | 6A 04                    | push 4                                                   |
6F575EDD | 6A 00                    | push 0                                                   |
6F575EDF | 56                       | push esi                                                 |
6F575EE0 | 6A 09                    | push 9                                                   |
6F575EE2 | 8D5424 18                | lea edx,dword ptr ss:[esp+18]                            |
6F575EE6 | B9 01000000              | mov ecx,1                                                |
6F575EEB | E8 C06FFEFF              | call game.6F55CEB0                                       | <--- 弹出 MessageBox
6F575EF0 | 8B86 04020000            | mov eax,dword ptr ds:[esi+204]                           |
6F575EF6 | 8B8C24 04020000          | mov ecx,dword ptr ss:[esp+204]                           |
6F575EFD | 8986 FC010000            | mov dword ptr ds:[esi+1FC],eax                           |
6F575F03 | 5E                       | pop esi                                                  |
6F575F04 | 33CC                     | xor ecx,esp                                              |
6F575F06 | 33C0                     | xor eax,eax                                              |
6F575F08 | E8 4CB12600              | call game.6F7E1059                                       |
6F575F0D | 81C4 04020000            | add esp,204                                              |
6F575F13 | C3                       | ret                                                      |
6F575E80 | sub esp, 204
```

**获取房间名指针的底层函数:**
```assembly
6F613EA0 | 8B81 E4010000            | mov eax,dword ptr ds:[ecx+1E4]                           | <--- 返回保存的房间名指针
6F613EA6 | C3                       | ret                                                      |
```
**分析结论:** 
虽然 UI 列表上画出了 "bot" 这行字，但其背后的逻辑数据（GameInfo）并没有被正确关联。`6F613EA0` 返回了一个指向 `00` 的无效指针，导致客户端认为用户没有选择任何有效的游戏。

---

## 2. 逆向分析：为何右侧面板空白？(UI 刷新逻辑)

**断点位置:** `6F591CA0` (列表项选中/刷新处理函数)

为了搞清楚为何数据是空的，我们追踪了**当选中列表项时**的逻辑。这里揭示了为何 **[test] (真人)** 能显示，而 **[bot] (机器人)** 不显示。

```assembly
6F591CA0 | 81EC 60010000            | sub esp,160                                              |
6F591CA6 | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]                          |
6F591CAB | 33C4                     | xor eax,esp                                              |
6F591CAD | 898424 5C010000          | mov dword ptr ss:[esp+15C],eax                           |
6F591CB4 | 53                       | push ebx                                                 |
6F591CB5 | 56                       | push esi                                                 |
6F591CB6 | 57                       | push edi                                                 |
6F591CB7 | 33DB                     | xor ebx,ebx                                              |
6F591CB9 | 8BF1                     | mov esi,ecx                                              |
6F591CBB | 8B8E 04020000            | mov ecx,dword ptr ds:[esi+204]                           |
6F591CC1 | 53                       | push ebx                                                 |
6F591CC2 | 68 9C52876F              | push game.6F87529C                                       |
6F591CC7 | E8 843E0800              | call game.6F615B50                                       |
6F591CCC | 8BCE                     | mov ecx,esi                                              |
6F591CCE | E8 9D45FFFF              | call game.6F586270                                       |
6F591CD3 | 8B8E 08020000            | mov ecx,dword ptr ds:[esi+208]                           |
6F591CD9 | E8 22C0FCFF              | call game.6F55DD00                                       | <--- GetItemData() 获取选中项关联的 Struct
6F591CDE | 8BF8                     | mov edi,eax                                              | <--- EDI = 房间数据指针
6F591CE0 | 3BFB                     | cmp edi,ebx                                              |
6F591CE2 | 0F84 B1010000            | je game.6F591E99                                         |
6F591CE8 | 8D57 0C                  | lea edx,dword ptr ds:[edi+C]                             | <--- EDX = 指向房间名 "bot"
6F591CEB | 8D4C24 24                | lea ecx,dword ptr ss:[esp+24]                            | <--- ECX = 栈上的临时缓冲区
6F591CEF | 885C24 24                | mov byte ptr ss:[esp+24],bl                              |
6F591CF3 | 885C24 44                | mov byte ptr ss:[esp+44],bl                              |
6F591CF7 | 885C24 78                | mov byte ptr ss:[esp+78],bl                              |
6F591CFB | 889C24 AE000000          | mov byte ptr ss:[esp+AE],bl                              |
6F591D02 | E8 09CA0200              | call game.6F5BE710                                       | <--- ParseStatString() 解析地图信息
6F591D07 | 85C0                     | test eax,eax                                             | <--- 检查解析结果 (1=成功, 0=失败)
6F591D09 | 0F84 8A010000            | je game.6F591E99                                         | <--- 如果解析失败，直接跳过下方所有 UI 更新代码！
6F591D0F | 8B8E 04020000            | mov ecx,dword ptr ds:[esi+204]                           |
6F591D15 | 53                       | push ebx                                                 |
6F591D16 | 8D4424 28                | lea eax,dword ptr ss:[esp+28]                            |
6F591D1A | 50                       | push eax                                                 |
6F591D1B | E8 303E0800              | call game.6F615B50                                       |
6F591D20 | 8B86 28020000            | mov eax,dword ptr ds:[esi+228]                           |
6F591D26 | 8B90 B4000000            | mov edx,dword ptr ds:[eax+B4]                            |
6F591D2C | 8D88 B4000000            | lea ecx,dword ptr ds:[eax+B4]                            |
6F591D32 | 8B42 18                  | mov eax,dword ptr ds:[edx+18]                            |
6F591D35 | FFD0                     | call eax                                                 |
6F591D37 | D95C24 0C                | fstp dword ptr ss:[esp+C]                                |
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
6F591D77 | E8 EA981500              | call <JMP.&Ordinal#506>                                  |
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
6F591E13 | B9 885B966F              | mov ecx,game.6F965B88                                    |
6F591E18 | 72 05                    | jb game.6F591E1F                                         |
6F591E1A | B9 805B966F              | mov ecx,game.6F965B80                                    |
6F591E1F | 0FB7D0                   | movzx edx,ax                                             |
6F591E22 | 0FB74424 1C              | movzx eax,word ptr ss:[esp+1C]                           |
6F591E27 | 52                       | push edx                                                 |
6F591E28 | 50                       | push eax                                                 |
6F591E29 | 51                       | push ecx                                                 |
6F591E2A | 8D8C24 E4000000          | lea ecx,dword ptr ss:[esp+E4]                            |
6F591E31 | 6A 10                    | push 10                                                  |
6F591E33 | 51                       | push ecx                                                 |
6F591E34 | E8 6D971500              | call <JMP.&Ordinal#578>                                  |
6F591E39 | 8B8E 30020000            | mov ecx,dword ptr ds:[esi+230]                           |
6F591E3F | 83C4 14                  | add esp,14                                               |
6F591E42 | 8D9424 D8000000          | lea edx,dword ptr ss:[esp+D8]                            |
6F591E49 | 52                       | push edx                                                 |
6F591E4A | E8 F1FE0700              | call game.6F611D40                                       |
6F591E4F | 33D2                     | xor edx,edx                                              |
6F591E51 | 8D4C24 78                | lea ecx,dword ptr ss:[esp+78]                            |
6F591E55 | E8 A6631900              | call game.6F728200                                       |
6F591E5A | 85C0                     | test eax,eax                                             |
6F591E5C | 74 2B                    | je game.6F591E89                                         |
6F591E5E | 8B8E 18020000            | mov ecx,dword ptr ds:[esi+218]                           |
6F591E64 | 8D4424 78                | lea eax,dword ptr ss:[esp+78]                            |
6F591E68 | 50                       | push eax                                                 |
6F591E69 | E8 D2C5FEFF              | call game.6F57E440                                       |
6F591E6E | F68424 C4000000 08       | test byte ptr ss:[esp+C4],8                              |
6F591E76 | 8B8E 18020000            | mov ecx,dword ptr ds:[esi+218]                           |
6F591E7C | 74 04                    | je game.6F591E82                                         |
6F591E7E | 6A 01                    | push 1                                                   |
6F591E80 | EB 02                    | jmp game.6F591E84                                        |
6F591E82 | 6A 02                    | push 2                                                   |
6F591E84 | E8 37C6FEFF              | call game.6F57E4C0                                       |
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
关于此函数的详细信息请跳转到 [Gamedll_591CA0_Select_Room_List_Item.md](https://github.com/wuxiancong/War3Bot/blob/main/details/1.26/Gamedll_591CA0_Select_Room_List_Item.md)

**被跳过的 UI 更新逻辑 (因为跳转导致未执行):**

```assembly
; --- 以下代码在 [bot] 房间选中时均未执行 ---
6F591D1B | call game.6F615B50       | <--- 更新游戏名编辑框
6F591D35 | call eax                 | <--- 更新“创建者”标签
6F591E4A | call game.6F611D40       | <--- 更新“游戏时长”
6F591E55 | call game.6F728200       | <--- 检查地图文件是否存在
6F591E69 | call game.6F57E440       | <--- 加载地图预览图 (LoadMapImage)
```

**对比分析:**
*   **[test] 房间:** StatString 为 `"91..."` (ASCII)。虽然格式错误，但侥幸通过了 `6F5BE710` 的校验。`je` 不跳转，右侧面板被填入数据。
*   **[bot] 房间:** StatString 为 `"99..."` (ASCII)。`6F5BE710` 校验失败（返回 0）。`je` 跳转触发，直接跳过了所有 UI 绘制代码。导致面板空白，且未生成有效的 GameInfo 供后续“加入游戏”使用。

---

## 3. 逆向分析：为何 Game State String 解析失败？

**断点位置:** `6F5BE710` (解析 Game State String 的函数)

### 3.2 解析函数入口 (`6F5BE710`)

```assembly
6F5BE710 | 56                       | push esi                                                 |
6F5BE711 | 8BF1                     | mov esi,ecx                                              |
6F5BE713 | E8 F85D0900              | call game.6F654510                                       | <--- 主要函数
6F5BE718 | 85C0                     | test eax,eax                                             |
6F5BE71A | 75 04                    | jne game.6F5BE720                                        |
6F5BE71C | 33C0                     | xor eax,eax                                              |
6F5BE71E | 5E                       | pop esi                                                  |
6F5BE71F | C3                       | ret                                                      |
6F5BE720 | 807E 31 02               | cmp byte ptr ds:[esi+31],2                               |
6F5BE724 | 77 F6                    | ja game.6F5BE71C                                         |
6F5BE726 | 33C0                     | xor eax,eax                                              |
6F5BE728 | 3846 54                  | cmp byte ptr ds:[esi+54],al                              |
6F5BE72B | 74 09                    | je game.6F5BE736                                         |
6F5BE72D | 3886 8A000000            | cmp byte ptr ds:[esi+8A],al                              |
6F5BE733 | 0F95C0                   | setne al                                                 |
6F5BE736 | 5E                       | pop esi                                                  |
6F5BE737 | C3                       | ret                                                      |
```

### 3.2 主要解析函数 (`6F654510`)

这个函数接收一个指向 StatString 数据的指针，并尝试提取出地图路径、创建者、游戏设置等信息。

```assembly
6F654510 | 6A FF                    | push FFFFFFFF                                            |
6F654512 | 68 DB12846F              | push game.6F8412DB                                       |
6F654517 | 64:A1 00000000           | mov eax,dword ptr fs:[0]                                 |
6F65451D | 50                       | push eax                                                 |
6F65451E | 81EC 8C000000            | sub esp,8C                                               |
6F654524 | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]                          |
6F654529 | 33C4                     | xor eax,esp                                              |
6F65452B | 898424 88000000          | mov dword ptr ss:[esp+88],eax                            |
6F654532 | 53                       | push ebx                                                 |
6F654533 | 55                       | push ebp                                                 |
6F654534 | 56                       | push esi                                                 |
6F654535 | 57                       | push edi                                                 |
6F654536 | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]                          |
6F65453B | 33C4                     | xor eax,esp                                              |
6F65453D | 50                       | push eax                                                 |
6F65453E | 8D8424 A0000000          | lea eax,dword ptr ss:[esp+A0]                            |
6F654545 | 64:A3 00000000           | mov dword ptr fs:[0],eax                                 |
6F65454B | 8BE9                     | mov ebp,ecx                                              |
6F65454D | 8BDA                     | mov ebx,edx                                              | <--- EBX 指向 "test" 或者 "bot"
6F65454F | B9 08000000              | mov ecx,8                                                |
6F654554 | 8BF3                     | mov esi,ebx                                              |
6F654556 | 8BFD                     | mov edi,ebp                                              |
6F654558 | F3:A5                    | rep movsd                                                |
6F65455A | 807D 00 00               | cmp byte ptr ss:[ebp],0                                  |
6F65455E | 0F84 79010000            | je game.6F6546DD                                         |
6F654564 | 6A 68                    | push 68                                                  |
6F654566 | 8D4424 38                | lea eax,dword ptr ss:[esp+38]                            |
6F65456A | 50                       | push eax                                                 |
6F65456B | 8D4C24 1C                | lea ecx,dword ptr ss:[esp+1C]                            |
6F65456F | E8 1CFCFFFF              | call game.6F654190                                       |
6F654574 | C74424 14 800E976F       | mov dword ptr ss:[esp+14],game.6F970E80                  |
6F65457C | 8D4B 30                  | lea ecx,dword ptr ds:[ebx+30]                            | <--- ECX 指向 StatString 的起始位置
6F65457F | 51                       | push ecx                                                 |
6F654580 | 33F6                     | xor esi,esi                                              |
6F654582 | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                            |
6F654586 | 89B424 AC000000          | mov dword ptr ss:[esp+AC],esi                            |
6F65458D | E8 8EC2FFFF              | call game.6F650820                                       | <--- call DecodeStatString()
6F654592 | 85C0                     | test eax,eax                                             | <--- 检查解码是否成功 (1=成功)
6F654594 | 8D4C24 14                | lea ecx,dword ptr ss:[esp+14]                            |
6F654598 | 0F84 2F010000            | je game.6F6546CD                                         |
6F65459E | 8D55 31                  | lea edx,dword ptr ss:[ebp+31]                            | <--- 指向输出缓冲区
6F6545A1 | 52                       | push edx                                                 |
6F6545A2 | 897424 2C                | mov dword ptr ss:[esp+2C],esi                            |
6F6545A6 | E8 65E6E6FF              | call game.6F4C2C10                                       | <--- ReadByte() 读取 Flags
6F6545AB | 8D45 34                  | lea eax,dword ptr ss:[ebp+34]                            | <--- 指向输出缓冲区 (地图路径)
6F6545AE | 50                       | push eax                                                 |
6F6545AF | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                            |
6F6545B3 | E8 78E7E6FF              | call game.6F4C2D30                                       | <--- ReadString() 读取地图路径
6F6545B8 | 8D4D 38                  | lea ecx,dword ptr ss:[ebp+38]                            |
6F6545BB | 51                       | push ecx                                                 |
6F6545BC | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                            |
6F6545C0 | E8 ABE6E6FF              | call game.6F4C2C70                                       |
6F6545C5 | 8D55 3A                  | lea edx,dword ptr ss:[ebp+3A]                            |
6F6545C8 | 52                       | push edx                                                 |
6F6545C9 | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                            |
6F6545CD | E8 9EE6E6FF              | call game.6F4C2C70                                       | <--- 指向输出缓冲区 (创建者)
6F6545D2 | 8D45 3C                  | lea eax,dword ptr ss:[ebp+3C]                            |
6F6545D5 | 50                       | push eax                                                 |
6F6545D6 | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                            |
6F6545DA | E8 51E7E6FF              | call game.6F4C2D30                                       | <--- ReadString() 读取创建者名字
6F6545DF | 6A 36                    | push 36                                                  |
6F6545E1 | 8D75 54                  | lea esi,dword ptr ss:[ebp+54]                            |
6F6545E4 | 56                       | push esi                                                 |
6F6545E5 | 8D4C24 1C                | lea ecx,dword ptr ss:[esp+1C]                            |
6F6545E9 | E8 22E8E6FF              | call game.6F4C2E10                                       |
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
6F65466A | 8B4C24 24                | mov ecx,dword ptr ss:[esp+24]                            | <--- ECX = 缓冲区结束指针 (End Pointer)
6F65466E | 394C24 28                | cmp dword ptr ss:[esp+28],ecx                            | <--- 比较缓冲区当前指针 (Current Pointer)
6F654672 | 75 55                    | jne game.6F6546C9                                        | <--- 错误发生在这里
6F654674 | 8B53 20                  | mov edx,dword ptr ds:[ebx+20]                            |
6F654677 | 8955 20                  | mov dword ptr ss:[ebp+20],edx                            |
6F65467A | 8B43 24                  | mov eax,dword ptr ds:[ebx+24]                            |
6F65467D | 8945 24                  | mov dword ptr ss:[ebp+24],eax                            |
6F654680 | 8B4B 28                  | mov ecx,dword ptr ds:[ebx+28]                            |
6F654683 | 894D 28                  | mov dword ptr ss:[ebp+28],ecx                            |
6F654686 | 8B53 2C                  | mov edx,dword ptr ds:[ebx+2C]                            |
6F654689 | 8955 2C                  | mov dword ptr ss:[ebp+2C],edx                            |
6F65468C | 8B83 B0000000            | mov eax,dword ptr ds:[ebx+B0]                            |
6F654692 | 83F8 01                  | cmp eax,1                                                |
6F654695 | 8985 9C000000            | mov dword ptr ss:[ebp+9C],eax                            |
6F65469B | 72 2C                    | jb game.6F6546C9                                         |
6F65469D | 83F8 10                  | cmp eax,10                                               |
6F6546A0 | 77 27                    | ja game.6F6546C9                                         |
6F6546A2 | 8B83 B4000000            | mov eax,dword ptr ds:[ebx+B4]                            |
6F6546A8 | 8D4C24 14                | lea ecx,dword ptr ss:[esp+14]                            |
6F6546AC | 8985 A0000000            | mov dword ptr ss:[ebp+A0],eax                            |
6F6546B2 | C78424 A8000000 FFFFFFFF | mov dword ptr ss:[esp+A8],FFFFFFFF                       |
6F6546BD | E8 AEFBFFFF              | call game.6F654270                                       |
6F6546C2 | B8 01000000              | mov eax,1                                                |
6F6546C7 | EB 16                    | jmp game.6F6546DF                                        |
6F6546C9 | 8D4C24 14                | lea ecx,dword ptr ss:[esp+14]                            |
6F6546CD | C78424 A8000000 FFFFFFFF | mov dword ptr ss:[esp+A8],FFFFFFFF                       |
6F6546D8 | E8 93FBFFFF              | call game.6F654270                                       |
6F6546DD | 33C0                     | xor eax,eax                                              |
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
### 核心故障点分析

请看这段代码：

```assembly
6F65466A | 8B4C24 24     | mov ecx,dword ptr ss:[esp+24]  ; ECX = 缓冲区结束指针 (End Pointer)
6F65466E | 394C24 28     | cmp dword ptr ss:[esp+28],ecx  ; 比较缓冲区当前指针 (Current Pointer)
6F654672 | 75 55         | jne game.6F6546C9              ; <--- 死在这里如果不相等，跳转报错
```

#### 这是什么检查？
这是一个 **“数据流完整性检查” (Stream Integrity / EOF Check)**。

*   War3 在解析 StatString 时，会像读取文件流一样，读一个字节、读一个字符串、读一个数组……指针会不断向后移动。
*   `[esp+24]`：是这段 StatString 数据的**总长度/结束位置**。
*   `[esp+28]`：是解析器当前**读到了哪里**。

**逻辑含义：**
> "我已经读完了所有定义的字段（地图名、创建者、参数等），现在的读取指针**必须**正好停在缓冲区的末尾。如果还有剩余数据没读，或者读过头了，说明数据格式错误！"

### 4 故障根本原因 (Root Cause Analysis)

在地址 `6F654672` 处，游戏进行了一次**数据流完整性检查 (Stream Integrity Check)**。
逻辑是：`CurrentReadPointer == EndOfStreamPointer`。

**现象:**
调试发现，在 Bot 创建的房间中，`CurrentReadPointer` 比 `EndOfStreamPointer` **少了一个字节** (或者由于编码错位导致指针未对齐)。

**数据包对比分析:**
通过对比正常数据包 (Block A) 和异常数据包 (Block B) 的 HEX 数据：

*   **Block A (正常):** `... [HostName] [00] [00] [SHA1] ...`
*   **Block B (异常):** `... [HostName] [00] [SHA1] ...`

**缺失的字节:**
协议规定在 `Host Name` 的结束符 `00` 之后，必须跟随一个 `(UINT8) Map Unknown` 字段（通常为 `00`，作为分隔符）。
由于我们的代码中缺失了这个字节：
1.  **原始数据层:** 少了一个 `00`。
2.  **编码层 (StatString):** 魔兽的编码算法会压缩连续的 `00`。由于缺失该字节，导致编码后的掩码字节 (Mask Byte) 发生变化（`01` 变为 `2F`），进而导致整个后续数据（SHA1及之后的乱码）**前移错位**。
3.  **解码层 (Game.dll):** 游戏解码后，读取完所有定义字段，发现**读取指针没有走到预期的末尾**（或者读取错位），触发 `jne` 错误跳转，判定数据包非法。

---

## 5. 解决方案 (Solution)

在生成 StatString 的代码中，于主机名（HostName）写入之后、SHA1 哈希写入之前，补上缺失的 `Map Unknown` 分隔符字节。

**修正后的 C++ 代码:**

```cpp
QByteArray War3Map::getEncodedStatString(const QString &hostName, const QString &netPathOverride)
{
    // ... (前序代码: Flags, Width, Height, CRC, MapPath)

    // 写入主机名
    out.writeRawData(hostName.toUtf8().constData(), hostName.toUtf8().size());
    out << (quint8)0; // Host Name Terminator (字符串结束符)

    // 【修复】添加协议要求的额外分隔符 (Map Unknown / Second Null Byte)
    out << (quint8)0; // <--- 补齐缺失的字节

    // 写入 SHA1
    out.writeRawData(m_mapSHA1.constData(), 20);

    QByteArray encoded = encodeStatString(rawData);
    return encoded;
}
```

**修复结果:**
添加该字节后，StatString 长度增加，内部编码对齐恢复。`6F5BE710` 校验通过，右侧面板成功显示地图信息，"加入游戏"功能恢复正常。

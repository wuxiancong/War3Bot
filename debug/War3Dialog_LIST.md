# War3 机器人主机创建房间后地图信息空白调试日志

![x64dbg调试截图](https://github.com/wuxiancong/War3Bot/raw/main/debug/images/War3Dialog1.PNG)

## 问题描述 (Issue Description)

在使用自定义机器人（或通过 Bot 代理的真人玩家）在 PVPGN 服务端创建房间时：
1.  房间可以正常显示在游戏列表中。
2.  **右侧面板（地图信息/Mappanel）显示为空白。**
3.  尝试点击“加入游戏”时，客户端弹出错误提示：**"You must specify a game to join."** (你必须指定一个要加入的游戏)。

**环境 (Environment):**
- **客户端:** Warcraft III (1.26a / Game.dll)
- **服务端:** PVPGN (C++)
- **调试工具:** x64dbg
- **关键函数地址:** 
    - 加入按钮逻辑: `Game.dll + 575E80`
    - 列表选中逻辑: `Game.dll + 591CA0`

---

## 1. 逆向分析：为何点击“加入游戏”报错？

**断点位置:** `6F575E80` (加入游戏按钮处理函数)

当我们点击加入按钮时，触发了错误弹窗 "You must specify a game to join"。通过反汇编分析，发现是因为获取到的**房间名指针指向了空数据**。

```assembly
6F575E80 | 81EC 04020000            | sub esp,204                                              |
6F575E86 | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]                          |
6F575E8B | 33C4                     | xor eax,esp                                              |
6F575E8D | 898424 00020000          | mov dword ptr ss:[esp+200],eax                           |
6F575E94 | 56                       | push esi                                                 |
6F575E95 | 8BF1                     | mov esi,ecx                                              |
6F575E97 | 8B8E 04020000            | mov ecx,dword ptr ds:[esi+204]                           |
6F575E9D | E8 FEDF0900              | call game.6F613EA0                                       | <- GetSelectedGameName() - 尝试获取选中房间的信息
6F575EA2 | 85C0                     | test eax,eax                                             | <- 检查返回的指针是否为 NULL
6F575EA4 | 74 20                    | je game.6F575EC6                                         | <- 如果为 NULL，跳转到错误处理
6F575EA6 | 8038 00                  | cmp byte ptr ds:[eax],0                                  | <- 检查字符串长度。此处 EAX 指向 00 (空字符串)
6F575EA9 | 74 1B                    | je game.6F575EC6                                         | <- 判定为空，跳转到报错逻辑
6F575EAB | B8 01000000              | mov eax,1                                                |
6F575EB0 | 5E                       | pop esi                                                  |
6F575EB1 | 8B8C24 00020000          | mov ecx,dword ptr ss:[esp+200]                           |
6F575EB8 | 33CC                     | xor ecx,esp                                              |
6F575EBA | E8 9AB12600              | call game.6F7E1059                                       |
6F575EBF | 81C4 04020000            | add esp,204                                              |
6F575EC5 | C3                       | ret                                                      |
6F575EC6 | 68 00020000              | push 200                                                 |
6F575ECB | 8D5424 08                | lea edx,dword ptr ss:[esp+8]                             |
6F575ECF | B9 2814966F              | mov ecx,game.6F961428                                    | <- 加载错误代码 "NETERROR_NOGAMESPECIFIED"
6F575ED4 | E8 77370500              | call game.6F5C9650                                       | <- 转换为文本 "You must specify a game to join"
6F575ED9 | 6A 01                    | push 1                                                   |
6F575EDB | 6A 04                    | push 4                                                   |
6F575EDD | 6A 00                    | push 0                                                   |
6F575EDF | 56                       | push esi                                                 |
6F575EE0 | 6A 09                    | push 9                                                   |
6F575EE2 | 8D5424 18                | lea edx,dword ptr ss:[esp+18]                            |
6F575EE6 | B9 01000000              | mov ecx,1                                                |
6F575EEB | E8 C06FFEFF              | call game.6F55CEB0                                       | <- 弹出 MessageBox
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
6F613EA0 | 8B81 E4010000            | mov eax,dword ptr ds:[ecx+1E4]                           | <- 返回保存的房间名指针
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
6F591CD9 | E8 22C0FCFF              | call game.6F55DD00                                       | <- GetItemData() 获取选中项关联的 Struct
6F591CDE | 8BF8                     | mov edi,eax                                              | <- EDI = 房间数据指针
6F591CE0 | 3BFB                     | cmp edi,ebx                                              |
6F591CE2 | 0F84 B1010000            | je game.6F591E99                                         |
6F591CE8 | 8D57 0C                  | lea edx,dword ptr ds:[edi+C]                             | <- EDX = 指向房间名 "bot"
6F591CEB | 8D4C24 24                | lea ecx,dword ptr ss:[esp+24]                            | <- ECX = 栈上的临时缓冲区
6F591CEF | 885C24 24                | mov byte ptr ss:[esp+24],bl                              |
6F591CF3 | 885C24 44                | mov byte ptr ss:[esp+44],bl                              |
6F591CF7 | 885C24 78                | mov byte ptr ss:[esp+78],bl                              |
6F591CFB | 889C24 AE000000          | mov byte ptr ss:[esp+AE],bl                              |
6F591D02 | E8 09CA0200              | call game.6F5BE710                                       | <- ParseStatString() 解析地图信息
6F591D07 | 85C0                     | test eax,eax                                             | <- 检查解析结果 (1=成功, 0=失败)
6F591D09 | 0F84 8A010000            | je game.6F591E99                                         | <- 如果解析失败，直接跳过下方所有 UI 更新代码！
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
6F591D1B | call game.6F615B50       | <- 更新游戏名编辑框
6F591D35 | call eax                 | <- 更新“创建者”标签
6F591E4A | call game.6F611D40       | <- 更新“游戏时长”
6F591E55 | call game.6F728200       | <- 检查地图文件是否存在
6F591E69 | call game.6F57E440       | <- 加载地图预览图 (LoadMapImage)
```

**对比分析:**
*   **[test] 房间:** StatString 为 `"91..."` (ASCII)。虽然格式错误，但侥幸通过了 `6F5BE710` 的校验。`je` 不跳转，右侧面板被填入数据。
*   **[bot] 房间:** StatString 为 `"99..."` (ASCII)。`6F5BE710` 校验失败（返回 0）。`je` 跳转触发，直接跳过了所有 UI 绘制代码。导致面板空白，且未生成有效的 GameInfo 供后续“加入游戏”使用。

---

## 3. 根本原因 (Root Cause)

问题的根源在于 PVPGN 服务端发送 `StatString` (地图/主机信息) 的数据格式错误。

*   **服务端发送了:** ASCII 格式的 Hex String (例如: `"99e3..."` -> 字节 `39 39 65 33...`)
*   **客户端期望:** 原始二进制数据 (Raw Binary) (例如: 字节 `99 E3 ...`)

War3 的解码算法（基于 XOR/位移）在处理 ASCII 字符 `0x39 ('9')` 时，解密出的数据完全错误（例如地图大小为0），导致合法性校验失败，从而丢弃了该房间的所有详细数据。

---

## 4. 修复方案 (Fix)

修改服务端代码，将 Hex String 转换回 Binary 再发送。

**C++ 修复示例 (`_glist_cb` 函数):**

```cpp
char const *info_str = game_get_info(game);

if (info_str) {
    unsigned char bin_buf[2048];
    // 关键修复: 将 Hex String "99e3..." 转换为 Binary {0x99, 0xE3...}
    int bin_len = hex_str_to_bin(info_str, bin_buf, sizeof(bin_buf));
    
    if (bin_len > 0) {
         // 发送二进制数据，客户端即可正确解析
         packet_append_data(cbdata->rpacket, bin_buf, bin_len);
    } else {
         packet_append_string(cbdata->rpacket, info_str);
    }
}
```
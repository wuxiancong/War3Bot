# War3 自定义游戏列表界面事件分发逻辑 (Custom Game List Event Dispatch)

- **模块**: `Game.dll`
- **功能**: 处理自定义游戏列表界面（Custom Game / Standard Game List）的用户交互事件。
- **机制**: 使用跳转表（Jump Table）根据输入的 Event ID (`ECX`) 分发到具体的处理函数。

## 1. 核心分发逻辑

代码首先读取事件 ID，判断是否超出范围（最大为 0x17），然后通过跳转表进行分发。

- **偏移**: 5BABF0
- **基址**: 6F000000
- **版本**: v1.26.0.6401
- **跳转表偏移**: 5BADBC
- **跳转表地址**: 6F5BADBC
- **跳转指令**: `jmp dword ptr ds:[ecx*4+6F5BADBC]`

## 2. 事件 ID 与功能映射表 (Switch-Case Mapping)

根据汇编代码中的跳转目标与注释，整理出以下事件映射关系：
- 基址: 当前值 - 6F000000

| Event ID (ECX) | 跳转地址 (Target) | 核心调用函数 | 功能描述 (Description) |
| :--- | :--- | :--- | :--- |
| **0** | `6F5BAC0A` | `call 6F591CA0` | **选择列表项** (Select List Item) |
| **1** | `6F5BAC15` | `call 6F591EC0` | **键盘输入** (Keyboard Input) |
| **2** | `6F5BAC20` | `call 6F597C40` | **创建游戏** (Create Game) |
| **3** | `6F5BAC2B` | `call 6F597C30` | **装载游戏** (Load Game / Load Replay) |
| **4** | `6F5BAC36` | `call 6F5B4FB0` | **加入游戏** (Join Game) - *核心函数* |
| **5** | `6F5BAC43` | `call 6F597C50` | **返回/取消** (Back/Cancel) |
| **6** | `6F5BAC4E` | `call 6F59ECF0` | *(未知/UI更新)* |
| **7** | `6F5BAC59` | `call 6F575C90` | **确定关闭对话框**(Close Dialog)  |
| **8** | `6F5BAC64` | `call 6F5861A0` | **设置过滤条件** (Open Filter Menu) |
| **9** | `6F5BAC6F` | `call 6F591C80` | **刷新列表开始** (Start Refresh) |
| **10** | `6F5BAC7A` | `call 6F591C60` | **确认过滤** (Confirm Filter) |
| **11** | `6F5BAC85` | `call 6F586140` | **取消过滤** (Cancel Filter) |
| **12** | `6F5BAC90` | `call 6F575C50` | **搜索游戏名** (Search Game Name) |
| **13** | `6F5BAC9B` | `call 6F5B9520` | *(结构类似，具体功能待定)* |
| **14** | `6F5BACA7` | `call 6F5861B0` | **获取列表结束** (End List Fetch) - 循环填入数据 |
| **15** | `6F5BACB3` | `call 6F597B40` | **刷新列表完毕** (Refresh Complete) |
| **16** | `6F5BACBF` | `call 6F591F00` | *(暂时未知)* |
| **17** | `6F5BACCC` | `call 6F5B51D0` | **加入错误，例如：房间已满或找不到** (Full Or Not Found) |

## 3. 辅助流程与状态处理

除了 Switch 表之外，代码段下方还包含了一些特定的状态处理逻辑和网络交互流程。

### 异常与特殊状态
*   **搜索游戏不存在**: `6F5BACD7` -> `call 6F5B4D40`
*   **进入自定义游戏 (Enter Game)**: `6F5BACE3`
    *   调用虚函数 `call edx` (偏移 `0xD4`)
    *   调用 `6F009E30` (Glue/UI 过渡处理)

### 网络与列表获取流程
*   **发起获取列表请求**: `6F5BAD1C`
    *   调用 `6F591B70`
    *   调用 `6F5FAF10` (发送 `SID_GETADVLISTEX` 数据包的核心位置)
    *   设置标志位 `6FA9A418`

### 画面/UI 切换 (Screen Transition)
*   **画面切换开始**: `6F5BAD46` -> `call 6F597D00`
*   **画面切换完毕**: `6F5BAD62`
    *   检查状态 (`esi+244`)，决定进入 BattleNet 聊天面板还是其他界面。
    *   相关函数: `call 6F573020` (BattleNetChatPanel)

## 4. 跳转表数据 (Jump Table Data)
*地址 `6F5BADBC` 及其后方的数据并非可执行代码，而是跳转地址表。x64dbg 尝试将其反汇编为指令，导致显示为 `or ch...` 等无意义指令。*

实际内存中存储的是上述 Case 0 到 Case 17 对应的 32 位地址。

---

## 5. 原始汇编代码参考 (game.dll + 5BABF0)

```assembly
6F5BABF0 | 8B4424 04                | mov eax,dword ptr ss:[esp+4]                             |
6F5BABF4 | 56                       | push esi                                                 |
6F5BABF5 | 8BF1                     | mov esi,ecx                                              |
6F5BABF7 | 8B48 08                  | mov ecx,dword ptr ds:[eax+8]                             |
6F5BABFA | 83F9 17                  | cmp ecx,17                                               |
6F5BABFD | 0F87 B2010000            | ja game.6F5BADB5                                         |
6F5BAC03 | FF248D BCAD5B6F          | jmp dword ptr ds:[ecx*4+6F5BADBC]                        | <--- 处理分支
6F5BAC0A | 8BCE                     | mov ecx,esi                                              |
6F5BAC0C | E8 8F70FDFF              | call game.6F591CA0                                       | <--- 选择列表
6F5BAC11 | 5E                       | pop esi                                                  |
6F5BAC12 | C2 0400                  | ret 4                                                    |
6F5BAC15 | 8BCE                     | mov ecx,esi                                              | <--- 键盘输入
6F5BAC17 | E8 A472FDFF              | call game.6F591EC0                                       |
6F5BAC1C | 5E                       | pop esi                                                  |
6F5BAC1D | C2 0400                  | ret 4                                                    |
6F5BAC20 | 8BCE                     | mov ecx,esi                                              | <--- 创建游戏
6F5BAC22 | E8 19D0FDFF              | call game.6F597C40                                       |
6F5BAC27 | 5E                       | pop esi                                                  |
6F5BAC28 | C2 0400                  | ret 4                                                    |
6F5BAC2B | 8BCE                     | mov ecx,esi                                              | <--- 装载游戏
6F5BAC2D | E8 FECFFDFF              | call game.6F597C30                                       |
6F5BAC32 | 5E                       | pop esi                                                  |
6F5BAC33 | C2 0400                  | ret 4                                                    |
6F5BAC36 | 6A 00                    | push 0                                                   | <--- 加入游戏
6F5BAC38 | 8BCE                     | mov ecx,esi                                              |
6F5BAC3A | E8 71A3FFFF              | call game.6F5B4FB0                                       |
6F5BAC3F | 5E                       | pop esi                                                  |
6F5BAC40 | C2 0400                  | ret 4                                                    |
6F5BAC43 | 8BCE                     | mov ecx,esi                                              | <--- 返回大厅
6F5BAC45 | E8 06D0FDFF              | call game.6F597C50                                       |
6F5BAC4A | 5E                       | pop esi                                                  |
6F5BAC4B | C2 0400                  | ret 4                                                    |
6F5BAC4E | 8BCE                     | mov ecx,esi                                              |
6F5BAC50 | E8 9B40FEFF              | call game.6F59ECF0                                       |
6F5BAC55 | 5E                       | pop esi                                                  |
6F5BAC56 | C2 0400                  | ret 4                                                    |
6F5BAC59 | 8BCE                     | mov ecx,esi                                              | <--- 确定关闭对话框
6F5BAC5B | E8 30B0FBFF              | call game.6F575C90                                       |
6F5BAC60 | 5E                       | pop esi                                                  |
6F5BAC61 | C2 0400                  | ret 4                                                    |
6F5BAC64 | 8BCE                     | mov ecx,esi                                              | <--- 设置过滤
6F5BAC66 | E8 35B5FCFF              | call game.6F5861A0                                       |
6F5BAC6B | 5E                       | pop esi                                                  |
6F5BAC6C | C2 0400                  | ret 4                                                    |
6F5BAC6F | 8BCE                     | mov ecx,esi                                              | <--- 刷新列表开始
6F5BAC71 | E8 0A70FDFF              | call game.6F591C80                                       |
6F5BAC76 | 5E                       | pop esi                                                  |
6F5BAC77 | C2 0400                  | ret 4                                                    |
6F5BAC7A | 8BCE                     | mov ecx,esi                                              | <--- 设置过滤确认
6F5BAC7C | E8 DF6FFDFF              | call game.6F591C60                                       |
6F5BAC81 | 5E                       | pop esi                                                  |
6F5BAC82 | C2 0400                  | ret 4                                                    |
6F5BAC85 | 8BCE                     | mov ecx,esi                                              | <--- 设置过滤取消
6F5BAC87 | E8 B4B4FCFF              | call game.6F586140                                       |
6F5BAC8C | 5E                       | pop esi                                                  |
6F5BAC8D | C2 0400                  | ret 4                                                    |
6F5BAC90 | 8BCE                     | mov ecx,esi                                              | <--- 搜索游戏名
6F5BAC92 | E8 B9AFFBFF              | call game.6F575C50                                       |
6F5BAC97 | 5E                       | pop esi                                                  |
6F5BAC98 | C2 0400                  | ret 4                                                    |
6F5BAC9B | 50                       | push eax                                                 |
6F5BAC9C | 8BCE                     | mov ecx,esi                                              |
6F5BAC9E | E8 7DE8FFFF              | call game.6F5B9520                                       |
6F5BACA3 | 5E                       | pop esi                                                  |
6F5BACA4 | C2 0400                  | ret 4                                                    |
6F5BACA7 | 50                       | push eax                                                 | <--- 获取列表结束 循环填入数据
6F5BACA8 | 8BCE                     | mov ecx,esi                                              |
6F5BACAA | E8 01B5FCFF              | call game.6F5861B0                                       |
6F5BACAF | 5E                       | pop esi                                                  |
6F5BACB0 | C2 0400                  | ret 4                                                    |
6F5BACB3 | 50                       | push eax                                                 | <--- 刷新列表完毕
6F5BACB4 | 8BCE                     | mov ecx,esi                                              |
6F5BACB6 | E8 85CEFDFF              | call game.6F597B40                                       |
6F5BACBB | 5E                       | pop esi                                                  |
6F5BACBC | C2 0400                  | ret 4                                                    |
6F5BACBF | 50                       | push eax                                                 |
6F5BACC0 | 8BCE                     | mov ecx,esi                                              |
6F5BACC2 | E8 3972FDFF              | call game.6F591F00                                       |
6F5BACC7 | 5E                       | pop esi                                                  |
6F5BACC8 | C2 0400                  | ret 4                                                    |
6F5BACCB | 50                       | push eax                                                 | <--- 弹出对话框 游戏找不到
6F5BACCC | 8BCE                     | mov ecx,esi                                              |
6F5BACCE | E8 FDA4FFFF              | call game.6F5B51D0                                       |
6F5BACD3 | 5E                       | pop esi                                                  |
6F5BACD4 | C2 0400                  | ret 4                                                    |
6F5BACD7 | 50                       | push eax                                                 | <--- 搜索游戏不存在
6F5BACD8 | 8BCE                     | mov ecx,esi                                              |
6F5BACDA | E8 61A0FFFF              | call game.6F5B4D40                                       |
6F5BACDF | 5E                       | pop esi                                                  |
6F5BACE0 | C2 0400                  | ret 4                                                    |
6F5BACE3 | 8B06                     | mov eax,dword ptr ds:[esi]                               | <--- 进入自定义游戏
6F5BACE5 | 8B90 D4000000            | mov edx,dword ptr ds:[eax+D4]                            |
6F5BACEB | 8BCE                     | mov ecx,esi                                              |
6F5BACED | FFD2                     | call edx                                                 |
6F5BACEF | 6A 16                    | push 16                                                  |
6F5BACF1 | 56                       | push esi                                                 |
6F5BACF2 | 6A 00                    | push 0                                                   |
6F5BACF4 | BA 08A2956F              | mov edx,game.6F95A208                                    |
6F5BACF9 | B9 00A2956F              | mov ecx,game.6F95A200                                    |
6F5BACFE | E8 2DF1A4FF              | call game.6F009E30                                       |
6F5BAD03 | 51                       | push ecx                                                 |
6F5BAD04 | D91C24                   | fstp dword ptr ss:[esp]                                  |
6F5BAD07 | 56                       | push esi                                                 |
6F5BAD08 | 8D8E B8010000            | lea ecx,dword ptr ds:[esi+1B8]                           |
6F5BAD0E | E8 BD9AD5FF              | call game.6F3147D0                                       |
6F5BAD13 | B8 01000000              | mov eax,1                                                |
6F5BAD18 | 5E                       | pop esi                                                  |
6F5BAD19 | C2 0400                  | ret 4                                                    |
6F5BAD1C | 6A 01                    | push 1                                                   | <--- 获取列表开始 发起网络请求
6F5BAD1E | 8BCE                     | mov ecx,esi                                              |
6F5BAD20 | E8 4B6EFDFF              | call game.6F591B70                                       |
6F5BAD25 | 8B8E 04020000            | mov ecx,dword ptr ds:[esi+204]                           |
6F5BAD2B | 6A 00                    | push 0                                                   |
6F5BAD2D | 6A 00                    | push 0                                                   |
6F5BAD2F | 6A 01                    | push 1                                                   |
6F5BAD31 | E8 DA010400              | call game.6F5FAF10                                       |
6F5BAD36 | 830D 18A4A96F 01         | or dword ptr ds:[6FA9A418],1                             |
6F5BAD3D | B8 01000000              | mov eax,1                                                |
6F5BAD42 | 5E                       | pop esi                                                  |
6F5BAD43 | C2 0400                  | ret 4                                                    |
6F5BAD46 | 8B06                     | mov eax,dword ptr ds:[esi]                               | <--- 画面切换开始
6F5BAD48 | 8B90 D0000000            | mov edx,dword ptr ds:[eax+D0]                            |
6F5BAD4E | 8BCE                     | mov ecx,esi                                              |
6F5BAD50 | FFD2                     | call edx                                                 |
6F5BAD52 | 8BCE                     | mov ecx,esi                                              |
6F5BAD54 | E8 A7CFFDFF              | call game.6F597D00                                       |
6F5BAD59 | B8 01000000              | mov eax,1                                                |
6F5BAD5E | 5E                       | pop esi                                                  |
6F5BAD5F | C2 0400                  | ret 4                                                    |
6F5BAD62 | 830D 18A4A96F 01         | or dword ptr ds:[6FA9A418],1                             | <--- 画面切换完毕
6F5BAD69 | 8B86 44020000            | mov eax,dword ptr ds:[esi+244]                           |
6F5BAD6F | 83F8 FE                  | cmp eax,FFFFFFFE                                         |
6F5BAD72 | 8B8E 68010000            | mov ecx,dword ptr ds:[esi+168]                           |
6F5BAD78 | 74 2B                    | je game.6F5BADA5                                         |
6F5BAD7A | 83F8 FF                  | cmp eax,FFFFFFFF                                         |
6F5BAD7D | 74 16                    | je game.6F5BAD95                                         |
6F5BAD7F | 8B0485 18FB956F          | mov eax,dword ptr ds:[eax*4+6F95FB18]                    |
6F5BAD86 | 50                       | push eax                                                 |
6F5BAD87 | E8 9482FBFF              | call game.6F573020                                       |
6F5BAD8C | B8 01000000              | mov eax,1                                                |
6F5BAD91 | 5E                       | pop esi                                                  |
6F5BAD92 | C2 0400                  | ret 4                                                    |
6F5BAD95 | 6A 02                    | push 2                                                   |
6F5BAD97 | E8 4483FEFF              | call game.6F5A30E0                                       |
6F5BAD9C | B8 01000000              | mov eax,1                                                |
6F5BADA1 | 5E                       | pop esi                                                  |
6F5BADA2 | C2 0400                  | ret 4                                                    |
6F5BADA5 | 6A 0B                    | push B                                                   |
6F5BADA7 | E8 3483FEFF              | call game.6F5A30E0                                       |
6F5BADAC | B8 01000000              | mov eax,1                                                |
6F5BADB1 | 5E                       | pop esi                                                  |
6F5BADB2 | C2 0400                  | ret 4                                                    |
6F5BADB5 | 33C0                     | xor eax,eax                                              |
6F5BADB7 | 5E                       | pop esi                                                  |
6F5BADB8 | C2 0400                  | ret 4                                                    |
6F5BADBB | 90                       | nop                                                      |
6F5BADBC | 0AAC5B 6F15AC5B          | or ch,byte ptr ds:[ebx+ebx*2+5BAC156F]                   |
6F5BADC3 | 6F                       | outsd                                                    |
6F5BADC4 | 20AC5B 6F2BAC5B          | and byte ptr ds:[ebx+ebx*2+5BAC2B6F],ch                  |
6F5BADCB | 6F                       | outsd                                                    |
6F5BADCC | 36:AC                    | lodsb                                                    |
6F5BADCE | 5B                       | pop ebx                                                  |
6F5BADCF | 6F                       | outsd                                                    |
6F5BADD0 | 64:AC                    | lodsb                                                    |
6F5BADD2 | 5B                       | pop ebx                                                  |
6F5BADD3 | 6F                       | outsd                                                    |
6F5BADD4 | 6F                       | outsd                                                    |
6F5BADD5 | AC                       | lodsb                                                    |
6F5BADD6 | 5B                       | pop ebx                                                  |
6F5BADD7 | 6F                       | outsd                                                    |
6F5BADD8 | 43                       | inc ebx                                                  |
6F5BADD9 | AC                       | lodsb                                                    |
6F5BADDA | 5B                       | pop ebx                                                  |
6F5BADDB | 6F                       | outsd                                                    |
6F5BADDC | 4E                       | dec esi                                                  |
6F5BADDD | AC                       | lodsb                                                    |
6F5BADDE | 5B                       | pop ebx                                                  |
6F5BADDF | 6F                       | outsd                                                    |
6F5BADE0 | 59                       | pop ecx                                                  |
6F5BADE1 | AC                       | lodsb                                                    |
6F5BADE2 | 5B                       | pop ebx                                                  |
6F5BADE3 | 6F                       | outsd                                                    |
6F5BADE4 | 7A AC                    | jp game.6F5BAD92                                         |
6F5BADE6 | 5B                       | pop ebx                                                  |
6F5BADE7 | 6F                       | outsd                                                    |
6F5BADE8 | 85AC5B 6F90AC5B          | test dword ptr ds:[ebx+ebx*2+5BAC906F],ebp               |
6F5BADEF | 6F                       | outsd                                                    |
6F5BADF0 | 9B                       | fwait                                                    |
6F5BADF1 | AC                       | lodsb                                                    |
6F5BADF2 | 5B                       | pop ebx                                                  |
6F5BADF3 | 6F                       | outsd                                                    |
6F5BADF4 | CB                       | ret far                                                  |
6F5BADF5 | AC                       | lodsb                                                    |
6F5BADF6 | 5B                       | pop ebx                                                  |
6F5BADF7 | 6F                       | outsd                                                    |
6F5BADF8 | B5 AD                    | mov ch,AD                                                |
6F5BADFA | 5B                       | pop ebx                                                  |
6F5BADFB | 6F                       | outsd                                                    |
6F5BADFC | A7                       | cmpsd                                                    |
6F5BADFD | AC                       | lodsb                                                    |
6F5BADFE | 5B                       | pop ebx                                                  |
6F5BADFF | 6F                       | outsd                                                    |
6F5BAE00 | B3 AC                    | mov bl,AC                                                |
6F5BAE02 | 5B                       | pop ebx                                                  |
6F5BAE03 | 6F                       | outsd                                                    |
6F5BAE04 | BF AC5B6FD7              | mov edi,D76F5BAC                                         |
6F5BAE09 | AC                       | lodsb                                                    |
6F5BAE0A | 5B                       | pop ebx                                                  |
6F5BAE0B | 6F                       | outsd                                                    |
6F5BAE0C | E3 AC                    | jecxz game.6F5BADBA                                      |
6F5BAE0E | 5B                       | pop ebx                                                  |
6F5BAE0F | 6F                       | outsd                                                    |
6F5BAE10 | 62AD 5B6F1CAD            | bound ebp,qword ptr ss:[ebp-52E390A5]                    |
6F5BAE16 | 5B                       | pop ebx                                                  |
6F5BAE17 | 6F                       | outsd                                                    |
6F5BAE18 | 46                       | inc esi                                                  |
6F5BAE19 | AD                       | lodsd                                                    |
6F5BAE1A | 5B                       | pop ebx                                                  |
6F5BAE1B | 6F                       | outsd                                                    |
```
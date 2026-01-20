### 函数分析总结：局域网网络事件分发器 (Network Event Dispatcher)

**函数名称**：`NetRouter::DispatchNetworkEvent` (推测)
**内存地址**：`0x6F672340` (Game.dll Base + 0x672340)
**功能描述**：
这是 `Game.dll` 网络层的核心**事件分发函数**。它接收一个事件 ID（Event ID），通过查表（Switch-Case）跳转到对应的处理逻辑。它是上层 UI 命令或底层网络包解析后，驱动游戏状态流转的总枢纽。

---

### 1. 核心逻辑流程

1.  **对象检查**：
    *   检查传入的网络对象指针是否为空。
    *   检查偏移 `+0x54` 处的标志位（可能代表是否已初始化）。

2.  **事件 ID 归一化**：
    *   从 `[esp+10]` 获取原始 Event ID。
    *   执行 `add edi, FFFFFFE3` (即减去 29/0x1D)。
    *   这意味着有效的事件 ID 起始于 **29 (0x1D)**。

3.  **越界检查**：
    *   `cmp edi, 2F` (47)：最大处理 48 个不同类型的事件。
    *   如果超出范围，跳转到默认处理或错误返回。

4.  **跳转表分发 (Jump Table)**：
    *   `jmp dword ptr ds:[edi*4+6F672654]`
    *   根据归一化后的 ID，从 `6F672654` 开始的数组中取出函数地址并跳转。

---

### 2. 关键分支解析

根据汇编代码中的 `Call` 目标，我们可以还原出部分关键事件的含义：

| 原始 Event ID | 归一化 ID (edi) | 跳转地址 (Jump Target) | 调用的核心函数 | 功能推测 (Context) |
| :--- | :--- | :--- | :--- | :--- |
| **29 (0x1D)** | **0** | **`6F67238A`** | **`call 6F671960`** | **★ 创建游戏/启动主机 (SNetCreateGame)** |
| 30 (0x1E) | 1 | `6F67239F` | `call 6F671D20` | 玩家加入处理 (Player Join) |
| ... | ... | ... | ... | ... |
| 34 (0x22) | 5 | `6F6723DE` | `call 6F66DFE0` | 游戏动作/状态更新 3 |
| 35 (0x23) | 6 | `6F6723F3` | `call 6F66E0E0` | 游戏动作/状态更新 2 |
| 36 (0x24) | 7 | `6F672408` | `call 6F66E240` | **开始倒计时 (Start Countdown)** |
| 37 (0x25) | 8 | `6F67241D` | `call 6F66FBE0` | **倒计时结束 (Countdown End)** |
| 38 (0x26) | 9 | `6F672432` | `call 6F66C4B0` | 离开游戏/销毁 (Leave Game) |
| 40 (0x28) | 11 | `6F67245C` | `call 6F66E430` | **地图加载完毕 (Load Complete)** |
| 41 (0x29) | 12 | `6F672471` | `call 6F671000` | 输入监听/键盘钩子 (Input Hook) |
| 43 (0x2B) | 14 | `6F6724A6` | `call 6F66E4F0` | 聊天/槽位信息更新 |
| 44 (0x2C) | 15 | `6F6724B0` | `call 6F66E7A0` | **踢人/玩家掉线 (Drop Player)** |

---

### 3. 数据区还原 (Jump Table Analysis)

代码末尾的“乱码”实际上是**跳转表数组**。反汇编器错误地将其解析为了指令。

*   地址 `6F672654`: `8A 23 67 6F` -> **`0x6F67238A`**
    *   对应 Event ID 29。
    *   正是之前发现的调用 `6F671960` (主机总管) 的分支。

---

### 4. 对“房主转让”的意义

这个函数确认了 **Event ID 29 (0x1D)** 是触发“主机创建流程”的信号。

*   **再次印证**：`6F671960` 是高层封装，它内部会调用 `6F671910` (点火器)。
*   **Hook 启示**：
    *   如果能找到是谁调用了这个 `DispatchNetworkEvent` 函数，并且传入了参数 `0x1D`，那就是 UI 界面点击“创建游戏”按钮后的第一反应堆。
    *   然而，对于 PID 1 这种已经在房间里的情况，**直接调用这个分发函数可能会因为状态检查（如 `test esi, esi` 或 `cmp [esi+54], 0`）而被拦截**，或者触发完整的创建流程导致掉线。

**结论**：这个函数是**路由（Router）**，虽然重要，但它是通用的入口。为了实现无缝转让，**直接调用 `6F671910` (Level 3 点火器) 依然是避开这些繁琐状态检查的最佳捷径**。

```assembly
6F672340  | 8B4424 04               | mov eax,dword ptr ss:[esp+4]                    |
6F672344  | 56                      | push esi                                        |
6F672345  | 8B30                    | mov esi,dword ptr ds:[eax]                      |
6F672347  | 85F6                    | test esi,esi                                    |
6F672349  | 74 0C                   | je game.6F672357                                |
6F67234B  | 837E 54 00              | cmp dword ptr ds:[esi+54],0                     |
6F67234F  | 74 06                   | je game.6F672357                                |
6F672351  | 33C0                    | xor eax,eax                                     |
6F672353  | 5E                      | pop esi                                         |
6F672354  | C2 1400                 | ret 14                                          |
6F672357  | 57                      | push edi                                        |
6F672358  | 0FB67C24 10             | movzx edi,byte ptr ss:[esp+10]                  | <--- 取出一个字节参数 (Mode
6F67235D  | 83C7 E3                 | add edi,FFFFFFE3                                | <--- edi = Mode - 0x1D (29)
6F672360  | 83FF 2F                 | cmp edi,2F                                      | <--- 检查是否越界 (> 47)
6F672363  | B8 01000000             | mov eax,1                                       |
6F672368  | 0F87 E1020000           | ja game.6F67264F                                | <--- 越界则跳转到默认/错误处理
6F67236E  | FF24BD 5426676F         | jmp dword ptr ds:[edi*4+6F672654]               | <--- 暂停条件(edi!=A)，这个是之前的全局变量A么？[0x6FACFEFC]: 值为A
6F672375  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   | <--- 心跳包，执行动作1
6F672379  | 50                      | push eax                                        |
6F67237A  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F67237E  | 50                      | push eax                                        |
6F67237F  | 56                      | push esi                                        |
6F672380  | E8 8B58FFFF             | call game.6F667C10                              |
6F672385  | 5F                      | pop edi                                         |
6F672386  | 5E                      | pop esi                                         |
6F672387  | C2 1400                 | ret 14                                          |
6F67238A  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   | <--- 创建游戏
6F67238E  | 50                      | push eax                                        |
6F67238F  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672393  | 50                      | push eax                                        |
6F672394  | 56                      | push esi                                        |
6F672395  | E8 C6F5FFFF             | call game.6F671960                              | <--- 主机专属
6F67239A  | 5F                      | pop edi                                         |
6F67239B  | 5E                      | pop esi                                         |
6F67239C  | C2 1400                 | ret 14                                          |
6F67239F  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   | <--- 有人加入
6F6723A3  | 50                      | push eax                                        |
6F6723A4  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6723A8  | 50                      | push eax                                        |
6F6723A9  | 56                      | push esi                                        |
6F6723AA  | E8 71F9FFFF             | call game.6F671D20                              |
6F6723AF  | 5F                      | pop edi                                         |
6F6723B0  | 5E                      | pop esi                                         |
6F6723B1  | C2 1400                 | ret 14                                          |
6F6723B4  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6723B8  | 50                      | push eax                                        |
6F6723B9  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6723BD  | 50                      | push eax                                        |
6F6723BE  | 56                      | push esi                                        |
6F6723BF  | E8 1C21FFFF             | call game.6F6644E0                              |
6F6723C4  | 5F                      | pop edi                                         |
6F6723C5  | 5E                      | pop esi                                         |
6F6723C6  | C2 1400                 | ret 14                                          |
6F6723C9  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6723CD  | 50                      | push eax                                        |
6F6723CE  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6723D2  | 50                      | push eax                                        |
6F6723D3  | 56                      | push esi                                        |
6F6723D4  | E8 1721FFFF             | call game.6F6644F0                              |
6F6723D9  | 5F                      | pop edi                                         |
6F6723DA  | 5E                      | pop esi                                         |
6F6723DB  | C2 1400                 | ret 14                                          |
6F6723DE  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   | <--- 执行动作3
6F6723E2  | 50                      | push eax                                        |
6F6723E3  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6723E7  | 50                      | push eax                                        |
6F6723E8  | 56                      | push esi                                        |
6F6723E9  | E8 F2BBFFFF             | call game.6F66DFE0                              |
6F6723EE  | 5F                      | pop edi                                         |
6F6723EF  | 5E                      | pop esi                                         |
6F6723F0  | C2 1400                 | ret 14                                          |
6F6723F3  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   | <--- 执行动作2
6F6723F7  | 50                      | push eax                                        |
6F6723F8  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6723FC  | 50                      | push eax                                        |
6F6723FD  | 56                      | push esi                                        |
6F6723FE  | E8 DDBCFFFF             | call game.6F66E0E0                              |
6F672403  | 5F                      | pop edi                                         |
6F672404  | 5E                      | pop esi                                         |
6F672405  | C2 1400                 | ret 14                                          |
6F672408  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   | <--- 开始倒计时
6F67240C  | 50                      | push eax                                        |
6F67240D  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672411  | 50                      | push eax                                        |
6F672412  | 56                      | push esi                                        |
6F672413  | E8 28BEFFFF             | call game.6F66E240                              |
6F672418  | 5F                      | pop edi                                         |
6F672419  | 5E                      | pop esi                                         |
6F67241A  | C2 1400                 | ret 14                                          |
6F67241D  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   | <--- 倒计时完毕
6F672421  | 50                      | push eax                                        |
6F672422  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672426  | 50                      | push eax                                        |
6F672427  | 56                      | push esi                                        |
6F672428  | E8 B3D7FFFF             | call game.6F66FBE0                              |
6F67242D  | 5F                      | pop edi                                         |
6F67242E  | 5E                      | pop esi                                         |
6F67242F  | C2 1400                 | ret 14                                          |
6F672432  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   | <--- 离开房间或结束游戏
6F672436  | 50                      | push eax                                        |
6F672437  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F67243B  | 50                      | push eax                                        |
6F67243C  | 56                      | push esi                                        |
6F67243D  | E8 6EA0FFFF             | call game.6F66C4B0                              |
6F672442  | 5F                      | pop edi                                         |
6F672443  | 5E                      | pop esi                                         |
6F672444  | C2 1400                 | ret 14                                          |
6F672447  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F67244B  | 50                      | push eax                                        |
6F67244C  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672450  | 50                      | push eax                                        |
6F672451  | 56                      | push esi                                        |
6F672452  | E8 A9BEFFFF             | call game.6F66E300                              |
6F672457  | 5F                      | pop edi                                         |
6F672458  | 5E                      | pop esi                                         |
6F672459  | C2 1400                 | ret 14                                          |
6F67245C  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   | <--- 加载完毕
6F672460  | 50                      | push eax                                        |
6F672461  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672465  | 50                      | push eax                                        |
6F672466  | 56                      | push esi                                        |
6F672467  | E8 C4BFFFFF             | call game.6F66E430                              |
6F67246C  | 5F                      | pop edi                                         |
6F67246D  | 5E                      | pop esi                                         |
6F67246E  | C2 1400                 | ret 14                                          |
6F672471  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   | <--- 键盘监听
6F672475  | 50                      | push eax                                        |
6F672476  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F67247A  | 50                      | push eax                                        |
6F67247B  | 56                      | push esi                                        |
6F67247C  | E8 7FEBFFFF             | call game.6F671000                              |
6F672481  | 5F                      | pop edi                                         |
6F672482  | 5E                      | pop esi                                         |
6F672483  | C2 1400                 | ret 14                                          |
6F672486  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   | <--- 循环执行
6F67248A  | 50                      | push eax                                        |
6F67248B  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F67248F  | 50                      | push eax                                        |
6F672490  | 56                      | push esi                                        |
6F672491  | E8 0AD8FFFF             | call game.6F66FCA0                              |
6F672496  | 5F                      | pop edi                                         |
6F672497  | 5E                      | pop esi                                         |
6F672498  | C2 1400                 | ret 14                                          |
6F67249B  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   | <--- 切换槽位，聊天等消息
6F67249F  | 50                      | push eax                                        |
6F6724A0  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6724A4  | 50                      | push eax                                        |
6F6724A5  | 56                      | push esi                                        |
6F6724A6  | E8 45C0FFFF             | call game.6F66E4F0                              |
6F6724AB  | 5F                      | pop edi                                         |
6F6724AC  | 5E                      | pop esi                                         |
6F6724AD  | C2 1400                 | ret 14                                          |
6F6724B0  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   | <--- 玩家掉线踢出玩家
6F6724B4  | 50                      | push eax                                        |
6F6724B5  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6724B9  | 50                      | push eax                                        |
6F6724BA  | 56                      | push esi                                        |
6F6724BB  | E8 E0C2FFFF             | call game.6F66E7A0                              |
6F6724C0  | 5F                      | pop edi                                         |
6F6724C1  | 5E                      | pop esi                                         |
6F6724C2  | C2 1400                 | ret 14                                          |
6F6724C5  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6724C9  | 50                      | push eax                                        |
6F6724CA  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6724CE  | 50                      | push eax                                        |
6F6724CF  | 56                      | push esi                                        |
6F6724D0  | E8 2B20FFFF             | call game.6F664500                              |
6F6724D5  | 5F                      | pop edi                                         |
6F6724D6  | 5E                      | pop esi                                         |
6F6724D7  | C2 1400                 | ret 14                                          |
6F6724DA  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6724DE  | 50                      | push eax                                        |
6F6724DF  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6724E3  | 50                      | push eax                                        |
6F6724E4  | 56                      | push esi                                        |
6F6724E5  | E8 A6C3FFFF             | call game.6F66E890                              |
6F6724EA  | 5F                      | pop edi                                         |
6F6724EB  | 5E                      | pop esi                                         |
6F6724EC  | C2 1400                 | ret 14                                          |
6F6724EF  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6724F3  | 50                      | push eax                                        |
6F6724F4  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6724F8  | 50                      | push eax                                        |
6F6724F9  | 56                      | push esi                                        |
6F6724FA  | E8 01A0FFFF             | call game.6F66C500                              |
6F6724FF  | 5F                      | pop edi                                         |
6F672500  | 5E                      | pop esi                                         |
6F672501  | C2 1400                 | ret 14                                          |
6F672504  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672508  | 50                      | push eax                                        |
6F672509  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F67250D  | 50                      | push eax                                        |
6F67250E  | 56                      | push esi                                        |
6F67250F  | E8 DCC5FFFF             | call game.6F66EAF0                              |
6F672514  | 5F                      | pop edi                                         |
6F672515  | 5E                      | pop esi                                         |
6F672516  | C2 1400                 | ret 14                                          |
6F672519  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F67251D  | 50                      | push eax                                        |
6F67251E  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672522  | 50                      | push eax                                        |
6F672523  | 56                      | push esi                                        |
6F672524  | E8 E71FFFFF             | call game.6F664510                              |
6F672529  | 5F                      | pop edi                                         |
6F67252A  | 5E                      | pop esi                                         |
6F67252B  | C2 1400                 | ret 14                                          |
6F67252E  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672532  | 50                      | push eax                                        |
6F672533  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672537  | 50                      | push eax                                        |
6F672538  | 56                      | push esi                                        |
6F672539  | E8 E21FFFFF             | call game.6F664520                              |
6F67253E  | 5F                      | pop edi                                         |
6F67253F  | 5E                      | pop esi                                         |
6F672540  | C2 1400                 | ret 14                                          |
6F672543  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672547  | 50                      | push eax                                        |
6F672548  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F67254C  | 50                      | push eax                                        |
6F67254D  | 56                      | push esi                                        |
6F67254E  | E8 DD59FFFF             | call game.6F667F30                              |
6F672553  | 5F                      | pop edi                                         |
6F672554  | 5E                      | pop esi                                         |
6F672555  | C2 1400                 | ret 14                                          |
6F672558  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F67255C  | 50                      | push eax                                        |
6F67255D  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672561  | 50                      | push eax                                        |
6F672562  | 56                      | push esi                                        |
6F672563  | E8 C81FFFFF             | call game.6F664530                              |
6F672568  | 5F                      | pop edi                                         |
6F672569  | 5E                      | pop esi                                         |
6F67256A  | C2 1400                 | ret 14                                          |
6F67256D  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672571  | 50                      | push eax                                        |
6F672572  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672576  | 50                      | push eax                                        |
6F672577  | 56                      | push esi                                        |
6F672578  | E8 A3C7FFFF             | call game.6F66ED20                              |
6F67257D  | 5F                      | pop edi                                         |
6F67257E  | 5E                      | pop esi                                         |
6F67257F  | C2 1400                 | ret 14                                          |
6F672582  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   | <--- 地图检测？
6F672586  | 50                      | push eax                                        |
6F672587  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F67258B  | 50                      | push eax                                        |
6F67258C  | 56                      | push esi                                        |
6F67258D  | E8 FED9FFFF             | call game.6F66FF90                              |
6F672592  | 5F                      | pop edi                                         |
6F672593  | 5E                      | pop esi                                         |
6F672594  | C2 1400                 | ret 14                                          |
6F672597  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F67259B  | 50                      | push eax                                        |
6F67259C  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6725A0  | 50                      | push eax                                        |
6F6725A1  | 56                      | push esi                                        |
6F6725A2  | E8 C9DBFFFF             | call game.6F670170                              |
6F6725A7  | 5F                      | pop edi                                         |
6F6725A8  | 5E                      | pop esi                                         |
6F6725A9  | C2 1400                 | ret 14                                          |
6F6725AC  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6725B0  | 50                      | push eax                                        |
6F6725B1  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6725B5  | 50                      | push eax                                        |
6F6725B6  | 56                      | push esi                                        |
6F6725B7  | E8 A4C9FFFF             | call game.6F66EF60                              |
6F6725BC  | 5F                      | pop edi                                         |
6F6725BD  | 5E                      | pop esi                                         |
6F6725BE  | C2 1400                 | ret 14                                          |
6F6725C1  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6725C5  | 50                      | push eax                                        |
6F6725C6  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6725CA  | 50                      | push eax                                        |
6F6725CB  | 56                      | push esi                                        |
6F6725CC  | E8 7FCAFFFF             | call game.6F66F050                              |
6F6725D1  | 5F                      | pop edi                                         |
6F6725D2  | 5E                      | pop esi                                         |
6F6725D3  | C2 1400                 | ret 14                                          |
6F6725D6  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6725DA  | 50                      | push eax                                        |
6F6725DB  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6725DF  | 50                      | push eax                                        |
6F6725E0  | 56                      | push esi                                        |
6F6725E1  | E8 9A2FFFFF             | call game.6F665580                              |
6F6725E6  | 5F                      | pop edi                                         |
6F6725E7  | 5E                      | pop esi                                         |
6F6725E8  | C2 1400                 | ret 14                                          |
6F6725EB  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6725EF  | 50                      | push eax                                        |
6F6725F0  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F6725F4  | 50                      | push eax                                        |
6F6725F5  | 56                      | push esi                                        |
6F6725F6  | E8 C559FFFF             | call game.6F667FC0                              |
6F6725FB  | 5F                      | pop edi                                         |
6F6725FC  | 5E                      | pop esi                                         |
6F6725FD  | C2 1400                 | ret 14                                          |
6F672600  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672604  | 50                      | push eax                                        |
6F672605  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672609  | 50                      | push eax                                        |
6F67260A  | 56                      | push esi                                        |
6F67260B  | E8 E059FFFF             | call game.6F667FF0                              |
6F672610  | 5F                      | pop edi                                         |
6F672611  | 5E                      | pop esi                                         |
6F672612  | C2 1400                 | ret 14                                          |
6F672615  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672619  | 50                      | push eax                                        |
6F67261A  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F67261E  | 50                      | push eax                                        |
6F67261F  | 56                      | push esi                                        |
6F672620  | E8 2B1FFFFF             | call game.6F664550                              |
6F672625  | 5F                      | pop edi                                         |
6F672626  | 5E                      | pop esi                                         |
6F672627  | C2 1400                 | ret 14                                          |
6F67262A  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F67262E  | 50                      | push eax                                        |
6F67262F  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672633  | 50                      | push eax                                        |
6F672634  | 56                      | push esi                                        |
6F672635  | E8 D659FFFF             | call game.6F668010                              |
6F67263A  | 5F                      | pop edi                                         |
6F67263B  | 5E                      | pop esi                                         |
6F67263C  | C2 1400                 | ret 14                                          |
6F67263F  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672643  | 50                      | push eax                                        |
6F672644  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F672648  | 50                      | push eax                                        |
6F672649  | 56                      | push esi                                        |
6F67264A  | E8 E159FFFF             | call game.6F668030                              |
6F67264F  | 5F                      | pop edi                                         |
6F672650  | 5E                      | pop esi                                         |
6F672651  | C2 1400                 | ret 14                                          |
6F672654  | 8A23                    | mov ah,byte ptr ds:[ebx]                        |
6F672656  | 67:6F                   | outsd                                           |
6F672658  | 9F                      | lahf                                            |
6F672659  | 2367 6F                 | and esp,dword ptr ds:[edi+6F]                   |
6F67265C  | 082467                  | or byte ptr ds:[edi],ah                         |
6F67265F  | 6F                      | outsd                                           |
6F672660  | 1D 24676F32             | sbb eax,326F6724                                |
6F672665  | 24 67                   | and al,67                                       |
6F672667  | 6F                      | outsd                                           |
6F672668  | 47                      | inc edi                                         |
6F672669  | 24 67                   | and al,67                                       |
6F67266B  | 6F                      | outsd                                           |
6F67266C  | 5C                      | pop esp                                         |
6F67266D  | 24 67                   | and al,67                                       |
6F67266F  | 6F                      | outsd                                           |
6F672670  | DE23                    | fisub word ptr ds:[ebx]                         |
6F672672  | 67:6F                   | outsd                                           |
6F672674  | F3:2367 6F              | and esp,dword ptr ds:[edi+6F]                   |
6F672678  | 71 24                   | jno game.6F67269E                               |
6F67267A  | 67:6F                   | outsd                                           |
6F67267C  | 862467                  | xchg byte ptr ds:[edi],ah                       |
6F67267F  | 6F                      | outsd                                           |
6F672680  | 9B                      | fwait                                           |
6F672681  | 24 67                   | and al,67                                       |
6F672683  | 6F                      | outsd                                           |
6F672684  | B0 24                   | mov al,24                                       |
6F672686  | 67:6F                   | outsd                                           |
6F672688  | C52467                  | lds esp,fword ptr ds:[edi]                      |
6F67268B  | 6F                      | outsd                                           |
6F67268C  | DA2467                  | fisub dword ptr ds:[edi]                        |
6F67268F  | 6F                      | outsd                                           |
6F672690  | EF                      | out dx,eax                                      |
6F672691  | 24 67                   | and al,67                                       |
6F672693  | 6F                      | outsd                                           |
6F672694  | 04 25                   | add al,25                                       |
6F672696  | 67:6F                   | outsd                                           |
6F672698  | 2E:25 676F4F26          | and eax,264F6F67                                |
6F67269E  | 67:6F                   | outsd                                           |
6F6726A0  | 4F                      | dec edi                                         |
6F6726A1  | 2667:6F                 | outsd                                           |
6F6726A4  | 4F                      | dec edi                                         |
6F6726A5  | 2667:6F                 | outsd                                           |
6F6726A8  | 4F                      | dec edi                                         |
6F6726A9  | 2667:6F                 | outsd                                           |
6F6726AC  | 4F                      | dec edi                                         |
6F6726AD  | 2667:6F                 | outsd                                           |
6F6726B0  | D6                      | salc                                            |
6F6726B1  | 25 676FEB25             | and eax,25EB6F67                                |
6F6726B6  | 67:6F                   | outsd                                           |
6F6726B8  | 0026                    | add byte ptr ds:[esi],ah                        |
6F6726BA  | 67:6F                   | outsd                                           |
6F6726BC  | 15 26676F2A             | adc eax,2A6F6726                                |
6F6726C1  | 2667:6F                 | outsd                                           |
6F6726C4  | 3F                      | aas                                             |
6F6726C5  | 2667:6F                 | outsd                                           |
6F6726C8  | 1925 676F4325           | sbb dword ptr ds:[25436F67],esp                 |
6F6726CE  | 67:6F                   | outsd                                           |
6F6726D0  | 58                      | pop eax                                         |
6F6726D1  | 25 676F4F26             | and eax,264F6F67                                |
6F6726D6  | 67:6F                   | outsd                                           |
6F6726D8  | 4F                      | dec edi                                         |
6F6726D9  | 2667:6F                 | outsd                                           |
6F6726DC  | 4F                      | dec edi                                         |
6F6726DD  | 2667:6F                 | outsd                                           |
6F6726E0  | 4F                      | dec edi                                         |
6F6726E1  | 2667:6F                 | outsd                                           |
6F6726E4  | 6D                      | insd                                            |
6F6726E5  | 25 676F8225             | and eax,25826F67                                |
6F6726EA  | 67:6F                   | outsd                                           |
6F6726EC  | 97                      | xchg edi,eax                                    |
6F6726ED  | 25 676FAC25             | and eax,25AC6F67                                |
6F6726F2  | 67:6F                   | outsd                                           |
6F6726F4  | C125 676F7523 67        | shl dword ptr ds:[23756F67],67                  |
6F6726FB  | 6F                      | outsd                                           |
6F6726FC  | 4F                      | dec edi                                         |
6F6726FD  | 2667:6F                 | outsd                                           |
6F672700  | 4F                      | dec edi                                         |
6F672701  | 2667:6F                 | outsd                                           |
6F672704  | 4F                      | dec edi                                         |
6F672705  | 2667:6F                 | outsd                                           |
6F672708  | 4F                      | dec edi                                         |
6F672709  | 2667:6F                 | outsd                                           |
6F67270C  | B4 23                   | mov ah,23                                       |
6F67270E  | 67:6F                   | outsd                                           |
6F672710  | C9                      | leave                                           |
6F672711  | 2367 6F                 | and esp,dword ptr ds:[edi+6F]                   |
```
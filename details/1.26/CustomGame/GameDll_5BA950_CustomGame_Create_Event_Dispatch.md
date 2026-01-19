# War3 创建游戏菜单事件分发 (Create Game Event Dispatch)

- **模块**: `Game.dll`
- **功能**: 处理“创建游戏”界面（Local/LAN 或 Battle.net）的 UI 事件与逻辑回调。
- **当前状态**: 玩家位于创建游戏菜单，正在配置游戏参数。
- **入口偏移**: `5BA950`
- **内存地址**: `6F5BA950` (基址 6F000000)
- **跳转表地址**: `6F5BAB94`
- **跳转指令**: `jmp dword ptr ds:[ecx*4+6F5BAB94]`

## 1. 核心分发逻辑

代码读取事件 ID (`ECX`)，最大支持到 **0x14 (20)**。通过跳转表分发到具体的 UI 控件处理函数。

## 2. 事件 ID 与功能映射表 (Switch-Case Mapping)

根据汇编代码逻辑及上下文推测，整理如下：

| Event ID (Hex) | Decimal | 跳转地址 (Target) | 核心调用 | 功能推测 (Description) |
| :--- | :--- | :--- | :--- | :--- |
| **0x00** | 0 | `6F5BA96A` | `call 6F585C50` | **游戏速度变更** (Game Speed Slider) |
| **0x01** | 1 | `6F5BA975` | `call 6F5919D0` | **地图列表选择** (Map List Select) |
| **0x02** | 2 | `6F5BA980` | `call 6F585C60` | **地图信息更新** (Map Info Update) |
| **0x03** | 3 | `6F5BA98B` | `call 6F585C70` | **高级选项** (Advanced Options) |
| **0x04** | 4 | `6F5BA996` | `call 6F5B4910` | **点击创建按钮** (Button: Create Game) |
| **0x05** | 5 | `6F5BA9A3` | `call 6F585E70` | **取消/返回** (Cancel) |
| **0x06** | 6 | `6F5BA9AE` | `call 6F591980` | **地图文件夹变更** (Directory Change) |
| **0x07** | 7 | `6F5BA9B9` | `call 6F574D40` | **模版/类型选择** |
| **0x08** | 8 | `6F5BA9C4` | `call 6F574CB0` | **公开/私有游戏切换** (Public/Private Radio) |
| **0x09** | 9 | `6F5BA9D0` | `call 6F5B9500` | **观察者设置** (Observers) |
| **0x0A** | 10 | `6F5BA9DC` | `call 6F5B4CA0` | **页面切换开始** (Page Transition Start) |
| **0x0B** | 11 | `6F5BA9E8` | `call 6F585E10` | **[关键] 执行创建逻辑** (Execute Creation) |
| **0x0C** | 12 | `6F5BA9F4` | `call 6F585E50` | **未知 UI 更新** |
| **0x0D** | 13 | `6F5BAA00` | `call 6F597990` | **容器切换/刷新** |
| **0x0F** | 15 | `6F5BAA34` | `call 6F009E30` | **进入下一屏** (Glue: Fade Out/Transition) |
| **0x13** | 19 | `6F5BAB10` | `call 6F597AF0` | **隐藏当前页** |
| **0x14** | 20 | `6F5BAB2C` | `call 6F573020` | **战网聊天面板交互** (BattleNetChatPanel) |

---

## 3. 关键分支详解

### Case 0x04 & 0x0B (4 & 11): 创建游戏逻辑
这是实现“静默建主”需要关注的核心。
*   **Case 4 (`6F5B4910`)**: 通常是“创建游戏”按钮的点击事件处理。它可能会进行参数校验（地图是否存在、名字是否合法），然后调用底层网络函数，最后触发 UI 跳转。
*   **Case 11 (`6F585E10`)**: 可能是实际执行创建动作的回调，或者是双击地图列表触发的“快速创建”。
*   **逆向建议**: 的目标是避免 UI 跳转。需要进入这两个函数内部，找到 `SNetCreateGame` 或 `CNetGame::Create` 的调用点，并剥离掉随后的 `Glue` 切换代码。

### Case 0x0F & 0x11 (15 & 17): 界面转场 (Glue Transition)
*   **地址**: `6F5BAA34` / `6F5BAAB7`
*   **功能**: 调用 `6F009E30` (Glue Manager) 和 `6F3147D0` (Fade/Sleep)。
*   **意义**: 这些指令负责处理屏幕变黑、淡入淡出以及切换到“房间大厅(Lobby)”的操作。如果想“只创建不切换”，这些就是需要通过 Hook 或 NOP 屏蔽掉的指令。

### Case 0x14 (20): 战网集成
*   **地址**: `6F5BAB2C`
*   **字符串**: `BattleNetChatPanel`
*   **功能**: 处理创建游戏界面下方可能存在的战网聊天窗口的消息或状态更新。

---

## 4. 汇编代码逻辑重组

以下是按照逻辑分支整理的汇编代码：

```assembly
6F5BA950  | 8B4424 04               | mov eax,dword ptr ss:[esp+4]                    | 获取 EventID
6F5BA954  | 56                      | push esi                                        |
6F5BA955  | 8BF1                    | mov esi,ecx                                     | this指针
6F5BA957  | 8B48 08                 | mov ecx,dword ptr ds:[eax+8]                    | 获取 SubID (Case值)
6F5BA95A  | 83F9 14                 | cmp ecx,14                                      | 最大支持 Case 20
6F5BA95D  | 0F87 28020000           | ja game.6F5BAB8B                                | Default
6F5BA963  | FF248D 94AB5B6F         | jmp dword ptr ds:[ecx*4+6F5BAB94]               | 跳转表分发

; --------------------------------------------------------------------------------------
; Case 0: 游戏速度 (Game Speed)
6F5BA96A  | 8BCE                    | mov ecx,esi                                     |
6F5BA96C  | E8 DFB2FCFF             | call game.6F585C50                              |
6F5BA971  | 5E                      | pop esi                                         |
6F5BA972  | C2 0400                 | ret 4                                           |

; Case 1: 地图列表选择 (List Selection)
6F5BA975  | 8BCE                    | mov ecx,esi                                     |
6F5BA977  | E8 5470FDFF             | call game.6F5919D0                              |
6F5BA97C  | 5E                      | pop esi                                         |
6F5BA97D  | C2 0400                 | ret 4                                           |

; Case 4: 点击创建游戏按钮 (Button Create)
6F5BA996  | 6A 00                   | push 0                                          |
6F5BA998  | 8BCE                    | mov ecx,esi                                     |
6F5BA99A  | E8 719FFFFF             | call game.6F5B4910                              | 包含建主逻辑与UI跳转
6F5BA99F  | 5E                      | pop esi                                         |
6F5BA9A0  | C2 0400                 | ret 4                                           |

; Case 8: 公开/私有单选框 (Public/Private)
6F5BA9C4  | 50                      | push eax                                        |
6F5BA9C5  | 8BCE                    | mov ecx,esi                                     |
6F5BA9C7  | E8 E4A2FBFF             | call game.6F574CB0                              |
6F5BA9CC  | 5E                      | pop esi                                         |
6F5BA9CD  | C2 0400                 | ret 4                                           |

; Case 11 (0xB): 创建动作执行 (Create Action)
6F5BA9E8  | 50                      | push eax                                        |
6F5BA9E9  | 8BCE                    | mov ecx,esi                                     |
6F5BA9EB  | E8 20B4FCFF             | call game.6F585E10                              | 可能直接调用网络层
6F5BA9F0  | 5E                      | pop esi                                         |
6F5BA9F1  | C2 0400                 | ret 4                                           |

; Case 15 (0xF): 容器显示/转场 (Show/Fade)
6F5BAA34  | 8B8E 24020000           | mov ecx,dword ptr ds:[esi+224]                  |
6F5BAA3A  | 6A 00                   | push 0                                          |
6F5BAA3C  | 6A 00                   | push 0                                          |
6F5BAA3E  | E8 7D340400             | call game.6F5FDEC0                              |
6F5BAA43  | ...                     | ...                                             |
6F5BAA5A  | B9 00A2956F             | mov ecx,game.6F95A200                           | "Glue" (UI管理器)
6F5BAA5F  | E8 CCF3A4FF             | call game.6F009E30                              | 切换屏幕
6F5BAA64  | ...                     | ...                                             |
6F5BAA80  | E8 4B9DD5FF             | call game.6F3147D0                              | Sleep/Fade
6F5BAA85  | B8 01000000             | mov eax,1                                       |
6F5BAA8A  | 5E                      | pop esi                                         |
6F5BAA8B  | C2 0400                 | ret 4                                           |

; Case 20 (0x14): 战网聊天面板 (BattleNetChatPanel)
6F5BAB2C  | 830D 18A4A96F 01        | or dword ptr ds:[6FA9A418],1                    |
6F5BAB33  | ...                     | ...                                             |
6F5BAB43  | 8B0C85 18FB956F         | mov ecx,dword ptr ds:[eax*4+6F95FB18]           | &"BattleNetChatPanel"
6F5BAB4A  | 51                      | push ecx                                        |
6F5BAB4B  | 8B8E 68010000           | mov ecx,dword ptr ds:[esi+168]                  |
6F5BAB51  | E8 CA84FBFF             | call game.6F573020                              | 控件交互
6F5BAB56  | B8 01000000             | mov eax,1                                       |
6F5BAB5B  | 5E                      | pop esi                                         |
6F5BAB5C  | C2 0400                 | ret 4                                           |

; Default Case
6F5BAB8B  | 33C0                    | xor eax,eax                                     |
6F5BAB8D  | 5E                      | pop esi                                         |
6F5BAB8E  | C2 0400                 | ret 4                                           |
```
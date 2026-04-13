# War3 战网大厅 UI 事件分发逻辑 (Lobby Event Dispatch)

## 1. 基础信息

*   **模块名称**: `Game.dll`
*   **当前偏移**: `5BB2A0`
*   **当前基址**: `02690000`
*   **代码位置**: `02C4B2A0`
*   **分发机制**: 
    1.  接收事件 ID (`ECX`)。
    2.  检查 ID 是否越界 (最大值检查)。
    3.  通过 **跳转表 (Jump Table)** `jmp dword ptr ds:[ecx*4 + TableBase]` 跳转到对应分支。
    4.  每个分支调用具体的处理函数 (`call game.xxxxxxx`)。

## 2. 核心功能映射表 (Event Mapping)

以下表格根据 `ECX` 寄存器的值（事件ID）分类。地址基于你提供的 Dump（`02C4xxxx` 段）。

### 🎯 游戏模式选择 (Game Modes)

| Event ID (Hex) | Event ID (Dec) | 功能描述 (Description) | 关键函数地址 (Call) | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| **0x00** | 0 | **Play Game (玩游戏)** | `game.2C48790` | 此时 `ecx=0` |
| **0x01** | 1 | **Quick Game (快速游戏)** | `game.2C487A0` | 此时 `ecx=1` |
| **0x02** | 2 | **Arrange Team (安排队伍)** | `game.2C48840` | |
| **0x03** | 3 | **Custom Game (自定义游戏)** | `game.2C48830` | 🚩 **核心关注目标** |
| **0x04** | 4 | **Tournaments (锦标赛)** | `game.2C41C50` | |
| **0x34** | 52 | **Match Start (开始匹配)** | `game.2C415B0` | 点击快速游戏后的准备阶段 |
| **0x35** | 53 | **Cancel Match (取消/Esc)** | `game.2C02150` | 退出匹配队列 |

### 💬 社交与频道 (Chat & Community)

| Event ID (Hex) | Event ID (Dec) | 功能描述 (Description) | 关键函数地址 (Call) | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| **0x0A** | 10 | **Select Channel (选择频道)** | `game.2C14590` | 进入频道选择界面 |
| **0x0B** | 11 | **Enter Chat (进入聊天)** | `game.2C14550` | 聊天按钮 |
| **0x0E** | 14 | **Double Click Channel** | `game.2C41AD0` | 双击频道列表项 |
| **0x12** | 18 | **Select Channel List** | `game.2C020D0` | 单击/选择频道列表 |
| **0x1B** | 27 | **Tab Switch** | `game.2C47B80` | 切换 Channel/Friends/Clan 标签页 |
| **0x1D** | 29 | **Chat Send (发送消息)** | `game.2C4A720` | 在输入框回车发送 |
| **0x3A** | 58 | **Chat UI Init** | `game.2C8DEC0` | 聊天界面初始化 |
| **0x3B** | 59 | **Chat Switch Done** | `game.2C27290` | 聊天页面切换完成 |
| **0x3D** | 61 | **Chat Switch Start** | `game.2C02A60` | 聊天页面切换开始 |

### 👥 好友系统 (Friends List)

| Event ID (Hex) | Event ID (Dec) | 功能描述 (Description) | 关键函数地址 (Call) | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| **0x16** | 22 | **Click Friend (单击好友)** | `game.2C48170` | 选中好友 |
| **0x17** | 23 | **Double Click Friend** | `game.2C48280` | 双击好友（通常是发消息或加入） |
| **0x27** | 39 | **Add Friend (添加好友)** | `game.2C411A0` | 点击添加按钮 |
| **0x2C** | 44 | **Get Friend Info** | `game.2C4A790` | 获取好友信息 (可能多次触发) |
| **0x3C** | 60 | **Update Friend List** | `game.2C41EC0` | 刷新/获取好友列表 |

### ⚙️ 系统与资料 (System & Profile)

| Event ID (Hex) | Event ID (Dec) | 功能描述 (Description) | 关键函数地址 (Call) | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| **0x07** | 7 | **Ladder Info (天梯信息)** | `game.2C41B90` | |
| **0x08** | 8 | **Profile (个人资料)** | `game.2C488D0` | 点击头像/资料按钮 |
| **0x0C** | 12 | **Exit Lobby (退出大厅)** | `game.2C48910` | 返回上一级菜单 |
| **0x0D** | 13 | **Help (帮助/问号)** | `game.2C21290` | 右下角问号 |
| **0x1C** | 28 | **Close Dialog** | `game.2C140E0` | 关闭弹窗 |
| **0x25** | 37 | **Dialog Yes** | `game.2C41210` | 确认框点击“是” |
| **0x26** | 38 | **Dialog No** | `game.2C41260` | 确认框点击“否” |

### 🔄 状态流转与新闻 (State & News)

| Event ID (Hex) | Event ID (Dec) | 功能描述 (Description) | 关键函数地址 (Call) | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| **0x2A** | 42 | **News Fetch (3)** | `game.2C14420` | 进入战网后第3次触发 |
| **0x2B** | 43 | **Channel List Done** | `game.2C412F0` | 获取频道列表完毕 |
| **0x3E** | 62 | **News Fetch (1)** | `game.2699E30` | 刚进入大厅时触发 |
| **0x3F** | 63 | **Transition End** | `game.2C03020` | 画面切换结束 (如进入自定义房间) |
| **0x40** | 64 | **News Fetch (2)** | `game.2CA7650` | 进入大厅后第2次触发 |
| **0x41** | 65 | **Transition Start** | `game.2CA7650` | 画面切换开始 (退出大厅时) |

## 3. 关键汇编逻辑片段

### 自定义游戏入口 (Target)
```assembly
02C4B2D2  | 8BCE          | mov ecx,esi       ; 恢复 ECX (Event ID)
02C4B2D4  | E8 57D5FFFF   | call game.2C48830 ; <--- 进入自定义游戏列表的核心函数
```

### 聊天输入发送
```assembly
02C4B365  | 8BCE          | mov ecx,esi       ; 恢复 ECX
02C4B367  | E8 B4F3FFFF   | call game.2C4A720 ; <--- 发送聊天消息
```

### 退出战网逻辑
```assembly
02C4B337  | 6A 00         | push 0
02C4B339  | 8BCE          | mov ecx,esi
02C4B33B  | E8 D0D5FFFF   | call game.2C48910 ; <--- 退出操作
```

## 4. 总结与建议

1.  **Hook 建议**:
    如果你想拦截玩家点击“自定义游戏”的行为（例如替换为显示你的云房间列表），你应该 Hook 地址 **`02C4B2D4`** 处的 `call` 指令，或者直接 Hook 目标函数 **`game.2C48830`**。

2.  **数据包发送时机**:
    在 `0x1D` (Chat Send) 事件中，可以拦截玩家的聊天输入。
    在 `0x00` (Play Game) 或 `0x03` (Custom Game) 事件中，是注入自定义逻辑的最佳时机。

3.  **UI 刷新**:
    `0x3C`, `0x2C` 等事件频繁触发，表明这是 UI 轮询或回调机制。如果你的 DLL 需要绘制 Overlay，需注意不要在这些频繁调用的函数中做耗时操作，否则会卡顿。
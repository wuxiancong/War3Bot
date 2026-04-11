# War3 网络事件分发循环 (Network Event BNET Dispatcher) 分析报告

*   **目标模块**: `Game.dll` (v1.26.0.6401)
*   **函数地址**: `0x02BD0B50`
*   **偏移地址**: `game.dll + 6B0B50`
*   **功能**: 战网命令服务 (Battle.net Command Service, BNCS) 的核心分发逻辑。根据从战网服务器接收到的 **Packet ID**，跳转到对应的处理分支进行封包解析。

---

## 1. 函数执行流程 (Dispatcher Logic)

该函数采用了高效的 **双表查找跳转机制**（Jump Table）：

1.  **参数获取**: 从栈中取出 Packet ID（例如 `0x09`）。
2.  **边界检查**: `cmp esi, 82` —— 确保 ID 不超过 `0x82`。
3.  **索引映射**: `movzx esi, byte ptr ds:[esi+2BD11EC]` —— 通过第一个查找表，将原始 Packet ID 映射为一个连续的索引值。
4.  **动态跳转**: `jmp dword ptr ds:[esi*4+2BD1104]` —— 根据索引值，从第二个跳转表中取出处理函数的起始地址并跳转。

---

## 2. 关键封包处理分支分析

该函数包含了战网通信的所有核心业务逻辑。根据 RTTI 信息和上下文，以下是关键分支的映射：

### 2.1 游戏列表处理 (核心点)
*   **Packet ID**: `0x09` (**SID_GETADVLISTEX**)
*   **跳转地址**: `02BD0D12`
*   **逻辑**: 
    ```assembly
    02BD0D12 | E8 A9D1FFFF | call game.2BCDEC0 | 处理战网 0x09 包
    ```
    这是游戏收到战网回传列表数据的“第一现场”。该函数会解析原始字节流，将其转换为 `CNetEventGameListAdd` 等事件，随后发送给你之前分析过的 `6F551D80` 分发器。

### 2.2 连接管理
*   **SID_AUTH_INFO (0x50)**: 初始化认证信息。
*   **SID_AUTH_CHECK (0x51)**: CD-Key 校验结果。
*   **SID_PING (0x25)**: 处理服务器心跳包。

### 2.3 聊天与社交
*   **SID_CHATEVENT (0x0F)**: 处理聊天信息（进入频道、接收消息、用户加入频道）。
*   **SID_FRIENDS_LIST (0x65)**: 处理好友列表更新。
*   **SID_CLAN_INFO (0x75)**: 战队信息处理。

---

## 3. 函数调用约定

*   **调用约定**: `__stdcall` (由 `ret 10` 可知，清理了 4 个参数的栈空间)。
*   **参数 1 (ECX)**: 通常是战网连接上下文对象 (`CBattleNet` 或 `CNetData`)。
*   **参数 2 (EDX)**: 原始数据缓冲区指针。
*   **参数 3 (Stack+0xC)**: **Packet ID** (ESI)。
*   **参数 4 (Stack+0x14)**: 数据载荷长度。

---

## 4. BNCS 事件 ID 映射总结表

此函数是战网通信的枢纽。以下是部分 Packet ID 及其在 War3 中的处理函数偏移：

| Packet ID (Hex) | 功能描述 (Description) | 处理分支偏移 (Branch Offset) | 核心处理函数 (Handler) |
| :--- | :--- | :--- | :--- |
| **0x09** | **SID_GETADVLISTEX (获取列表)** | `02BD0D12` | `game.2BCDEC0` |
| **0x0F** | **SID_CHATEVENT (聊天事件)** | `02BD10B5` | `game.2BCF470` |
| **0x25** | **SID_PING (心跳同步)** | `02BD0CD0` | `game.2BC4930` |
| **0x1E** | **SID_JOIN_GAME (加入游戏响应)** | `02BD0B8B` | `game.2BBC920` |
| **0x1F** | **SID_CREATE_GAME (创建游戏响应)** | `02BD0BA4` | `game.2BBC970` |
| **0x50** | **SID_AUTH_INFO (认证初始化)** | `02BD0F5A` | `game.2BC5480` |
| **0x51** | **SID_AUTH_CHECK (Key校验)** | `02BD0F73` | `game.2BC5500` |

---

## 5. 逆向应用建议

1.  **数据包嗅探**: 如果你想获取“最原始”的战网数据（未经过魔兽事件包装的），在这个函数的入口点 `02BD0B50` 下断点是最佳选择。
2.  **修改游戏名/彩色 ID**: 
    *   在 `02BD0D12` 处调用的 `game.2BCDEC0` 内部会对 `0x09` 包进行字符串拆解。
    *   **Hook 建议**: 拦截 `game.2BCDEC0`。在它解析字节流时，直接修改缓冲区内的游戏名字节，可以实现比在 W3GS 层修改更早、更底层的改名效果。
3.  **封包过滤**: 如果你想屏蔽某些战网消息（如屏蔽某人的悄悄话或特定频道的消息），可以在此处判断 Packet ID 后直接 `ret 10` 跳过原函数执行。

**关联参考**: 
本函数产生的业务结果（Event）最终会汇集到 `6F551D80` (`W3GS Dispatcher`) 进行逻辑分发和 UI 展现。`02BD0B50` 负责“协议层”，`6F551D80` 负责“逻辑/表现层”。
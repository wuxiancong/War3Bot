### Warcraft III 内部发包函数 (F7 1D - Game Info)

**环境:**
- **模块**：`game.dll`
- **偏移**： `678670`
- **基地址**：`6F000000`
- **  地址**：`game.dll + 678670`
- **客户端**：Warcraft III (v1.26.0.6401)

#### 1. 函数基本信息

*   **函数名称 (推测):** `NetSend_Packet_F7_1D_GameInfo`
*   **内存地址:** `02C88670`
*   **协议功能:** 发送游戏详细信息 (`W3GS_GAMEINFO`)
*   **调用约定:** `__fastcall` (推测)
    *   `ECX`: `CNetConnection` / `CClientSocket` 对象指针 (接收数据包的目标连接)
    *   `EDX`: 源数据对象指针 (包含 GameCounter, GameName, MapInfo 等信息的结构体，通常是 `CGame` 或 `CGameProtocol`)
    *   `Stack`: 内部使用大栈帧 (`sub esp, 5D8`) 构建 `CNetPacket`

#### 2. C++ 伪代码还原

```cpp
// 还原的函数签名
// ecx: 网络连接对象 (发送通道)
// edx: 游戏状态源数据 (数据来源，包含那个导致冲突的 Counter)
void __fastcall NetSend_GameInfo(void* pConnection, void* pGameStateSource) {
    CDataStore packetBuilder; // 栈上分配 (esp+5D8)
    
    // 1. 初始化包构建器
    packetBuilder.Initialize(); 

    // 2. 写入主协议头 0xF7
    packetBuilder.WriteInt8(0xF7);

    // 3. 写入子协议号 0x1D (W3GS_GAMEINFO)
    packetBuilder.WriteInt8(0x1D);

    // 4. 预留包大小位置 (写入0占位)
    packetBuilder.WriteInt16(0); 

    // 5. [核心] 写入 Payload (游戏信息)
    // 这里调用 game.2C942A0 从 pGameStateSource 读取数据
    // 包括: GameCounter, EntryKey, HostCounter, GameName, MapInfo 等
    WriteGameInfoPayload(&packetBuilder, pGameStateSource); 

    // 6. 封包 (计算长度并回写到位置 2)
    packetBuilder.Finalize();
    
    // 7. 通过网络发送
    // pConnection -> Send(packet)
    pConnection->SendPacket(&packetBuilder);
}
```

#### 3. 汇编代码详细注释

```assembly
02C88670  | 6A FF                   | push FFFFFFFF                                   | ; SEH 帧
...
02C8867E  | 81EC D8050000           | sub esp,5D8                                     | ; 分配 huge buffer 用于构建包
...
02C886AA  | 8BF1                    | mov esi,ecx                                     | ; [Save] ESI = 网络连接对象
02C886B0  | 8BFA                    | mov edi,edx                                     | ; [Save] EDI = 源数据对象 (GameCounter在这里!)
02C886B2  | E8 D90BC5FF             | call game.28D9290                               | ; CDataStore::Constructor
02C886B7  | 68 F7000000             | push F7                                         | ; PUSH 0xF7 (Header)
02C886CB  | E8 909AE4FF             | call game.2AD2160                               | ; Stream_WriteByte
02C886D0  | 6A 1D                   | push 1D                                         | ; PUSH 0x1D (Packet ID: W3GS_GAMEINFO)
02C886D6  | E8 859AE4FF             | call game.2AD2160                               | ; Stream_WriteByte
02C886DF  | 6A 00                   | push 0                                          | ; Size Placeholder
02C886E5  | E8 269BE4FF             | call game.2AD2210                               | ; Stream_WriteInt16
02C886EA  | 8BD7                    | mov edx,edi                                     | ; EDX = 源数据对象
02C886F0  | E8 ABBB0000             | call game.2C942A0                               | ; [核心] 写入 Payload (这里写入了 3 或 1)
02C886FF  | E8 6C97E4FF             | call game.2AD1E70                               | ; Finalize Packet Size
...
02C88722  | 8BCE                    | mov ecx,esi                                     | ; ECX = 网络连接对象
02C88724  | E8 F7260600             | call game.2CEAE20                               | ; [核心] SendPacket (发送出去)
...
02C88769  | C3                      | ret                                             |
```

#### 4. 协议结构分析 (W3GS_GAMEINFO)

此函数构建并发送的数据包结构如下：

| 偏移 (Offset) | 类型 (Type) | 值 (Value) | 描述 (Description) | 来源 |
| :--- | :--- | :--- | :--- | :--- |
| 0x00 | BYTE | **0xF7** | 包头 ID | 硬编码 |
| 0x01 | BYTE | **0x1D** | 命令 ID (GAMEINFO) | 硬编码 |
| 0x02 | WORD | *Size* | 包总大小 | 自动计算 |
| 0x04 | DWORD | **Counter** | **Game ID / Counter** | **`[EDI+Offset]`** (冲突根源) |
| 0x08 | DWORD | *EntryKey* | 随机进入密钥 | `[EDI+Offset]` |
| 0x0C | DWORD | *HostCnt* | 主机计数器 | `[EDI+Offset]` |
| 0x14 | String | *Name* | 游戏房间名 ("admin") | `[EDI+Offset]` |

#### 5. 总结与应用

*   **功能定位**: 此函数是 Host 端向 Client 广播房间信息的核心函数。
*   **问题关联**: 在你的双修（Host+Join）场景中，当你作为 Host 广播时，此函数会读取本地内存中的 `GameCounter`（通常是 1）。如果这与 Bot 同步的 Counter（例如 3）不一致，会导致连接你的客户端断开。
*   **Hook 建议**:
    *   **位置**: `02C886F0` 处的 `call game.2C942A0`。
    *   **操作**: 进入该 payload 写入函数，拦截对 GameCounter 的写入，强制将其修改为与 Bot 同步的数值（即 3），从而实现完美的“伪装 Host”。

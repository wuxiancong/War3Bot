### Warcraft III 内部发包函数 (F7 1E - Join Request)

**环境:**
- **模块**：`game.dll`
- **偏移**： `678770`
- **基地址**：`6F000000`
- **  地址**：`game.dll + 678770`
- **客户端**：Warcraft III (v1.26.0.6401)

#### 1. 函数基本信息

*   **函数名称 (推测):** `NetSend_Packet_F7_1E_JoinReq`
*   **内存地址:** `02C88770`
*   **协议功能:** 发送加入游戏请求 (`W3GS_JOINREQ`)
*   **调用约定:** `__fastcall` (推测)
    *   `ECX`: `CNetConnection` / `CClientSocket` 对象指针 (与主机的连接)
    *   `EDX`: 源数据对象指针 (包含要加入的游戏ID、密钥、本机监听端口等信息的结构体)
    *   `Stack`: 内部使用大栈帧 (`sub esp, 5D8`) 构建 `CNetPacket`

#### 2. C++ 伪代码还原

```cpp
// 还原的函数签名
// ecx: 网络连接对象 (通常是指向 Host 的 Socket)
// edx: 加入参数结构 (CJoinGameParams?)
void __fastcall NetSend_JoinRequest(void* pConnection, void* pJoinParams) {
    CDataStore packetBuilder; // 栈上分配 (esp+5D8)
    
    // 1. 初始化包构建器
    packetBuilder.Initialize(); 

    // 2. 写入主协议头 0xF7
    packetBuilder.WriteInt8(0xF7);

    // 3. 写入子协议号 0x1E (W3GS_JOINREQ)
    packetBuilder.WriteInt8(0x1E);

    // 4. 预留包大小位置 (写入0占位)
    packetBuilder.WriteInt16(0); 

    // 5. [核心] 写入 Payload (加入信息)
    // 调用 game.2C94F90 从 pJoinParams 读取数据
    // 关键数据包括: GameCounter(3?), EntryKey, ClientPort, InternalIP 等
    WriteJoinReqPayload(&packetBuilder, pJoinParams); 

    // 6. 封包 (计算长度并回写)
    packetBuilder.Finalize();
    
    // 7. 通过网络发送
    pConnection->SendPacket(&packetBuilder);
}
```

#### 3. 汇编代码详细注释

```assembly
02C88770  | 6A FF                   | push FFFFFFFF                                   | ; SEH 帧
...
02C8877E  | 81EC D8050000           | sub esp,5D8                                     | ; 分配 buffer
...
02C887AA  | 8BF1                    | mov esi,ecx                                     | ; [Save] ESI = 连接对象
02C887B0  | 8BFA                    | mov edi,edx                                     | ; [Save] EDI = 源参数对象 (Join Params)
02C887B2  | E8 D90AC5FF             | call game.28D9290                               | ; CDataStore::Constructor
02C887B7  | 68 F7000000             | push F7                                         | ; PUSH 0xF7 (Header)
02C887CB  | E8 9099E4FF             | call game.2AD2160                               | ; Stream_WriteByte
02C887D0  | 6A 1E                   | push 1E                                         | ; PUSH 0x1E (Packet ID: W3GS_JOINREQ)
02C887D6  | E8 8599E4FF             | call game.2AD2160                               | ; Stream_WriteByte
02C887DF  | 6A 00                   | push 0                                          | ; Size Placeholder
02C887E5  | E8 269AE4FF             | call game.2AD2210                               | ; Stream_WriteInt16
02C887EA  | 8BD7                    | mov edx,edi                                     | ; EDX = 源参数对象
02C887EC  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]                   |
02C887F0  | E8 9BC70000             | call game.2C94F90                               | ; [核心] 写入 Payload (JoinReq 内容)
02C887FF  | E8 6C96E4FF             | call game.2AD1E70                               | ; Finalize Packet Size
...
02C88824  | E8 F7250600             | call game.2CEAE20                               | ; [核心] SendPacket (发送给 Host/Bot)
...
02C88869  | C3                      | ret                                             |
```

#### 4. 协议结构分析 (W3GS_JOINREQ)

当客户端尝试加入一个房间时，发送此包。结构如下：

| 偏移 (Offset) | 类型 (Type) | 描述 (Description) | 关键性 (Criticality) |
| :--- | :--- | :--- | :--- |
| 0x00 | BYTE | **0xF7** | 包头 |
| 0x01 | BYTE | **0x1E** | 命令 ID (JOINREQ) |
| 0x02 | WORD | *Size* | 包总大小 |
| 0x04 | DWORD | **Game ID** | **必须与 Host 的 Game Counter 匹配** (冲突点) |
| 0x08 | DWORD | **Entry Key** | 必须与 Host 的 Entry Key 匹配 |
| 0x0C | BYTE | *Unknown* | 通常为 0x01 |
| 0x0D | WORD | *Listen Port* | 客户端的主机监听端口 |
| 0x0F | DWORD | *Peer Key* | 客户端生成的 Key |
| ... | ... | *Name/IP* | 玩家名、内部IP等 |

#### 5. 总结与调试价值

*   **场景**: 当你的客户端尝试加入 HostBot 创建的房间时，会调用此函数。
*   **问题关联**:
    *   服务端 Bot 正在等待 Game ID 为 **3** 的加入请求。
    *   你的客户端本地内存如果认为 Game ID 是 **1**，那么 `call game.2C94F90` 会将 **1** 写入数据包。
    *   Bot 收到 ID 为 1 的请求，与它自己的 ID 3 不匹配 -> **拒绝连接** (这可能就是你遇到的"无法创建"或"连接失败"的根本原因)。
*   **验证方法**:
    *   在此函数下断点，观察 `2C94F90` 内部写入的第一个 DWORD。
    *   如果它写入的是 1 而不是 3，你需要 **Hook `2C94F90`**，在发送 Join 请求时，强制将 Game ID 修改为服务端发来的那个 **3**。
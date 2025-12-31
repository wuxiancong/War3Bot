### Warcraft III 内部发包函数 (F7 24)

**环境:**
- **模块**：`game.dll`
- **偏移**： `677A70`
- **基地址**：`6F000000`
- **  地址**：`game.dll + 677A70`
- **客户端**：Warcraft III (v1.26.0.6401)

#### 1. 函数基本信息

*   **函数名称 (推测):** `NetSend_Packet_F7_24`
*   **协议 ID:** `0xF7` (Game Packet), `0x24` (Sub Command)
*   **调用约定:** `__fastcall`
    *   `ECX`: `CNetClient` / `CGameServer` 对象指针 (网络连接实例)
    *   `EDX`: 待发送数据的指针 (Payload Pointer)
    *   `Stack [esp+4]`: 数据长度或辅助参数 (传递给 `6F6843A0` 和 `PacketFinalize` 使用)

#### 2. 协议推测 (0x24)

在常见的 W3GS (Warcraft 3 Game Server) 协议文档中，`0x21` 是离开请求，`0x23` 是玩家加载完成 (Game Loaded Self)。
*   **0x24** 的定义较少见，通常处于 **加载过程 (Loading)** 或 **握手 (Handshake)** 阶段。
*   根据上下文，它可能与 **`W3GS_SLOTINFO_JOIN`** (槽位信息) 或 **`W3GS_GAME_LOAD_INFO`** 相关，也有可能是某种特定的 **客户端状态同步 (Client Status/Checksum)** 包。
*   它使用了一个特定的写入函数 `6F6843A0`，不同于 `LeaveReq` 使用的 `6F684340` (写入4字节整数)，这暗示 Payload 类型可能不同（例如可能是结构体、不同长度的整数或字符串）。

#### 3. C++ 伪代码还原

```cpp
// 还原的函数签名
// ecx: 网络对象指针
// edx: 数据内容指针
// [esp+4]: 数据参数/长度 (ebx)
void __fastcall NetSend_Packet_F7_24(void* pNetObject, void* pDataPtr, size_t param) {
    CDataStore packetBuilder; // 栈上分配 Packet 构建器 (约 0x5D8 大小)
    
    // 1. 初始化 Packet 结构 (6F2C9290)
    packetBuilder.Initialize(); 

    // 2. 写入主协议头 0xF7 (6F4C2160 - WriteInt8)
    packetBuilder.WriteInt8(0xF7);

    // 3. 写入子协议号 0x24 (6F4C2160 - WriteInt8)
    packetBuilder.WriteInt8(0x24);

    // 4. 写入填充/标志位 0 (6F4C2210)
    packetBuilder.Write(0); 

    // 5. 写入 Payload (6F6843A0)
    // 注意：此处调用的写入函数与 F7 21 (6F684340) 不同
    // 可能是 WriteInt16, WriteInt64 或特定结构体写入
    packetBuilder.WriteSpecificType(pDataPtr, param); 

    // 6. 封包与校验 (6F4C1E70)
    packetBuilder.Finalize();
    
    // 7. 发送数据包 (6F6DAE20)
    pNetObject->SendPacket(&packetBuilder);

    // 8. 销毁构建器 (6F2C95B0)
    packetBuilder.Destroy();
}
```

#### 4. 汇编代码详细注释

```assembly
6F677A70  | 6A FF                   | push FFFFFFFF                                   | ; SEH 起始
6F677A72  | 68 DB42846F             | push game.6F8442DB                              | ; 异常处理函数
6F677A77  | 64:A1 00000000          | mov eax,dword ptr fs:[0]                        |
6F677A7D  | 50                      | push eax                                        |
6F677A7E  | 81EC D8050000           | sub esp,5D8                                     | ; 分配栈空间
...
6F677AAA  | 8BF1                    | mov esi,ecx                                     | ; 保存 this 指针 (pNetObject)
6F677AAC  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]                   | ; ECX = Packet对象地址
6F677AB0  | 8BFA                    | mov edi,edx                                     | ; 保存参数指针 (pDataPtr)
6F677AB2  | E8 D917C5FF             | call game.6F2C9290                              | ; Initialize Packet
6F677AB7  | 68 F7000000             | push F7                                         | ; 压入 0xF7
6F677ABC  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]                   |
6F677AC0  | C78424 F4050000 0000000 | mov dword ptr ss:[esp+5F4],0                    |
6F677ACB  | E8 90A6E4FF             | call game.6F4C2160                              | ; WriteInt8(0xF7)
6F677AD0  | 6A 24                   | push 24                                         | ; [关键] 0x24 - Sub Command ID
6F677AD2  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]                   |
6F677AD6  | E8 85A6E4FF             | call game.6F4C2160                              | ; WriteInt8(0x24)
6F677ADB  | 8B5C24 28               | mov ebx,dword ptr ss:[esp+28]                   | ; 获取栈参数 (长度/标志)
6F677ADF  | 6A 00                   | push 0                                          |
6F677AE1  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]                   |
6F677AE5  | E8 26A7E4FF             | call game.6F4C2210                              | ; Write(0) - Padding
6F677AEA  | 8BD7                    | mov edx,edi                                     | ; 恢复参数指针 -> EDX
6F677AEC  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]                   |
6F677AF0  | E8 ABC80000             | call game.6F6843A0                              | ; [关键] 写入 Payload (与F7 21不同)
6F677AF5  | 8B4424 28               | mov eax,dword ptr ss:[esp+28]                   |
6F677AF9  | 50                      | push eax                                        |
6F677AFA  | 53                      | push ebx                                        |
6F677AFB  | 8D4C24 20               | lea ecx,dword ptr ss:[esp+20]                   |
6F677AFF  | E8 6CA3E4FF             | call game.6F4C1E70                              | ; Finalize Packet
...
6F677B24  | E8 F7320600             | call game.6F6DAE20                              | ; Send Packet
...
6F677B3C  | E8 6F1AC5FF             | call game.6F2C95B0                              | ; Destroy Packet
6F677B69  | C3                      | ret                                             |
```

#### 5. 协议结构分析

| 偏移 (Offset) | 类型 (Type) | 值 (Value) | 描述 (Description) |
| :--- | :--- | :--- | :--- |
| 0x00 | BYTE | **0xF7** | 包头 (Game Packet) |
| 0x01 | BYTE | **0x24** | 子协议 ID (Sub Command) |
| 0x02 | BYTE/DWORD | **0x00** | 填充/标志 |
| 0x03+ | Unknown | **Payload** | 由 `6F6843A0` 函数写入的数据 (根据 EDX 指针) |

#### 6. 总结
该函数是 `F7` 系列发包函数的一个特例，专门用于构建和发送子命令为 **0x24** 的数据包。
*   它与 `LeaveReq (0x21)` 的区别在于使用了不同的 Payload 写入函数 (`6F6843A0`)，且协议 ID 为 `0x24`。
*   此函数通常由游戏逻辑层直接调用，用于同步特定的游戏状态或客户端信息。
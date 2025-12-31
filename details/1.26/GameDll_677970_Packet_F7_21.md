### Warcraft III 内部发包函数 (F7 21 - Leave Request)

**环境:**
- **模块**：`game.dll`
- **偏移**： `677970`
- **基地址**：`6F000000`
- **  地址**：`game.dll + 677970`
- **客户端**：Warcraft III (v1.26.0.6401)


#### 1. 函数基本信息

*   **函数名称 (推测):** `NetSend_Packet_F7_21_LeaveReq`
*   **内存地址:** `6F677970`
*   **协议功能:** 发送离开游戏请求 (`W3GS_LEAVEREQ`)
*   **调用约定:** `__fastcall`
    *   `ECX`: `CNetClient` / `CGameServer` 对象指针 (网络连接实例)
    *   `EDX`: 指向负载数据的指针 (即 `Reason` 的地址)
    *   `Stack [esp+4]`: 数据长度 (通常为 4 字节，对应 UINT32)

#### 2. C++ 伪代码还原

```cpp
// 还原的函数签名
// ecx: 网络对象指针
// edx: 离开原因(Reason)的指针
// [esp+4]: 数据长度 (sizeof(UINT32) = 4)
void __fastcall NetSend_LeaveRequest(void* pNetObject, void* pReasonPtr, size_t dataLength) {
    CDataStore packetBuilder; // 栈上分配的数据包构建对象
    
    // 1. 初始化包构建器
    packetBuilder.Initialize(); 

    // 2. 写入主协议头 0xF7 (Game Packet)
    packetBuilder.WriteInt8(0xF7);

    // 3. 写入子协议号 0x21 (W3GS_LEAVEREQ)
    packetBuilder.WriteInt8(0x21);

    // 4. 写入填充/标志位 0
    packetBuilder.Write(0); 

    // 5. 写入离开原因 (Reason Code)
    // 根据上下文，这里写入的是由 edx 指向的 4字节 Reason
    // 常见 Reason: 0x01(断开), 0x09(获胜), 0x0D(离开大厅) 等
    packetBuilder.WriteBytes(pReasonPtr, dataLength); 

    // 6. 封包与校验 (Finalize)
    packetBuilder.Finalize();
    
    // 7. 执行发送
    pNetObject->SendPacket(&packetBuilder);

    // 8. 销毁构建器
    packetBuilder.Destroy();
}
```

#### 3. 汇编代码详细注释

```assembly
6F677970  | 6A FF                   | push FFFFFFFF                                   | ; SEH 帧起始
6F677972  | 68 9B42846F             | push game.6F84429B                              | ; 异常处理句柄
6F677977  | 64:A1 00000000          | mov eax,dword ptr fs:[0]                        |
6F67797D  | 50                      | push eax                                        |
6F67797E  | 81EC D8050000           | sub esp,5D8                                     | ; 分配栈空间 (Packet结构体)
...
6F6779AA  | 8BF1                    | mov esi,ecx                                     | ; 保存 this 指针 (pNetObject)
6F6779AC  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]                   |
6F6779B0  | 8BFA                    | mov edi,edx                                     | ; 保存 pReasonPtr 到 edi
6F6779B2  | E8 D918C5FF             | call game.6F2C9290                              | ; 初始化 CDataStore
6F6779B7  | 68 F7000000             | push F7                                         |
6F6779BC  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]                   |
6F6779C0  | C78424 F4050000 0000000 | mov dword ptr ss:[esp+5F4],0                    |
6F6779CB  | E8 90A7E4FF             | call game.6F4C2160                              | ; WriteInt8(0xF7)
6F6779D0  | 6A 21                   | push 21                                         | ; [关键] 0x21 = W3GS_LEAVEREQ
6F6779D2  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]                   |
6F6779D6  | E8 85A7E4FF             | call game.6F4C2160                              | ; WriteInt8(0x21)
6F6779DB  | 8B5C24 28               | mov ebx,dword ptr ss:[esp+28]                   | ; 读取数据长度 (通常为4)
6F6779DF  | 6A 00                   | push 0                                          |
6F6779E1  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]                   |
6F6779E5  | E8 26A8E4FF             | call game.6F4C2210                              | ; Write(0) - 填充位
6F6779EA  | 8BD7                    | mov edx,edi                                     | ; 恢复 pReasonPtr
6F6779EC  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]                   |
6F6779F0  | E8 4BC90000             | call game.6F684340                              | ; [关键] 写入 Payload (Reason)
6F6779F5  | 8B4424 28               | mov eax,dword ptr ss:[esp+28]                   |
6F6779F9  | 50                      | push eax                                        |
6F6779FA  | 53                      | push ebx                                        |
6F6779FB  | 8D4C24 20               | lea ecx,dword ptr ss:[esp+20]                   |
6F6779FF  | E8 6CA4E4FF             | call game.6F4C1E70                              | ; 封包处理
6F677A04  | 6A 00                   | push 0                                          |
... (中间处理略) ...
6F677A22  | 8BCE                    | mov ecx,esi                                     | ; 恢复网络对象指针
6F677A24  | E8 F7330600             | call game.6F6DAE20                              | ; [关键] 发送数据包
...
6F677A69  | C3                      | ret                                             |
```

#### 4. 协议结构分析 (W3GS_LEAVEREQ)

该函数构建的数据包物理结构如下：

| 偏移 (Offset) | 类型 (Type) | 值 (Value) | 描述 (Description) |
| :--- | :--- | :--- | :--- |
| 0x00 | BYTE | **0xF7** | 包头 ID (Packet Header) |
| 0x01 | BYTE | **0x21** | 子命令 ID (LEAVEREQ) |
| 0x02 | BYTE/WORD | **0x00** | 填充或标志位 |
| 0x03+ | UINT32 | **Reason** | 离开原因代码 (见下表) |

**Reason 代码参考:**
- `0x01`: 主动断开 (Disconnect)
- `0x07`: 连接丢失 (Lost)
- `0x09`: 胜利 (Won)
- `0x0D`: 离开大厅 (Lobby Leave)

#### 5. 总结
此函数 (`6F677970`) 是 `game.dll` 中专门负责通知服务器或对等端“**我请求离开游戏**”的底层实现。它将上层逻辑传入的离开原因代码（如玩家点击退出菜单、游戏结束胜利/失败等）封装为 `F7 21` 协议包并发送。
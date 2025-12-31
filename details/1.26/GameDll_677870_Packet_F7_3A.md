### Warcraft III 内部发包函数 (F7 3A)

**环境:**
- **模块**：`game.dll`
- **偏移**： `677870`
- **基地址**：`6F000000`
- **  地址**：`game.dll + 677870`
- **客户端**：Warcraft III (v1.26.0.6401)

#### 1. 函数基本信息

*   **函数名称 (推测):** `NetSend_Packet_F7_3A` (非官方命名，基于行为命名)
*   **内存地址:** `6F677870`
*   **调用约定:** `__fastcall` (部分参数通过寄存器传递)
    *   `ECX`: `CNetClient` 或 `CGameServer` 对象指针 (网络连接类实例)
    *   `EDX`: 待发送的数据负载 (Payload) 指针
    *   `Stack`: 包含数据长度等参数

#### 2. C++ 伪代码还原

```cpp
// 还原的函数签名
// ecx: 网络对象指针 (this)
// edx: 数据内容的指针
// [esp+4]: 数据内容的长度 (在汇编中通过栈获取)
void __fastcall NetSend_Packet_F7_3A(void* pNetObject, void* pData, size_t dataLength) {
    CDataStore packetBuilder; // 局部变量，大约大小为 0x5D8+
    
    // 1. 初始化数据包构建器 (6F2C9290)
    packetBuilder.Initialize(); 

    // 2. 写入协议头 F7 (6F4C2160 - WriteInt8)
    packetBuilder.WriteInt8(0xF7);

    // 3. 写入子协议号 3A (6F4C2160 - WriteInt8)
    packetBuilder.WriteInt8(0x3A);

    // 4. 写入一个 0 (6F4C2210 - WriteInt32 或 WriteInt8/16 变种)
    // 注意：汇编中 push 0 后调用，这可能是填充或标志位
    packetBuilder.Write(0); 

    // 5. 写入实际负载数据 (6F6850F0)
    // 这里使用了传入的 pData (edi) 和长度 (ebx)
    packetBuilder.WriteBytes(pData, dataLength); 

    // 6. 数据包封包/加密/校验 (6F4C1E70, 6F4C1BB0)
    packetBuilder.Finalize();
    
    // 7. 发送数据包 (6F6DAE20)
    // 使用 ecx (pNetObject) 进行底层发送
    pNetObject->SendPacket(&packetBuilder);

    // 8. 清理 (6F2C95B0)
    packetBuilder.Destroy();
}
```

#### 3. 汇编代码详细注释

```assembly
6F677870  | 6A FF                   | push FFFFFFFF                                   | ; SEH 结构起始
6F677872  | 68 5B42846F             | push game.6F84425B                              | ; 异常处理函数
6F677877  | 64:A1 00000000          | mov eax,dword ptr fs:[0]                        |
6F67787D  | 50                      | push eax                                        |
6F67787E  | 81EC D8050000           | sub esp,5D8                                     | ; 分配栈空间 (用于 CDataStore 结构)
... (Stack Cookie 检查代码略) ...
6F6778AA  | 8BF1                    | mov esi,ecx                                     | ; 保存 this 指针 (网络对象) 到 ESI
6F6778AC  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]                   | ; ECX 指向栈上的 Packet 结构
6F6778B0  | 8BFA                    | mov edi,edx                                     | ; 保存数据指针参数 到 EDI
6F6778B2  | E8 D919C5FF             | call game.6F2C9290                              | ; [关键] 初始化 CDataStore/Packet
6F6778B7  | 68 F7000000             | push F7                                         | ; 压入 0xF7
6F6778BC  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]                   |
6F6778C0  | C78424 F4050000 0000000 | mov dword ptr ss:[esp+5F4],0                    |
6F6778CB  | E8 90A8E4FF             | call game.6F4C2160                              | ; [关键] WriteInt8(0xF7) - 写入主协议头
6F6778D0  | 6A 3A                   | push 3A                                         | ; 压入 0x3A
6F6778D2  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]                   |
6F6778D6  | E8 85A8E4FF             | call game.6F4C2160                              | ; [关键] WriteInt8(0x3A) - 写入子协议头
6F6778DB  | 8B5C24 28               | mov ebx,dword ptr ss:[esp+28]                   | ; 获取数据长度 (来自栈参数) 到 EBX
6F6778DF  | 6A 00                   | push 0                                          |
6F6778E1  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]                   |
6F6778E5  | E8 26A9E4FF             | call game.6F4C2210                              | ; 写入 0 (可能是 Flag 或 Padding)
6F6778EA  | 8BD7                    | mov edx,edi                                     | ; 恢复数据指针到 EDX
6F6778EC  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]                   |
6F6778F0  | E8 FBD70000             | call game.6F6850F0                              | ; [关键] 将数据拷贝到 Packet 缓冲区
6F6778F5  | 8B4424 28               | mov eax,dword ptr ss:[esp+28]                   |
6F6778F9  | 50                      | push eax                                        |
6F6778FA  | 53                      | push ebx                                        |
6F6778FB  | 8D4C24 20               | lea ecx,dword ptr ss:[esp+20]                   |
6F6778FF  | E8 6CA5E4FF             | call game.6F4C1E70                              | ; Packet 封包处理/校验计算
6F677904  | 6A 00                   | push 0                                          |
6F677906  | 8D4C24 14               | lea ecx,dword ptr ss:[esp+14]                   |
6F67790A  | 51                      | push ecx                                        |
6F67790B  | 8D5424 1C               | lea edx,dword ptr ss:[esp+1C]                   |
6F67790F  | 52                      | push edx                                        |
6F677910  | 8D4C24 24               | lea ecx,dword ptr ss:[esp+24]                   |
6F677914  | E8 97A2E4FF             | call game.6F4C1BB0                              | ; Packet 进一步处理
6F677919  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]                   |
6F67791D  | 8B5424 14               | mov edx,dword ptr ss:[esp+14]                   |
6F677921  | 50                      | push eax                                        |
6F677922  | 8BCE                    | mov ecx,esi                                     | ; 恢复网络对象指针到 ECX
6F677924  | E8 F7340600             | call game.6F6DAE20                              | ; [关键] 发送函数 (Send Packet)
6F677929  | 8B7424 10               | mov esi,dword ptr ss:[esp+10]                   |
6F67792D  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]                   |
6F677931  | C78424 F0050000 FFFFFFF | mov dword ptr ss:[esp+5F0],FFFFFFFF             |
6F67793C  | E8 6F1CC5FF             | call game.6F2C95B0                              | ; 析构 CDataStore，释放资源
... (返回及栈恢复) ...
6F677969  | C3                      | ret                                             |
```

#### 4. 协议结构分析

该函数构建的数据包物理结构如下：

| 偏移 (Offset) | 类型 (Type) | 值 (Value) | 描述 (Description) |
| :--- | :--- | :--- | :--- |
| 0x00 | BYTE | **0xF7** | 包头 ID (Packet Header) |
| 0x01 | BYTE | **0x3A** | 子命令 ID (Sub Command) |
| 0x02 | DWORD/BYTE? | **0x00** | 可能是填充或特定标志位 (由 6F4C2210 写入) |
| 0x03+ | BYTES | **Payload** | 由参数传入的动态数据内容 |

#### 5. 功能总结
该函数是 War3 内部用于同步特定游戏状态或命令的封装函数。调用方只需要提供数据负载指针，该函数会自动加上 `F7 3A` 的头部并完成网络发送。在魔兽争霸的协议中，`0xF7` 系列包通常与**游戏内部状态更新**、**批处理命令**或**Lag（卡顿）屏幕信息**有关，而 `0x3A` 是具体的动作标识。
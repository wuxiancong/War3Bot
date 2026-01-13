# War3 地图分块验证失败发包函数 (Map Chunk Validation Failure Handler)

**函数地址**: `0x6F66AB60` (Game.dll base `0x6F000000` + Offset `0x66AB60`)
**版本**: 1.26a
**功能**: 当客户端接收并校验地图数据分块失败（例如 CRC 校验错误、写入失败等）时，调用此函数构造并发送错误数据包。

## 1. 函数参数与上下文
*   **ECX (this)**: 指向网络连接对象或游戏状态对象的指针（`CNetConnection` 或 `CGameClient`）。
*   **EDX**: 错误代码或相关的状态值（作为参数传入）。

## 2. 详细反汇编分析

### 2.1 栈帧初始化
*   **`6F66AB60 - 6F66AB94`**: 标准的函数序言，保存寄存器 `ebx`, `esi`, `edi`，设置 SEH (Structured Exception Handling) 异常处理链，并初始化栈空间 (`sub esp, 5D8`)。

### 2.2 数据包构造准备
*   **`6F66AB9A`**: `mov esi, ecx` — 保存 `this` 指针到 `esi`。
*   **`6F66ABA0`**: `mov edi, edx` — 保存错误代码参数到 `edi`。
*   **`6F66ABA2`**: `call game.6F2C9290` — 初始化一个局部的数据包构建对象（通常是 `CDataStore` 或类似的缓冲区类）。

### 2.3 写入包头 (Packet Header)
*   **`6F66ABA7`**: `push F7` — **协议标识符** (Protocol ID)，`0xF7` 通常是 War3 的扩展游戏协议。
*   **`6F66ABBB`**: `call game.6F4C2160` — 写入 1 字节数据 (`WriteByte`)。
*   **`6F66ABC0`**: `push 45` — **包 ID (Packet ID)**。`0x45` 在 W3GS 协议中通常表示 **地图数据校验错误** 或 **请求重发**。
*   **`6F66ABC6`**: `call game.6F4C2160` — 写入 1 字节数据 (`WriteByte`)。

### 2.4 写入错误载荷 (Payload)
*   **`6F66ABCB`**: `mov ebx, dword ptr ss:[esp+28]` — 读取之前初始化对象时准备好的某些数据长度或指针。
*   **`6F66ABCF`**: `push 0` — 写入一个 `0` (可能表示“无额外数据”或“错误类型 A”)。
*   **`6F66ABD5`**: `call game.6F4C2210` — 写入 4 字节整数 (`WriteDWORD`)。
*   **`6F66ABDA`**: `mov edx, edi` — 恢复之前的错误代码参数。
*   **`6F66ABE0`**: `call game.6F684A60` — **核心逻辑**：将具体的错误信息、地图块偏移量或 checksum 写入到当前的数据包缓冲区中。这个函数负责序列化具体的错误细节。

### 2.5 封包与发送
*   **`6F66ABEF`**: `call game.6F4C1E70` — 可能是完成数据写入，更新包长度。
*   **`6F66AC04`**: `call game.6F4C1BB0` — 最终封包，准备发送缓冲区。
*   **`6F66AC09`**: `mov eax, dword ptr ss:[esp+10]` — 获取数据包缓冲区的指针。
*   **`6F66AC0D`**: `mov edx, dword ptr ss:[esp+14]` — 获取数据包长度。
*   **`6F66AC14`**: `call game.6F6DAE20` — **发送函数** (`SendPacket`)。
    *   `ecx` = `esi` (网络连接对象)。
    *   参数 = 数据包内容和长度。
    *   注释中的 `0x45` 再次印证了这是 Packet ID。

### 2.6 清理与返回
*   **`6F66AC2C`**: `call game.6F2C95B0` — 析构局部的数据包构建对象（释放内存）。
*   **`6F66AC3A - 6F66AC59`**: 恢复寄存器，撤销栈帧，返回。

## 3. 总结
该函数是 War3 P2P 地图下载机制的一部分。当接收方发现下载的数据块有问题时，它不会默默失败，而是调用此函数构造一个 `0xF7 0x45` 数据包通知发送方（主机）。

**数据包结构 (推测):**
1.  **Header**: `F7` (Protocol)
2.  **ID**: `45` (Map Data Error)
3.  **Size**: (2 bytes, handled by `6F4C1E70`/`6F4C1BB0`)
4.  **DWORD**: `0` (Unknown flag)
5.  **Error Details**: (Serialized by `6F684A60`, 包含错误代码 `edi` 和可能的块索引)

**主要用途**:
*   用于排查地图下载卡在 99% 或下载中断的问题。
*   Hook 此函数可以用来监控地图下载错误，或者强制伪造“下载失败”请求以触发重新下载。
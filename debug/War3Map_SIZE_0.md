# War3 机器人主机创建房间后加入房间槽位界面空白调试日志

![x64dbg调试截图](https://github.com/wuxiancong/War3Bot/raw/main/debug/images/War3Create_MAPERROR_NOSOLTSANDSEND42PACKET.PNG)

## 问题描述 (Issue Description)

**现象:**
在使用自定义 C++ 编写的 War3Bot 创建房间后，点击加入房间后槽位界面一片空白，拦截的数据显示发送 `0x42` 数据包，报告给服务器地图大小为0。

**环境 (Environment):**
- **客户端:** Warcraft III (v1.26.0.6401 / Game.dll)
- **服务端:** PVPGN
- **调试工具:** x64dbg
- **基地址:** 6F000000
- **关键函数地址:** 
    - 字符处理逻辑: `Game.dll + 5C9650`
    - 错误分发逻辑: `Game.dll + 5B51D0`
    - 界面处理逻辑: `Game.dll + 5BABF0`
	- 事件处理逻辑: `Game.dll + 62A5D0`
    - 状态解析逻辑: `Game.dll + 5BE710`

## 逆向入口
**日志拦截**
- 在拦截的数据中会收到一个 0x42 数据包，此包的发送必然会触发 ws2_32.dll 的 send 函数，在这里断点等待触发。
```dump
[2025-12-30 00:04:32.717] === send调用开始 (C->S) ===
[2025-12-30 00:04:32.717] Socket: 18972
[2025-12-30 00:04:32.717] 数据长度: 13 bytes
[2025-12-30 00:04:32.717] 缓冲区地址: 0x0019F654
[2025-12-30 00:04:32.717] 本地地址: 10.22.20.40:52261
[2025-12-30 00:04:32.717] === 捕获 W3GS 包 (Socket: 18972, ID: 0x42, Len: 13) ===
[2025-12-30 00:04:32.717]   未知字段:   0x00000001
[2025-12-30 00:04:32.717]   大小标志:   0x01
[2025-12-30 00:04:32.717]   地图大小:   0 字节
[2025-12-30 00:04:32.717]***[C->S] 解析出 W3GS 包: W3GS_MAPSIZE***
[2025-12-30 00:04:32.717] 数据内容(Hex):
F7 42 0D 00 01 00 00 00 01 00 00 00 00 
[2025-12-30 00:04:32.717] 数据内容(ASCII):
.B...........
[2025-12-30 00:04:32.717] 发送到主机 (从映射表): 207.90.237.73:6201
[2025-12-30 00:04:32.717] 决策: 是游戏主Socket，通过IPC转发并拦截。
[2025-12-30 00:04:32.717] 通过IPC将数据包转发给主程序...
[2025-12-30 00:04:32.717] IPC写入成功！
[2025-12-30 00:04:32.717] IPC转发完成，正在执行物理发送...
[2025-12-30 00:04:32.717] 游戏Socket真实发送结果: 13
[2025-12-30 00:04:32.717] === send调用结束 ===
```
**函数原型**


```c
int send(
  SOCKET     s,
  const char *buf,
  int        len,
  int        flags
);
```

**参数说明**

| 参数 | 类型 | 描述 | 必需 |
|------|------|------|------|
| `s` | `SOCKET` | 已连接的套接字描述符 | 是 |
| `buf` | `const char*` | 待发送数据的缓冲区指针 | 是 |
| `len` | `int` | 缓冲区数据的字节长度 | 是 |
| `flags` | `int` | 控制发送行为的标志位 | 否（默认0） |

**返回值**

- **成功**：返回实际发送的字节数（可能小于 `len`）
- **失败**：返回 `SOCKET_ERROR`（-1）
  - 使用 `WSAGetLastError()` 获取详细错误代码

---

断点后查看调用栈的第二个参数，查看内存数据的数据包是不是以 '000D42F7' 开始，如果不是直接 'F9' 跳过，或者直接看调用栈有没有 "Maps\\Download\\DotA" 这样的字样。


## 数据发送
- **地址**：`6F6DF040`
- **偏移**：`game.dll + 6DF040`

```assembly
6F6DF040  | 51                      | push ecx                            |
6F6DF041  | 56                      | push esi                            |
6F6DF042  | 57                      | push edi                            |
6F6DF043  | 8BF1                    | mov esi,ecx                         |
6F6DF045  | 8D7E 44                 | lea edi,dword ptr ds:[esi+44]       |
6F6DF048  | 57                      | push edi                            |
6F6DF049  | 897C24 0C               | mov dword ptr ss:[esp+C],edi        |
6F6DF04D  | FF15 74D1866F           | call dword ptr ds:[<EnterCriticalSe |
6F6DF053  | 8B4E 04                 | mov ecx,dword ptr ds:[esi+4]        |
6F6DF056  | 83F9 FF                 | cmp ecx,FFFFFFFF                    |
6F6DF059  | 75 0D                   | jne game.6F6DF068                   |
6F6DF05B  | 57                      | push edi                            |
6F6DF05C  | FF15 70D1866F           | call dword ptr ds:[<LeaveCriticalSe |
6F6DF062  | 5F                      | pop edi                             |
6F6DF063  | 5E                      | pop esi                             |
6F6DF064  | 59                      | pop ecx                             |
6F6DF065  | C2 0800                 | ret 8                               |
6F6DF068  | 8B46 70                 | mov eax,dword ptr ds:[esi+70]       |
6F6DF06B  | 33D2                    | xor edx,edx                         |
6F6DF06D  | 8B7C24 14               | mov edi,dword ptr ss:[esp+14]       |
6F6DF071  | 85C0                    | test eax,eax                        |
6F6DF073  | 0F9EC2                  | setle dl                            |
6F6DF076  | 53                      | push ebx                            |
6F6DF077  | 8B5C24 14               | mov ebx,dword ptr ss:[esp+14]       |
6F6DF07B  | 55                      | push ebp                            |
6F6DF07C  | 83EA 01                 | sub edx,1                           |
6F6DF07F  | 23C2                    | and eax,edx                         |
6F6DF081  | 33D2                    | xor edx,edx                         |
6F6DF083  | 85C0                    | test eax,eax                        |
6F6DF085  | 0F95C2                  | setne dl                            |
6F6DF088  | 8BEA                    | mov ebp,edx                         |
6F6DF08A  | 85ED                    | test ebp,ebp                        |
6F6DF08C  | 75 2A                   | jne game.6F6DF0B8                   |
6F6DF08E  | 52                      | push edx                            |
6F6DF08F  | 57                      | push edi                            |
6F6DF090  | 53                      | push ebx                            |
6F6DF091  | 51                      | push ecx                            |
6F6DF092  | FF15 C0D8866F           | call dword ptr ds:[<Ordinal#19>]    | <--- 调用 send 发送 #42 包
6F6DF098  | 83F8 FF                 | cmp eax,FFFFFFFF                    |
6F6DF09B  | 74 3E                   | je game.6F6DF0DB                    |
6F6DF09D  | 3BC7                    | cmp eax,edi                         |
6F6DF09F  | 7C 13                   | jl game.6F6DF0B4                    |
6F6DF0A1  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6DF0A5  | 50                      | push eax                            |
6F6DF0A6  | FF15 70D1866F           | call dword ptr ds:[<LeaveCriticalSe |
6F6DF0AC  | 5D                      | pop ebp                             |
6F6DF0AD  | 5B                      | pop ebx                             |
6F6DF0AE  | 5F                      | pop edi                             |
6F6DF0AF  | 5E                      | pop esi                             |
6F6DF0B0  | 59                      | pop ecx                             |
6F6DF0B1  | C2 0800                 | ret 8                               |
6F6DF0B4  | 03D8                    | add ebx,eax                         |
6F6DF0B6  | 2BF8                    | sub edi,eax                         |
6F6DF0B8  | 57                      | push edi                            |
6F6DF0B9  | 53                      | push ebx                            |
6F6DF0BA  | 8BCE                    | mov ecx,esi                         |
6F6DF0BC  | E8 FFFDFFFF             | call game.6F6DEEC0                  |
6F6DF0C1  | 85ED                    | test ebp,ebp                        |
6F6DF0C3  | 75 2C                   | jne game.6F6DF0F1                   |
6F6DF0C5  | 85C0                    | test eax,eax                        |
6F6DF0C7  | 74 28                   | je game.6F6DF0F1                    |
6F6DF0C9  | 8B16                    | mov edx,dword ptr ds:[esi]          |
6F6DF0CB  | 50                      | push eax                            |
6F6DF0CC  | 8B42 3C                 | mov eax,dword ptr ds:[edx+3C]       |
6F6DF0CF  | 8BCE                    | mov ecx,esi                         |
6F6DF0D1  | FFD0                    | call eax                            |
6F6DF0D3  | 5D                      | pop ebp                             |
6F6DF0D4  | 5B                      | pop ebx                             |
6F6DF0D5  | 5F                      | pop edi                             |
6F6DF0D6  | 5E                      | pop esi                             |
6F6DF0D7  | 59                      | pop ecx                             |
6F6DF0D8  | C2 0800                 | ret 8                               |
6F6DF0DB  | FF15 9CD8866F           | call dword ptr ds:[<Ordinal#111>]   |
6F6DF0E1  | 3D 33270000             | cmp eax,2733                        |
6F6DF0E6  | 74 D0                   | je game.6F6DF0B8                    |
6F6DF0E8  | 8B16                    | mov edx,dword ptr ds:[esi]          |
6F6DF0EA  | 8B42 24                 | mov eax,dword ptr ds:[edx+24]       |
6F6DF0ED  | 8BCE                    | mov ecx,esi                         |
6F6DF0EF  | FFD0                    | call eax                            |
6F6DF0F1  | 8B4C24 10               | mov ecx,dword ptr ss:[esp+10]       |
6F6DF0F5  | 51                      | push ecx                            |
6F6DF0F6  | FF15 70D1866F           | call dword ptr ds:[<LeaveCriticalSe |
6F6DF0FC  | 5D                      | pop ebp                             |
6F6DF0FD  | 5B                      | pop ebx                             |
6F6DF0FE  | 5F                      | pop edi                             |
6F6DF0FF  | 5E                      | pop esi                             |
6F6DF100  | 59                      | pop ecx                             |
6F6DF101  | C2 0800                 | ret 8                               |
```

**调用栈情况**
- **0019F654** 是缓冲区指针
- **0000000D** 是数据的大小

```dump
0019F5E4  6F6DF098  返回到 game.GameMain+6D5848 自 ???
0019F5E8  00004E94  
0019F5EC  0019F654
0019F5F0  0000000D  
0019F5F4  00000000  
0019F5F8  1D9100A4  "Maps\\Download\\DotA v6.83d.w3x"
0019F5FC  00000002  
0019F600  0019FC1C  
0019F604  0000000D  
0019F608  03B90724  
0019F60C  6F6DAE3F  返回到 game.GameMain+6D15EF 自 ???

```

**发送的内容**
- **42F7** 是包协议头
- **000D** 是数据大小

```
0019F654  000D42F7
0019F658  00000001
0019F65C  00000001
```

**继续栈追踪**
- **6F6DAE20** 是一个包装函数，暂时不管它，继续向上追踪。

```assembly
6F6DAE20  | 85C9                    | test ecx,ecx                        |
6F6DAE22  | 75 0D                   | jne game.6F6DAE31                   |
6F6DAE24  | C74424 04 57000000      | mov dword ptr ss:[esp+4],57         |
6F6DAE2C  | E9 7B070100             | jmp <JMP.&Ordinal#465>              |
6F6DAE31  | 8B01                    | mov eax,dword ptr ds:[ecx]          |
6F6DAE33  | 56                      | push esi                            |
6F6DAE34  | 8B7424 08               | mov esi,dword ptr ss:[esp+8]        |
6F6DAE38  | 56                      | push esi                            |
6F6DAE39  | 52                      | push edx                            |
6F6DAE3A  | 8B50 2C                 | mov edx,dword ptr ds:[eax+2C]       |
6F6DAE3D  | FFD2                    | call edx                            |
6F6DAE3F  | 5E                      | pop esi                             |
6F6DAE40  | C2 0400                 | ret 4                               |
```
---

## 发包逻辑
- **地址**：`6F678A70`
- **偏移**：`game.dll + 678A70`
- **关键发现**：注意观察反汇编中是不是有 `F7` 和 `42` 这样的字符，很显然这是发送 `C>S 0x42 W3GS_MAPSIZE` 的逻辑。
- **参考文档**：[https://bnetdocs.org/packet/467/w3gs-mapsize](https://bnetdocs.org/packet/467/w3gs-mapsize)
```
(UINT32) Unknown
(UINT8) Size Flag
(UINT32) Map Size
```

```assembly
6F678A70  | 6A FF                   | push FFFFFFFF                       |
6F678A72  | 68 DB46846F             | push game.6F8446DB                  |
6F678A77  | 64:A1 00000000          | mov eax,dword ptr fs:[0]            |
6F678A7D  | 50                      | push eax                            |
6F678A7E  | 81EC D8050000           | sub esp,5D8                         |
6F678A84  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]     |
6F678A89  | 33C4                    | xor eax,esp                         |
6F678A8B  | 898424 D4050000         | mov dword ptr ss:[esp+5D4],eax      |
6F678A92  | 53                      | push ebx                            |
6F678A93  | 56                      | push esi                            |
6F678A94  | 57                      | push edi                            |
6F678A95  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]     |
6F678A9A  | 33C4                    | xor eax,esp                         |
6F678A9C  | 50                      | push eax                            |
6F678A9D  | 8D8424 E8050000         | lea eax,dword ptr ss:[esp+5E8]      |
6F678AA4  | 64:A3 00000000          | mov dword ptr fs:[0],eax            |
6F678AAA  | 8BF1                    | mov esi,ecx                         |
6F678AAC  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]       |
6F678AB0  | 8BFA                    | mov edi,edx                         |
6F678AB2  | E8 D907C5FF             | call game.6F2C9290                  |
6F678AB7  | 68 F7000000             | push F7                             | <--- F7（协议类型）
6F678ABC  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]       |
6F678AC0  | C78424 F4050000 0000000 | mov dword ptr ss:[esp+5F4],0        |
6F678ACB  | E8 9096E4FF             | call game.6F4C2160                  |
6F678AD0  | 6A 42                   | push 42                             | <--- 42（包类型）
6F678AD2  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]       |
6F678AD6  | E8 8596E4FF             | call game.6F4C2160                  |
6F678ADB  | 8B5C24 28               | mov ebx,dword ptr ss:[esp+28]       |
6F678ADF  | 6A 00                   | push 0                              |
6F678AE1  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]       |
6F678AE5  | E8 2697E4FF             | call game.6F4C2210                  |
6F678AEA  | 8BD7                    | mov edx,edi                         |
6F678AEC  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]       |
6F678AF0  | E8 EBB90000             | call game.6F6844E0                  |
6F678AF5  | 8B4424 28               | mov eax,dword ptr ss:[esp+28]       |
6F678AF9  | 50                      | push eax                            |
6F678AFA  | 53                      | push ebx                            |
6F678AFB  | 8D4C24 20               | lea ecx,dword ptr ss:[esp+20]       |
6F678AFF  | E8 6C93E4FF             | call game.6F4C1E70                  |
6F678B04  | 6A 00                   | push 0                              |
6F678B06  | 8D4C24 14               | lea ecx,dword ptr ss:[esp+14]       |
6F678B0A  | 51                      | push ecx                            |
6F678B0B  | 8D5424 1C               | lea edx,dword ptr ss:[esp+1C]       |
6F678B0F  | 52                      | push edx                            |
6F678B10  | 8D4C24 24               | lea ecx,dword ptr ss:[esp+24]       |
6F678B14  | E8 9790E4FF             | call game.6F4C1BB0                  |
6F678B19  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F678B1D  | 8B5424 14               | mov edx,dword ptr ss:[esp+14]       |
6F678B21  | 50                      | push eax                            |
6F678B22  | 8BCE                    | mov ecx,esi                         |
6F678B24  | E8 F7220600             | call game.6F6DAE20                  | <--- 发送 0x42 包
6F678B29  | 8B7424 10               | mov esi,dword ptr ss:[esp+10]       |
6F678B2D  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]       |
6F678B31  | C78424 F0050000 FFFFFFF | mov dword ptr ss:[esp+5F0],FFFFFFFF |
6F678B3C  | E8 6F0AC5FF             | call game.6F2C95B0                  |
6F678B41  | 8BC6                    | mov eax,esi                         |
6F678B43  | 8B8C24 E8050000         | mov ecx,dword ptr ss:[esp+5E8]      |
6F678B4A  | 64:890D 00000000        | mov dword ptr fs:[0],ecx            |
6F678B51  | 59                      | pop ecx                             |
6F678B52  | 5F                      | pop edi                             |
6F678B53  | 5E                      | pop esi                             |
6F678B54  | 5B                      | pop ebx                             |
6F678B55  | 8B8C24 D4050000         | mov ecx,dword ptr ss:[esp+5D4]      |
6F678B5C  | 33CC                    | xor ecx,esp                         |
6F678B5E  | E8 F6841600             | call game.6F7E1059                  |
6F678B63  | 81C4 E4050000           | add esp,5E4                         |
6F678B69  | C3                      | ret                                 |
```

## 发包准备
- **地址**：`6F656E00`
- **偏移**：`game.dll + 656E00`
```assembly
6F656E00  | 56                      | push esi                            |
6F656E01  | 8BF1                    | mov esi,ecx                         |
6F656E03  | 8B06                    | mov eax,dword ptr ds:[esi]          |
6F656E05  | 85C0                    | test eax,eax                        |
6F656E07  | 57                      | push edi                            |
6F656E08  | 8BFA                    | mov edi,edx                         |
6F656E0A  | 74 12                   | je game.6F656E1E                    |
6F656E0C  | 6A 00                   | push 0                              |
6F656E0E  | 68 2F040000             | push 42F                            |
6F656E13  | 68 8C10976F             | push game.6F97108C                  | <--- NetProvider
6F656E18  | 50                      | push eax                            |
6F656E19  | E8 3A470900             | call <JMP.&Ordinal#403>             |
6F656E1E  | C706 00000000           | mov dword ptr ds:[esi],0            |
6F656E24  | C707 00000000           | mov dword ptr ds:[edi],0            |
6F656E2A  | 5F                      | pop edi                             |
6F656E2B  | B8 01000000             | mov eax,1                           |
6F656E30  | 5E                      | pop esi                             |
6F656E31  | C3                      | ret                                 |

## 核心函数
- **地址**：`6F657C10`
- **偏移**：`game.dll + 657930`

```assembly
6F657930  | 83EC 44                 | sub esp,44                          |
6F657933  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]     |
6F657938  | 33C4                    | xor eax,esp                         |
6F65793A  | 894424 40               | mov dword ptr ss:[esp+40],eax       |
6F65793E  | 8B4424 48               | mov eax,dword ptr ss:[esp+48]       |
6F657942  | 53                      | push ebx                            |
6F657943  | 56                      | push esi                            |
6F657944  | 57                      | push edi                            |
6F657945  | 8BF9                    | mov edi,ecx                         |
6F657947  | 8B4C24 58               | mov ecx,dword ptr ss:[esp+58]       |
6F65794B  | 8B5C24 70               | mov ebx,dword ptr ss:[esp+70]       |
6F65794F  | 894C24 24               | mov dword ptr ss:[esp+24],ecx       |
6F657953  | 8B4C24 60               | mov ecx,dword ptr ss:[esp+60]       |
6F657957  | 8BF2                    | mov esi,edx                         |
6F657959  | 8B5424 5C               | mov edx,dword ptr ss:[esp+5C]       |
6F65795D  | 894C24 2C               | mov dword ptr ss:[esp+2C],ecx       |
6F657961  | 8B4C24 68               | mov ecx,dword ptr ss:[esp+68]       |
6F657965  | 895424 28               | mov dword ptr ss:[esp+28],edx       |
6F657969  | 8B5424 64               | mov edx,dword ptr ss:[esp+64]       |
6F65796D  | 894C24 34               | mov dword ptr ss:[esp+34],ecx       |
6F657971  | 8B4C24 6C               | mov ecx,dword ptr ss:[esp+6C]       |
6F657975  | 51                      | push ecx                            |
6F657976  | 895424 34               | mov dword ptr ss:[esp+34],edx       |
6F65797A  | 8B5424 78               | mov edx,dword ptr ss:[esp+78]       |
6F65797E  | 895424 20               | mov dword ptr ss:[esp+20],edx       |
6F657982  | 8B5424 7C               | mov edx,dword ptr ss:[esp+7C]       |
6F657986  | 8D4C24 3C               | lea ecx,dword ptr ss:[esp+3C]       |
6F65798A  | 51                      | push ecx                            |
6F65798B  | 50                      | push eax                            |
6F65798C  | 895424 20               | mov dword ptr ss:[esp+20],edx       |
6F657990  | 8D5424 24               | lea edx,dword ptr ss:[esp+24]       |
6F657994  | 52                      | push edx                            |
6F657995  | 8D4424 30               | lea eax,dword ptr ss:[esp+30]       |
6F657999  | 50                      | push eax                            |
6F65799A  | 8D4C24 20               | lea ecx,dword ptr ss:[esp+20]       |
6F65799E  | 51                      | push ecx                            |
6F65799F  | 8D5424 28               | lea edx,dword ptr ss:[esp+28]       |
6F6579A3  | 8BCF                    | mov ecx,edi                         | <--- Maps\\Download\\DotA v6.83d.w3x
6F6579A5  | E8 C6FDFFFF             | call game.6F657770                  | <--- 本地CRC和Sha1计算
6F6579AA  | 83F8 01                 | cmp eax,1                           | <--- eax=1
6F6579AD  | 0F85 BE000000           | jne game.6F657A71                   | <--- 跳转不会执行
6F6579B3  | 397424 18               | cmp dword ptr ss:[esp+18],esi       | <--- esi=3AEBCEF3（对比 CRC）
6F6579B7  | 0F85 B4000000           | jne game.6F657A71                   | <--- 跳转不会执行（结果: Ok）
6F6579BD  | B8 14000000             | mov eax,14                          |
6F6579C2  | 8D4C24 24               | lea ecx,dword ptr ss:[esp+24]       |
6F6579C6  | 8D5424 38               | lea edx,dword ptr ss:[esp+38]       |
6F6579CA  | 55                      | push ebp                            |
6F6579CB  | EB 03                   | jmp game.6F6579D0                   |
6F6579CD  | 8D49 00                 | lea ecx,dword ptr ds:[ecx]          |
6F6579D0  | 8B32                    | mov esi,dword ptr ds:[edx]          | <--- 读取远程服务端 SHA1 的前 4 字节
6F6579D2  | 3B31                    | cmp esi,dword ptr ds:[ecx]          | <--- 对比本地客户端 SHA1 的前 4 字节
6F6579D4  | 75 12                   | jne game.6F6579E8                   | <--- 跳转将要执行（结果: Sha1不匹配）
6F6579D6  | 83E8 04                 | sub eax,4                           | 
6F6579D9  | 83C1 04                 | add ecx,4                           | 
6F6579DC  | 83C2 04                 | add edx,4                           | 
6F6579DF  | 83F8 04                 | cmp eax,4                           | 
6F6579E2  | 73 EC                   | jae game.6F6579D0                   | 
6F6579E4  | 85C0                    | test eax,eax                        | 
6F6579E6  | 74 5D                   | je game.6F657A45                    | 
6F6579E8  | 0FB632                  | movzx esi,byte ptr ds:[edx]         | <--- byte ptr ds:[edx]=[0019F734]=90
6F6579EB  | 0FB629                  | movzx ebp,byte ptr ds:[ecx]         | <--- byte ptr ds:[ecx]=[0019F720]=2E
6F6579EE  | 2BF5                    | sub esi,ebp                         | <--- esi=90, ebp=2E
6F6579F0  | 75 45                   | jne game.6F657A37                   | <--- 跳转将要执行
6F6579F2  | 83E8 01                 | sub eax,1                           |
6F6579F5  | 83C1 01                 | add ecx,1                           |
6F6579F8  | 83C2 01                 | add edx,1                           |
6F6579FB  | 85C0                    | test eax,eax                        |
6F6579FD  | 74 46                   | je game.6F657A45                    |
6F6579FF  | 0FB632                  | movzx esi,byte ptr ds:[edx]         |
6F657A02  | 0FB629                  | movzx ebp,byte ptr ds:[ecx]         |
6F657A05  | 2BF5                    | sub esi,ebp                         |
6F657A07  | 75 2E                   | jne game.6F657A37                   |
6F657A09  | 83E8 01                 | sub eax,1                           |
6F657A0C  | 83C1 01                 | add ecx,1                           |
6F657A0F  | 83C2 01                 | add edx,1                           |
6F657A12  | 85C0                    | test eax,eax                        |
6F657A14  | 74 2F                   | je game.6F657A45                    |
6F657A16  | 0FB632                  | movzx esi,byte ptr ds:[edx]         |
6F657A19  | 0FB629                  | movzx ebp,byte ptr ds:[ecx]         |
6F657A1C  | 2BF5                    | sub esi,ebp                         |
6F657A1E  | 75 17                   | jne game.6F657A37                   |
6F657A20  | 83E8 01                 | sub eax,1                           |
6F657A23  | 83C1 01                 | add ecx,1                           |
6F657A26  | 83C2 01                 | add edx,1                           |
6F657A29  | 85C0                    | test eax,eax                        |
6F657A2B  | 74 18                   | je game.6F657A45                    |
6F657A2D  | 0FB632                  | movzx esi,byte ptr ds:[edx]         |
6F657A30  | 0FB611                  | movzx edx,byte ptr ds:[ecx]         |
6F657A33  | 2BF2                    | sub esi,edx                         |
6F657A35  | 74 0E                   | je game.6F657A45                    |
6F657A37  | 85F6                    | test esi,esi                        |
6F657A39  | B8 01000000             | mov eax,1                           |
6F657A3E  | 7F 07                   | jg game.6F657A47                    |
6F657A40  | 83C8 FF                 | or eax,FFFFFFFF                     |
6F657A43  | EB 02                   | jmp game.6F657A47                   |
6F657A45  | 33C0                    | xor eax,eax                         |
6F657A47  | 85C0                    | test eax,eax                        |
6F657A49  | 5D                      | pop ebp                             |
6F657A4A  | 75 25                   | jne game.6F657A71                   |
6F657A4C  | 8B4C24 14               | mov ecx,dword ptr ss:[esp+14]       |
6F657A50  | 8D70 01                 | lea esi,dword ptr ds:[eax+1]        |
6F657A53  | 8B4424 7C               | mov eax,dword ptr ss:[esp+7C]       |
6F657A57  | 50                      | push eax                            |
6F657A58  | 57                      | push edi                            |
6F657A59  | 51                      | push ecx                            |
6F657A5A  | E8 653B0900             | call <JMP.&Ordinal#501>             |
6F657A5F  | 8B5424 10               | mov edx,dword ptr ss:[esp+10]       |
6F657A63  | 8B4424 0C               | mov eax,dword ptr ss:[esp+C]        |
6F657A67  | 8B4C24 1C               | mov ecx,dword ptr ss:[esp+1C]       |
6F657A6B  | 8913                    | mov dword ptr ds:[ebx],edx          |
6F657A6D  | 8901                    | mov dword ptr ds:[ecx],eax          |
6F657A6F  | EB 0F                   | jmp game.6F657A80                   |
6F657A71  | 8D5424 0C               | lea edx,dword ptr ss:[esp+C]        |
6F657A75  | 8D4C24 10               | lea ecx,dword ptr ss:[esp+10]       |
6F657A79  | 33F6                    | xor esi,esi                         |
6F657A7B  | E8 80F3FFFF             | call game.6F656E00                  | <--- SHA1对比失败，报告给服务器，发送 0x42 包。
6F657A80  | 8B4C24 4C               | mov ecx,dword ptr ss:[esp+4C]       |
6F657A84  | 5F                      | pop edi                             |
6F657A85  | 8BC6                    | mov eax,esi                         |
6F657A87  | 5E                      | pop esi                             |
6F657A88  | 5B                      | pop ebx                             |
6F657A89  | 33CC                    | xor ecx,esp                         |
6F657A8B  | E8 C9951800             | call game.6F7E1059                  |
6F657A90  | 83C4 44                 | add esp,44                          |
6F657A93  | C2 2C00                 | ret 2C                              |
```

## 地图路径
- **地址**：`6F657C10`
- **偏移**：`game.dll + 657C10`

下面的反汇编是为 '6F657D2E' 处准备数据（如：地图路径）给 `game.6F657930` 函数调用

```assembly
6F657C10  | 81EC 70040000           | sub esp,470                         |
6F657C16  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]     |
6F657C1B  | 33C4                    | xor eax,esp                         |
6F657C1D  | 898424 6C040000         | mov dword ptr ss:[esp+46C],eax      |
6F657C24  | 8B8424 7C040000         | mov eax,dword ptr ss:[esp+47C]      |
6F657C2B  | 8B9424 80040000         | mov edx,dword ptr ss:[esp+480]      |
6F657C32  | 894424 10               | mov dword ptr ss:[esp+10],eax       |
6F657C36  | 8B8424 84040000         | mov eax,dword ptr ss:[esp+484]      |
6F657C3D  | 53                      | push ebx                            |
6F657C3E  | 55                      | push ebp                            |
6F657C3F  | 8BAC24 A0040000         | mov ebp,dword ptr ss:[esp+4A0]      |
6F657C46  | 85ED                    | test ebp,ebp                        |
6F657C48  | 895424 1C               | mov dword ptr ss:[esp+1C],edx       |
6F657C4C  | 8B9424 90040000         | mov edx,dword ptr ss:[esp+490]      |
6F657C53  | 894424 20               | mov dword ptr ss:[esp+20],eax       |
6F657C57  | 8B8424 94040000         | mov eax,dword ptr ss:[esp+494]      |
6F657C5E  | 56                      | push esi                            | <--- FileInfo->NetClient->Net
6F657C5F  | 895424 28               | mov dword ptr ss:[esp+28],edx       |
6F657C63  | 8B9424 A0040000         | mov edx,dword ptr ss:[esp+4A0]      |
6F657C6A  | 894424 2C               | mov dword ptr ss:[esp+2C],eax       |
6F657C6E  | 8B8424 9C040000         | mov eax,dword ptr ss:[esp+49C]      |
6F657C75  | 57                      | push edi                            |
6F657C76  | 8BBC24 84040000         | mov edi,dword ptr ss:[esp+484]      | <--- Maps\\Download\\DotA v6.83d.w3x
6F657C7D  | 894C24 1C               | mov dword ptr ss:[esp+1C],ecx       | <--- NetProviderBNET
6F657C81  | 894424 14               | mov dword ptr ss:[esp+14],eax       |
6F657C85  | 895424 18               | mov dword ptr ss:[esp+18],edx       |
6F657C89  | C700 00000000           | mov dword ptr ds:[eax],0            |
6F657C8F  | 74 0E                   | je game.6F657C9F                    | <--- 跳转不会执行
6F657C91  | 83BC24 AC040000 00      | cmp dword ptr ss:[esp+4AC],0        | <--- 0!=104
6F657C99  | 74 04                   | je game.6F657C9F                    | <--- 跳转不会执行
6F657C9B  | C645 00 00              | mov byte ptr ss:[ebp],0             |
6F657C9F  | 8B81 EC050000           | mov eax,dword ptr ds:[ecx+5EC]      |
6F657CA5  | 8B99 E8050000           | mov ebx,dword ptr ds:[ecx+5E8]      |
6F657CAB  | BA 04010000             | mov edx,104                         |
6F657CB0  | 8D8C24 74020000         | lea ecx,dword ptr ss:[esp+274]      |
6F657CB7  | 894424 10               | mov dword ptr ss:[esp+10],eax       |
6F657CBB  | E8 40E30600             | call game.6F6C6000                  |
6F657CC0  | 8D8C24 74020000         | lea ecx,dword ptr ss:[esp+274]      | <--- 安装目录（D:\\我的游戏\\iCCup Warcraft III\\）
6F657CC7  | 51                      | push ecx                            |
6F657CC8  | E8 99390900             | call <JMP.&Ordinal#506>             |
6F657CCD  | 8BF0                    | mov esi,eax                         | <--- eax=1F
6F657CCF  | BA 04010000             | mov edx,104                         | <--- edx=104
6F657CD4  | 2BD6                    | sub edx,esi                         |
6F657CD6  | 52                      | push edx                            | <--- edx=E5
6F657CD7  | 57                      | push edi                            | <--- 相对路径（Maps\\Download\\DotA v6.83d.w3x）
6F657CD8  | 8D8434 7C020000         | lea eax,dword ptr ss:[esp+esi+27C]  |
6F657CDF  | 50                      | push eax                            |
6F657CE0  | E8 DF380900             | call <JMP.&Ordinal#501>             | <--- SStrPrintf (Storm.dll Ordinal #501)
6F657CE5  | 8B8C24 AC040000         | mov ecx,dword ptr ss:[esp+4AC]      | <--- 拼接成完整的本地绝对路径
6F657CEC  | 8B5424 18               | mov edx,dword ptr ss:[esp+18]       |
6F657CF0  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F657CF4  | 51                      | push ecx                            |
6F657CF5  | 8B4C24 14               | mov ecx,dword ptr ss:[esp+14]       |
6F657CF9  | 55                      | push ebp                            |
6F657CFA  | 52                      | push edx                            |
6F657CFB  | 8B5424 2C               | mov edx,dword ptr ss:[esp+2C]       |
6F657CFF  | 50                      | push eax                            |
6F657D00  | 51                      | push ecx                            |
6F657D01  | 8B4C24 38               | mov ecx,dword ptr ss:[esp+38]       |
6F657D05  | 83EC 14                 | sub esp,14                          |
6F657D08  | 8BC4                    | mov eax,esp                         |
6F657D0A  | 8910                    | mov dword ptr ds:[eax],edx          |
6F657D0C  | 8B5424 50               | mov edx,dword ptr ss:[esp+50]       |
6F657D10  | 8948 04                 | mov dword ptr ds:[eax+4],ecx        |
6F657D13  | 8B4C24 54               | mov ecx,dword ptr ss:[esp+54]       |
6F657D17  | 8950 08                 | mov dword ptr ds:[eax+8],edx        |
6F657D1A  | 8B5424 58               | mov edx,dword ptr ss:[esp+58]       |
6F657D1E  | 8948 0C                 | mov dword ptr ds:[eax+C],ecx        |
6F657D21  | 8950 10                 | mov dword ptr ds:[eax+10],edx       |
6F657D24  | 8B9424 B0040000         | mov edx,dword ptr ss:[esp+4B0]      |
6F657D2B  | 53                      | push ebx                            |
6F657D2C  | 8BCF                    | mov ecx,edi                         | <--- 地图路径（Maps\\Download\\DotA v6.83d.w3x）
6F657D2E  | E8 FDFBFFFF             | call game.6F657930                  | <--- 关键函数
6F657D33  | 85C0                    | test eax,eax                        | <--- eax=0 -> 失败
6F657D35  | 74 0A                   | je game.6F657D41                    |
6F657D37  | B8 01000000             | mov eax,1                           |
6F657D3C  | E9 1F010000             | jmp game.6F657E60                   |
6F657D41  | 8B4C24 20               | mov ecx,dword ptr ss:[esp+20]       |
6F657D45  | 8B8424 88040000         | mov eax,dword ptr ss:[esp+488]      |
6F657D4C  | 8B5424 24               | mov edx,dword ptr ss:[esp+24]       |
6F657D50  | 894C24 40               | mov dword ptr ss:[esp+40],ecx       |
6F657D54  | 8B4C24 2C               | mov ecx,dword ptr ss:[esp+2C]       |
6F657D58  | 894424 34               | mov dword ptr ss:[esp+34],eax       |
6F657D5C  | 8B4424 28               | mov eax,dword ptr ss:[esp+28]       |
6F657D60  | 895424 44               | mov dword ptr ss:[esp+44],edx       |
6F657D64  | 8B5424 30               | mov edx,dword ptr ss:[esp+30]       |
6F657D68  | 894C24 4C               | mov dword ptr ss:[esp+4C],ecx       |
6F657D6C  | 8B8C24 AC040000         | mov ecx,dword ptr ss:[esp+4AC]      |
6F657D73  | 895C24 38               | mov dword ptr ss:[esp+38],ebx       |
6F657D77  | 894424 48               | mov dword ptr ss:[esp+48],eax       |
6F657D7B  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F657D7F  | 895424 50               | mov dword ptr ss:[esp+50],edx       |
6F657D83  | 33DB                    | xor ebx,ebx                         |
6F657D85  | 894C24 60               | mov dword ptr ss:[esp+60],ecx       |
6F657D89  | 68 04010000             | push 104                            |
6F657D8E  | 8D5424 68               | lea edx,dword ptr ss:[esp+68]       |
6F657D92  | 8BCF                    | mov ecx,edi                         |
6F657D94  | 894424 40               | mov dword ptr ss:[esp+40],eax       |
6F657D98  | 895C24 58               | mov dword ptr ss:[esp+58],ebx       |
6F657D9C  | 895C24 5C               | mov dword ptr ss:[esp+5C],ebx       |
6F657DA0  | 896C24 60               | mov dword ptr ss:[esp+60],ebp       |
6F657DA4  | 89B424 70020000         | mov dword ptr ss:[esp+270],esi      |
6F657DAB  | E8 50DC0600             | call game.6F6C5A00                  |
6F657DB0  | 83C6 01                 | add esi,1                           |
6F657DB3  | 81FE 04010000           | cmp esi,104                         |
6F657DB9  | 76 05                   | jbe game.6F657DC0                   |
6F657DBB  | BE 04010000             | mov esi,104                         |
6F657DC0  | 56                      | push esi                            |
6F657DC1  | 8D9424 78020000         | lea edx,dword ptr ss:[esp+278]      |
6F657DC8  | 52                      | push edx                            |
6F657DC9  | 8D8424 70010000         | lea eax,dword ptr ss:[esp+170]      |
6F657DD0  | 50                      | push eax                            |
6F657DD1  | E8 EE370900             | call <JMP.&Ordinal#501>             |
6F657DD6  | 8B7C24 1C               | mov edi,dword ptr ss:[esp+1C]       |
6F657DDA  | 8DAF C2020000           | lea ebp,dword ptr ds:[edi+2C2]      |
6F657DE0  | 8BCD                    | mov ecx,ebp                         |
6F657DE2  | 898424 70020000         | mov dword ptr ss:[esp+270],eax      |
6F657DE9  | 33F6                    | xor esi,esi                         |
6F657DEB  | E8 B0050800             | call game.6F6D83A0                  |
6F657DF0  | 8D87 DE030000           | lea eax,dword ptr ds:[edi+3DE]      |
6F657DF6  | 3BC3                    | cmp eax,ebx                         |
6F657DF8  | 894424 10               | mov dword ptr ss:[esp+10],eax       |
6F657DFC  | 74 41                   | je game.6F657E3F                    |
6F657DFE  | 8BFF                    | mov edi,edi                         |
6F657E00  | 3818                    | cmp byte ptr ds:[eax],bl            |
6F657E02  | 74 3B                   | je game.6F657E3F                    |
6F657E04  | 3BF3                    | cmp esi,ebx                         |
6F657E06  | 75 37                   | jne game.6F657E3F                   |
6F657E08  | 53                      | push ebx                            |
6F657E09  | 68 8810976F             | push game.6F971088                  |
6F657E0E  | 68 04010000             | push 104                            |
6F657E13  | 8D8C24 84030000         | lea ecx,dword ptr ss:[esp+384]      |
6F657E1A  | 51                      | push ecx                            |
6F657E1B  | 8D5424 20               | lea edx,dword ptr ss:[esp+20]       |
6F657E1F  | 52                      | push edx                            |
6F657E20  | E8 E1370900             | call <JMP.&Ordinal#504>             |
6F657E25  | 8D9424 78030000         | lea edx,dword ptr ss:[esp+378]      |
6F657E2C  | 8D4C24 34               | lea ecx,dword ptr ss:[esp+34]       |
6F657E30  | E8 3BFDFFFF             | call game.6F657B70                  |
6F657E35  | 8BF0                    | mov esi,eax                         |
6F657E37  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F657E3B  | 3BC3                    | cmp eax,ebx                         |
6F657E3D  | 75 C1                   | jne game.6F657E00                   |
6F657E3F  | 8BCD                    | mov ecx,ebp                         |
6F657E41  | E8 6A050800             | call game.6F6D83B0                  |
6F657E46  | 3BF3                    | cmp esi,ebx                         |
6F657E48  | 74 14                   | je game.6F657E5E                    |
6F657E4A  | 8B4424 54               | mov eax,dword ptr ss:[esp+54]       |
6F657E4E  | 8B4C24 14               | mov ecx,dword ptr ss:[esp+14]       |
6F657E52  | 8B5424 58               | mov edx,dword ptr ss:[esp+58]       |
6F657E56  | 8901                    | mov dword ptr ds:[ecx],eax          |
6F657E58  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F657E5C  | 8910                    | mov dword ptr ds:[eax],edx          |
6F657E5E  | 8BC6                    | mov eax,esi                         |
6F657E60  | 8B8C24 7C040000         | mov ecx,dword ptr ss:[esp+47C]      |
6F657E67  | 5F                      | pop edi                             |
6F657E68  | 5E                      | pop esi                             |
6F657E69  | 5D                      | pop ebp                             |
6F657E6A  | 5B                      | pop ebx                             |
6F657E6B  | 33CC                    | xor ecx,esp                         |
6F657E6D  | E8 E7911800             | call game.6F7E1059                  |
6F657E72  | 81C4 70040000           | add esp,470                         |
6F657E78  | C2 2C00                 | ret 2C                              |
```

---

## 处理分支
- **地址**：`6F682300`
- **偏移**：`game.dll + 682300`

下面的反汇编是魔兽争霸v1.26.0.6401 处理数据包的分支的逻辑，是上面函数的顶层函数。

详细内容请参考[https://github.com/wuxiancong/War3Bot/blob/main/details/1.26/GameDll_682300_CustomGame_W3GSPacket_Dispatch.md](https://github.com/wuxiancong/War3Bot/blob/main/details/1.26/GameDll_682300_CustomGame_W3GSPacket_Dispatch.md)

<details>
	<summary>头部</summary>
	
```
6F682300  | 56                      | push esi                            |
6F682301  | 0FB67424 0C             | movzx esi,byte ptr ss:[esp+C]       |
6F682306  | 83C6 FF                 | add esi,FFFFFFFF                    |
6F682309  | 83FE 50                 | cmp esi,50                          |
6F68230C  | B8 01000000             | mov eax,1                           |
6F682311  | 0F87 D2030000           | ja game.6F6826E9                    |
6F682317  | 0FB6B6 8827686F         | movzx esi,byte ptr ds:[esi+6F682788 |
6F68231E  | FF24B5 F026686F         | jmp dword ptr ds:[esi*4+6F6826F0]   |
6F682325  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682329  | 50                      | push eax                            |
6F68232A  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68232E  | 50                      | push eax                            |
6F68232F  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682333  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682335  | 50                      | push eax                            |
6F682336  | E8 4587FFFF             | call game.6F67AA80                  |
6F68233B  | 5E                      | pop esi                             |
6F68233C  | C2 1400                 | ret 14                              |
6F68233F  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682343  | 50                      | push eax                            |
6F682344  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682348  | 50                      | push eax                            |
6F682349  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68234D  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F68234F  | 50                      | push eax                            |
6F682350  | E8 DBAAFFFF             | call game.6F67CE30                  |
6F682355  | 5E                      | pop esi                             |
6F682356  | C2 1400                 | ret 14                              |
6F682359  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68235D  | 50                      | push eax                            |
6F68235E  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682362  | 50                      | push eax                            |
6F682363  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682367  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682369  | 50                      | push eax                            |
6F68236A  | E8 3114FFFF             | call game.6F6737A0                  |
6F68236F  | 5E                      | pop esi                             |
6F682370  | C2 1400                 | ret 14                              |
6F682373  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682377  | 50                      | push eax                            |
6F682378  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68237C  | 50                      | push eax                            |
6F68237D  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682381  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682383  | 50                      | push eax                            |
6F682384  | E8 B7ACFFFF             | call game.6F67D040                  |
6F682389  | 5E                      | pop esi                             |
6F68238A  | C2 1400                 | ret 14                              |
6F68238D  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682391  | 50                      | push eax                            |
6F682392  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682396  | 50                      | push eax                            |
6F682397  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68239B  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F68239D  | 50                      | push eax                            |
6F68239E  | E8 8D87FFFF             | call game.6F67AB30                  |
6F6823A3  | 5E                      | pop esi                             |
6F6823A4  | C2 1400                 | ret 14                              |
6F6823A7  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6823AB  | 50                      | push eax                            |
6F6823AC  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6823B0  | 50                      | push eax                            |
6F6823B1  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6823B5  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6823B7  | 50                      | push eax                            |
6F6823B8  | E8 13F4FFFF             | call game.6F6817D0                  |
6F6823BD  | 5E                      | pop esi                             |
6F6823BE  | C2 1400                 | ret 14                              |
6F6823C1  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6823C5  | 50                      | push eax                            |
6F6823C6  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6823CA  | 50                      | push eax                            |
6F6823CB  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6823CF  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6823D1  | 50                      | push eax                            |
6F6823D2  | E8 29E2FFFF             | call game.6F680600                  |
6F6823D7  | 5E                      | pop esi                             |
6F6823D8  | C2 1400                 | ret 14                              |
6F6823DB  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6823DF  | 50                      | push eax                            |
6F6823E0  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6823E4  | 50                      | push eax                            |
6F6823E5  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6823E9  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6823EB  | 50                      | push eax                            |
6F6823EC  | E8 6F88FFFF             | call game.6F67AC60                  |
6F6823F1  | 5E                      | pop esi                             |
6F6823F2  | C2 1400                 | ret 14                              |
6F6823F5  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6823F9  | 50                      | push eax                            |
6F6823FA  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6823FE  | 50                      | push eax                            |
6F6823FF  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682403  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682405  | 50                      | push eax                            |
6F682406  | E8 8589FFFF             | call game.6F67AD90                  |
6F68240B  | 5E                      | pop esi                             |
6F68240C  | C2 1400                 | ret 14                              |
6F68240F  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682413  | 50                      | push eax                            |
6F682414  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682418  | 50                      | push eax                            |
6F682419  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68241D  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F68241F  | 50                      | push eax                            |
6F682420  | E8 4B8AFFFF             | call game.6F67AE70                  |
6F682425  | 5E                      | pop esi                             |
6F682426  | C2 1400                 | ret 14                              |
6F682429  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68242D  | 50                      | push eax                            |
6F68242E  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682432  | 50                      | push eax                            |
6F682433  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682437  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682439  | 50                      | push eax                            |
6F68243A  | E8 E1E1FFFF             | call game.6F680620                  |
6F68243F  | 5E                      | pop esi                             |
6F682440  | C2 1400                 | ret 14                              |
6F682443  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682447  | 50                      | push eax                            |
6F682448  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68244C  | 50                      | push eax                            |
6F68244D  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682451  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682453  | 50                      | push eax                            |
6F682454  | E8 57C4FFFF             | call game.6F67E8B0                  |
6F682459  | 5E                      | pop esi                             |
6F68245A  | C2 1400                 | ret 14                              |
6F68245D  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682461  | 50                      | push eax                            |
6F682462  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682466  | 50                      | push eax                            |
6F682467  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68246B  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F68246D  | 50                      | push eax                            |
6F68246E  | E8 5DC4FFFF             | call game.6F67E8D0                  |
6F682473  | 5E                      | pop esi                             |
6F682474  | C2 1400                 | ret 14                              |
6F682477  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68247B  | 50                      | push eax                            |
6F68247C  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682480  | 50                      | push eax                            |
6F682481  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682485  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682487  | 50                      | push eax                            |
6F682488  | E8 A38AFFFF             | call game.6F67AF30                  |
6F68248D  | 5E                      | pop esi                             |
6F68248E  | C2 1400                 | ret 14                              |
6F682491  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F682495  | 50                      | push eax                            |
6F682496  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F68249A  | 50                      | push eax                            |
6F68249B  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F68249F  | 50                      | push eax                            |
6F6824A0  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6824A4  | 50                      | push eax                            |
6F6824A5  | E8 96EAFFFF             | call game.6F680F40                  |
6F6824AA  | 5E                      | pop esi                             |
6F6824AB  | C2 1400                 | ret 14                              |
6F6824AE  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6824B2  | 50                      | push eax                            |
6F6824B3  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6824B7  | 50                      | push eax                            |
6F6824B8  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6824BC  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6824BE  | 50                      | push eax                            |
6F6824BF  | E8 2C8BFFFF             | call game.6F67AFF0                  |
6F6824C4  | 5E                      | pop esi                             |
6F6824C5  | C2 1400                 | ret 14                              |
6F6824C8  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6824CC  | 50                      | push eax                            |
6F6824CD  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6824D1  | 50                      | push eax                            |
6F6824D2  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6824D6  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6824D8  | 50                      | push eax                            |
6F6824D9  | E8 92ADFFFF             | call game.6F67D270                  |
6F6824DE  | 5E                      | pop esi                             |
6F6824DF  | C2 1400                 | ret 14                              |
6F6824E2  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6824E6  | 50                      | push eax                            |
6F6824E7  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6824EB  | 50                      | push eax                            |
6F6824EC  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6824F0  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6824F2  | 50                      | push eax                            |
6F6824F3  | E8 A848FFFF             | call game.6F676DA0                  |
6F6824F8  | 5E                      | pop esi                             |
6F6824F9  | C2 1400                 | ret 14                              |
6F6824FC  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F682500  | 50                      | push eax                            |
6F682501  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F682505  | 50                      | push eax                            |
6F682506  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F68250A  | 50                      | push eax                            |
6F68250B  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68250F  | 50                      | push eax                            |
6F682510  | E8 2BE2FFFF             | call game.6F680740                  |
6F682515  | 5E                      | pop esi                             |
6F682516  | C2 1400                 | ret 14                              |
6F682519  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68251D  | 50                      | push eax                            |
6F68251E  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682522  | 50                      | push eax                            |
6F682523  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682527  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682529  | 50                      | push eax                            |
6F68252A  | E8 418EFFFF             | call game.6F67B370                  |
6F68252F  | 5E                      | pop esi                             |
6F682530  | C2 1400                 | ret 14                              |
6F682533  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682537  | 50                      | push eax                            |
6F682538  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68253C  | 50                      | push eax                            |
6F68253D  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682541  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682543  | 50                      | push eax                            |
6F682544  | E8 F790FFFF             | call game.6F67B640                  |
6F682549  | 5E                      | pop esi                             |
6F68254A  | C2 1400                 | ret 14                              |
6F68254D  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682551  | 50                      | push eax                            |
6F682552  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682556  | 50                      | push eax                            |
6F682557  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68255B  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F68255D  | 50                      | push eax                            |
6F68255E  | E8 FD90FFFF             | call game.6F67B660                  |
6F682563  | 5E                      | pop esi                             |
6F682564  | C2 1400                 | ret 14                              |
6F682567  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68256B  | 50                      | push eax                            |
6F68256C  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682570  | 50                      | push eax                            |
6F682571  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682575  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682577  | 50                      | push eax                            |
6F682578  | E8 9391FFFF             | call game.6F67B710                  |
6F68257D  | 5E                      | pop esi                             |
6F68257E  | C2 1400                 | ret 14                              |
6F682581  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682585  | 50                      | push eax                            |
6F682586  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68258A  | 50                      | push eax                            |
6F68258B  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68258F  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682591  | 50                      | push eax                            |
6F682592  | E8 D9E2FFFF             | call game.6F680870                  |
6F682597  | 5E                      | pop esi                             |
6F682598  | C2 1400                 | ret 14                              |
6F68259B  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68259F  | 50                      | push eax                            |
6F6825A0  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6825A4  | 50                      | push eax                            |
6F6825A5  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6825A9  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6825AB  | 50                      | push eax                            |
6F6825AC  | E8 4FE3FFFF             | call game.6F680900                  |
6F6825B1  | 5E                      | pop esi                             |
6F6825B2  | C2 1400                 | ret 14                              |
6F6825B5  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6825B9  | 50                      | push eax                            |
6F6825BA  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6825BE  | 50                      | push eax                            |
6F6825BF  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6825C3  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6825C5  | 50                      | push eax                            |
6F6825C6  | E8 E511FFFF             | call game.6F6737B0                  |
6F6825CB  | 5E                      | pop esi                             |
6F6825CC  | C2 1400                 | ret 14                              |
6F6825CF  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6825D3  | 50                      | push eax                            |
6F6825D4  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6825D8  | 50                      | push eax                            |
6F6825D9  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6825DD  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6825DF  | 50                      | push eax                            |
6F6825E0  | E8 4B92FFFF             | call game.6F67B830                  |
6F6825E5  | 5E                      | pop esi                             |
6F6825E6  | C2 1400                 | ret 14                              |
```
</details>

esi=1A，跳转到这里处理服务器发过来的 '0x3D' 地图数据包

```assembly
6F6825E9  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6825ED  | 50                      | push eax                            |
6F6825EE  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6825F2  | 50                      | push eax                            |
6F6825F3  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6825F7  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6825F9  | 50                      | push eax                            |
6F6825FA  | E8 11C6FFFF             | call game.6F67EC10                  |
6F6825FF  | 5E                      | pop esi                             |
6F682600  | C2 1400                 | ret 14                              |
```

<details>
	<summary>尾部</summary>
	
```assembly
6F682603  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682607  | 50                      | push eax                            |
6F682608  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68260C  | 50                      | push eax                            |
6F68260D  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682611  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682613  | 50                      | push eax                            |
6F682614  | E8 F7C7FFFF             | call game.6F67EE10                  |
6F682619  | 5E                      | pop esi                             |
6F68261A  | C2 1400                 | ret 14                              |
6F68261D  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682621  | 50                      | push eax                            |
6F682622  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682626  | 50                      | push eax                            |
6F682627  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68262B  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F68262D  | 50                      | push eax                            |
6F68262E  | E8 5DC9FFFF             | call game.6F67EF90                  |
6F682633  | 5E                      | pop esi                             |
6F682634  | C2 1400                 | ret 14                              |
6F682637  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68263B  | 50                      | push eax                            |
6F68263C  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682640  | 50                      | push eax                            |
6F682641  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682645  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682647  | 50                      | push eax                            |
6F682648  | E8 93CAFFFF             | call game.6F67F0E0                  |
6F68264D  | 5E                      | pop esi                             |
6F68264E  | C2 1400                 | ret 14                              |
6F682651  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682655  | 50                      | push eax                            |
6F682656  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68265A  | 50                      | push eax                            |
6F68265B  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68265F  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682661  | 50                      | push eax                            |
6F682662  | E8 5992FFFF             | call game.6F67B8C0                  |
6F682667  | 5E                      | pop esi                             |
6F682668  | C2 1400                 | ret 14                              |
6F68266B  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68266F  | 50                      | push eax                            |
6F682670  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682674  | 50                      | push eax                            |
6F682675  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682679  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F68267B  | 50                      | push eax                            |
6F68267C  | E8 DF94FFFF             | call game.6F67BB60                  |
6F682681  | 5E                      | pop esi                             |
6F682682  | C2 1400                 | ret 14                              |
6F682685  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682689  | 50                      | push eax                            |
6F68268A  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68268E  | 50                      | push eax                            |
6F68268F  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682693  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682695  | 50                      | push eax                            |
6F682696  | E8 E594FFFF             | call game.6F67BB80                  |
6F68269B  | 5E                      | pop esi                             |
6F68269C  | C2 1400                 | ret 14                              |
6F68269F  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6826A3  | 50                      | push eax                            |
6F6826A4  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6826A8  | 50                      | push eax                            |
6F6826A9  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6826AD  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6826AF  | 50                      | push eax                            |
6F6826B0  | E8 7BD2FFFF             | call game.6F67F930                  |
6F6826B5  | 5E                      | pop esi                             |
6F6826B6  | C2 1400                 | ret 14                              |
6F6826B9  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6826BD  | 50                      | push eax                            |
6F6826BE  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6826C2  | 50                      | push eax                            |
6F6826C3  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6826C7  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6826C9  | 50                      | push eax                            |
6F6826CA  | E8 91D5FFFF             | call game.6F67FC60                  |
6F6826CF  | 5E                      | pop esi                             |
6F6826D0  | C2 1400                 | ret 14                              |
6F6826D3  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6826D7  | 50                      | push eax                            |
6F6826D8  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6826DC  | 50                      | push eax                            |
6F6826DD  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6826E1  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6826E3  | 50                      | push eax                            |
6F6826E4  | E8 47D6FFFF             | call game.6F67FD30                  |
6F6826E9  | 5E                      | pop esi                             |
6F6826EA  | C2 1400                 | ret 14                              |
6F6826ED  | 8D49 00                 | lea ecx,dword ptr ds:[ecx]          |
6F6826F0  | 25 23686F3F             | and eax,3F6F6823                    |
6F6826F5  | 2368 6F                 | and ebp,dword ptr ds:[eax+6F]       |
6F6826F8  | 59                      | pop ecx                             |
6F6826F9  | 2368 6F                 | and ebp,dword ptr ds:[eax+6F]       |
6F6826FC  | 73 23                   | jae game.6F682721                   |
6F6826FE  | 68 6F8D2368             | push 68238D6F                       |
6F682703  | 6F                      | outsd                               |
6F682704  | A7                      | cmpsd                               |
6F682705  | 2368 6F                 | and ebp,dword ptr ds:[eax+6F]       |
6F682708  | C123 68                 | shl dword ptr ds:[ebx],68           |
6F68270B  | 6F                      | outsd                               |
6F68270C  | DB                      | ???                                 |
6F68270D  | 2368 6F                 | and ebp,dword ptr ds:[eax+6F]       |
6F682710  | F5                      | cmc                                 |
6F682711  | 2368 6F                 | and ebp,dword ptr ds:[eax+6F]       |
6F682714  | 0F                      | ???                                 |
6F682715  | 24 68                   | and al,68                           |
6F682717  | 6F                      | outsd                               |
6F682718  | 292468                  | sub dword ptr ds:[eax+ebp*2],esp    |
6F68271B  | 6F                      | outsd                               |
6F68271C  | 5D                      | pop ebp                             |
6F68271D  | 24 68                   | and al,68                           |
6F68271F  | 6F                      | outsd                               |
6F682720  | 77 24                   | ja game.6F682746                    |
6F682722  | 68 6F912468             | push 6824916F                       |
6F682727  | 6F                      | outsd                               |
6F682728  | 3325 686FAE24           | xor esp,dword ptr ds:[24AE6F68]     |
6F68272E  | 68 6FC82468             | push 6824C86F                       |
6F682733  | 6F                      | outsd                               |
6F682734  | E2 24                   | loop game.6F68275A                  |
6F682736  | 68 6FFC2468             | push 6824FC6F                       |
6F68273B  | 6F                      | outsd                               |
6F68273C  | 1925 686F4D25           | sbb dword ptr ds:[254D6F68],esp     |
6F682742  | 68 6F672568             | push 6825676F                       |
6F682747  | 6F                      | outsd                               |
6F682748  | 8125 686F9B25 686FB525  | and dword ptr ds:[259B6F68],25B56F6 |
6F682752  | 68 6FCF2568             | push 6825CF6F                       |
6F682757  | 6F                      | outsd                               |
6F682758  | E9 25686F03             | jmp 72D78F82                        |
6F68275D  | 26:68 6F1D2668          | push 68261D6F                       |
6F682763  | 6F                      | outsd                               |
6F682764  | 37                      | aaa                                 |
6F682765  | 26:68 6F9F2668          | push 68269F6F                       |
6F68276B  | 6F                      | outsd                               |
6F68276C  | B9 26686FD3             | mov ecx,D36F6826                    |
6F682771  | 26:68 6F512668          | push 6826516F                       |
6F682777  | 6F                      | outsd                               |
6F682778  | 43                      | inc ebx                             |
6F682779  | 24 68                   | and al,68                           |
6F68277B  | 6F                      | outsd                               |
6F68277C  | 6B26 68                 | imul esp,dword ptr ds:[esi],68      |
6F68277F  | 6F                      | outsd                               |
6F682780  | 8526                    | test dword ptr ds:[esi],esp         |
6F682782  | 68 6FE92668             | push 6826E96F                       |
6F682787  | 6F                      | outsd                               |
6F682788  | 0001                    | add byte ptr ds:[ecx],al            |
6F68278A  | 0203                    | add al,byte ptr ds:[ebx]            |
6F68278C  | 04 05                   | add al,5                            |
6F68278E  | 06                      | push es                             |
6F68278F  | 07                      | pop es                              |
6F682790  | 0809                    | or byte ptr ds:[ecx],cl             |
6F682792  | 0A0B                    | or cl,byte ptr ds:[ebx]             |
6F682794  | 0C 0D                   | or al,D                             |
6F682796  | 0E                      | push cs                             |
6F682797  | 0F1025 11122513         | movups xmm4,xmmword ptr ds:[1325121 |
6F68279E  | 14 15                   | adc al,15                           |
6F6827A0  | 16                      | push ss                             |
6F6827A1  | 17                      | pop ss                              |
6F6827A2  | 1819                    | sbb byte ptr ds:[ecx],bl            |
6F6827A4  | 25 25252525             | and eax,25252525                    |
6F6827A9  | 25 25252525             | and eax,25252525                    |
6F6827AE  | 25 25252525             | and eax,25252525                    |
6F6827B3  | 25 25252525             | and eax,25252525                    |
6F6827B8  | 25 25252525             | and eax,25252525                    |
6F6827BD  | 25 25252525             | and eax,25252525                    |
6F6827C2  | 25 251A1B1C             | and eax,1C1B1A25                    |
6F6827C7  | 1D 25251E1F             | sbb eax,1F1E2525                    |
6F6827CC  | 2025 21222525           | and byte ptr ds:[25252221],ah       |
6F6827D2  | 25 25252525             | and eax,25252525                    |
6F6827D7  | 2324CC                  | and esp,dword ptr ss:[esp+ecx*8]    |
```
</details>


## 处理数据
- **地址**：`6F67EC10`
- **偏移**：`game.dll + 67EC10`


```assembly
6F67EC10  | 81EC 3C020000           | sub esp,23C                         |
6F67EC16  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]     |
6F67EC1B  | 33C4                    | xor eax,esp                         |
6F67EC1D  | 898424 38020000         | mov dword ptr ss:[esp+238],eax      |
6F67EC24  | 8B8C24 44020000         | mov ecx,dword ptr ss:[esp+244]      | <--- 地图路径   指针（Maps\Download\DotA v6.83d.w3x）
6F67EC2B  | 8B9424 48020000         | mov edx,dword ptr ss:[esp+248]      |
6F67EC32  | 57                      | push edi                            | <--- 旧的 网络客户端 指针（NetClient）
6F67EC33  | 8BBC24 44020000         | mov edi,dword ptr ss:[esp+244]      | <--- 新的 网络客户端 指针（NetClient）
6F67EC3A  | 8D4424 10               | lea eax,dword ptr ss:[esp+10]       | <--- 指向 DataRecycler 的指针
6F67EC3E  | 50                      | push eax                            |
6F67EC3F  | C64424 18 00            | mov byte ptr ss:[esp+18],0          |
6F67EC44  | E8 A771FFFF             | call game.6F675DF0                  |
6F67EC49  | 85C0                    | test eax,eax                        |
6F67EC4B  | 0F84 99010000           | je game.6F67EDEA                    |
6F67EC51  | 8B8424 18010000         | mov eax,dword ptr ss:[esp+118]      |
6F67EC58  | 85C0                    | test eax,eax                        |
6F67EC5A  | 0F84 8A010000           | je game.6F67EDEA                    |
6F67EC60  | 3D 00008000             | cmp eax,800000                      |
6F67EC65  | 0F87 7F010000           | ja game.6F67EDEA                    |
6F67EC6B  | 8B57 40                 | mov edx,dword ptr ds:[edi+40]       |
6F67EC6E  | 56                      | push esi                            |
6F67EC6F  | 6A 01                   | push 1                              |
6F67EC71  | 33F6                    | xor esi,esi                         |
6F67EC73  | 56                      | push esi                            |
6F67EC74  | 8D4C24 10               | lea ecx,dword ptr ss:[esp+10]       |
6F67EC78  | 51                      | push ecx                            |
6F67EC79  | 6A 01                   | push 1                              |
6F67EC7B  | 52                      | push edx                            |
6F67EC7C  | B9 70FFAC6F             | mov ecx,game.6FACFF70               |
6F67EC81  | E8 FAAEFFFF             | call game.6F679B80                  |
6F67EC86  | 8BF8                    | mov edi,eax                         |
6F67EC88  | 85FF                    | test edi,edi                        |
6F67EC8A  | 0F84 3F010000           | je game.6F67EDCF                    |
6F67EC90  | 55                      | push ebp                            |
6F67EC91  | 8B6F 58                 | mov ebp,dword ptr ds:[edi+58]       |
6F67EC94  | 8D4424 1C               | lea eax,dword ptr ss:[esp+1C]       |
6F67EC98  | 50                      | push eax                            |
6F67EC99  | 8BCD                    | mov ecx,ebp                         |
6F67EC9B  | E8 D080FDFF             | call game.6F656D70                  |
6F67ECA0  | 85C0                    | test eax,eax                        |
6F67ECA2  | 0F84 0A010000           | je game.6F67EDB2                    |
6F67ECA8  | 68 04010000             | push 104                            |
6F67ECAD  | 8D4C24 20               | lea ecx,dword ptr ss:[esp+20]       |
6F67ECB1  | 51                      | push ecx                            |
6F67ECB2  | 8D9424 48010000         | lea edx,dword ptr ss:[esp+148]      |
6F67ECB9  | 52                      | push edx                            |
6F67ECBA  | E8 05C90600             | call <JMP.&Ordinal#501>             |
6F67ECBF  | 8D8C24 40010000         | lea ecx,dword ptr ss:[esp+140]      |
6F67ECC6  | E8 456E0400             | call game.6F6C5B10                  |
6F67ECCB  | 8B5424 18               | mov edx,dword ptr ss:[esp+18]       |
6F67ECCF  | 8BCF                    | mov ecx,edi                         |
6F67ECD1  | E8 AAFDFFFF             | call game.6F67EA80                  |
6F67ECD6  | 85C0                    | test eax,eax                        |
6F67ECD8  | 0F85 D4000000           | jne game.6F67EDB2                   |
6F67ECDE  | 50                      | push eax                            |
6F67ECDF  | 50                      | push eax                            |
6F67ECE0  | 6A 02                   | push 2                              |
6F67ECE2  | 8D8F 80020000           | lea ecx,dword ptr ds:[edi+280]      |
6F67ECE8  | E8 13DAFFFF             | call game.6F67C700                  |
6F67ECED  | 8BF0                    | mov esi,eax                         |
6F67ECEF  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F67ECF3  | 8D8C24 40010000         | lea ecx,dword ptr ss:[esp+140]      |
6F67ECFA  | 51                      | push ecx                            |
6F67ECFB  | 8D4E 0C                 | lea ecx,dword ptr ds:[esi+C]        |
6F67ECFE  | 8946 08                 | mov dword ptr ds:[esi+8],eax        |
6F67ED01  | E8 DAE8DBFF             | call game.6F43D5E0                  |
6F67ED06  | 8B9424 20010000         | mov edx,dword ptr ss:[esp+120]      |
6F67ED0D  | 8996 14020000           | mov dword ptr ds:[esi+214],edx      |
6F67ED13  | 8B8424 20010000         | mov eax,dword ptr ss:[esp+120]      |
6F67ED1A  | 8986 18020000           | mov dword ptr ds:[esi+218],eax      |
6F67ED20  | 8B8C24 24010000         | mov ecx,dword ptr ss:[esp+124]      |
6F67ED27  | 898E 1C020000           | mov dword ptr ds:[esi+21C],ecx      |
6F67ED2D  | 8B9424 28010000         | mov edx,dword ptr ss:[esp+128]      |
6F67ED34  | 8996 20020000           | mov dword ptr ds:[esi+220],edx      |
6F67ED3A  | 8B8424 2C010000         | mov eax,dword ptr ss:[esp+12C]      |
6F67ED41  | 8986 24020000           | mov dword ptr ds:[esi+224],eax      |
6F67ED47  | 8B8C24 30010000         | mov ecx,dword ptr ss:[esp+130]      |
6F67ED4E  | 898E 28020000           | mov dword ptr ds:[esi+228],ecx      |
6F67ED54  | 8B9424 34010000         | mov edx,dword ptr ss:[esp+134]      |
6F67ED5B  | 8996 2C020000           | mov dword ptr ds:[esi+22C],edx      |
6F67ED61  | 8B8424 38010000         | mov eax,dword ptr ss:[esp+138]      |
6F67ED68  | 8986 30020000           | mov dword ptr ds:[esi+230],eax      |
6F67ED6E  | 8B8C24 3C010000         | mov ecx,dword ptr ss:[esp+13C]      |
6F67ED75  | 898E 34020000           | mov dword ptr ds:[esi+234],ecx      |
6F67ED7B  | C786 40020000 00000000  | mov dword ptr ds:[esi+240],0        |
6F67ED85  | 8B57 0C                 | mov edx,dword ptr ds:[edi+C]        |
6F67ED88  | 6A 08                   | push 8                              |
6F67ED8A  | 8D4C24 14               | lea ecx,dword ptr ss:[esp+14]       |
6F67ED8E  | 895424 14               | mov dword ptr ss:[esp+14],edx       |
6F67ED92  | 8B46 08                 | mov eax,dword ptr ds:[esi+8]        |
6F67ED95  | 51                      | push ecx                            |
6F67ED96  | 8BCD                    | mov ecx,ebp                         |
6F67ED98  | 894424 1C               | mov dword ptr ss:[esp+1C],eax       |
6F67ED9C  | E8 2F82FDFF             | call game.6F656FD0                  |
6F67EDA1  | BA 12000000             | mov edx,12                          |
6F67EDA6  | 8BC8                    | mov ecx,eax                         |
6F67EDA8  | E8 A3C4FAFF             | call game.6F62B250                  |
6F67EDAD  | BE 02000000             | mov esi,2                           |
6F67EDB2  | 8B4424 0C               | mov eax,dword ptr ss:[esp+C]        |
6F67EDB6  | 83F8 FF                 | cmp eax,FFFFFFFF                    |
6F67EDB9  | 5D                      | pop ebp                             |
6F67EDBA  | 74 13                   | je game.6F67EDCF                    |
6F67EDBC  | 83F8 01                 | cmp eax,1                           |
6F67EDBF  | 1BD2                    | sbb edx,edx                         |
6F67EDC1  | 83C2 01                 | add edx,1                           |
6F67EDC4  | 52                      | push edx                            |
6F67EDC5  | B9 90FFAC6F             | mov ecx,game.6FACFF90               |
6F67EDCA  | E8 819B0500             | call game.6F6D8950                  |
6F67EDCF  | 8BC6                    | mov eax,esi                         |
6F67EDD1  | 5E                      | pop esi                             |
6F67EDD2  | 5F                      | pop edi                             |
6F67EDD3  | 8B8C24 38020000         | mov ecx,dword ptr ss:[esp+238]      |
6F67EDDA  | 33CC                    | xor ecx,esp                         |
6F67EDDC  | E8 78221600             | call game.6F7E1059                  |
6F67EDE1  | 81C4 3C020000           | add esp,23C                         |
6F67EDE7  | C2 0C00                 | ret C                               |
6F67EDEA  | 8B8C24 3C020000         | mov ecx,dword ptr ss:[esp+23C]      |
6F67EDF1  | 5F                      | pop edi                             |
6F67EDF2  | 33CC                    | xor ecx,esp                         |
6F67EDF4  | B8 01000000             | mov eax,1                           |
6F67EDF9  | E8 5B221600             | call game.6F7E1059                  |
6F67EDFE  | 81C4 3C020000           | add esp,23C                         |
6F67EE04  | C2 0C00                 | ret C                               |
```

这段反汇编代码属于 **Warcraft III (Game.dll)** 的网络处理模块，具体逻辑是 **解析来自主机发送的地图信息数据包 (Packet 0x3D)**，并根据解析结果验证本地是否拥有该地图，或者是否需要下载。

根据我提供的反汇编和堆栈数据，以下是详细的逻辑分析：

### 1. 核心功能概述
这段代码的主要功能是：
1.  **接收数据包**：获取地图的文件路径、大小、CRC校验码、SHA1哈希值。
2.  **安全性检查**：检查地图大小是否超过限制（此处硬编码检查是否超过 8MB，`0x800000`）。
3.  **对象构造**：创建一个 `FileInfo`（或类似名称）的网络对象，将解析出的地图元数据填入其中。
4.  **本地验证**：调用文件系统接口，检查本地 `Maps\Download` 目录下是否存在该地图，且校验码是否匹配。

### 2. 数据包结构还原 (W3GS 0x3D Payload)
根据代码中 `mov` 指令从栈中取值（`esp+xxx`）并存入对象（`esi+xxx`）的行为，可以还原数据包的载荷结构：

*   **Map File Path (字符串)**: `"Maps\\Download\\DotA v6.83d.w3x"`
*   **Unknown (DWORD)**: `1` (可能代表地图类型或状态)
*   **Map Size (DWORD)**: `0x007E9836` (8,296,502 字节)
*   **Map Info (DWORD)**: `0x91FA5A45` (通常包含玩家数量、地图标志等)
*   **Map CRC32 (DWORD)**: `0x3AEBCEF3`
*   **Map SHA1 (20 Bytes)**: `0x0AA52F2E...` (由5个DWORD组成)

### 3. 详细代码逻辑分析

#### 第一阶段：初始化与安全检查
```assembly
6F67EC24  | mov ecx,dword ptr ss:[esp+244]  ; 获取地图路径指针
...
6F67EC51  | mov eax,dword ptr ss:[esp+118]  ; 从栈中获取地图大小 [007E9836]
6F67EC60  | cmp eax,800000                  ; 比较大小是否 > 8,388,608 (8MB)
6F67EC65  | ja game.6F67EDEA                ; 如果大于8MB，跳转到错误处理/拒绝
```
*   **分析**：这里有一个硬编码的限制。如果是旧版本的魔兽争霸，地图传输有 4MB 或 8MB 的限制。`0x7E9836` (约 7.9MB) 小于 `0x800000`，所以检查通过。

#### 第二阶段：分配网络对象
```assembly
6F67EC6B  | mov edx,dword ptr ds:[edi+40]   ; edx = 4 (类型ID?)
...
6F67EC7C  | mov ecx,game.6FACFF70           ; 数据回收器/内存池对象
6F67EC81  | call game.6F679B80              ; 申请一个新的 Packet/Event 对象
6F67EC86  | mov edi,eax                     ; edi 指向新对象
```
*   **分析**：申请内存准备存放解析后的数据。

#### 第三阶段：处理地图路径字符串
```assembly
6F67EC90  | push ebp
6F67EC91  | mov ebp,dword ptr ds:[edi+58]   ; 获取 NetProvider 指针
6F67EC94  | lea eax,dword ptr ss:[esp+1C]   ; 指向栈上的字符串 "Maps\\Download\\DotA v6.83d.w3x"
6F67EC9B  | call game.6F656D70              ; 规范化路径或安全检查
```
*   **分析**：确保地图路径合法，防止目录遍历攻击（如 `..\..\windows`）。

#### 第四阶段：填充 FileInfo 对象 (核心解析逻辑)
代码在 `6F67ECE8` 处调用 `game.6F67C700` 创建了一个对象（存在 `esi` 中），随后开始大量填充数据。这是**逆向协议结构的关键点**：

```assembly
; [esi] 是 FileInfo@NetClient 对象

; 1. 填充路径 (略过部分字符串拷贝代码)
...
; 2. 填充地图大小 (Size)
6F67ED06  | mov edx,dword ptr ss:[esp+120]  ; 取栈数据: 007E9836
6F67ED0D  | mov dword ptr ds:[esi+214],edx  ; 存入对象偏移 0x214
6F67ED13  | mov eax,dword ptr ss:[esp+120]  ; 再次取大小
6F67ED1A  | mov dword ptr ds:[esi+218],eax  ; 存入对象偏移 0x218 (可能分别表示压缩前/压缩后大小，或者当前下载进度/总大小)

; 3. 填充地图属性 (Map Info/Flags)
6F67ED20  | mov ecx,dword ptr ss:[esp+124]  ; 取栈数据: 91FA5A45
6F67ED27  | mov dword ptr ds:[esi+21C],ecx  ; 存入对象偏移 0x21C

; 4. 填充 CRC32
6F67ED2D  | mov edx,dword ptr ss:[esp+128]  ; 取栈数据: 3AEBCEF3
6F67ED34  | mov dword ptr ds:[esi+220],edx  ; 存入对象偏移 0x220

; 5. 填充 SHA1 (20字节 = 5 * 4字节)
; SHA1 Part 1
6F67ED3A  | mov eax,dword ptr ss:[esp+12C]  ; 0AA52F2E
6F67ED41  | mov dword ptr ds:[esi+224],eax
; SHA1 Part 2
6F67ED47  | mov ecx,dword ptr ss:[esp+130]  ; E3FF9B37
6F67ED4E  | mov dword ptr ds:[esi+228],ecx
; SHA1 Part 3
6F67ED54  | mov edx,dword ptr ss:[esp+134]  ; E74C5752
6F67ED5B  | mov dword ptr ds:[esi+22C],edx
; SHA1 Part 4
6F67ED61  | mov eax,dword ptr ss:[esp+138]  ; A7B9F545
6F67ED68  | mov dword ptr ds:[esi+230],eax
; SHA1 Part 5
6F67ED6E  | mov ecx,dword ptr ss:[esp+13C]  ; 5FA4DF1A
6F67ED75  | mov dword ptr ds:[esi+234],ecx
```

#### 第五阶段：本地文件校验与回调

```assembly
6F656FD0  | 8B41 14                 | mov eax,dword ptr ds:[ecx+14]                   | ecx+14:GameMain+AC5258
6F656FD3  | C3                      | ret                                             |
```

```assembly
6F62B250  | 83EC 08                 | sub esp,8                                       |
6F62B253  | 53                      | push ebx                                        |
6F62B254  | 8BC1                    | mov eax,ecx                                     |
6F62B256  | 33DB                    | xor ebx,ebx                                     |
6F62B258  | 85C0                    | test eax,eax                                    |
6F62B25A  | 56                      | push esi                                        |
6F62B25B  | 895424 08               | mov dword ptr ss:[esp+8],edx                    |
6F62B25F  | 75 05                   | jne game.6F62B266                               |
6F62B261  | E8 6A82E9FF             | call game.6F4C34D0                              |
6F62B266  | 6A 01                   | push 1                                          |
6F62B268  | 6A 00                   | push 0                                          |
6F62B26A  | 8D4C24 14               | lea ecx,dword ptr ss:[esp+14]                   |
6F62B26E  | 51                      | push ecx                                        |
6F62B26F  | 6A 00                   | push 0                                          |
6F62B271  | 50                      | push eax                                        |
6F62B272  | B9 68EAAC6F             | mov ecx,game.6FACEA68                           |
6F62B277  | E8 94FCFFFF             | call game.6F62AF10                              |
6F62B27C  | 8BF0                    | mov esi,eax                                     |
6F62B27E  | 85F6                    | test esi,esi                                    |
6F62B280  | 74 62                   | je game.6F62B2E4                                |
6F62B282  | 55                      | push ebp                                        |
6F62B283  | 57                      | push edi                                        |
6F62B284  | 8D7E 10                 | lea edi,dword ptr ds:[esi+10]                   |
6F62B287  | 8BCF                    | mov ecx,edi                                     |
6F62B289  | E8 12D10A00             | call game.6F6D83A0                              |
6F62B28E  | 33D2                    | xor edx,edx                                     |
6F62B290  | 837E 2C 02              | cmp dword ptr ds:[esi+2C],2                     |
6F62B294  | 8BCF                    | mov ecx,edi                                     |
6F62B296  | 0F94C2                  | sete dl                                         |
6F62B299  | 8BEA                    | mov ebp,edx                                     |
6F62B29B  | E8 10D10A00             | call game.6F6D83B0                              |
6F62B2A0  | 5F                      | pop edi                                         |
6F62B2A1  | 85ED                    | test ebp,ebp                                    |
6F62B2A3  | 5D                      | pop ebp                                         |
6F62B2A4  | 75 1A                   | jne game.6F62B2C0                               |
6F62B2A6  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]                   |
6F62B2AA  | 8B4C24 14               | mov ecx,dword ptr ss:[esp+14]                   |
6F62B2AE  | 8B5424 08               | mov edx,dword ptr ss:[esp+8]                    |
6F62B2B2  | 50                      | push eax                                        |
6F62B2B3  | 51                      | push ecx                                        |
6F62B2B4  | 8BCE                    | mov ecx,esi                                     |
6F62B2B6  | E8 156D0000             | call game.6F631FD0                              |
6F62B2BB  | BB 01000000             | mov ebx,1                                       |
6F62B2C0  | 8B4424 0C               | mov eax,dword ptr ss:[esp+C]                    |
6F62B2C4  | 83F8 FF                 | cmp eax,FFFFFFFF                                |
6F62B2C7  | 74 1B                   | je game.6F62B2E4                                |
6F62B2C9  | 83F8 08                 | cmp eax,8                                       |
6F62B2CC  | 1BD2                    | sbb edx,edx                                     |
6F62B2CE  | 83E0 07                 | and eax,7                                       |
6F62B2D1  | 83C2 01                 | add edx,1                                       |
6F62B2D4  | 8D0C40                  | lea ecx,dword ptr ds:[eax+eax*2]                |
6F62B2D7  | 52                      | push edx                                        |
6F62B2D8  | 8D0C8D 88EAAC6F         | lea ecx,dword ptr ds:[ecx*4+6FACEA88]           |
6F62B2DF  | E8 6CD60A00             | call game.6F6D8950                              |
6F62B2E4  | 5E                      | pop esi                                         |
6F62B2E5  | 8BC3                    | mov eax,ebx                                     |
6F62B2E7  | 5B                      | pop ebx                                         |
6F62B2E8  | 83C4 08                 | add esp,8                                       |
6F62B2EB  | C2 0800                 | ret 8                                           |
```


#### `6F656FD0` (Getter / 状态获取器)
```assembly
6F656FD0  | 8B41 14   | mov eax,dword ptr ds:[ecx+14]  |
6F656FD3  | C3        | ret                            |
```
*   **功能**：这确实只是一个极其简单的 **Getter** 函数。
*   **上下文**：这里的 `ecx` 是 `NetProvider` 对象。偏移 `0x14` 存储的是 **"地图是否匹配/是否存在"** 的状态位（Boolean）。
*   **作用**：它只是把之前负责计算 SHA1 和 CRC 比较的结果（存放在对象里的）取出来，放到 `eax` 里返回。`1` 代表有地图且匹配，`0` 代表没有或不匹配。

---

#### `6F62B250` (Event Dispatcher / 状态机处理)
**关键逻辑控制函数**，它负责根据地图检查的结果（EAX=1 或 0），触发后续的游戏逻辑（是发送 "我有地图" 包，还是触发 "开始下载"）。

我们来逐段拆解这个函数：

**输入参数**:
*   `ECX`: **MapStatus** (来自上面的 Getter，1=Found, 0=Missing)。
*   `EDX`: **EventID** (之前代码传入的是 `12`，代表 `EVENT_MAP_CHECK_RESULT`)。

```assembly
; 1. 检查状态
6F62B254  | mov eax,ecx                     ; EAX = MapStatus (0 or 1)
6F62B25B  | mov dword ptr ss:[esp+8],edx    ; 保存 EventID (12) 到栈上
6F62B25F  | jne game.6F62B266               ; 如果 MapStatus == 1 (有地图)，跳转处理
6F62B261  | call game.6F4C34D0              ; 如果 MapStatus == 0 (无地图)，调用这个函数 (可能是重置状态或记录日志)

; 2. 查找事件处理器 (Event Handler)
6F62B266  | ...                             ; 压入参数
6F62B272  | mov ecx,game.6FACEA68           ; 获取全局网络控制器单例
6F62B277  | call game.6F62AF10              ; 根据 EventID 查找对应的处理器对象
6F62B27C  | mov esi,eax                     ; ESI = 处理器对象 (Handler)
6F62B27E  | test esi,esi                    ; 如果没找到处理器，直接退出
6F62B280  | je game.6F62B2E4

; 3. 线程安全锁 (Mutex Lock)
6F62B284  | lea edi,dword ptr ds:[esi+10]   ; EDI 指向锁对象
6F62B289  | call game.6F6D83A0              ; 进入临界区 (EnterCriticalSection)

; 4. 检查处理器当前状态
6F62B290  | cmp dword ptr ds:[esi+2C],2     ; 检查状态是否为 2 (例如：正在忙/已完成?)
6F62B296  | sete dl                         ; 设置标志位
6F62B29B  | call game.6F6D83B0              ; 离开临界区

; 5. 执行核心逻辑 (Callback)
6F62B2A4  | jne game.6F62B2C0               ; 如果状态不对，跳过执行
6F62B2B2  | push eax                        ; 参数
6F62B2B3  | push ecx                        ; 参数
6F62B2B4  | mov ecx,esi                     ; ECX = 处理器对象
6F62B2B6  | call game.6F631FD0              ; ProcessEvent(EventID, Data)
                                            ; 这里是真正根据“有地图”还是“没地图”去干活的地方！
                                            ; 如果有地图，这里会构建 0x3D 回包说 "OK"
                                            ; 如果没地图，这里会初始化下载流程

; 6. 信号通知 (Signal)
6F62B2C0  | ...                             ; 计算信号量索引
6F62B2DF  | call game.6F6D8950              ; 发送系统信号/事件，通知主线程网络状态已更新
```

### 总结与我的问题关联

1.  **Getter (`6F656FD0`) 返回了 0**：
    因为我的服务端发来的 SHA1 带有魔法字节，客户端计算本地文件 SHA1 后发现不匹配。所以 `NetProvider` 里的状态被置为 `0`。

2.  **Dispatcher (`6F62B250`) 接收到 0**：
    它拿着这个 `0`，调用了内部的 `6F631FD0`。

3.  **结果**：
    `6F631FD0` 发现状态是 0，判定为 **Map Check Failed**。它**不会**发送 "我已拥有地图" 的确认包，而是会尝试进入地图下载阶段（这也是为什么我可能会看到客户端卡住，或者请求下载，或者提示找不到地图）。

4.  **分析**：
    *   如果 `6F656FD0` 返回 1，说明玩家已经有这张图了，游戏将发送 "Map Check Pass" 包给主机。
    *   如果返回 0，将触发地图下载流程。

### 总结
这段代码是 W3GS 协议中 **客户端处理主机下发的地图元数据** 的逻辑。它验证地图大小是否合法（<=8MB），解析地图的 CRC 和 SHA1 签名，并立即检查本地磁盘缓存中是否已有匹配的文件，最后将结果（有地图/无地图）通过内部事件总线分发出去，以决定是进入游戏加载画面还是开始下载地图。

## 线程通知
```
6F631FD0  | 56                      | push esi                                        |
6F631FD1  | 8BF1                    | mov esi,ecx                                     |
6F631FD3  | 8BC6                    | mov eax,esi                                     |
6F631FD5  | F7D8                    | neg eax                                         |
6F631FD7  | 1BC0                    | sbb eax,eax                                     |
6F631FD9  | 57                      | push edi                                        |
6F631FDA  | 8BFA                    | mov edi,edx                                     |
6F631FDC  | 75 0C                   | jne game.6F631FEA                               |
6F631FDE  | 6A 57                   | push 57                                         |
6F631FE0  | E8 C7950B00             | call <JMP.&Ordinal#465>                         |
6F631FE5  | 5F                      | pop edi                                         |
6F631FE6  | 5E                      | pop esi                                         |
6F631FE7  | C2 0800                 | ret 8                                           |
6F631FEA  | 53                      | push ebx                                        |
6F631FEB  | 55                      | push ebp                                        |
6F631FEC  | 8B6C24 18               | mov ebp,dword ptr ss:[esp+18]                   |
6F631FF0  | 8BC5                    | mov eax,ebp                                     |
6F631FF2  | E8 B9FFFFFF             | call game.6F631FB0                              |
6F631FF7  | 8BD8                    | mov ebx,eax                                     |
6F631FF9  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]                   |
6F631FFD  | 85C0                    | test eax,eax                                    |
6F631FFF  | 897B 0C                 | mov dword ptr ds:[ebx+C],edi                    |
6F632002  | 74 0E                   | je game.6F632012                                |
6F632004  | 55                      | push ebp                                        |
6F632005  | 50                      | push eax                                        |
6F632006  | 8D43 10                 | lea eax,dword ptr ds:[ebx+10]                   |
6F632009  | 50                      | push eax                                        |
6F63200A  | E8 DDF51A00             | call <JMP.&memcpy>                              |
6F63200F  | 83C4 0C                 | add esp,C                                       |
6F632012  | 8D6E 10                 | lea ebp,dword ptr ds:[esi+10]                   |
6F632015  | 8BCD                    | mov ecx,ebp                                     |
6F632017  | E8 84630A00             | call game.6F6D83A0                              |
6F63201C  | 8B86 B8010000           | mov eax,dword ptr ds:[esi+1B8]                  |
6F632022  | 8B1418                  | mov edx,dword ptr ds:[eax+ebx]                  |
6F632025  | 03C3                    | add eax,ebx                                     |
6F632027  | 85D2                    | test edx,edx                                    |
6F632029  | 74 29                   | je game.6F632054                                |
6F63202B  | 8B78 04                 | mov edi,dword ptr ds:[eax+4]                    |
6F63202E  | 85FF                    | test edi,edi                                    |
6F632030  | 7F 04                   | jg game.6F632036                                |
6F632032  | F7D7                    | not edi                                         |
6F632034  | EB 07                   | jmp game.6F63203D                               |
6F632036  | 8BC8                    | mov ecx,eax                                     |
6F632038  | 2B4A 04                 | sub ecx,dword ptr ds:[edx+4]                    |
6F63203B  | 03F9                    | add edi,ecx                                     |
6F63203D  | 8917                    | mov dword ptr ds:[edi],edx                      |
6F63203F  | 8B08                    | mov ecx,dword ptr ds:[eax]                      |
6F632041  | 8B50 04                 | mov edx,dword ptr ds:[eax+4]                    |
6F632044  | 8951 04                 | mov dword ptr ds:[ecx+4],edx                    |
6F632047  | C700 00000000           | mov dword ptr ds:[eax],0                        |
6F63204D  | C740 04 00000000        | mov dword ptr ds:[eax+4],0                      |
6F632054  | 8B8E BC010000           | mov ecx,dword ptr ds:[esi+1BC]                  |
6F63205A  | 8908                    | mov dword ptr ds:[eax],ecx                      |
6F63205C  | 8B51 04                 | mov edx,dword ptr ds:[ecx+4]                    |
6F63205F  | 8950 04                 | mov dword ptr ds:[eax+4],edx                    |
6F632062  | 8959 04                 | mov dword ptr ds:[ecx+4],ebx                    |
6F632065  | 8BCD                    | mov ecx,ebp                                     |
6F632067  | 8986 BC010000           | mov dword ptr ds:[esi+1BC],eax                  |
6F63206D  | E8 3E630A00             | call game.6F6D83B0                              |
6F632072  | 5D                      | pop ebp                                         |
6F632073  | 5B                      | pop ebx                                         |
6F632074  | 5F                      | pop edi                                         |
6F632075  | 5E                      | pop esi                                         |
6F632076  | C2 0800                 | ret 8                                           |
```

这是一个非常经典的 **多线程消息队列（Producer-Consumer）** 的入队函数。

**结论：**
这个函数**并没有**执行“下载地图”或“发送已有地图包”的具体业务逻辑。
它的作用是：**把“地图检查结果”这个消息，打包并安全地放入一个队列中，等待主线程去处理。**

打个比方：之前分析的函数是写信的人，这个函数是**邮筒**，主线程才是**邮递员**（真正去送信办事的人）。

### 详细代码分析

这个函数做了三件事：**打包消息**、**加锁**、**入队**。

#### 1. 检查与分配内存 (打包)
```assembly
; 检查事件ID (EDX) 是否有效
6F631FDA  | mov edi,edx                     ; EDX 是事件ID (12)
6F631FDC  | jne game.6F631FEA               ; 如果 ID != 0，跳转继续
6F631FDE  | ...                             ; 如果 ID == 0，报错返回

; 分配内存块 (创建一个 Event 对象)
6F631FEA  | push ebx
6F631FEB  | push ebp
6F631FEC  | mov ebp,dword ptr ss:[esp+18]   ; 获取数据长度 (Payload Size)
6F631FF0  | mov eax,ebp
6F631FF2  | call game.6F631FB0              ; 申请内存 -> new EventNode()
6F631FF7  | mov ebx,eax                     ; EBX = 新申请的事件节点指针

; 填充数据
6F631FF9  | mov eax,dword ptr ss:[esp+14]   ; 获取数据指针 (MapStatus: 0 or 1)
6F631FFF  | mov dword ptr ds:[ebx+C],edi    ; 将事件ID (12) 写入节点偏移 +0xC
6F632002  | je game.6F632012                ; 如果数据为空，跳过拷贝
6F632004  | push ebp                        ; Size
6F632005  | push eax                        ; Source
6F632006  | lea eax,dword ptr ds:[ebx+10]   ; Dest (节点偏移 +0x10)
6F632009  | push eax
6F63200A  | call <JMP.&memcpy>              ; 复制数据 -> 把 0 或 1 存进节点里
```
**解读**：这里创建了一个结构体，把 `EventID=12` 和 `Data=0/1` 存了起来。

#### 2. 线程加锁 (Thread Safety)
```assembly
6F632012  | lea ebp,dword ptr ds:[esi+10]   ; ESI 是网络管理器对象
6F632015  | mov ecx,ebp
6F632017  | call game.6F6D83A0              ; 进入临界区 -> EnterCriticalSection
```
**解读**：因为网络线程（当前线程）和主线程（游戏逻辑线程）都会访问这个队列，所以必须加锁，防止数据竞争。

#### 3. 插入链表 (Enqueue)
```assembly
; 下面是一堆复杂的指针操作，典型的双向链表尾部插入操作
6F63201C  | mov eax,dword ptr ds:[esi+1B8]  ; 获取队列尾部指针?
6F632022  | mov edx,dword ptr ds:[eax+ebx]  ; ...
; ... (中间省略链表指针调整逻辑) ...
6F63205A  | mov dword ptr ds:[eax],ecx      ; prev->next = node
6F63205C  | mov dword ptr ds:[ecx+4],edx    ; node->prev = ...
6F632067  | mov dword ptr ds:[esi+1BC],eax  ; 更新队列头/尾信息
```
**解读**：把刚才创建的那个“信件”（Event节点）挂到了链表的末尾。

#### 4. 解锁与通知
```assembly
6F63206D  | call game.6F6D83B0              ; 离开临界区 -> LeaveCriticalSection
6F632076  | ret 8
```
**解读**：解锁。注意，这里没有调用 `Signal` 或 `SetEvent`，因为那个信号通知的动作在**上一层函数**（`6F62B250` 的末尾 `call game.6F6D8950`）已经做过了。

---

### 全局视野：为什么这么设计？

Warcraft III 是一个多线程游戏：
1.  **网络线程**（我现在分析的代码）：负责收包、解包、校验。它不能直接修改游戏状态（比如不能直接加载地图、不能直接弹窗），因为这不安全，会导致崩溃。
2.  **主线程**：负责渲染画面、更新游戏逻辑。

**流程如下**：
1.  **网络线程**收到 `0x3D` 包 -> 解析 -> 发现 SHA1 不匹配 (MapStatus=0)。
2.  **网络线程**调用 `6F631FD0`（本函数） -> 把 `{ID:12, Status:0}` 扔进**消息队列**。
3.  **网络线程**调用 `SetEvent` 通知主线程：“嘿，有新消息了”。
4.  **主线程**在下一帧循环中，看到信号，锁住队列，取出 `{ID:12, Status:0}`。
5.  **主线程**根据 ID=12 执行 `HandleMapCheckResult` -> **此时才会真正触发地图下载界面或者发送 "I Have Map" 包**。

### 对我排错的意义

1.  如果网络线程现在计算出的 SHA1 匹配。
2.  Getter 会返回 `1`。
3.  本函数会把 `{ID:12, Status:1}` 扔进队列。
4.  主线程取出消息，看到 `Status=1`，就会认为地图校验通过，**发送 `0x23` 或 `0x2F` 包告诉服务端 "我准备好了"**。

所以，这个函数只要没崩溃，就不用管它，它是底层的搬运工。**核心还是在于那个 SHA1 到底算对没有。**


## 设置事件

```assembly
6F62AF10  | 53                      | push ebx                                        |
6F62AF11  | 55                      | push ebp                                        |
6F62AF12  | 56                      | push esi                                        |
6F62AF13  | 8B7424 10               | mov esi,dword ptr ss:[esp+10]                   |
6F62AF17  | 85F6                    | test esi,esi                                    |
6F62AF19  | 57                      | push edi                                        |
6F62AF1A  | 8BD9                    | mov ebx,ecx                                     |
6F62AF1C  | 74 4A                   | je game.6F62AF68                                |
6F62AF1E  | 8B7C24 18               | mov edi,dword ptr ss:[esp+18]                   |
6F62AF22  | 83E6 07                 | and esi,7                                       |
6F62AF25  | 8D0476                  | lea eax,dword ptr ds:[esi+esi*2]                |
6F62AF28  | 8D6C83 20               | lea ebp,dword ptr ds:[ebx+eax*4+20]             |
6F62AF2C  | 57                      | push edi                                        |
6F62AF2D  | 8BCD                    | mov ecx,ebp                                     |
6F62AF2F  | E8 ECE80A00             | call game.6F6D9820                              |
6F62AF34  | 8D4C76 21               | lea ecx,dword ptr ds:[esi+esi*2+21]             |
6F62AF38  | 8D048B                  | lea eax,dword ptr ds:[ebx+ecx*4]                |
6F62AF3B  | 8B40 04                 | mov eax,dword ptr ds:[eax+4]                    |
6F62AF3E  | 33D2                    | xor edx,edx                                     |
6F62AF40  | 85C0                    | test eax,eax                                    |
6F62AF42  | 0F9EC2                  | setle dl                                        |
6F62AF45  | 83EA 01                 | sub edx,1                                       |
6F62AF48  | 23C2                    | and eax,edx                                     |
6F62AF4A  | 85C0                    | test eax,eax                                    |
6F62AF4C  | 7E 12                   | jle game.6F62AF60                               |
6F62AF4E  | 8BFF                    | mov edi,edi                                     |
6F62AF50  | 8B4C24 14               | mov ecx,dword ptr ss:[esp+14]                   |
6F62AF54  | 3948 0C                 | cmp dword ptr ds:[eax+C],ecx                    |
6F62AF57  | 74 22                   | je game.6F62AF7B                                |
6F62AF59  | 8B40 08                 | mov eax,dword ptr ds:[eax+8]                    |
6F62AF5C  | 85C0                    | test eax,eax                                    |
6F62AF5E  | 7F F0                   | jg game.6F62AF50                                |
6F62AF60  | 57                      | push edi                                        |
6F62AF61  | 8BCD                    | mov ecx,ebp                                     |
6F62AF63  | E8 E8D90A00             | call game.6F6D8950                              |
6F62AF68  | 8B4424 1C               | mov eax,dword ptr ss:[esp+1C]                   |
6F62AF6C  | 5F                      | pop edi                                         |
6F62AF6D  | 5E                      | pop esi                                         |
6F62AF6E  | 5D                      | pop ebp                                         |
6F62AF6F  | C700 FFFFFFFF           | mov dword ptr ds:[eax],FFFFFFFF                 |
6F62AF75  | 33C0                    | xor eax,eax                                     |
6F62AF77  | 5B                      | pop ebx                                         |
6F62AF78  | C2 1400                 | ret 14                                          |
6F62AF7B  | 8B5424 1C               | mov edx,dword ptr ss:[esp+1C]                   |
6F62AF7F  | F7DF                    | neg edi                                         |
6F62AF81  | 1BFF                    | sbb edi,edi                                     |
6F62AF83  | 83E7 08                 | and edi,8                                       |
6F62AF86  | 03FE                    | add edi,esi                                     |
6F62AF88  | 893A                    | mov dword ptr ds:[edx],edi                      |
6F62AF8A  | 5F                      | pop edi                                         |
6F62AF8B  | 5E                      | pop esi                                         |
6F62AF8C  | 5D                      | pop ebp                                         |
6F62AF8D  | 5B                      | pop ebx                                         |
6F62AF8E  | C2 1400                 | ret 14                                          |
```
```assembly
6F6D9820  | 8BC1                    | mov eax,ecx                                     |
6F6D9822  | E9 89FEFFFF             | jmp game.6F6D96B0                               |
```
```assembly
6F6D96B0  | 837C24 04 00            | cmp dword ptr ss:[esp+4],0                      |
6F6D96B5  | 57                      | push edi                                        |
6F6D96B6  | 8BF8                    | mov edi,eax                                     |
6F6D96B8  | 74 0A                   | je game.6F6D96C4                                |
6F6D96BA  | 57                      | push edi                                        |
6F6D96BB  | E8 10FFFFFF             | call game.6F6D95D0                              |
6F6D96C0  | 5F                      | pop edi                                         |
6F6D96C1  | C2 0400                 | ret 4                                           |
6F6D96C4  | 53                      | push ebx                                        |
6F6D96C5  | 56                      | push esi                                        |
6F6D96C6  | 6A 02                   | push 2                                          |
6F6D96C8  | 8D5F 08                 | lea ebx,dword ptr ds:[edi+8]                    |
6F6D96CB  | 6A 01                   | push 1                                          |
6F6D96CD  | E8 0EEBFFFF             | call game.6F6D81E0                              |
6F6D96D2  | 8BF0                    | mov esi,eax                                     |
6F6D96D4  | 25 0000E0FF             | and eax,FFE00000                                |
6F6D96D9  | 3BF0                    | cmp esi,eax                                     |
6F6D96DB  | 75 2C                   | jne game.6F6D9709                               |
6F6D96DD  | 57                      | push edi                                        |
6F6D96DE  | E8 EDFEFFFF             | call game.6F6D95D0                              |
6F6D96E3  | 8BCB                    | mov ecx,ebx                                     |
6F6D96E5  | E8 76A9FEFF             | call game.6F6C4060                              |
6F6D96EA  | 85F6                    | test esi,esi                                    |
6F6D96EC  | 0F84 7C000000           | je game.6F6D976E                                |
6F6D96F2  | C1EE 15                 | shr esi,15                                      |
6F6D96F5  | 8B0CB5 8C6DAD6F         | mov ecx,dword ptr ds:[esi*4+6FAD6D8C]           |
6F6D96FC  | 5E                      | pop esi                                         |
6F6D96FD  | 5B                      | pop ebx                                         |
6F6D96FE  | 5F                      | pop edi                                         |
6F6D96FF  | 894C24 04               | mov dword ptr ss:[esp+4],ecx                    |
6F6D9703  | FF25 CCD1866F           | jmp dword ptr ds:[<SetEvent>]                   |
6F6D9709  | 55                      | push ebp                                        |
6F6D970A  | 8B2D 84D1866F           | mov ebp,dword ptr ds:[<WaitForSingleObject>]    |
6F6D9710  | 8B3D 04FFA96F           | mov edi,dword ptr ds:[6FA9FF04]                 |
6F6D9716  | F603 01                 | test byte ptr ds:[ebx],1                        |
6F6D9719  | 75 52                   | jne game.6F6D976D                               |
6F6D971B  | E8 D047FFFF             | call game.6F6CDEF0                              |
6F6D9720  | 83EF 01                 | sub edi,1                                       |
6F6D9723  | 75 F1                   | jne game.6F6D9716                               |
6F6D9725  | 8BD6                    | mov edx,esi                                     |
6F6D9727  | C1EA 15                 | shr edx,15                                      |
6F6D972A  | 8B0495 8C6DAD6F         | mov eax,dword ptr ds:[edx*4+6FAD6D8C]           |
6F6D9731  | 68 60EA0000             | push EA60                                       |
6F6D9736  | 50                      | push eax                                        |
6F6D9737  | FFD5                    | call ebp                                        |
6F6D9739  | 85C0                    | test eax,eax                                    |
6F6D973B  | 74 30                   | je game.6F6D976D                                |
6F6D973D  | B9 08A6AD6F             | mov ecx,game.6FADA608                           |
6F6D9742  | E8 19A9FEFF             | call game.6F6C4060                              |
6F6D9747  | FF15 50D1866F           | call dword ptr ds:[<GetCurrentThreadId>]        |
6F6D974D  | 50                      | push eax                                        |
6F6D974E  | A1 08A6AD6F             | mov eax,dword ptr ds:[6FADA608]                 |
6F6D9753  | 50                      | push eax                                        |
6F6D9754  | 68 B054976F             | push game.6F9754B0                              |
6F6D9759  | E8 0225B3FF             | call game.6F20BC60                              |
6F6D975E  | 83C4 0C                 | add esp,C                                       |
6F6D9761  | E8 9AFDFFFF             | call game.6F6D9500                              |
6F6D9766  | E8 A5FDFFFF             | call game.6F6D9510                              |
6F6D976B  | EB A3                   | jmp game.6F6D9710                               |
6F6D976D  | 5D                      | pop ebp                                         |
6F6D976E  | 5E                      | pop esi                                         |
6F6D976F  | 5B                      | pop ebx                                         |
6F6D9770  | 5F                      | pop edi                                         |
6F6D9771  | C2 0400                 | ret 4                                           |
```
这几段代码揭示了 Warcraft III 内部封装的 **同步对象（Synchronization Object）** 机制，类似于 C++ 的 `CEvent` 或 `CMutex` 封装类。暴雪为了防止死锁（Deadlock）和方便调试，在标准的 Windows API 外面包裹了一层逻辑。

我们一段一段来拆解确认：

### 1. `6F6D96B0`: 确实是 Signal / SetEvent (带死锁检测)

我问的：**"这里是 setEvent 对么？"**
**答案：是的，这是触发信号的函数。**

```assembly
6F6D9703  | FF25 CCD1866F           | jmp dword ptr ds:[<SetEvent>]                   |
```
这一行直接跳转到了 Windows API `SetEvent`。

但是，注意看它下面的 `6F6D9709` 及其后的代码：
```assembly
6F6D970A  | 8B2D 84D1866F           | mov ebp,dword ptr ds:[<WaitForSingleObject>]    |
...
6F6D9731  | 68 60EA0000             | push EA60                                       | ; 60000 ms = 60秒
6F6D9737  | FFD5                    | call ebp                                        | ; WaitForSingleObject
...
6F6D9754  | 68 B054976F             | push game.6F9754B0                              | ; "[%u] Suspected deadlock, thread %u\n"
```
**分析：**
这是一个高级的封装。它不只是简单的 `SetEvent`。
1.  它首先尝试获取某种锁或状态。
2.  如果一切正常，它调用 `SetEvent` 唤醒等待的线程。
3.  如果发现状态异常（可能是为了实现 `SignalAndWait` 或者某种互斥逻辑），它会调用 `WaitForSingleObject` 等待。
4.  **关键点：** 它设置了 `0xEA60` (60秒) 的超时时间。如果 60秒还没等到，它会打印 **"Suspected deadlock"**（疑似死锁）。这是游戏开发中非常典型的调试手段。

```assembly
6F6D8950  | 8B4424 04               | mov eax,dword ptr ss:[esp+4]                    |
6F6D8954  | 56                      | push esi                                        |
6F6D8955  | 50                      | push eax                                        |
6F6D8956  | 8BF1                    | mov esi,ecx                                     |
6F6D8958  | E8 E3F9FFFF             | call game.6F6D8340                              |
6F6D895D  | 5E                      | pop esi                                         |
6F6D895E  | C2 0400                 | ret 4                                           |
```
```assembly
6F6D8340  | 837C24 04 00            | cmp dword ptr ss:[esp+4],0                      |
6F6D8345  | 75 20                   | jne game.6F6D8367                               |
6F6D8347  | 53                      | push ebx                                        |
6F6D8348  | 57                      | push edi                                        |
6F6D8349  | 8B7E 08                 | mov edi,dword ptr ds:[esi+8]                    |
6F6D834C  | 8D5E 08                 | lea ebx,dword ptr ds:[esi+8]                    |
6F6D834F  | 6A 02                   | push 2                                          |
6F6D8351  | 81E7 0000E0FF           | and edi,FFE00000                                |
6F6D8357  | 83C7 01                 | add edi,1                                       |
6F6D835A  | 6A 01                   | push 1                                          |
6F6D835C  | E8 DFFEFFFF             | call game.6F6D8240                              |
6F6D8361  | 85C0                    | test eax,eax                                    |
6F6D8363  | 5F                      | pop edi                                         |
6F6D8364  | 5B                      | pop ebx                                         |
6F6D8365  | 74 07                   | je game.6F6D836E                                |
6F6D8367  | 8BC6                    | mov eax,esi                                     |
6F6D8369  | E8 72FFFFFF             | call game.6F6D82E0                              |
6F6D836E  | C2 0400                 | ret 4                                           |
```
```assembly
6F6D8240  | 55                      | push ebp                                        |
6F6D8241  | 8B6C24 0C               | mov ebp,dword ptr ss:[esp+C]                    |
6F6D8245  | 56                      | push esi                                        |
6F6D8246  | 57                      | push edi                                        |
6F6D8247  | 33D2                    | xor edx,edx                                     |
6F6D8249  | 8BCB                    | mov ecx,ebx                                     |
6F6D824B  | E8 60BEFEFF             | call game.6F6C40B0                              |
6F6D8250  | 8BF0                    | mov esi,eax                                     |
6F6D8252  | 3BF7                    | cmp esi,edi                                     |
6F6D8254  | 74 20                   | je game.6F6D8276                                |
6F6D8256  | 8BD6                    | mov edx,esi                                     |
6F6D8258  | 56                      | push esi                                        |
6F6D8259  | 2BD5                    | sub edx,ebp                                     |
6F6D825B  | 8BCB                    | mov ecx,ebx                                     |
6F6D825D  | E8 4EBEFEFF             | call game.6F6C40B0                              |
6F6D8262  | 3BF0                    | cmp esi,eax                                     |
6F6D8264  | 74 27                   | je game.6F6D828D                                |
6F6D8266  | 57                      | push edi                                        |
6F6D8267  | 33D2                    | xor edx,edx                                     |
6F6D8269  | 8BCB                    | mov ecx,ebx                                     |
6F6D826B  | E8 40BEFEFF             | call game.6F6C40B0                              |
6F6D8270  | 8BF0                    | mov esi,eax                                     |
6F6D8272  | 3BF7                    | cmp esi,edi                                     |
6F6D8274  | 75 E0                   | jne game.6F6D8256                               |
6F6D8276  | 8B4C24 0C               | mov ecx,dword ptr ss:[esp+C]                    |
6F6D827A  | 6A 01                   | push 1                                          |
6F6D827C  | 8BC7                    | mov eax,edi                                     |
6F6D827E  | E8 6DFEFFFF             | call game.6F6D80F0                              |
6F6D8283  | 5E                      | pop esi                                         |
6F6D8284  | B8 01000000             | mov eax,1                                       |
6F6D8289  | 5D                      | pop ebp                                         |
6F6D828A  | C2 0800                 | ret 8                                           |
6F6D828D  | 5E                      | pop esi                                         |
6F6D828E  | 33C0                    | xor eax,eax                                     |
6F6D8290  | 5D                      | pop ebp                                         |
6F6D8291  | C2 0800                 | ret 8                                           |
```
```assembly
6F6D80F0  | 53                      | push ebx                                        |
6F6D80F1  | 56                      | push esi                                        |
6F6D80F2  | 8BF1                    | mov esi,ecx                                     |
6F6D80F4  | 57                      | push edi                                        |
6F6D80F5  | 8D0CB5 781DAD6F         | lea ecx,dword ptr ds:[esi*4+6FAD1D78]           |
6F6D80FC  | 8BD8                    | mov ebx,eax                                     |
6F6D80FE  | E8 5DBFFEFF             | call game.6F6C4060                              |
6F6D8103  | 837C24 10 00            | cmp dword ptr ss:[esp+10],0                     |
6F6D8108  | 74 1A                   | je game.6F6D8124                                |
6F6D810A  | 8BC3                    | mov eax,ebx                                     |
6F6D810C  | 8BCE                    | mov ecx,esi                                     |
6F6D810E  | C1E8 15                 | shr eax,15                                      |
6F6D8111  | C1E1 0A                 | shl ecx,A                                       |
6F6D8114  | 03C1                    | add eax,ecx                                     |
6F6D8116  | 8B1485 8C5DAD6F         | mov edx,dword ptr ds:[eax*4+6FAD5D8C]           |
6F6D811D  | 52                      | push edx                                        |
6F6D811E  | FF15 80D1866F           | call dword ptr ds:[<ResetEvent>]                |
6F6D8124  | 8D0CB5 801DAD6F         | lea ecx,dword ptr ds:[esi*4+6FAD1D80]           |
6F6D812B  | E8 30BFFEFF             | call game.6F6C4060                              |
6F6D8130  | 25 FF070000             | and eax,7FF                                     |
6F6D8135  | C1E6 0B                 | shl esi,B                                       |
6F6D8138  | 03F0                    | add esi,eax                                     |
6F6D813A  | 8D3CB5 901DAD6F         | lea edi,dword ptr ds:[esi*4+6FAD1D90]           |
6F6D8141  | 8BD3                    | mov edx,ebx                                     |
6F6D8143  | 81E2 0000E0FF           | and edx,FFE00000                                |
6F6D8149  | 8BCF                    | mov ecx,edi                                     |
6F6D814B  | E8 50BFFEFF             | call game.6F6C40A0                              |
6F6D8150  | 8BF0                    | mov esi,eax                                     |
6F6D8152  | 85F6                    | test esi,esi                                    |
6F6D8154  | 74 2C                   | je game.6F6D8182                                |
6F6D8156  | 8B1D 04FFA96F           | mov ebx,dword ptr ds:[6FA9FF04]                 |
6F6D815C  | 8D6424 00               | lea esp,dword ptr ss:[esp]                      |
6F6D8160  | 8BD6                    | mov edx,esi                                     |
6F6D8162  | 8BCF                    | mov ecx,edi                                     |
6F6D8164  | E8 37BFFEFF             | call game.6F6C40A0                              |
6F6D8169  | 8BF0                    | mov esi,eax                                     |
6F6D816B  | 85F6                    | test esi,esi                                    |
6F6D816D  | 74 13                   | je game.6F6D8182                                |
6F6D816F  | E8 7C5DFFFF             | call game.6F6CDEF0                              |
6F6D8174  | 83EB 01                 | sub ebx,1                                       |
6F6D8177  | 75 E7                   | jne game.6F6D8160                               |
6F6D8179  | 33C9                    | xor ecx,ecx                                     |
6F6D817B  | E8 205DFFFF             | call game.6F6CDEA0                              |
6F6D8180  | EB D4                   | jmp game.6F6D8156                               |
6F6D8182  | 5F                      | pop edi                                         |
6F6D8183  | 5E                      | pop esi                                         |
6F6D8184  | 5B                      | pop ebx                                         |
6F6D8185  | C2 0400                 | ret 4                                           |
```

### 2. `6F6D8950`: 包含 ResetEvent (设置状态)


它的逻辑是根据传入的参数（`0` 或 `1`）来决定是 `Set` 还是 `Reset`。

**调用链分析：**

1.  **入口** `6F6D8950`:
    ```assembly
    6F6D8950  | 8B4424 04   | mov eax,dword ptr ss:[esp+4] ; 获取参数 (bool)
    6F6D8958  | call game.6F6D8340
    ```

2.  **分发** `6F6D8340`:
    ```assembly
    6F6D8340  | cmp dword ptr ss:[esp+4],0  ; 检查参数是 0 还是 1
    6F6D8345  | jne game.6F6D8367           ; 如果是 1 (True)，跳转去 SetEvent
    ; --- 如果是 0 (False)，走下面 ---
    6F6D835C  | call game.6F6D8240          ; 准备 Reset
    ```

3.  **准备 Reset** `6F6D8240`:
    ```assembly
    6F6D827E  | call game.6F6D80F0
    ```

4.  **最终执行 Reset** `6F6D80F0`:
    ```assembly
    6F6D811E  | call dword ptr ds:[<ResetEvent>] ; <--- 就在这里！
    ```

5.  **反之，如果是 Set** (`6F6D8367` -> `6F6D82E0`):
    ```assembly
    6F6D82F8  | call dword ptr ds:[<SetEvent>]
    ```

**结论：**
*   `6F6D8950(0)`  == `ResetEvent` (清除信号，设为无信号状态)
*   `6F6D8950(1)`  == `SetEvent` (触发信号，设为有信号状态)

---

### 3. `6F62AF10`: 这是一个事件分发循环 (Event Dispatcher)

这段代码展示了主线程如何处理这些事件：

```assembly
6F62AF63  | call game.6F6D8950  ; 更新事件状态
```
这里调用了上面的 `SetState` 函数。

**整体流程总结 (结合我之前的分析)：**

1.  **网络线程** (Producer):
    *   收包 -> 校验 SHA1 -> 结果不对。
    *   调用 `6F631FD0` 把消息 `{ID:12, Status:0}` 入队。
    *   调用 `6F6D96B0` (`SetEvent`) -> **唤醒主线程**。

2.  **主线程** (Consumer):
    *   平时卡在 `WaitForSingleObject` (在 `6F6D9710` 那个位置附近)。
    *   收到信号，醒来。
    *   执行 `6F62AF10` 这样的函数，检查队列。
    *   取出 `{ID:12, Status:0}`。
    *   处理完后，可能会调用 `6F6D8950(0)` 即 `ResetEvent`，把信号灭掉，表示“我处理完了，继续睡，等下一个信号”。
	

## 错误代码

```cpp
bool War3Map::load(const QString &mapPath)
{
    m_valid = false;
    m_mapPath = mapPath;

    LOG_INFO(QString("[War3Map] 开始加载地图: %1").arg(mapPath));

    QFile file(mapPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("[War3Map] ❌ 无法打开地图文件: %1").arg(mapPath));
        return false;
    }

    // 1. 读取地图基础信息 (Size & CRC32 & SHA1)
    QByteArray mapRawData = file.readAll();
    file.close();

    quint32 mapSizeInt = mapRawData.size();
    m_mapSize = toBytes(mapSizeInt);

    // 标准 Zip CRC32 (用于 MPQ 完整性)
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef*)mapRawData.constData(), mapRawData.size());
    m_mapInfo = toBytes((quint32)crc);

    // 计算 SHA1 (Map Data + Magic String)
    // 这是标准的 Map SHA1 计算方式，替代原本基于 combinedData 的逻辑
    QCryptographicHash sha1(QCryptographicHash::Sha1);
    sha1.addData(mapRawData);
    sha1.addData("\x9E\x37\xF1\x03", 4); // War3 Map SHA1 Magic
    m_mapSHA1Bytes = sha1.result();

    // 2. 打开 MPQ 档案
    HANDLE hMpq = NULL;
    QString nativePath = QDir::toNativeSeparators(mapPath);
    DWORD flags = MPQ_OPEN_READ_ONLY;

#ifdef UNICODE
    const wchar_t *pathStr = (const wchar_t*)nativePath.utf16();
#else
    const char *pathStr = nativePath.toLocal8Bit().constData();
#endif

    if (!SFileOpenArchive(pathStr, 0, flags, &hMpq)) {
        LOG_ERROR(QString("[War3Map] ❌ 无法打开 MPQ: %1").arg(nativePath));
        return false;
    }

    // -------------------------------------------------------------
    // ★★★ 地图一致性校验 (Map Checksum) - Stage 1 & 2 ★★★
    // -------------------------------------------------------------

    // Helper: 从本地磁盘读取环境脚本 (common.j / blizzard.j)
    auto readLocalScript = [&](const QString &fileName) -> QByteArray {
        // A. 优先目录
        if (!s_priorityCrcDir.isEmpty()) {
            QFile f(s_priorityCrcDir + "/" + fileName);
            if (f.exists() && f.open(QIODevice::ReadOnly)) return f.readAll();
        }
        // B. 默认目录
        QFile fDefault("war3files/" + fileName);
        if (fDefault.open(QIODevice::ReadOnly)) return fDefault.readAll();
        return QByteArray();
    };

    // Helper: 从 MPQ 读取文件
    auto readMpqFile = [&](const QString &fileName) -> QByteArray {
        HANDLE hFile = NULL;
        QByteArray buffer;
        if (SFileOpenFileEx(hMpq, fileName.toLocal8Bit().constData(), 0, &hFile)) {
            DWORD s = SFileGetFileSize(hFile, NULL);
            if (s > 0 && s != 0xFFFFFFFF) {
                buffer.resize(s);
                DWORD read = 0;
                SFileReadFile(hFile, buffer.data(), s, &read, NULL);
            }
            SFileCloseFile(hFile);
        }
        return buffer;
    };

    // --- Step 1: 准备环境数据 ---
    QByteArray dataCommon = readLocalScript("common.j");
    QByteArray dataBlizzard = readLocalScript("blizzard.j");

    // 尝试读取 war3map.j，如果是 Lua 地图则尝试 war3map.lua
    QByteArray dataMapScript = readMpqFile("war3map.j");
    if (dataMapScript.isEmpty()) dataMapScript = readMpqFile("scripts\\war3map.j");
    if (dataMapScript.isEmpty()) dataMapScript = readMpqFile("war3map.lua");

    if (dataCommon.isEmpty()) {
        LOG_ERROR("[War3Map] ❌ 严重错误: 无法读取 common.j (路径: war3files/common.j)");
    }
    if (dataBlizzard.isEmpty()) {
        LOG_ERROR("[War3Map] ❌ 严重错误: 无法读取 blizzard.j (路径: war3files/blizzard.j)");
    }
    if (dataMapScript.isEmpty()) {
        LOG_ERROR("[War3Map] ❌ 严重错误: 无法在地图中找到脚本文件");
    }

    quint32 hCommon = calcBlizzardHash(dataCommon);
    quint32 hBlizzard = calcBlizzardHash(dataBlizzard);
    quint32 hMapScript = calcBlizzardHash(dataMapScript);

    LOG_INFO(QString("🔹 Common.j Hash:   %1 (Size: %2)").arg(QString::number(hCommon, 16).toUpper()).arg(dataCommon.size()));
    LOG_INFO(QString("🔹 Blizzard.j Hash: %1 (Size: %2)").arg(QString::number(hBlizzard, 16).toUpper()).arg(dataBlizzard.size()));
    LOG_INFO(QString("🔹 War3Map.j Hash:  %1 (Size: %2)").arg(QString::number(hMapScript, 16).toUpper()).arg(dataMapScript.size()));

    // --- Step 2: 脚本环境混合 (Stage 1) ---
    // 公式: ROL(ROL(Bliz ^ Com, 3) ^ Magic, 3) ^ MapScript
    quint32 val = 0;

    val = hBlizzard ^ hCommon;      // Xor
    val = rotateLeft(val, 3);       // Rol 1
    val = val ^ 0x03F1379E;         // Salt
    val = rotateLeft(val, 3);       // Rol 2
    val = hMapScript ^ val;         // Mix Map
    val = rotateLeft(val, 3);       // Rol 3

    LOG_INFO(QString("[War3Map] Stage 1 Checksum: %1").arg(QString::number(val, 16).toUpper()));

    // --- Step 3: 组件校验 (Stage 2) ---
    // 严格顺序: w3e, wpm, doo, w3u, w3b, w3d, w3a, w3q
    const char *componentFiles[] = {
        "war3map.w3e", "war3map.wpm", "war3map.doo", "war3map.w3u",
        "war3map.w3b", "war3map.w3d", "war3map.w3a", "war3map.w3q"
    };

    for (const char *compName : componentFiles) {
        QByteArray compData = readMpqFile(compName);

        // 文件不存在或为空则跳过 (不混入0)
        if (compData.isEmpty()) continue;

        quint32 hComp = calcBlizzardHash(compData);

        // Mix Component
        val = val ^ hComp;
        val = rotateLeft(val, 3);

        LOG_INFO(QString("   + Mixed Component: %1 (Hash: %2)").arg(compName, QString::number(hComp, 16).toUpper()));
    }

    // 保存最终 CRC
    m_mapCRC = toBytes(val);

    LOG_INFO(QString("[War3Map] Final Checksum: %1").arg(QString(m_mapCRC.toHex().toUpper())));
    LOG_INFO(QString("[War3Map] Map SHA1:       %1").arg(QString(m_mapSHA1Bytes.toHex().toUpper())));


    // 3. 解析 war3map.w3i (获取地图信息)
    QByteArray w3iData = readMpqFile("war3map.w3i");
    if (!w3iData.isEmpty()) {
        QDataStream in(w3iData);
        in.setByteOrder(QDataStream::LittleEndian);

        quint32 fileFormat;
        in >> fileFormat;

        // 支持版本 18 (RoC) 和 25 (TFT)
        if (fileFormat == 18 || fileFormat == 25) {
            in.skipRawData(4); // saves
            in.skipRawData(4); // editor ver

            // 跳过地图名称/作者/描述/建议玩家 (4个字符串)
            auto skipStr = [&]() {
                char c;
                do { in >> (quint8&)c; } while(c != 0 && !in.atEnd());
            };
            skipStr(); skipStr(); skipStr(); skipStr();

            in.skipRawData(32); // camera bounds
            in.skipRawData(16); // camera complements

            quint32 rawW, rawH, rawFlags;
            in >> rawW >> rawH >> rawFlags;

            m_mapWidth = toBytes16((quint16)rawW);
            m_mapHeight = toBytes16((quint16)rawH);
            m_mapOptions = rawFlags;

            LOG_INFO(QString("[War3Map] w3i 解析成功. Size: %1x%2 Flags: 0x%3")
                         .arg(rawW).arg(rawH).arg(QString::number(m_mapOptions, 16).toUpper()));
        }
    } else {
        LOG_WARNING("[War3Map] ⚠️ 无法读取 war3map.w3i (可能是受保护的地图)");
    }

    SFileCloseArchive(hMpq);
    m_valid = true;
    return true;
}
```
## 修正错误

// 辅助函数：将 uint32 以小端序写入 SHA1
static void sha1UpdateInt32(QCryptographicHash &sha1, quint32 value) {
    quint32 le = qToLittleEndian(value);
    sha1.addData((const char*)&le, 4);
}

// =========================================================
// 核心加载函数 (最终整理版)
// =========================================================
bool War3Map::load(const QString &mapPath)
{
    m_valid = false;
    m_mapPath = mapPath;

    LOG_INFO(QString("[War3Map] 开始加载地图: %1").arg(mapPath));

    // -------------------------------------------------------
    // 1. 基础文件检查与 MPQ 打开
    // -------------------------------------------------------
    QFile file(mapPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("[War3Map] ❌ 无法打开地图文件: %1").arg(mapPath));
        return false;
    }
    
    // 计算文件物理大小 (StatString 需要)
    m_mapSize = toBytes((quint32)file.size());
    file.close();

    // 打开 MPQ
    HANDLE hMpq = NULL;
    QString nativePath = QDir::toNativeSeparators(mapPath);
    
#ifdef UNICODE
    const wchar_t *pathStr = (const wchar_t*)nativePath.utf16();
#else
    const char *pathStr = nativePath.toLocal8Bit().constData();
#endif

    if (!SFileOpenArchive(pathStr, 0, MPQ_OPEN_READ_ONLY, &hMpq)) {
        LOG_ERROR(QString("[War3Map] ❌ 无法打开 MPQ: %1").arg(nativePath));
        return false;
    }

    // -------------------------------------------------------
    // 2. 定义读取辅助 Lambda
    // -------------------------------------------------------
    
    // 从本地 war3 目录读取环境文件
    auto readLocalScript = [&](const QString &fileName) -> QByteArray {
        if (!s_priorityCrcDir.isEmpty()) {
            QFile f(s_priorityCrcDir + "/" + fileName);
            if (f.exists() && f.open(QIODevice::ReadOnly)) return f.readAll();
        }
        QFile fDefault("war3files/" + fileName);
        if (fDefault.open(QIODevice::ReadOnly)) return fDefault.readAll();
        return QByteArray();
    };

    // 从 MPQ 读取文件
    auto readMpqFile = [&](const QString &fileName) -> QByteArray {
        HANDLE hFile = NULL;
        QByteArray buffer;
        if (SFileOpenFileEx(hMpq, fileName.toLocal8Bit().constData(), 0, &hFile)) {
            DWORD s = SFileGetFileSize(hFile, NULL);
            if (s > 0 && s != 0xFFFFFFFF) {
                buffer.resize(s);
                DWORD read = 0;
                SFileReadFile(hFile, buffer.data(), s, &read, NULL);
            }
            SFileCloseFile(hFile);
        }
        return buffer;
    };

    // -------------------------------------------------------
    // 3. 准备核心数据 (Script & Env)
    // -------------------------------------------------------
    
    // 读取环境脚本
    QByteArray dataCommon = readLocalScript("common.j");
    QByteArray dataBlizzard = readLocalScript("blizzard.j");

    if (dataCommon.isEmpty() || dataBlizzard.isEmpty()) {
        LOG_ERROR("[War3Map] ❌ 严重错误: 缺少 common.j 或 blizzard.j，校验无法进行！");
        SFileCloseArchive(hMpq);
        return false;
    }

    // 读取地图脚本 (支持 war3map.j / scripts\war3map.j / war3map.lua)
    QByteArray dataMapScript = readMpqFile("war3map.j");
    if (dataMapScript.isEmpty()) dataMapScript = readMpqFile("scripts\\war3map.j");
    if (dataMapScript.isEmpty()) dataMapScript = readMpqFile("war3map.lua"); // 兼容 Lua

    if (dataMapScript.isEmpty()) {
        LOG_ERROR("[War3Map] ❌ 严重错误: 无法在地图中找到脚本文件");
        SFileCloseArchive(hMpq);
        return false;
    }

    // -------------------------------------------------------
    // 4. 初始化校验算法 (Legacy CRC & New SHA1)
    // -------------------------------------------------------

    // === A. 初始化 SHA-1 (1.26a 核心逻辑) ===
    QCryptographicHash sha1Ctx(QCryptographicHash::Sha1);
    
    sha1Ctx.addData(dataCommon);          // 1. common.j
    sha1Ctx.addData(dataBlizzard);        // 2. blizzard.j
    sha1UpdateInt32(sha1Ctx, 0x03F1379E); // 3. Salt (0x03F1379E)
    sha1Ctx.addData(dataMapScript);       // 4. war3map.j

    // === B. 初始化 Legacy CRC (XORO 算法, 兼容旧平台) ===
    quint32 crcVal = 0;
    quint32 hCommon = calcBlizzardHash(dataCommon);
    quint32 hBlizz = calcBlizzardHash(dataBlizzard);
    quint32 hScript = calcBlizzardHash(dataMapScript);

    crcVal = hBlizz ^ hCommon;      // Xor
    crcVal = rotateLeft(crcVal, 3); // Rol 1
    crcVal = crcVal ^ 0x03F1379E;   // Salt
    crcVal = rotateLeft(crcVal, 3); // Rol 2
    crcVal = hScript ^ crcVal;      // Mix Map
    crcVal = rotateLeft(crcVal, 3); // Rol 3

    // -------------------------------------------------------
    // 5. 统一遍历组件 (同时更新两个算法)
    // -------------------------------------------------------
    const char *componentFiles[] = {
        "war3map.w3e", "war3map.wpm", "war3map.doo", "war3map.w3u",
        "war3map.w3b", "war3map.w3d", "war3map.w3a", "war3map.w3q"
    };

    for (const char *compName : componentFiles) {
        // 读取组件数据
        QByteArray compData = readMpqFile(compName);

        // 如果文件存在，同时加入两个算法的计算
        if (!compData.isEmpty()) {
            // A. Update SHA-1
            sha1Ctx.addData(compData);

            // B. Update Legacy CRC
            quint32 hComp = calcBlizzardHash(compData);
            crcVal = crcVal ^ hComp;
            crcVal = rotateLeft(crcVal, 3);
            
            LOG_INFO(QString("   + [Checksum] 组件已加入: %1 (Size: %2)").arg(compName).arg(compData.size()));
        }
    }

    // -------------------------------------------------------
    // 6. 结算与保存结果
    // -------------------------------------------------------
    
    // 保存 SHA-1 (StatString 真正用到的 20 字节)
    m_mapSHA1Bytes = sha1Ctx.result();
    
    // 保存 CRC (兼容字段)
    m_mapCRC = toBytes(crcVal);

    LOG_INFO(QString("[War3Map] ✅ SHA1 Checksum: %1").arg(QString(m_mapSHA1Bytes.toHex().toUpper())));
    LOG_INFO(QString("[War3Map] ✅ CRC  Checksum: %1").arg(QString(m_mapCRC.toHex().toUpper())));

    // -------------------------------------------------------
    // 7. 解析 war3map.w3i (获取地图信息)
    // -------------------------------------------------------
    QByteArray w3iData = readMpqFile("war3map.w3i");
    if (!w3iData.isEmpty()) {
        QDataStream in(w3iData);
        in.setByteOrder(QDataStream::LittleEndian);

        quint32 fileFormat;
        in >> fileFormat;

        if (fileFormat == 18 || fileFormat == 25) {
            in.skipRawData(4); // saves
            in.skipRawData(4); // editor ver

            // 跳过变长字符串
            auto skipStr = [&]() {
                char c;
                do { in >> (quint8&)c; } while(c != 0 && !in.atEnd());
            };
            skipStr(); skipStr(); skipStr(); skipStr();

            in.skipRawData(32); // camera bounds
            in.skipRawData(16); // camera complements

            quint32 rawW, rawH, rawFlags;
            in >> rawW >> rawH >> rawFlags;

            m_mapWidth = toBytes16((quint16)rawW);
            m_mapHeight = toBytes16((quint16)rawH);
            m_mapOptions = rawFlags;
        }
    } else {
        LOG_WARNING("[War3Map] ⚠️ 无法读取 war3map.w3i，将使用默认参数");
    }

    // -------------------------------------------------------
    // 8. 清理与完成
    // -------------------------------------------------------
    SFileCloseArchive(hMpq);
    m_valid = true;
    return true;
}






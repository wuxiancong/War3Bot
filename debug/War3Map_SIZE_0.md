

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

## 对比数据
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
6F6579D4  | 75 12                   | jne game.6F6579E8                   | <--- 跳转将要执行（结果: Fail）
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
6F657A7B  | E8 80F3FFFF             | call game.6F656E00                  | <--- SHA1对比失败，报告给服务器。
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

## 数据来源
- **地址**：`6F682300`
- **偏移**：`game.dll + 682300`

下面的反汇编是魔兽争霸1.26a 处理数据包的分支的逻辑，详细内容请参考[https://github.com/wuxiancong/War3Bot/blob/main/details/1.26/GameDll_682300_CustomGame_W3GSPacket_Dispatch.md](https://github.com/wuxiancong/War3Bot/blob/main/details/1.26/GameDll_682300_CustomGame_W3GSPacket_Dispatch.md)

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

```
6F67EC10  | 81EC 3C020000           | sub esp,23C                         |
6F67EC16  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]     |
6F67EC1B  | 33C4                    | xor eax,esp                         |
6F67EC1D  | 898424 38020000         | mov dword ptr ss:[esp+238],eax      |
6F67EC24  | 8B8C24 44020000         | mov ecx,dword ptr ss:[esp+244]      |
6F67EC2B  | 8B9424 48020000         | mov edx,dword ptr ss:[esp+248]      |
6F67EC32  | 57                      | push edi                            |
6F67EC33  | 8BBC24 44020000         | mov edi,dword ptr ss:[esp+244]      |
6F67EC3A  | 8D4424 10               | lea eax,dword ptr ss:[esp+10]       |
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














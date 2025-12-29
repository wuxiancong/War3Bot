

## 逆向入口
**在拦截的数据中会收到一个 0x42 数据包，此包的发送必然会触发 ws2_32.dll 的 send 函数，在这里断点等待触发。**
```
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
**Win32 ws2_32.dll send 函数原型**


```c
int send(
  SOCKET     s,
  const char *buf,
  int        len,
  int        flags
);
```

### 参数说明

| 参数 | 类型 | 描述 | 必需 |
|------|------|------|------|
| `s` | `SOCKET` | 已连接的套接字描述符 | 是 |
| `buf` | `const char*` | 待发送数据的缓冲区指针 | 是 |
| `len` | `int` | 缓冲区数据的字节长度 | 是 |
| `flags` | `int` | 控制发送行为的标志位 | 否（默认0） |

### 返回值

-**成功**：返回实际发送的字节数（可能小于 `len`）
-**失败**：返回 `SOCKET_ERROR`（-1）
  - 使用 `WSAGetLastError()` 获取详细错误代码


**断点后查看调用栈的第二个参数，查看内存数据的数据包是不是以****42F7 开始，如果不是直接 F9 跳过，或者直接看调用栈有没有 "Maps\\Download\\DotA" 这样的字样。**


## 数据发送
-**地址**：`6F6DF040`
-**偏移**：`game.dll + 6DF040`

```
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
-**0019F654**就是缓冲区指针
-**0000000D**就是数据大小

```
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
-**42F7**是协议头
-**000D**是数据大小

```
0019F654  000D42F7
0019F658  00000001
0019F65C  00000001
```

**继续栈追踪**
**6F6DAE20**是一个包装函数，暂时不管它，继续向上追踪。

```
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

## 发包逻辑
-**地址**：`6F678A70`
-**偏移**：`game.dll + 678A70`
-**关键发现**：注意观察反汇编中是不是有 `F7` 和 `42` 这样的字符，很显然这是发送 `C>S 0x42 W3GS_MAPSIZE` 的逻辑。
-**参考文档**：[参考文档](https://bnetdocs.org/packet/467/w3gs-mapsize)
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
6F678AB7  | 68 F7000000             | push F7                             |
6F678ABC  | 8D4C24 1C               | lea ecx,dword ptr ss:[esp+1C]       |
6F678AC0  | C78424 F4050000 0000000 | mov dword ptr ss:[esp+5F4],0        |
6F678ACB  | E8 9096E4FF             | call game.6F4C2160                  |
6F678AD0  | 6A 42                   | push 42                             |
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
6F678B24  | E8 F7220600             | call game.6F6DAE20                  | 发送 0x42 包
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
6F657A7B  | E8 80F3FFFF             | call game.6F656E00                  |
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
















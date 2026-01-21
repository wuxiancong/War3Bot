# WarCraft III 创建者模式和加入者模式反汇编分析

本文档对比分析了游戏在**创建主机（Host）**与**加入游戏（Guest）**时调用的关键函数。

## 1. 创建游戏 (Host Mode)

创建游戏的过程主要包含三个阶段：
1.  **预检查与入口**：检查游戏名，准备环境。
2.  **主逻辑 (CreateGame)**：创建核心对象，注册 `NetRouter`（这是 Host 独有的）。
3.  **NetClient 初始化**：以 Host 模式初始化客户端网络对象。

### a. 预检查入口函数 (`6F650DF0`)

此函数负责检查参数有效性（如游戏名），并跳转到实际的处理逻辑。
- **入口偏移**: `650DF0`
- **内存地址**: `game.dll+650DF0` (基址 6F000000)

```assembly
6F650DF0  | 56                      | push esi                                        |
6F650DF1  | 8BF1                    | mov esi,ecx                                     |
6F650DF3  | 803E 00                 | cmp byte ptr ds:[esi],0                         | <--- 检查游戏名是否为空
6F650DF6  | 57                      | push edi                                        |
6F650DF7  | 8BFA                    | mov edi,edx                                     |
6F650DF9  | 75 05                   | jne game.6F650E00                               |
6F650DFB  | 5F                      | pop edi                                         |
6F650DFC  | 33C0                    | xor eax,eax                                     |
6F650DFE  | 5E                      | pop esi                                         |
6F650DFF  | C3                      | ret                                             |
6F650E00  | 8D46 30                 | lea eax,dword ptr ds:[esi+30]                   |
6F650E03  | 50                      | push eax                                        |
6F650E04  | E8 5DA80900             | call <JMP.&Ordinal#506>                         |
6F650E09  | 83C0 01                 | add eax,1                                       |
6F650E0C  | 83F8 78                 | cmp eax,78                                      |
6F650E0F  | 77 EA                   | ja game.6F650DFB                                |
6F650E11  | B9 0E000000             | mov ecx,E                                       |
6F650E16  | E8 B526E7FF             | call game.6F4C34D0                              |
6F650E1B  | 8BC8                    | mov ecx,eax                                     |
6F650E1D  | 85C9                    | test ecx,ecx                                    |
6F650E1F  | B8 05000000             | mov eax,5                                       |
6F650E24  | 74 0B                   | je game.6F650E31                                |
6F650E26  | 8B11                    | mov edx,dword ptr ds:[ecx]                      |
6F650E28  | 8B42 64                 | mov eax,dword ptr ds:[edx+64]                   |
6F650E2B  | 6A 01                   | push 1                                          |
6F650E2D  | 57                      | push edi                                        |
6F650E2E  | 56                      | push esi                                        |
6F650E2F  | FFD0                    | call eax                                        | <--- 调用**创建者模式**函数 (6F65EC70) [调用 trampoline]
6F650E31  | 5F                      | pop edi                                         |
6F650E32  | 5E                      | pop esi                                         |
6F650E33  | C3                      | ret                                             |
```

**Trampoline (跳转中继)**

```assembly
6F65DFD0  | C74424 0C 00000000      | mov dword ptr ss:[esp+C],0                      | <--- 清理栈参数
6F65DFD8  | E9 C390FFFF             | jmp game.6F6570A0                               | <--- 跳转至主逻辑
```

### b. 创建游戏主逻辑 (`6F6570A0`)

这是创建游戏的核心函数。它负责申请 `NetProvider`，并关键性地调用 `NetRouter` 的注册函数。
- **入口偏移**: `6570A0`
- **内存地址**: `game.dll+6570A0` (基址 6F000000)

```assembly
6F6570A0  | 83EC 20                 | sub esp,20                                      | <--- 创建者模式
6F6570A3  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]                 | 
6F6570A8  | 33C4                    | xor eax,esp                                     |
6F6570AA  | 894424 1C               | mov dword ptr ss:[esp+1C],eax                   |
6F6570AE  | 53                      | push ebx                                        |
6F6570AF  | 8B5C24 2C               | mov ebx,dword ptr ss:[esp+2C]                   |
6F6570B3  | 56                      | push esi                                        | 
6F6570B4  | 57                      | push edi                                        | 
6F6570B5  | 8B7C24 30               | mov edi,dword ptr ss:[esp+30]                   |
6F6570B9  | 6A 00                   | push 0                                          |
6F6570BB  | 8BF1                    | mov esi,ecx                                     | 
6F6570BD  | 8B06                    | mov eax,dword ptr ds:[esi]                      | 
6F6570BF  | 8B90 90000000           | mov edx,dword ptr ds:[eax+90]                   | 
6F6570C5  | 6A 00                   | push 0                                          |
6F6570C7  | 6A 03                   | push 3                                          |
6F6570C9  | FFD2                    | call edx                                        | <--- 检查 NetProviderBNET 对象是否申请
6F6570CB  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]                   |
6F6570CF  | E8 AC95FFFF             | call game.6F650680                              |
6F6570D4  | 8B06                    | mov eax,dword ptr ds:[esi]                      | 
6F6570D6  | 8B50 30                 | mov edx,dword ptr ds:[eax+30]                   | 
6F6570D9  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]                   |
6F6570DD  | 51                      | push ecx                                        |
6F6570DE  | 8BCE                    | mov ecx,esi                                     | 
6F6570E0  | FFD2                    | call edx                                        | 
6F6570E2  | 0FB796 C0020000         | movzx edx,word ptr ds:[esi+2C0]                 | 
6F6570E9  | 894424 10               | mov dword ptr ss:[esp+10],eax                   |
6F6570ED  | 8D42 01                 | lea eax,dword ptr ds:[edx+1]                    | 
6F6570F0  | 50                      | push eax                                        | <--- 端口 6112
6F6570F1  | 8BCE                    | mov ecx,esi                                     |
6F6570F3  | C74424 10 00000000      | mov dword ptr ss:[esp+10],0                     |
6F6570FB  | C74424 18 00000000      | mov dword ptr ss:[esp+18],0                     |
6F657103  | E8 58B90100             | call game.6F672A60                              | <--- 注册 NetRouter 回调
6F657108  | 0FB7C0                  | movzx eax,ax                                    | 
6F65710B  | 66:85C0                 | test ax,ax                                      |
6F65710E  | 74 57                   | je game.6F657167                                |
6F657110  | 8B16                    | mov edx,dword ptr ds:[esi]                      | 
6F657112  | 8B52 34                 | mov edx,dword ptr ds:[edx+34]                   | 
6F657115  | 50                      | push eax                                        | 
6F657116  | 8D4424 1C               | lea eax,dword ptr ss:[esp+1C]                   |
6F65711A  | 8BC8                    | mov ecx,eax                                     | 
6F65711C  | 50                      | push eax                                        | 
6F65711D  | 51                      | push ecx                                        |
6F65711E  | 8BCE                    | mov ecx,esi                                     | 
6F657120  | FFD2                    | call edx                                        | 
6F657122  | 8B5424 38               | mov edx,dword ptr ss:[esp+38]                   | 
6F657126  | 8D4424 14               | lea eax,dword ptr ss:[esp+14]                   | 
6F65712A  | 50                      | push eax                                        | 
6F65712B  | 8B46 28                 | mov eax,dword ptr ds:[esi+28]                   | <--- 游戏版本
6F65712E  | 8D4C24 10               | lea ecx,dword ptr ss:[esp+10]                   |
6F657132  | 51                      | push ecx                                        |
6F657133  | 8B4E 24                 | mov ecx,dword ptr ds:[esi+24]                   |
6F657136  | 52                      | push edx                                        |
6F657137  | 6A 00                   | push 0                                          |
6F657139  | 50                      | push eax                                        |
6F65713A  | 51                      | push ecx                                        |
6F65713B  | 8D5424 28               | lea edx,dword ptr ss:[esp+28]                   |
6F65713F  | 52                      | push edx                                        |
6F657140  | 8BD7                    | mov edx,edi                                     |
6F657142  | 8BCE                    | mov ecx,esi                                     |
6F657144  | E8 079C0100             | call game.6F670D50                              |
6F657149  | 837C24 0C 00            | cmp dword ptr ss:[esp+C],0                      |
6F65714E  | 75 0D                   | jne game.6F65715D                               |
6F657150  | 6A 01                   | push 1                                          |
6F657152  | 33D2                    | xor edx,edx                                     |
6F657154  | 8BCE                    | mov ecx,esi                                     |
6F657156  | E8 65A10100             | call game.6F6712C0                              |
6F65715B  | EB 0A                   | jmp game.6F657167                               |
6F65715D  | C786 B8020000 01000000  | mov dword ptr ds:[esi+2B8],1                    |
6F657167  | 8B8E B4020000           | mov ecx,dword ptr ds:[esi+2B4]                  |
6F65716D  | 8B01                    | mov eax,dword ptr ds:[ecx]                      |
6F65716F  | 8B40 08                 | mov eax,dword ptr ds:[eax+8]                    |
6F657172  | 53                      | push ebx                                        |
6F657173  | 57                      | push edi                                        |
6F657174  | 8D5424 18               | lea edx,dword ptr ss:[esp+18]                   |
6F657178  | 52                      | push edx                                        |
6F657179  | 8B5424 20               | mov edx,dword ptr ss:[esp+20]                   |
6F65717D  | 52                      | push edx                                        |
6F65717E  | 8B5424 1C               | mov edx,dword ptr ss:[esp+1C]                   |
6F657182  | 52                      | push edx                                        |
6F657183  | 8D5424 2C               | lea edx,dword ptr ss:[esp+2C]                   |
6F657187  | 52                      | push edx                                        |
6F657188  | 56                      | push esi                                        |
6F657189  | FFD0                    | call eax                                        | <--- 创建 NetClient (Host模式) eax=game.6F6835C0
6F65718B  | 8B4C24 28               | mov ecx,dword ptr ss:[esp+28]                   |
6F65718F  | 5F                      | pop edi                                         |
6F657190  | 5E                      | pop esi                                         |
6F657191  | 5B                      | pop ebx                                         |
6F657192  | 33CC                    | xor ecx,esp                                     |
6F657194  | B8 01000000             | mov eax,1                                       | 
6F657199  | E8 BB9E1800             | call game.6F7E1059                              |
6F65719E  | 83C4 20                 | add esp,20                                      |
6F6571A1  | C2 0C00                 | ret C                                           |
```

### c. NetRouter 注册 (`6F672A60`) - Host 独有

此函数仅在创建者模式下调用，负责绑定端口（如 6112）并注册路由对象。
- **入口偏移**: `672A60`
- **内存地址**: `game.dll+672A60` (基址 6F000000)

```assembly
6F672A60  | 56                      | push esi                                        | <--- 注册 NetRouter 对象
6F672A61  | 8BF1                    | mov esi,ecx                                     | <--- NetProviderBNET 对象
6F672A63  | 57                      | push edi                                        | <--- 游戏名
6F672A64  | B9 E0FEAC6F             | mov ecx,game.6FACFEE0                           | <--- 常量：-1
6F672A69  | 8BFA                    | mov edi,edx                                     | <--- 端口
6F672A6B  | E8 30590600             | call game.6F6D83A0                              | <--- 进入临界区
6F672A70  | A1 88FEAC6F             | mov eax,dword ptr ds:[6FACFE88]                 | <--- 全局变量：其值为0
6F672A75  | 8BC8                    | mov ecx,eax                                     | <--- eax=0
6F672A77  | 83C0 01                 | add eax,1                                       |
6F672A7A  | 85C9                    | test ecx,ecx                                    |
6F672A7C  | A3 88FEAC6F             | mov dword ptr ds:[6FACFE88],eax                 | <--- 给全局变量赋值1
6F672A81  | 75 4A                   | jne game.6F672ACD                               |
6F672A83  | 8B16                    | mov edx,dword ptr ds:[esi]                      | <--- 获取虚函数地址
6F672A85  | 8B42 28                 | mov eax,dword ptr ds:[edx+28]                   |
6F672A88  | 8BCE                    | mov ecx,esi                                     | <--- NetProviderBNET 对象
6F672A8A  | FFD0                    | call eax                                        |
6F672A8C  | 66:85FF                 | test di,di                                      | <--- 端口
6F672A8F  | A3 84FEAC6F             | mov dword ptr ds:[6FACFE84],eax                 | <--- 把 延迟值或者帧率 传递给全局变量
6F672A98  | 75 05                   | jne game.6F672A9F                               |
6F672A9A  | 66:85C0                 | test ax,ax                                      |
6F672A9D  | 74 2E                   | je game.6F672ACD                                |
6F672A9F  | 8B16                    | mov edx,dword ptr ds:[esi]                      |
6F672AA1  | 68 2027676F             | push game.6F672720                              | <--- 注册 NetRouter 回调
6F672AA6  | 50                      | push eax                                        | <--- 把端口作为参数传入
6F672AA7  | 8B42 3C                 | mov eax,dword ptr ds:[edx+3C]                   | <--- 处理端口端序
6F672AAA  | 57                      | push edi                                        | <--- 返回端口
6F672AAB  | 8BCE                    | mov ecx,esi                                     |
6F672AAD  | FFD0                    | call eax                                        |
6F672AAF  | 66:85C0                 | test ax,ax                                      |
6F672AB2  | 66:A3 80FEAC6F          | mov word ptr ds:[6FACFE80],ax                   | <--- 端口赋值给全局变量
6F672AB8  | 75 19                   | jne game.6F672AD3                               |
6F672ABA  | 832D 88FEAC6F 01        | sub dword ptr ds:[6FACFE88],1                   |
6F672AC1  | C705 84FEAC6F 00000000  | mov dword ptr ds:[6FACFE84],0                   |
6F672ACB  | EB 06                   | jmp game.6F672AD3                               |
6F672ACD  | 66:A1 80FEAC6F          | mov ax,word ptr ds:[6FACFE80]                   |
6F672AD3  | B9 E0FEAC6F             | mov ecx,game.6FACFEE0                           |
6F672AD8  | 0FB7F0                  | movzx esi,ax                                    |
6F672ADB  | E8 D0580600             | call game.6F6D83B0                              | <--- 离开临界区
6F672AE0  | 5F                      | pop edi                                         |
6F672AE1  | 66:8BC6                 | mov ax,si                                       |
6F672AE4  | 5E                      | pop esi                                         |
6F672AE5  | C2 0400                 | ret 4                                           |
```

### d. NetClient 初始化 (Host 模式) (`6F6835C0`)

Host 模式下的客户端初始化，使用了特定的虚表 (`6F844E01`) 和标志位。
- **入口偏移**: `6835C0`
- **内存地址**: `game.dll+6835C0` (基址 6F000000)

```assembly
6F6835C0  | 6A FF                   | push FFFFFFFF                                   | <--- 创建 NetClient (Host模式)
6F6835C2  | 68 014E846F             | push game.6F844E01                              | <--- Host 专用虚表/标识
6F6835C7  | 64:A1 00000000          | mov eax,dword ptr fs:[0]                        |
6F6835CD  | 50                      | push eax                                        |
6F6835CE  | 51                      | push ecx                                        |
6F6835CF  | 53                      | push ebx                                        |
6F6835D0  | 55                      | push ebp                                        |
6F6835D1  | 56                      | push esi                                        |
6F6835D2  | 57                      | push edi                                        |
6F6835D3  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]                 |
6F6835D8  | 33C4                    | xor eax,esp                                     |
6F6835DA  | 50                      | push eax                                        |
6F6835DB  | 8D4424 18               | lea eax,dword ptr ss:[esp+18]                   |
6F6835DF  | 64:A3 00000000          | mov dword ptr fs:[0],eax                        |
6F6835E5  | 894C24 14               | mov dword ptr ss:[esp+14],ecx                   |
6F6835E9  | 8B7C24 30               | mov edi,dword ptr ss:[esp+30]                   |
6F6835ED  | 33DB                    | xor ebx,ebx                                     |
6F6835EF  | 3BFB                    | cmp edi,ebx                                     |
6F6835F1  | 75 1F                   | jne game.6F683612                               |
6F6835F3  | 8B41 04                 | mov eax,dword ptr ds:[ecx+4]                    |
6F6835F6  | 8B4C24 3C               | mov ecx,dword ptr ss:[esp+3C]                   |
6F6835FA  | 8B5424 40               | mov edx,dword ptr ss:[esp+40]                   |
6F6835FE  | 50                      | push eax                                        |
6F6835FF  | 51                      | push ecx                                        |
6F683600  | 8B4C24 30               | mov ecx,dword ptr ss:[esp+30]                   |
6F683604  | 52                      | push edx                                        |
6F683605  | 53                      | push ebx                                        |
6F683606  | 6A 07                   | push 7                                          |
6F683608  | E8 B381FDFF             | call game.6F65B7C0                              |
6F68360D  | E9 D8010000             | jmp game.6F6837EA                               |
6F683612  | 6A 08                   | push 8                                          |
6F683614  | 68 B2000000             | push B2                                         |
6F683619  | 68 3C17976F             | push game.6F97173C                              |
6F68361E  | 68 E0000000             | push E0                                         |
6F683623  | E8 8A7F0600             | call <JMP.&Ordinal#401>                         |
6F683628  | 3BC3                    | cmp eax,ebx                                     |
6F68362A  | 74 0B                   | je game.6F683637                                |
6F68362C  | 8BC8                    | mov ecx,eax                                     |
6F68362E  | E8 BDBEFEFF             | call game.6F66F4F0                              |
6F683633  | 8BE8                    | mov ebp,eax                                     |
6F683635  | EB 02                   | jmp game.6F683639                               |
6F683637  | 33ED                    | xor ebp,ebp                                     |
6F683639  | 8D4D 10                 | lea ecx,dword ptr ss:[ebp+10]                   |
6F68363C  | E8 1F0A0400             | call game.6F6C4060                              |
6F683641  | E8 BA170400             | call <JMP.&GetTickCount>                        |
6F683646  | 8B7424 40               | mov esi,dword ptr ss:[esp+40]                   |
6F68364A  | 8945 50                 | mov dword ptr ss:[ebp+50],eax                   |
6F68364D  | C745 44 01000000        | mov dword ptr ss:[ebp+44],1                     |
6F683654  | 8B06                    | mov eax,dword ptr ds:[esi]                      |
6F683656  | 8985 B5000000           | mov dword ptr ss:[ebp+B5],eax                   |
6F68365C  | 8B4E 04                 | mov ecx,dword ptr ds:[esi+4]                    |
6F68365F  | 898D B9000000           | mov dword ptr ss:[ebp+B9],ecx                   |
6F683665  | 8B56 08                 | mov edx,dword ptr ds:[esi+8]                    |
6F683668  | 8995 BD000000           | mov dword ptr ss:[ebp+BD],edx                   |
6F68366E  | 8B46 0C                 | mov eax,dword ptr ds:[esi+C]                    |
6F683671  | 8985 C1000000           | mov dword ptr ss:[ebp+C1],eax                   |
6F683677  | 8A46 10                 | mov al,byte ptr ds:[esi+10]                     |
6F68367A  | 0FB6C8                  | movzx ecx,al                                    |
6F68367D  | 51                      | push ecx                                        |
6F68367E  | 8D56 11                 | lea edx,dword ptr ds:[esi+11]                   |
6F683681  | 8885 C5000000           | mov byte ptr ss:[ebp+C5],al                     |
6F683687  | 52                      | push edx                                        |
6F683688  | 8D85 C6000000           | lea eax,dword ptr ss:[ebp+C6]                   |
6F68368E  | 50                      | push eax                                        |
6F68368F  | E8 58DF1500             | call <JMP.&memcpy>                              |
6F683694  | 83C4 0C                 | add esp,C                                       |
6F683697  | 53                      | push ebx                                        |
6F683698  | 68 BA000000             | push BA                                         |
6F68369D  | 68 3C17976F             | push game.6F97173C                              |
6F6836A2  | 6A 29                   | push 29                                         |
6F6836A4  | C645 5D FF              | mov byte ptr ss:[ebp+5D],FF                     |
6F6836A8  | E8 057F0600             | call <JMP.&Ordinal#401>                         |
6F6836AD  | 3BC3                    | cmp eax,ebx                                     |
6F6836AF  | 74 0C                   | je game.6F6836BD                                |
6F6836B1  | 8858 10                 | mov byte ptr ds:[eax+10],bl                     |
6F6836B4  | 8858 20                 | mov byte ptr ds:[eax+20],bl                     |
6F6836B7  | 894424 40               | mov dword ptr ss:[esp+40],eax                   |
6F6836BB  | EB 06                   | jmp game.6F6836C3                               |
6F6836BD  | 895C24 40               | mov dword ptr ss:[esp+40],ebx                   |
6F6836C1  | 8BC3                    | mov eax,ebx                                     |
6F6836C3  | 8B4C24 34               | mov ecx,dword ptr ss:[esp+34]                   |
6F6836C7  | 8B5424 38               | mov edx,dword ptr ss:[esp+38]                   |
6F6836CB  | 8938                    | mov dword ptr ds:[eax],edi                      |
6F6836CD  | 8948 04                 | mov dword ptr ds:[eax+4],ecx                    |
6F6836D0  | 8B0A                    | mov ecx,dword ptr ds:[edx]                      |
6F6836D2  | 8948 08                 | mov dword ptr ds:[eax+8],ecx                    |
6F6836D5  | 8B16                    | mov edx,dword ptr ds:[esi]                      |
6F6836D7  | 8950 10                 | mov dword ptr ds:[eax+10],edx                   |
6F6836DA  | 8B4E 04                 | mov ecx,dword ptr ds:[esi+4]                    |
6F6836DD  | 8948 14                 | mov dword ptr ds:[eax+14],ecx                   |
6F6836E0  | 8B56 08                 | mov edx,dword ptr ds:[esi+8]                    |
6F6836E3  | 8950 18                 | mov dword ptr ds:[eax+18],edx                   |
6F6836E6  | 8B4E 0C                 | mov ecx,dword ptr ds:[esi+C]                    |
6F6836E9  | 8948 1C                 | mov dword ptr ds:[eax+1C],ecx                   |
6F6836EC  | 8A4E 10                 | mov cl,byte ptr ds:[esi+10]                     |
6F6836EF  | 0FB6D1                  | movzx edx,cl                                    |
6F6836F2  | 52                      | push edx                                        |
6F6836F3  | 8848 20                 | mov byte ptr ds:[eax+20],cl                     |
6F6836F6  | 83C6 11                 | add esi,11                                      |
6F6836F9  | 83C0 21                 | add eax,21                                      |
6F6836FC  | 56                      | push esi                                        |
6F6836FD  | 50                      | push eax                                        |
6F6836FE  | E8 E9DE1500             | call <JMP.&memcpy>                              |
6F683703  | 83C4 0C                 | add esp,C                                       |
6F683706  | 6A 08                   | push 8                                          |
6F683708  | 68 C3000000             | push C3                                         |
6F68370D  | 68 3C17976F             | push game.6F97173C                              |
6F683712  | 68 90020000             | push 290                                        |
6F683717  | E8 967E0600             | call <JMP.&Ordinal#401>                         |
6F68371C  | 894424 30               | mov dword ptr ss:[esp+30],eax                   |
6F683720  | 894424 34               | mov dword ptr ss:[esp+34],eax                   |
6F683724  | 3BC3                    | cmp eax,ebx                                     |
6F683726  | 895C24 20               | mov dword ptr ss:[esp+20],ebx                   |
6F68372A  | 74 09                   | je game.6F683735                                |
6F68372C  | 8BC8                    | mov ecx,eax                                     |
6F68372E  | E8 FDA1FFFF             | call game.6F67D930                              |
6F683733  | 8BD8                    | mov ebx,eax                                     |
6F683735  | 8B7424 28               | mov esi,dword ptr ss:[esp+28]                   |
6F683739  | 8D4E 10                 | lea ecx,dword ptr ds:[esi+10]                   |
6F68373C  | C74424 20 FFFFFFFF      | mov dword ptr ss:[esp+20],FFFFFFFF              |
6F683744  | E8 17090400             | call game.6F6C4060                              |
6F683749  | 8973 58                 | mov dword ptr ds:[ebx+58],esi                   |
6F68374C  | 8B7424 3C               | mov esi,dword ptr ss:[esp+3C]                   |
6F683750  | 8D7B 5C                 | lea edi,dword ptr ds:[ebx+5C]                   |
6F683753  | B9 2E000000             | mov ecx,2E                                      |
6F683758  | F3:A5                   | rep movsd                                       |
6F68375A  | 8D8B 50010000           | lea ecx,dword ptr ds:[ebx+150]                  |
6F683760  | C783 34010000 02000000  | mov dword ptr ds:[ebx+134],2                    |
6F68376A  | C783 44020000 01000000  | mov dword ptr ds:[ebx+244],1                    |
6F683774  | 89AB 48010000           | mov dword ptr ds:[ebx+148],ebp                  |
6F68377A  | E8 01CFFCFF             | call game.6F650680                              |
6F68377F  | 8B4424 40               | mov eax,dword ptr ss:[esp+40]                   |
6F683783  | BA 01000000             | mov edx,1                                       |
6F683788  | 8BCB                    | mov ecx,ebx                                     |
6F68378A  | 8983 78020000           | mov dword ptr ds:[ebx+278],eax                  |
6F683790  | E8 CB71FFFF             | call game.6F67A960                              |
6F683795  | 53                      | push ebx                                        |
6F683796  | B9 70FFAC6F             | mov ecx,game.6FACFF70                           |
6F68379B  | E8 E062FFFF             | call game.6F679A80                              |
6F6837A0  | 8B7C24 14               | mov edi,dword ptr ss:[esp+14]                   |
6F6837A4  | 8945 40                 | mov dword ptr ss:[ebp+40],eax                   |
6F6837A7  | 55                      | push ebp                                        |
6F6837A8  | B9 ACFFAC6F             | mov ecx,game.6FACFFAC                           |
6F6837AD  | 8947 04                 | mov dword ptr ds:[edi+4],eax                    |
6F6837B0  | E8 6B66FFFF             | call game.6F679E20                              |
6F6837B5  | D9EE                    | fldz                                            |
6F6837B7  | 51                      | push ecx                                        |
6F6837B8  | D91C24                  | fstp dword ptr ss:[esp]                         |
6F6837BB  | 8B4F 04                 | mov ecx,dword ptr ds:[edi+4]                    |
6F6837BE  | 51                      | push ecx                                        |
6F6837BF  | BA B0EA676F             | mov edx,game.6F67EAB0                           |
6F6837C4  | B9 12000000             | mov ecx,12                                      |
6F6837C9  | 8BF0                    | mov esi,eax                                     |
6F6837CB  | E8 C07BFAFF             | call game.6F62B390                              |
6F6837D0  | 8B4C24 28               | mov ecx,dword ptr ss:[esp+28]                   |
6F6837D4  | 8B11                    | mov edx,dword ptr ds:[ecx]                      |
6F6837D6  | 8B4424 2C               | mov eax,dword ptr ss:[esp+2C]                   |
6F6837DA  | 8B52 38                 | mov edx,dword ptr ds:[edx+38]                   |
6F6837DD  | 6A 00                   | push 0                                          |
6F6837DF  | 6A 00                   | push 0                                          |
6F6837E1  | 56                      | push esi                                        |
6F6837E2  | 68 002E686F             | push game.6F682E00                              |
6F6837E7  | 50                      | push eax                                        |
6F6837E8  | FFD2                    | call edx                                        |
6F6837EA  | 8B4C24 18               | mov ecx,dword ptr ss:[esp+18]                   |
6F6837EE  | 64:890D 00000000        | mov dword ptr fs:[0],ecx                        |
6F6837F5  | 59                      | pop ecx                                         |
6F6837F6  | 5F                      | pop edi                                         |
6F6837F7  | 5E                      | pop esi                                         |
6F6837F8  | 5D                      | pop ebp                                         |
6F6837F9  | 5B                      | pop ebx                                         |
6F6837FA  | 83C4 10                 | add esp,10                                      |
6F6837FD  | C2 1C00                 | ret 1C                                          |
```

---

## 2. 加入游戏 (Guest Mode)

加入游戏的过程包含：
1.  **预检查**：获取 `NetProviderBNET`。
2.  **主逻辑 (JoinGame)**：连接 Battle.net 或 LAN，扫描端口，处理连接。
3.  **NetClient 初始化**：以 Guest 模式初始化客户端网络对象。

### a. 预检查与入口 (`6F650000`)
- **入口偏移**: `650000`
- **内存地址**: `game.dll+650000` (基址 6F000000)

```assembly
6F650000  | 56                      | push esi                                        |
6F650001  | 8BF1                    | mov esi,ecx                                     |
6F650003  | 85F6                    | test esi,esi                                    |
6F650005  | 57                      | push edi                                        |
6F650006  | 8BFA                    | mov edi,edx                                     |
6F650008  | 75 07                   | jne game.6F650011                               |
6F65000A  | 5F                      | pop edi                                         |
6F65000B  | 33C0                    | xor eax,eax                                     |
6F65000D  | 5E                      | pop esi                                         |
6F65000E  | C2 0400                 | ret 4                                           |
6F650011  | B9 0E000000             | mov ecx,E                                       |
6F650016  | E8 B534E7FF             | call game.6F4C34D0                              | <--- 获取全局 NetProviderBNET 对象
6F65001B  | 8BC8                    | mov ecx,eax                                     |
6F65001D  | 85C9                    | test ecx,ecx                                    |
6F65001F  | B8 05000000             | mov eax,5                                       |
6F650024  | 74 0E                   | je game.6F650034                                |
6F650026  | 8B5424 0C               | mov edx,dword ptr ss:[esp+C]                    |
6F65002A  | 8B01                    | mov eax,dword ptr ds:[ecx]                      |
6F65002C  | 8B40 6C                 | mov eax,dword ptr ds:[eax+6C]                   |
6F65002F  | 52                      | push edx                                        |
6F650030  | 57                      | push edi                                        |
6F650031  | 56                      | push esi                                        |
6F650032  | FFD0                    | call eax                                        | <--- 调用**加入者模式**函数 (6F65EC70)
6F650034  | 5F                      | pop edi                                         |
6F650035  | 5E                      | pop esi                                         |
6F650036  | C2 0400                 | ret 4                                           |
```

### b. 加入游戏主逻辑 (`6F65EC70`)

此函数没有 `NetRouter` 的创建过程，而是侧重于连接逻辑和端口检测。
- **入口偏移**: `6F65EC70`
- **内存地址**: `game.dll+65EC70` (基址 6F000000)

```assembly
6F65EC70  | 81EC D4000000           | sub esp,D4                                      | <--- 加入者模式
6F65EC76  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]                 |
6F65EC7B  | 33C4                    | xor eax,esp                                     |
6F65EC7D  | 898424 D0000000         | mov dword ptr ss:[esp+D0],eax                   |
6F65EC84  | 53                      | push ebx                                        |
6F65EC85  | 55                      | push ebp                                        |
6F65EC86  | 8BAC24 E8000000         | mov ebp,dword ptr ss:[esp+E8]                   |
6F65EC8D  | 56                      | push esi                                        |
6F65EC8E  | 57                      | push edi                                        |
6F65EC8F  | 8BBC24 EC000000         | mov edi,dword ptr ss:[esp+EC]                   |
6F65EC96  | 8BF1                    | mov esi,ecx                                     |
6F65EC98  | 8B06                    | mov eax,dword ptr ds:[esi]                      |
6F65EC9A  | 8B90 A4000000           | mov edx,dword ptr ds:[eax+A4]                   |
6F65ECA0  | 33DB                    | xor ebx,ebx                                     |
6F65ECA2  | 53                      | push ebx                                        |
6F65ECA3  | 6A 02                   | push 2                                          |
6F65ECA5  | FFD2                    | call edx                                        |
6F65ECA7  | 6A 10                   | push 10                                         |
6F65ECA9  | 57                      | push edi                                        |
6F65ECAA  | 8D4424 50               | lea eax,dword ptr ss:[esp+50]                   |
6F65ECAE  | 50                      | push eax                                        |
6F65ECAF  | 885C24 34               | mov byte ptr ss:[esp+34],bl                     |
6F65ECB3  | 885C24 54               | mov byte ptr ss:[esp+54],bl                     |
6F65ECB7  | 885C24 64               | mov byte ptr ss:[esp+64],bl                     |
6F65ECBB  | E8 04C90800             | call <JMP.&Ordinal#501>                         |
6F65ECC0  | 8D4C24 28               | lea ecx,dword ptr ss:[esp+28]                   |
6F65ECC4  | 51                      | push ecx                                        |
6F65ECC5  | 8D5424 1C               | lea edx,dword ptr ss:[esp+1C]                   | 
6F65ECC9  | 52                      | push edx                                        | 
6F65ECCA  | 8B9424 F0000000         | mov edx,dword ptr ss:[esp+F0]                   |
6F65ECD1  | 8D4424 18               | lea eax,dword ptr ss:[esp+18]                   | 
6F65ECD5  | 50                      | push eax                                        | 
6F65ECD6  | 8D4C24 20               | lea ecx,dword ptr ss:[esp+20]                   |
6F65ECDA  | 51                      | push ecx                                        |
6F65ECDB  | 53                      | push ebx                                        | 
6F65ECDC  | 52                      | push edx                                        | 
6F65ECDD  | 8BCE                    | mov ecx,esi                                     |
6F65ECDF  | E8 9CE0FFFF             | call game.6F65CD80                              |
6F65ECE4  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]                   | 
6F65ECE8  | 3BC3                    | cmp eax,ebx                                     | 
6F65ECEA  | 53                      | push ebx                                        | 
6F65ECEB  | 74 2A                   | je game.6F65ED17                                |
6F65ECED  | 0FB79E C0020000         | movzx ebx,word ptr ds:[esi+2C0]                 | 
6F65ECF4  | 8B8E B4020000           | mov ecx,dword ptr ds:[esi+2B4]                  | 
6F65ECFA  | 8B11                    | mov edx,dword ptr ds:[ecx]                      | 
6F65ECFC  | 8B52 0C                 | mov edx,dword ptr ds:[edx+C]                    | 
6F65ECFF  | 53                      | push ebx                                        | 
6F65ED00  | 57                      | push edi                                        | 
6F65ED01  | 55                      | push ebp                                        | 
6F65ED02  | 8D7C24 38               | lea edi,dword ptr ss:[esp+38]                   | 
6F65ED06  | 57                      | push edi                                        | 
6F65ED07  | 8B7C24 24               | mov edi,dword ptr ss:[esp+24]                   | 
6F65ED0B  | 57                      | push edi                                        | 
6F65ED0C  | 50                      | push eax                                        | 
6F65ED0D  | 8D4424 34               | lea eax,dword ptr ss:[esp+34]                   | 
6F65ED11  | 50                      | push eax                                        | 
6F65ED12  | 56                      | push esi                                        |
6F65ED13  | FFD2                    | call edx                                        |
6F65ED15  | EB 15                   | jmp game.6F65ED2C                               |
6F65ED17  | 8B06                    | mov eax,dword ptr ds:[esi]                      |
6F65ED19  | 8B90 A0000000           | mov edx,dword ptr ds:[eax+A0]                   |
6F65ED1F  | 8D4C24 2C               | lea ecx,dword ptr ss:[esp+2C]                   |
6F65ED23  | 51                      | push ecx                                        |
6F65ED24  | 55                      | push ebp                                        |
6F65ED25  | 53                      | push ebx                                        |
6F65ED26  | 6A 07                   | push 7                                          |
6F65ED28  | 8BCE                    | mov ecx,esi                                     |
6F65ED2A  | FFD2                    | call edx                                        | <--- 创建 NetClient (Guest模式)** edx=game.6F683800
6F65ED2C  | 8B8C24 E0000000         | mov ecx,dword ptr ss:[esp+E0]                   |
6F65ED33  | 5F                      | pop edi                                         |
6F65ED34  | 5E                      | pop esi                                         |
6F65ED35  | 5D                      | pop ebp                                         |
6F65ED36  | 5B                      | pop ebx                                         
6F65ED37  | 33CC                    | xor ecx,esp                                     |
6F65ED39  | B8 01000000             | mov eax,1                                       |
6F65ED3E  | E8 16231800             | call game.6F7E1059                              |
6F65ED43  | 81C4 D4000000           | add esp,D4                                      |
6F65ED49  | C2 0C00                 | ret C                                           |
```

### c. NetClient 初始化 (Guest 模式) (`6F683800`)

Guest 模式下初始化，使用不同的虚表 (`6F844E31`)，并且包含端口扫描（6112, 6113）的逻辑。
- **入口偏移**: `683800`
- **内存地址**: `game.dll+683800` (基址 6F000000)

```assembly
6F683800  | 6A FF                   | push FFFFFFFF                                   | <--- 创建 NetClient (Guest模式)
6F683802  | 68 314E846F             | push game.6F844E31                              | <--- Guest 专用虚表/标识
6F683807  | 64:A1 00000000          | mov eax,dword ptr fs:[0]                        |
6F68380D  | 50                      | push eax                                        |
6F68380E  | 51                      | push ecx                                        |
6F68380F  | 53                      | push ebx                                        | <--- 端口 6112
6F683810  | 55                      | push ebp                                        | <--- 房主名称
6F683811  | 56                      | push esi                                        | <--- NetProviderBNET 对象
6F683812  | 57                      | push edi                                        |
6F683813  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]                 |
6F683818  | 33C4                    | xor eax,esp                                     |
6F68381A  | 50                      | push eax                                        |
6F68381B  | 8D4424 18               | lea eax,dword ptr ss:[esp+18]                   |
6F68381F  | 64:A3 00000000          | mov dword ptr fs:[0],eax                        |
6F683825  | 8BF1                    | mov esi,ecx                                     |
6F683827  | 897424 14               | mov dword ptr ss:[esp+14],esi                   |
6F68382B  | 8B6C24 30               | mov ebp,dword ptr ss:[esp+30]                   |
6F68382F  | 85ED                    | test ebp,ebp                                    |
6F683831  | 75 22                   | jne game.6F683855                               |
6F683833  | 8B56 04                 | mov edx,dword ptr ds:[esi+4]                    |
6F683836  | 8B4C24 28               | mov ecx,dword ptr ss:[esp+28]                   |
6F68383A  | 8B01                    | mov eax,dword ptr ds:[ecx]                      |
6F68383C  | 8B80 A0000000           | mov eax,dword ptr ds:[eax+A0]                   |
6F683842  | 52                      | push edx                                        |
6F683843  | 8B5424 3C               | mov edx,dword ptr ss:[esp+3C]                   |
6F683847  | 52                      | push edx                                        |
6F683848  | 8B5424 44               | mov edx,dword ptr ss:[esp+44]                   |
6F68384C  | 52                      | push edx                                        |
6F68384D  | 55                      | push ebp                                        |
6F68384E  | 6A 07                   | push 7                                          |
6F683850  | E9 76020000             | jmp game.6F683ACB                               |
6F683855  | 8B4424 44               | mov eax,dword ptr ss:[esp+44]                   |
6F683859  | 66:85C0                 | test ax,ax                                      |
6F68385C  | 8B7C24 28               | mov edi,dword ptr ss:[esp+28]                   |
6F683860  | 74 3B                   | je game.6F68389D                                |
6F683862  | 8B17                    | mov edx,dword ptr ds:[edi]                      |
6F683864  | 8B52 3C                 | mov edx,dword ptr ds:[edx+3C]                   |
6F683867  | 68 1015686F             | push game.6F681510                              |
6F68386C  | 8D48 01                 | lea ecx,dword ptr ds:[eax+1]                    |
6F68386F  | 51                      | push ecx                                        |
6F683870  | 50                      | push eax                                        |
6F683871  | 8BCF                    | mov ecx,edi                                     |
6F683873  | FFD2                    | call edx                                        |
6F683875  | 66:85C0                 | test ax,ax                                      |
6F683878  | 75 23                   | jne game.6F68389D                               |
6F68387A  | 8B4E 04                 | mov ecx,dword ptr ds:[esi+4]                    |
6F68387D  | 8B5424 38               | mov edx,dword ptr ss:[esp+38]                   |
6F683881  | 8B07                    | mov eax,dword ptr ds:[edi]                      |
6F683883  | 51                      | push ecx                                        |
6F683884  | 8B4C24 40               | mov ecx,dword ptr ss:[esp+40]                   |
6F683888  | 52                      | push edx                                        |
6F683889  | 8B90 A0000000           | mov edx,dword ptr ds:[eax+A0]                   |
6F68388F  | 51                      | push ecx                                        |
6F683890  | 6A 00                   | push 0                                          |
6F683892  | 6A 0D                   | push D                                          |
6F683894  | 8BCF                    | mov ecx,edi                                     |
6F683896  | FFD2                    | call edx                                        |
6F683898  | E9 30020000             | jmp game.6F683ACD                               |
6F68389D  | 6A 08                   | push 8                                          |
6F68389F  | 68 0D010000             | push 10D                                        |
6F6838A4  | 68 3C17976F             | push game.6F97173C                              |
6F6838A9  | 68 E0000000             | push E0                                         |
6F6838AE  | E8 FF7C0600             | call <JMP.&Ordinal#401>                         |
6F6838B3  | 85C0                    | test eax,eax                                    | <--- NetClient 对象
6F6838B5  | 74 0F                   | je game.6F6838C6                                |
6F6838B7  | 8BC8                    | mov ecx,eax                                     |
6F6838B9  | E8 32BCFEFF             | call game.6F66F4F0                              | <--- 初始化
6F6838BE  | 8BD8                    | mov ebx,eax                                     |
6F6838C0  | 895C24 30               | mov dword ptr ss:[esp+30],ebx                   |
6F6838C4  | EB 0C                   | jmp game.6F6838D2                               |
6F6838C6  | C74424 30 00000000      | mov dword ptr ss:[esp+30],0                     |
6F6838CE  | 8B5C24 30               | mov ebx,dword ptr ss:[esp+30]                   |
6F6838D2  | 8D4B 10                 | lea ecx,dword ptr ds:[ebx+10]                   |
6F6838D5  | E8 86070400             | call game.6F6C4060                              |
6F6838DA  | E8 21150400             | call <JMP.&GetTickCount>                        |
6F6838DF  | 8B7424 3C               | mov esi,dword ptr ss:[esp+3C]                   |
6F6838E3  | 8943 50                 | mov dword ptr ds:[ebx+50],eax                   |
6F6838E6  | C743 44 01000000        | mov dword ptr ds:[ebx+44],1                     |
6F6838ED  | 89AB B0000000           | mov dword ptr ds:[ebx+B0],ebp                   |
6F6838F3  | 8B06                    | mov eax,dword ptr ds:[esi]                      |
6F6838F5  | 8983 B5000000           | mov dword ptr ds:[ebx+B5],eax                   |
6F6838FB  | 8B4E 04                 | mov ecx,dword ptr ds:[esi+4]                    |
6F6838FE  | 898B B9000000           | mov dword ptr ds:[ebx+B9],ecx                   |
6F683904  | 8B56 08                 | mov edx,dword ptr ds:[esi+8]                    |
6F683907  | 8993 BD000000           | mov dword ptr ds:[ebx+BD],edx                   |
6F68390D  | 8B46 0C                 | mov eax,dword ptr ds:[esi+C]                    |
6F683910  | 8983 C1000000           | mov dword ptr ds:[ebx+C1],eax                   |
6F683916  | 8A46 10                 | mov al,byte ptr ds:[esi+10]                     |
6F683919  | 0FB6C8                  | movzx ecx,al                                    |
6F68391C  | 51                      | push ecx                                        |
6F68391D  | 8D56 11                 | lea edx,dword ptr ds:[esi+11]                   |
6F683920  | 8883 C5000000           | mov byte ptr ds:[ebx+C5],al                     |
6F683926  | 52                      | push edx                                        |
6F683927  | 8D83 C6000000           | lea eax,dword ptr ds:[ebx+C6]                   |
6F68392D  | 50                      | push eax                                        |
6F68392E  | E8 B9DC1500             | call <JMP.&memcpy>                              |
6F683933  | 83C4 0C                 | add esp,C                                       |
6F683936  | 837C24 48 00            | cmp dword ptr ss:[esp+48],0                     |
6F68393B  | C643 5D FF              | mov byte ptr ds:[ebx+5D],FF                     |
6F68393F  | 74 0A                   | je game.6F68394B                                |
6F683941  | 818B 80000000 00001000  | or dword ptr ds:[ebx+80],100000                 |
6F68394B  | 6A 00                   | push 0                                          |
6F68394D  | 68 18010000             | push 118                                        |
6F683952  | 68 3C17976F             | push game.6F97173C                              |
6F683957  | 6A 47                   | push 47                                         |
6F683959  | E8 547C0600             | call <JMP.&Ordinal#401>                         |
6F68395E  | 85C0                    | test eax,eax                                    |
6F683960  | 74 10                   | je game.6F683972                                |
6F683962  | C640 08 00              | mov byte ptr ds:[eax+8],0                       |
6F683966  | C640 1E 00              | mov byte ptr ds:[eax+1E],0                      |
6F68396A  | C640 2E 00              | mov byte ptr ds:[eax+2E],0                      |
6F68396E  | 8BD8                    | mov ebx,eax                                     |
6F683970  | EB 02                   | jmp game.6F683974                               |
6F683972  | 33DB                    | xor ebx,ebx                                     |
6F683974  | 8B5424 40               | mov edx,dword ptr ss:[esp+40]                   |
6F683978  | 8B4C24 34               | mov ecx,dword ptr ss:[esp+34]                   |
6F68397C  | 6A 10                   | push 10                                         |
6F68397E  | 8D43 08                 | lea eax,dword ptr ds:[ebx+8]                    |
6F683981  | 52                      | push edx                                        |
6F683982  | 50                      | push eax                                        |
6F683983  | 892B                    | mov dword ptr ds:[ebx],ebp                      |
6F683985  | 894B 04                 | mov dword ptr ds:[ebx+4],ecx                    |
6F683988  | E8 377C0600             | call <JMP.&Ordinal#501>                         |
6F68398D  | 66:8B4424 44            | mov ax,word ptr ss:[esp+44]                     |
6F683992  | 66:8943 18              | mov word ptr ds:[ebx+18],ax                     |
6F683996  | 8B0E                    | mov ecx,dword ptr ds:[esi]                      |
6F683998  | 894B 1E                 | mov dword ptr ds:[ebx+1E],ecx                   |
6F68399B  | 8B56 04                 | mov edx,dword ptr ds:[esi+4]                    |
6F68399E  | 8953 22                 | mov dword ptr ds:[ebx+22],edx                   |
6F6839A1  | 8B46 08                 | mov eax,dword ptr ds:[esi+8]                    |
6F6839A4  | 8943 26                 | mov dword ptr ds:[ebx+26],eax                   |
6F6839A7  | 8B4E 0C                 | mov ecx,dword ptr ds:[esi+C]                    |
6F6839AA  | 894B 2A                 | mov dword ptr ds:[ebx+2A],ecx                   |
6F6839AD  | 8A46 10                 | mov al,byte ptr ds:[esi+10]                     |
6F6839B0  | 0FB6D0                  | movzx edx,al                                    |
6F6839B3  | 52                      | push edx                                        |
6F6839B4  | 8843 2E                 | mov byte ptr ds:[ebx+2E],al                     |
6F6839B7  | 83C6 11                 | add esi,11                                      |
6F6839BA  | 8D43 2F                 | lea eax,dword ptr ds:[ebx+2F]                   |
6F6839BD  | 56                      | push esi                                        |
6F6839BE  | 50                      | push eax                                        |
6F6839BF  | E8 28DC1500             | call <JMP.&memcpy>                              |
6F6839C4  | 83C4 0C                 | add esp,C                                       |
6F6839C7  | 6A 08                   | push 8                                          |
6F6839C9  | 68 22010000             | push 122                                        |
6F6839CE  | 68 3C17976F             | push game.6F97173C                              |
6F6839D3  | 68 90020000             | push 290                                        |
6F6839D8  | E8 D57B0600             | call <JMP.&Ordinal#401>                         |
6F6839DD  | 894424 3C               | mov dword ptr ss:[esp+3C],eax                   |
6F6839E1  | 894424 48               | mov dword ptr ss:[esp+48],eax                   |
6F6839E5  | 33ED                    | xor ebp,ebp                                     |
6F6839E7  | 3BC5                    | cmp eax,ebp                                     |
6F6839E9  | 896C24 20               | mov dword ptr ss:[esp+20],ebp                   |
6F6839ED  | 74 09                   | je game.6F6839F8                                |
6F6839EF  | 8BC8                    | mov ecx,eax                                     |
6F6839F1  | E8 3A9FFFFF             | call game.6F67D930                              |
6F6839F6  | 8BE8                    | mov ebp,eax                                     |
6F6839F8  | 8D4F 10                 | lea ecx,dword ptr ds:[edi+10]                   |
6F6839FB  | C74424 20 FFFFFFFF      | mov dword ptr ss:[esp+20],FFFFFFFF              |
6F683A03  | E8 58060400             | call game.6F6C4060                              |
6F683A08  | 8B7424 38               | mov esi,dword ptr ss:[esp+38]                   |
6F683A0C  | 66:8B5424 44            | mov dx,word ptr ss:[esp+44]                     |
6F683A11  | 897D 58                 | mov dword ptr ss:[ebp+58],edi                   |
6F683A14  | 8D7D 5C                 | lea edi,dword ptr ss:[ebp+5C]                   |
6F683A17  | B9 2E000000             | mov ecx,2E                                      |
6F683A1C  | F3:A5                   | rep movsd                                       |
6F683A1E  | 8B7424 2C               | mov esi,dword ptr ss:[esp+2C]                   |
6F683A22  | 8B4C24 34               | mov ecx,dword ptr ss:[esp+34]                   |
6F683A26  | 8B7C24 30               | mov edi,dword ptr ss:[esp+30]                   |
6F683A2A  | 898D 40010000           | mov dword ptr ss:[ebp+140],ecx                  |
6F683A30  | 66:8995 48020000        | mov word ptr ss:[ebp+248],dx                    |
6F683A37  | C785 34010000 02000000  | mov dword ptr ss:[ebp+134],2                    |
6F683A41  | 89BD 48010000           | mov dword ptr ss:[ebp+148],edi                  |
6F683A47  | 8B06                    | mov eax,dword ptr ds:[esi]                      |
6F683A49  | 8985 50010000           | mov dword ptr ss:[ebp+150],eax                  |
6F683A4F  | 8B4E 04                 | mov ecx,dword ptr ds:[esi+4]                    |
6F683A52  | 898D 54010000           | mov dword ptr ss:[ebp+154],ecx                  |
6F683A58  | 8B56 08                 | mov edx,dword ptr ds:[esi+8]                    |
6F683A5B  | 8995 58010000           | mov dword ptr ss:[ebp+158],edx                  |
6F683A61  | 8B46 0C                 | mov eax,dword ptr ds:[esi+C]                    |
6F683A64  | BA 01000000             | mov edx,1                                       |
6F683A69  | 8BCD                    | mov ecx,ebp                                     |
6F683A6B  | 8985 5C010000           | mov dword ptr ss:[ebp+15C],eax                  |
6F683A71  | 899D 78020000           | mov dword ptr ss:[ebp+278],ebx                  |
6F683A77  | E8 E46EFFFF             | call game.6F67A960                              |
6F683A7C  | 55                      | push ebp                                        |
6F683A7D  | B9 70FFAC6F             | mov ecx,game.6FACFF70                           |
6F683A82  | E8 F95FFFFF             | call game.6F679A80                              |
6F683A87  | 8B5C24 14               | mov ebx,dword ptr ss:[esp+14]                   |
6F683A8B  | 8947 40                 | mov dword ptr ds:[edi+40],eax                   |
6F683A8E  | 57                      | push edi                                        |
6F683A8F  | B9 ACFFAC6F             | mov ecx,game.6FACFFAC                           |
6F683A94  | 8943 04                 | mov dword ptr ds:[ebx+4],eax                    |
6F683A97  | E8 8463FFFF             | call game.6F679E20                              |
6F683A9C  | D9EE                    | fldz                                            |
6F683A9E  | 51                      | push ecx                                        |
6F683A9F  | D91C24                  | fstp dword ptr ss:[esp]                         |
6F683AA2  | 8B4B 04                 | mov ecx,dword ptr ds:[ebx+4]                    |
6F683AA5  | 51                      | push ecx                                        |
6F683AA6  | BA B0EA676F             | mov edx,game.6F67EAB0                           |
6F683AAB  | B9 12000000             | mov ecx,12                                      |
6F683AB0  | 8BF8                    | mov edi,eax                                     |
6F683AB2  | E8 D978FAFF             | call game.6F62B390                              |
6F683AB7  | 8B4C24 28               | mov ecx,dword ptr ss:[esp+28]                   |
6F683ABB  | 8B11                    | mov edx,dword ptr ds:[ecx]                      |
6F683ABD  | 8B42 38                 | mov eax,dword ptr ds:[edx+38]                   |
6F683AC0  | 6A 00                   | push 0                                          |
6F683AC2  | 6A 00                   | push 0                                          |
6F683AC4  | 57                      | push edi                                        |
6F683AC5  | 68 002E686F             | push game.6F682E00                              |
6F683ACA  | 56                      | push esi                                        |
6F683ACB  | FFD0                    | call eax                                        |
6F683ACD  | 8B4C24 18               | mov ecx,dword ptr ss:[esp+18]                   |
6F683AD1  | 64:890D 00000000        | mov dword ptr fs:[0],ecx                        |
6F683AD8  | 59                      | pop ecx                                         |
6F683AD9  | 5F                      | pop edi                                         |
6F683ADA  | 5E                      | pop esi                                         |
6F683ADB  | 5D                      | pop ebp                                         |
6F683ADC  | 5B                      | pop ebx                                         |
6F683ADD  | 83C4 10                 | add esp,10                                      |
6F683AE0  | C2 2400                 | ret 24                                          |
```

### c. 销毁 NetClient (`6F67E280`)

Guest 模式 和 Host 模式销毁 NetClient 的逻辑。
- **入口偏移**: `67E280`
- **内存地址**: `game.dll+67E280` (基址 6F000000)

```assembly
6F67E280  | 53                      | push ebx                                        | 销毁 NetClient
6F67E281  | 55                      | push ebp                                        |
6F67E282  | 56                      | push esi                                        |
6F67E283  | 57                      | push edi                                        |
6F67E284  | 8BF9                    | mov edi,ecx                                     |
6F67E286  | 8B87 48010000           | mov eax,dword ptr ds:[edi+148]                  |
6F67E28C  | 8B48 3C                 | mov ecx,dword ptr ds:[eax+3C]                   |
6F67E28F  | 33F6                    | xor esi,esi                                     |
6F67E291  | 3BCE                    | cmp ecx,esi                                     |
6F67E293  | 8BDA                    | mov ebx,edx                                     |
6F67E295  | BD 01000000             | mov ebp,1                                       |
6F67E29A  | 0F84 F4000000           | je game.6F67E394                                |
6F67E2A0  | 3BDE                    | cmp ebx,esi                                     |
6F67E2A2  | 0F84 C0000000           | je game.6F67E368                                |
6F67E2A8  | 397424 14               | cmp dword ptr ss:[esp+14],esi                   |
6F67E2AC  | 0F84 B6000000           | je game.6F67E368                                |
6F67E2B2  | F780 80000000 00001000  | test dword ptr ds:[eax+80],100000               |
6F67E2BC  | 0F84 A6000000           | je game.6F67E368                                |
6F67E2C2  | 6A 08                   | push 8                                          |
6F67E2C4  | 68 A4060000             | push 6A4                                        |
6F67E2C9  | 68 3C17976F             | push game.6F97173C                              |
6F67E2CE  | 68 E0000000             | push E0                                         |
6F67E2D3  | E8 DAD20600             | call <JMP.&Ordinal#401>                         |
6F67E2D8  | 3BC6                    | cmp eax,esi                                     |
6F67E2DA  | 74 09                   | je game.6F67E2E5                                |
6F67E2DC  | 8BC8                    | mov ecx,eax                                     |
6F67E2DE  | E8 0D12FFFF             | call game.6F66F4F0                              |
6F67E2E3  | 8BF0                    | mov esi,eax                                     |
6F67E2E5  | 8D6E 10                 | lea ebp,dword ptr ds:[esi+10]                   |
6F67E2E8  | 8BCD                    | mov ecx,ebp                                     |
6F67E2EA  | E8 715D0400             | call game.6F6C4060                              |
6F67E2EF  | E8 0C6B0400             | call <JMP.&GetTickCount>                        |
6F67E2F4  | 818E 80000000 00001000  | or dword ptr ds:[esi+80],100000                 |
6F67E2FE  | 8946 50                 | mov dword ptr ds:[esi+50],eax                   |
6F67E301  | 8B47 0C                 | mov eax,dword ptr ds:[edi+C]                    |
6F67E304  | 56                      | push esi                                        |
6F67E305  | B9 ACFFAC6F             | mov ecx,game.6FACFFAC                           |
6F67E30A  | 8946 40                 | mov dword ptr ds:[esi+40],eax                   |
6F67E30D  | E8 0EBBFFFF             | call game.6F679E20                              |
6F67E312  | 89B7 4C010000           | mov dword ptr ds:[edi+14C],esi                  |
6F67E318  | 8B0B                    | mov ecx,dword ptr ds:[ebx]                      |
6F67E31A  | 51                      | push ecx                                        |
6F67E31B  | B9 A8FFAC6F             | mov ecx,game.6FACFFA8                           |
6F67E320  | E8 6BBAFFFF             | call game.6F679D90                              |
6F67E325  | 8BCD                    | mov ecx,ebp                                     |
6F67E327  | 8933                    | mov dword ptr ds:[ebx],esi                      |
6F67E329  | E8 325D0400             | call game.6F6C4060                              |
6F67E32E  | 8B97 48010000           | mov edx,dword ptr ds:[edi+148]                  |
6F67E334  | 8B4A 3C                 | mov ecx,dword ptr ds:[edx+3C]                   |
6F67E337  | 8B03                    | mov eax,dword ptr ds:[ebx]                      |
6F67E339  | 8948 3C                 | mov dword ptr ds:[eax+3C],ecx                   |
6F67E33C  | 8B03                    | mov eax,dword ptr ds:[ebx]                      |
6F67E33E  | 8B50 0C                 | mov edx,dword ptr ds:[eax+C]                    |
6F67E341  | 8B48 3C                 | mov ecx,dword ptr ds:[eax+3C]                   |
6F67E344  | E8 47CA0500             | call game.6F6DAD90                              |
6F67E349  | 8B03                    | mov eax,dword ptr ds:[ebx]                      |
6F67E34B  | 8B48 3C                 | mov ecx,dword ptr ds:[eax+3C]                   |
6F67E34E  | BA 80D3676F             | mov edx,game.6F67D380                           |
6F67E353  | E8 18CA0500             | call game.6F6DAD70                              |
6F67E358  | 8B4C24 14               | mov ecx,dword ptr ss:[esp+14]                   |
6F67E35C  | 33ED                    | xor ebp,ebp                                     |
6F67E35E  | C701 30C4676F           | mov dword ptr ds:[ecx],game.6F67C430            |
6F67E364  | 33F6                    | xor esi,esi                                     |
6F67E366  | EB 13                   | jmp game.6F67E37B                               |
6F67E368  | E8 C3C90500             | call game.6F6DAD30                              |
6F67E36D  | 8B97 48010000           | mov edx,dword ptr ds:[edi+148]                  |
6F67E373  | 8B4A 3C                 | mov ecx,dword ptr ds:[edx+3C]                   |
6F67E376  | E8 75C90500             | call game.6F6DACF0                              |
6F67E37B  | 8B87 48010000           | mov eax,dword ptr ds:[edi+148]                  |
6F67E381  | 8970 3C                 | mov dword ptr ds:[eax+3C],esi                   |
6F67E384  | 8B87 48010000           | mov eax,dword ptr ds:[edi+148]                  |
6F67E38A  | 81A0 80000000 FFFFEFFF  | and dword ptr ds:[eax+80],FFEFFFFF              |
6F67E394  | 8BCF                    | mov ecx,edi                                     |
6F67E396  | E8 7553FFFF             | call game.6F673710                              |
6F67E39B  | 8B9F 74020000           | mov ebx,dword ptr ds:[edi+274]                  |
6F67E3A1  | 3BDE                    | cmp ebx,esi                                     |
6F67E3A3  | 89B7 68010000           | mov dword ptr ds:[edi+168],esi                  |
6F67E3A9  | 74 1B                   | je game.6F67E3C6                                |
6F67E3AB  | 8BCB                    | mov ecx,ebx                                     |
6F67E3AD  | E8 6EC6FFFF             | call game.6F67AA20                              |
6F67E3B2  | 56                      | push esi                                        |
6F67E3B3  | 6A FF                   | push FFFFFFFF                                   |
6F67E3B5  | 68 044F876F             | push game.6F874F04                              |
6F67E3BA  | 53                      | push ebx                                        |
6F67E3BB  | E8 98D10600             | call <JMP.&Ordinal#403>                         |
6F67E3C0  | 89B7 74020000           | mov dword ptr ds:[edi+274],esi                  |
6F67E3C6  | 5F                      | pop edi                                         |
6F67E3C7  | 5E                      | pop esi                                         |
6F67E3C8  | 8BC5                    | mov eax,ebp                                     |
6F67E3CA  | 5D                      | pop ebp                                         |
6F67E3CB  | 5B                      | pop ebx                                         |
6F67E3CC  | C2 0400                 | ret 4                                           |
```
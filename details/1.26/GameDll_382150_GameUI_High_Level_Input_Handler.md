# 魔兽争霸 III (v1.26) 高级输入响应器逆向分析

这是对 **高级输入响应器 (High-Level Input Handler)** 函数的逆向分析总结。

这个函数是 Warcraft III 键盘输入逻辑的“总调度中心”。它负责接收原始按键，判断其类型（系统键、编队键、技能键、物品键），并将指令分发给底层的具体执行函数。

## 1. 函数概览
- **基址**：`6F000000`
- **地址**：`6F382150`
- **所在模块**：`game.dll`
- **函数名称**：`HighLevelInputHandler` (推测命名)
- **功能总结**：
该函数是游戏内的**总键盘消息分发器**。它位于输入处理链的上层，负责拦截按键并按照优先级进行路由：
1.  **系统/修饰键**：处理 Alt (显血/阵型)、Ctrl (编队)、Tab (切换单位)。
2.  **单位/英雄选择**：处理 F1-F5 (选择英雄) 和 0-9 (编队选择)。
3.  **技能施法**：判断按键是否对应当前选中单位的技能。
4.  **物品使用**：如果不是技能，则尝试作为物品键处理（这是改建的入口）。

---

## 2. 核心逻辑流程 (反汇编分析)

### 2.1 上下文环境与预处理
```assembly
6F382150  | push FFFFFFFF                   | SEH 异常处理帧建立
...
6F38217D  | call game.6F339B90              | 获取 GameUI 单例对象
6F382186  | mov eax,dword ptr ds:[eax+1AC]  | 检查游戏运行状态 (是否在加载/暂停?)
6F3821C6  | mov eax,dword ptr ds:[ebx+10]   | [关键] 获取虚拟按键码 (Virtual Key Code)
```

### 2.2 特殊功能键路由 (Alt, F-Keys, Tab)
```assembly
; --- 检查 F1-F5 (英雄选择) ---
6F3823A7  | cmp eax,305                     | 检查是否超过 F5
6F3823BC  | call game.6F2F3E40              | 如果是 F1-F5，调用英雄选择逻辑

; --- 检查 Tab 键 ---
6F3821F9  | mov edi,9                       | 09 = VK_TAB
6F382202  | call game.6F53F160              | 切换子组 (Sub-group switching)

; --- 检查 Alt 键 (显血/阵型) ---
6F3821E5  | ja game.6F382385                | 跳转进入 Alt 组合键逻辑
6F3821F2  | jmp dword ptr ds:[eax*4+6F382A00] | 分支表跳转 (Switch Case)
```

### 2.3 技能判定 (Ability Cast)
这是输入处理中最关键的分水岭。游戏优先响应技能，其次才是物品。
```assembly
6F38283F  | call game.6F35E390              | [查询] 该按键是否对应当前单位的一个技能？
6F382844  | cmp eax,ebp                     | EBP 通常为 0，比较返回值
6F382846  | je game.6F382962                | [关键跳转] 如果不是技能，跳转去尝试物品逻辑

; --- 是技能，执行施法 ---
6F382958  | call game.6F37C420              | 执行技能 (Cast Ability)
6F38295D  | jmp game.6F3829EA               | 处理完毕，退出函数
```

### 2.4 物品判定 (Item Usage)
只有当按键**不是**技能键时，才会走到这里。这解释了为什么如果技能改建和物品改建冲突，技能会优先触发。
```assembly
; --- 物品逻辑入口 (From 6F382962) ---
6F382992  | call game.6F339B90              | 获取 GameUI
6F382997  | mov eax,dword ptr ds:[eax+3C4]  | 获取 UI 组件指针
6F38299D  | mov eax,dword ptr ds:[eax+148]  | 获取 InventoryBar 对象
6F3829AC  | call game.6F360CD0              | [核心] 调用物品遍历函数
6F3829B1  | cmp eax,ebp                     | 检查物品是否使用成功
```

---

```assembly
6F382150  | 6A FF                    | push FFFFFFFF                                   | 高级输入响应器（High-Level Input Handler）
6F382152  | 68 081C826F              | push game.6F821C08                              |
6F382157  | 64:A1 00000000           | mov eax,dword ptr fs:[0]                        |
6F38215D  | 50                       | push eax                                        |
6F38215E  | 83EC 48                  | sub esp,48                                      |
6F382161  | 53                       | push ebx                                        |
6F382162  | 55                       | push ebp                                        |
6F382163  | 56                       | push esi                                        |
6F382164  | 57                       | push edi                                        |
6F382165  | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]                 |
6F38216A  | 33C4                     | xor eax,esp                                     |
6F38216C  | 50                       | push eax                                        |
6F38216D  | 8D4424 5C                | lea eax,dword ptr ss:[esp+5C]                   |
6F382171  | 64:A3 00000000           | mov dword ptr fs:[0],eax                        |
6F382177  | 8BF1                     | mov esi,ecx                                     |
6F382179  | 897424 20                | mov dword ptr ss:[esp+20],esi                   |
6F38217D  | E8 0E7AFBFF              | call game.6F339B90                              | <--- 获取 GameUI
6F382182  | 894424 14                | mov dword ptr ss:[esp+14],eax                   |
6F382186  | 8B80 AC010000            | mov eax,dword ptr ds:[eax+1AC]                  | <--- 游戏运行状态？
6F38218C  | 33ED                     | xor ebp,ebp                                     |
6F38218E  | 3BC5                     | cmp eax,ebp                                     |
6F382190  | 75 07                    | jne game.6F382199                               |
6F382192  | 33C0                     | xor eax,eax                                     |
6F382194  | E9 51080000              | jmp game.6F3829EA                               |
6F382199  | 8BCE                     | mov ecx,esi                                     |
6F38219B  | E8 F079FBFF              | call game.6F339B90                              | <--- 世界或者鼠标管理器？
6F3821A0  | 8BB0 4C020000            | mov esi,dword ptr ds:[eax+24C]                  |
6F3821A6  | B9 02000000              | mov ecx,2                                       |
6F3821AB  | E8 C08F2A00              | call game.6F62B170                              |
6F3821B0  | B9 01000000              | mov ecx,1                                       |
6F3821B5  | 894424 28                | mov dword ptr ss:[esp+28],eax                   |
6F3821B9  | E8 B28F2A00              | call game.6F62B170                              | <--- 线程安全与锁
6F3821BE  | 8B5C24 6C                | mov ebx,dword ptr ss:[esp+6C]                   |
6F3821C2  | 894424 24                | mov dword ptr ss:[esp+24],eax                   |
6F3821C6  | 8B43 10                  | mov eax,dword ptr ds:[ebx+10]                   | <--- 获取按键代码
6F3821C9  | 3D 13010000              | cmp eax,113                                     | <--- 判断是否为技能按键和物品栏
6F3821CE  | 0F8F B1010000            | jg game.6F382385                                |
6F3821D4  | 3D 12010000              | cmp eax,112                                     |
6F3821D9  | 0F8D 33010000            | jge game.6F382312                               |
6F3821DF  | 83E8 02                  | sub eax,2                                       |
6F3821E2  | 83F8 37                  | cmp eax,37                                      |
6F3821E5  | 0F87 9A010000            | ja game.6F382385                                | <--- 判断是否为 Alt
6F3821EB  | 0FB680 302A386F          | movzx eax,byte ptr ds:[eax+6F382A30]            |
6F3821F2  | FF2485 002A386F          | jmp dword ptr ds:[eax*4+6F382A00]               | <--- eax=0,单独按alt
6F3821F9  | BF 09000000              | mov edi,9                                       |
6F3821FE  | 897C24 6C                | mov dword ptr ss:[esp+6C],edi                   |
6F382202  | E8 59CF1B00              | call game.6F53F160                              |
6F382207  | 85C0                     | test eax,eax                                    |
6F382209  | 0F85 76010000            | jne game.6F382385                               |
6F38220F  | 8B0D F465AB6F            | mov ecx,dword ptr ds:[6FAB65F4]                 |
6F382215  | 0FB741 28                | movzx eax,word ptr ds:[ecx+28]                  |
6F382219  | 50                       | push eax                                        |
6F38221A  | 894C24 20                | mov dword ptr ss:[esp+20],ecx                   |
6F38221E  | E8 2DF40100              | call game.6F3A1650                              |
6F382223  | 396C24 24                | cmp dword ptr ss:[esp+24],ebp                   |
6F382227  | 8B58 34                  | mov ebx,dword ptr ds:[eax+34]                   |
6F38222A  | 0FB640 30                | movzx eax,byte ptr ds:[eax+30]                  |
6F38222E  | 895C24 28                | mov dword ptr ss:[esp+28],ebx                   |
6F382232  | 894424 18                | mov dword ptr ss:[esp+18],eax                   |
6F382236  | 0F84 21020000            | je game.6F38245D                                |
6F38223C  | 896C24 2C                | mov dword ptr ss:[esp+2C],ebp                   |
6F382240  | 896C24 30                | mov dword ptr ss:[esp+30],ebp                   |
6F382244  | 896C24 34                | mov dword ptr ss:[esp+34],ebp                   |
6F382248  | 896C24 38                | mov dword ptr ss:[esp+38],ebp                   |
6F38224C  | 896C24 64                | mov dword ptr ss:[esp+64],ebp                   |
6F382250  | 896C24 3C                | mov dword ptr ss:[esp+3C],ebp                   |
6F382254  | 896C24 40                | mov dword ptr ss:[esp+40],ebp                   |
6F382258  | 896C24 44                | mov dword ptr ss:[esp+44],ebp                   |
6F38225C  | 896C24 48                | mov dword ptr ss:[esp+48],ebp                   |
6F382260  | 8D4C24 2C                | lea ecx,dword ptr ss:[esp+2C]                   |
6F382264  | 51                       | push ecx                                        |
6F382265  | 8BCB                     | mov ecx,ebx                                     |
6F382267  | C64424 68 01             | mov byte ptr ss:[esp+68],1                      |
6F38226C  | E8 BFFB0900              | call game.6F421E30                              |
6F382271  | 396C24 30                | cmp dword ptr ss:[esp+30],ebp                   |
6F382275  | 0F85 4B010000            | jne game.6F3823C6                               |
6F38227B  | 55                       | push ebp                                        |
6F38227C  | 55                       | push ebp                                        |
6F38227D  | 8BCB                     | mov ecx,ebx                                     |
6F38227F  | E8 8CD70900              | call game.6F41FA10                              |
6F382284  | F7D8                     | neg eax                                         |
6F382286  | 1BC0                     | sbb eax,eax                                     |
6F382288  | F7D8                     | neg eax                                         |
6F38228A  | 83C0 05                  | add eax,5                                       |
6F38228D  | 50                       | push eax                                        |
6F38228E  | 8B4C24 20                | mov ecx,dword ptr ss:[esp+20]                   |
6F382292  | E8 9969F7FF              | call game.6F2F8C30                              |
6F382297  | 8D4C24 3C                | lea ecx,dword ptr ss:[esp+3C]                   |
6F38229B  | C64424 64 00             | mov byte ptr ss:[esp+64],0                      |
6F3822A0  | E8 EB42F0FF              | call game.6F286590                              |
6F3822A5  | 8D4C24 2C                | lea ecx,dword ptr ss:[esp+2C]                   |
6F3822A9  | C74424 64 FFFFFFFF       | mov dword ptr ss:[esp+64],FFFFFFFF              |
6F3822B1  | E8 DA42F0FF              | call game.6F286590                              |
6F3822B6  | E9 2A070000              | jmp game.6F3829E5                               |
6F3822BB  | 33FF                     | xor edi,edi                                     |
6F3822BD  | E9 3CFFFFFF              | jmp game.6F3821FE                               |
6F3822C2  | BF 01000000              | mov edi,1                                       |
6F3822C7  | E9 32FFFFFF              | jmp game.6F3821FE                               |
6F3822CC  | BF 02000000              | mov edi,2                                       |
6F3822D1  | E9 28FFFFFF              | jmp game.6F3821FE                               |
6F3822D6  | BF 03000000              | mov edi,3                                       |
6F3822DB  | E9 1EFFFFFF              | jmp game.6F3821FE                               |
6F3822E0  | BF 04000000              | mov edi,4                                       |
6F3822E5  | E9 14FFFFFF              | jmp game.6F3821FE                               |
6F3822EA  | BF 05000000              | mov edi,5                                       |
6F3822EF  | E9 0AFFFFFF              | jmp game.6F3821FE                               |
6F3822F4  | BF 06000000              | mov edi,6                                       |
6F3822F9  | E9 00FFFFFF              | jmp game.6F3821FE                               |
6F3822FE  | BF 07000000              | mov edi,7                                       |
6F382303  | E9 F6FEFFFF              | jmp game.6F3821FE                               |
6F382308  | BF 08000000              | mov edi,8                                       |
6F38230D  | E9 ECFEFFFF              | jmp game.6F3821FE                               |
6F382312  | B9 02000000              | mov ecx,2                                       | <--- 处理Alt
6F382317  | E8 548E2A00              | call game.6F62B170                              | <--- 按键状态查询函数
6F38231C  | 8BF8                     | mov edi,eax                                     |
6F38231E  | 3BFD                     | cmp edi,ebp                                     |
6F382320  | 75 12                    | jne game.6F382334                               |
6F382322  | B9 12010000              | mov ecx,112                                     |
6F382327  | E8 448E2A00              | call game.6F62B170                              | <--- 按键状态查询函数
6F38232C  | 85C0                     | test eax,eax                                    |
6F38232E  | 75 04                    | jne game.6F382334                               |
6F382330  | 33F6                     | xor esi,esi                                     |
6F382332  | EB 09                    | jmp game.6F38233D                               |
6F382334  | 3BFD                     | cmp edi,ebp                                     |
6F382336  | BE 01000000              | mov esi,1                                       |
6F38233B  | 75 0E                    | jne game.6F38234B                               |
6F38233D  | B9 13010000              | mov ecx,113                                     |
6F382342  | E8 298E2A00              | call game.6F62B170                              | <--- 按键状态查询函数
6F382347  | 85C0                     | test eax,eax                                    |
6F382349  | 74 05                    | je game.6F382350                                |
6F38234B  | B8 01000000              | mov eax,1                                       |
6F382350  | 8B4C24 14                | mov ecx,dword ptr ss:[esp+14]                   |
6F382354  | 39A9 D8010000            | cmp dword ptr ds:[ecx+1D8],ebp                  |
6F38235A  | 74 10                    | je game.6F38236C                                |
6F38235C  | 33D2                     | xor edx,edx                                     |
6F38235E  | 3BF5                     | cmp esi,ebp                                     |
6F382360  | 0F94C2                   | sete dl                                         |
6F382363  | 33C0                     | xor eax,eax                                     |
6F382365  | 3BFD                     | cmp edi,ebp                                     |
6F382367  | 0F94C0                   | sete al                                         |
6F38236A  | 8BF2                     | mov esi,edx                                     |
6F38236C  | 3BF5                     | cmp esi,ebp                                     |
6F38236E  | 75 08                    | jne game.6F382378                               |
6F382370  | 3BC5                     | cmp eax,ebp                                     |
6F382372  | 75 04                    | jne game.6F382378                               |
6F382374  | 33C9                     | xor ecx,ecx                                     |
6F382376  | EB 05                    | jmp game.6F38237D                               |
6F382378  | B9 01000000              | mov ecx,1                                       |
6F38237D  | 50                       | push eax                                        |
6F38237E  | 8BD6                     | mov edx,esi                                     |
6F382380  | E8 EBF2FFFF              | call game.6F381670                              |
6F382385  | 396C24 28                | cmp dword ptr ss:[esp+28],ebp                   | <--- 判断是否为修饰键Alt
6F382389  | 0F85 56060000            | jne game.6F3829E5                               |
6F38238F  | 396C24 24                | cmp dword ptr ss:[esp+24],ebp                   | <--- 判断是否为修饰键Ctrl
6F382393  | 0F85 4C060000            | jne game.6F3829E5                               |
6F382399  | 8B43 10                  | mov eax,dword ptr ds:[ebx+10]                   |
6F38239C  | 3D 00030000              | cmp eax,300                                     |
6F3823A1  | 0F8C 27040000            | jl game.6F3827CE                                |
6F3823A7  | 3D 05030000              | cmp eax,305                                     | <--- 判断是否为功能键（F1: 0x300 到 F5: 0x304）普通按键将会跳转
6F3823AC  | 0F8D 1C040000            | jge game.6F3827CE                               |
6F3823B2  | 8B4C24 14                | mov ecx,dword ptr ss:[esp+14]                   |
6F3823B6  | 05 00FDFFFF              | add eax,FFFFFD00                                |
6F3823BB  | 50                       | push eax                                        |
6F3823BC  | E8 7F1AF7FF              | call game.6F2F3E40                              | <--- 处理 F1 到 F5
6F3823C1  | E9 1F060000              | jmp game.6F3829E5                               |
6F3823C6  | 33ED                     | xor ebp,ebp                                     | <--- 这个区域不处理任何按键，待研究
6F3823C8  | 33FF                     | xor edi,edi                                     |
6F3823CA  | 396C24 30                | cmp dword ptr ss:[esp+30],ebp                   |
6F3823CE  | 76 54                    | jbe game.6F382424                               |
6F3823D0  | 8B5424 34                | mov edx,dword ptr ss:[esp+34]                   |
6F3823D4  | 8B34AA                   | mov esi,dword ptr ds:[edx+ebp*4]                |
6F3823D7  | 85F6                     | test esi,esi                                    |
6F3823D9  | 74 40                    | je game.6F38241B                                |
6F3823DB  | 8BCE                     | mov ecx,esi                                     |
6F3823DD  | E8 BEF4CBFF              | call game.6F0418A0                              |
6F3823E2  | 85C0                     | test eax,eax                                    |
6F3823E4  | 74 35                    | je game.6F38241B                                |
6F3823E6  | 8B4424 18                | mov eax,dword ptr ss:[esp+18]                   |
6F3823EA  | 8B16                     | mov edx,dword ptr ds:[esi]                      |
6F3823EC  | 50                       | push eax                                        |
6F3823ED  | 8B82 EC000000            | mov eax,dword ptr ds:[edx+EC]                   |
6F3823F3  | 8BCE                     | mov ecx,esi                                     |
6F3823F5  | FFD0                     | call eax                                        |
6F3823F7  | 8B4C24 20                | mov ecx,dword ptr ss:[esp+20]                   |
6F3823FB  | 50                       | push eax                                        |
6F3823FC  | E8 2F110200              | call game.6F3A3530                              |
6F382401  | 85C0                     | test eax,eax                                    |
6F382403  | 75 05                    | jne game.6F38240A                               |
6F382405  | 8D78 01                  | lea edi,dword ptr ds:[eax+1]                    |
6F382408  | EB 11                    | jmp game.6F38241B                               |
6F38240A  | 56                       | push esi                                        |
6F38240B  | 8D4C24 40                | lea ecx,dword ptr ss:[esp+40]                   |
6F38240F  | E8 1C23D2FF              | call game.6F0A4730                              |
6F382414  | 8BC8                     | mov ecx,eax                                     |
6F382416  | E8 4581CAFF              | call game.6F02A560                              |
6F38241B  | 83C5 01                  | add ebp,1                                       |
6F38241E  | 3B6C24 30                | cmp ebp,dword ptr ss:[esp+30]                   |
6F382422  | 72 AC                    | jb game.6F3823D0                                |
6F382424  | 837C24 40 00             | cmp dword ptr ss:[esp+40],0                     |
6F382429  | 75 11                    | jne game.6F38243C                               |
6F38242B  | F7DF                     | neg edi                                         |
6F38242D  | 6A 00                    | push 0                                          |
6F38242F  | 1BFF                     | sbb edi,edi                                     |
6F382431  | 6A 00                    | push 0                                          |
6F382433  | 83C7 05                  | add edi,5                                       |
6F382436  | 57                       | push edi                                        |
6F382437  | E9 52FEFFFF              | jmp game.6F38228E                               |
6F38243C  | 8B7424 6C                | mov esi,dword ptr ss:[esp+6C]                   |
6F382440  | 8D4C24 3C                | lea ecx,dword ptr ss:[esp+3C]                   |
6F382444  | 51                       | push ecx                                        |
6F382445  | 56                       | push esi                                        |
6F382446  | 8BCB                     | mov ecx,ebx                                     |
6F382448  | E8 83D60900              | call game.6F41FAD0                              |
6F38244D  | 8D5424 3C                | lea edx,dword ptr ss:[esp+3C]                   |
6F382451  | 8BCE                     | mov ecx,esi                                     |
6F382453  | E8 78D3F4FF              | call game.6F2CF7D0                              |
6F382458  | E9 3AFEFFFF              | jmp game.6F382297                               |
6F38245D  | 33C9                     | xor ecx,ecx                                     |
6F38245F  | E8 0C8D2A00              | call game.6F62B170                              |
6F382464  | 85C0                     | test eax,eax                                    |
6F382466  | 0F84 BC020000            | je game.6F382728                                |
6F38246C  | 896C24 3C                | mov dword ptr ss:[esp+3C],ebp                   |
6F382470  | 896C24 40                | mov dword ptr ss:[esp+40],ebp                   |
6F382474  | 896C24 44                | mov dword ptr ss:[esp+44],ebp                   |
6F382478  | 896C24 48                | mov dword ptr ss:[esp+48],ebp                   |
6F38247C  | C74424 64 02000000       | mov dword ptr ss:[esp+64],2                     |
6F382484  | 896C24 4C                | mov dword ptr ss:[esp+4C],ebp                   |
6F382488  | 896C24 50                | mov dword ptr ss:[esp+50],ebp                   |
6F38248C  | 896C24 54                | mov dword ptr ss:[esp+54],ebp                   |
6F382490  | 896C24 58                | mov dword ptr ss:[esp+58],ebp                   |
6F382494  | 896C24 2C                | mov dword ptr ss:[esp+2C],ebp                   |
6F382498  | 896C24 30                | mov dword ptr ss:[esp+30],ebp                   |
6F38249C  | 896C24 34                | mov dword ptr ss:[esp+34],ebp                   |
6F3824A0  | 896C24 38                | mov dword ptr ss:[esp+38],ebp                   |
6F3824A4  | 8D5424 3C                | lea edx,dword ptr ss:[esp+3C]                   |
6F3824A8  | 52                       | push edx                                        |
6F3824A9  | 8BCB                     | mov ecx,ebx                                     |
6F3824AB  | C64424 68 04             | mov byte ptr ss:[esp+68],4                      |
6F3824B0  | E8 7BF90900              | call game.6F421E30                              |
6F3824B5  | 8D4424 4C                | lea eax,dword ptr ss:[esp+4C]                   |
6F3824B9  | 50                       | push eax                                        |
6F3824BA  | 57                       | push edi                                        |
6F3824BB  | 8BCB                     | mov ecx,ebx                                     |
6F3824BD  | E8 3EFA0900              | call game.6F421F00                              |
6F3824C2  | 396C24 40                | cmp dword ptr ss:[esp+40],ebp                   |
6F3824C6  | 75 4E                    | jne game.6F382516                               |
6F3824C8  | 55                       | push ebp                                        |
6F3824C9  | 55                       | push ebp                                        |
6F3824CA  | 8BCB                     | mov ecx,ebx                                     |
6F3824CC  | E8 3FD50900              | call game.6F41FA10                              |
6F3824D1  | F7D8                     | neg eax                                         |
6F3824D3  | 1BC0                     | sbb eax,eax                                     |
6F3824D5  | F7D8                     | neg eax                                         |
6F3824D7  | 8B4C24 1C                | mov ecx,dword ptr ss:[esp+1C]                   |
6F3824DB  | 83C0 05                  | add eax,5                                       |
6F3824DE  | 50                       | push eax                                        |
6F3824DF  | E8 4C67F7FF              | call game.6F2F8C30                              |
6F3824E4  | 8D4C24 2C                | lea ecx,dword ptr ss:[esp+2C]                   |
6F3824E8  | C64424 64 03             | mov byte ptr ss:[esp+64],3                      |
6F3824ED  | E8 9E40F0FF              | call game.6F286590                              |
6F3824F2  | 8D4C24 4C                | lea ecx,dword ptr ss:[esp+4C]                   |
6F3824F6  | C64424 64 02             | mov byte ptr ss:[esp+64],2                      |
6F3824FB  | E8 9040F0FF              | call game.6F286590                              |
6F382500  | 8D4C24 3C                | lea ecx,dword ptr ss:[esp+3C]                   |
6F382504  | C74424 64 FFFFFFFF       | mov dword ptr ss:[esp+64],FFFFFFFF              |
6F38250C  | E8 7F40F0FF              | call game.6F286590                              |
6F382511  | E9 CF040000              | jmp game.6F3829E5                               |
6F382516  | 8B4424 50                | mov eax,dword ptr ss:[esp+50]                   |
6F38251A  | 3BC5                     | cmp eax,ebp                                     |
6F38251C  | 896C24 20                | mov dword ptr ss:[esp+20],ebp                   |
6F382520  | 894424 24                | mov dword ptr ss:[esp+24],eax                   |
6F382524  | 896C24 18                | mov dword ptr ss:[esp+18],ebp                   |
6F382528  | 0F86 B4000000            | jbe game.6F3825E2                               |
6F38252E  | 8B4C24 54                | mov ecx,dword ptr ss:[esp+54]                   |
6F382532  | 8B5424 18                | mov edx,dword ptr ss:[esp+18]                   |
6F382536  | 8B2C91                   | mov ebp,dword ptr ds:[ecx+edx*4]                |
6F382539  | 85ED                     | test ebp,ebp                                    |
6F38253B  | 0F84 8A000000            | je game.6F3825CB                                |
6F382541  | 8BCD                     | mov ecx,ebp                                     |
6F382543  | E8 58F3CBFF              | call game.6F0418A0                              |
6F382548  | 85C0                     | test eax,eax                                    |
6F38254A  | 74 7F                    | je game.6F3825CB                                |
6F38254C  | 8B45 00                  | mov eax,dword ptr ss:[ebp]                      |
6F38254F  | 8B90 3C010000            | mov edx,dword ptr ds:[eax+13C]                  |
6F382555  | 8BCD                     | mov ecx,ebp                                     |
6F382557  | FFD2                     | call edx                                        |
6F382559  | 85C0                     | test eax,eax                                    |
6F38255B  | 75 6E                    | jne game.6F3825CB                               |
6F38255D  | 8B7424 1C                | mov esi,dword ptr ss:[esp+1C]                   |
6F382561  | 0FB746 28                | movzx eax,word ptr ds:[esi+28]                  |
6F382565  | 50                       | push eax                                        |
6F382566  | 8B45 00                  | mov eax,dword ptr ss:[ebp]                      |
6F382569  | 8B90 EC000000            | mov edx,dword ptr ds:[eax+EC]                   |
6F38256F  | 8BCD                     | mov ecx,ebp                                     |
6F382571  | FFD2                     | call edx                                        |
6F382573  | 50                       | push eax                                        |
6F382574  | 8BCE                     | mov ecx,esi                                     |
6F382576  | E8 B50F0200              | call game.6F3A3530                              |
6F38257B  | 85C0                     | test eax,eax                                    |
6F38257D  | 75 0A                    | jne game.6F382589                               |
6F38257F  | C74424 20 01000000       | mov dword ptr ss:[esp+20],1                     |
6F382587  | EB 42                    | jmp game.6F3825CB                               |
6F382589  | 8B5C24 30                | mov ebx,dword ptr ss:[esp+30]                   |
6F38258D  | 33FF                     | xor edi,edi                                     |
6F38258F  | 85DB                     | test ebx,ebx                                    |
6F382591  | 76 27                    | jbe game.6F3825BA                               |
6F382593  | 8B4424 34                | mov eax,dword ptr ss:[esp+34]                   |
6F382597  | 8B34B8                   | mov esi,dword ptr ds:[eax+edi*4]                |
6F38259A  | 85F6                     | test esi,esi                                    |
6F38259C  | 74 18                    | je game.6F3825B6                                |
6F38259E  | 8BCE                     | mov ecx,esi                                     |
6F3825A0  | E8 FBF2CBFF              | call game.6F0418A0                              |
6F3825A5  | 85C0                     | test eax,eax                                    |
6F3825A7  | 74 0D                    | je game.6F3825B6                                |
6F3825A9  | 3BF5                     | cmp esi,ebp                                     |
6F3825AB  | 74 09                    | je game.6F3825B6                                |
6F3825AD  | 83C7 01                  | add edi,1                                       |
6F3825B0  | 3BFB                     | cmp edi,ebx                                     |
6F3825B2  | 72 DF                    | jb game.6F382593                                |
6F3825B4  | EB 04                    | jmp game.6F3825BA                               |
6F3825B6  | 3BFB                     | cmp edi,ebx                                     |
6F3825B8  | 72 11                    | jb game.6F3825CB                                |
6F3825BA  | 55                       | push ebp                                        |
6F3825BB  | 8D4C24 30                | lea ecx,dword ptr ss:[esp+30]                   |
6F3825BF  | E8 6C21D2FF              | call game.6F0A4730                              |
6F3825C4  | 8BC8                     | mov ecx,eax                                     |
6F3825C6  | E8 957FCAFF              | call game.6F02A560                              |
6F3825CB  | 8B4424 18                | mov eax,dword ptr ss:[esp+18]                   |
6F3825CF  | 83C0 01                  | add eax,1                                       |
6F3825D2  | 3B4424 24                | cmp eax,dword ptr ss:[esp+24]                   |
6F3825D6  | 894424 18                | mov dword ptr ss:[esp+18],eax                   |
6F3825DA  | 0F82 4EFFFFFF            | jb game.6F38252E                                |
6F3825E0  | 33ED                     | xor ebp,ebp                                     |
6F3825E2  | 396C24 40                | cmp dword ptr ss:[esp+40],ebp                   |
6F3825E6  | 896C24 18                | mov dword ptr ss:[esp+18],ebp                   |
6F3825EA  | 0F86 FD000000            | jbe game.6F3826ED                               |
6F3825F0  | 8B4C24 44                | mov ecx,dword ptr ss:[esp+44]                   |
6F3825F4  | 8B5424 18                | mov edx,dword ptr ss:[esp+18]                   |
6F3825F8  | 8B3491                   | mov esi,dword ptr ds:[ecx+edx*4]                |
6F3825FB  | 85F6                     | test esi,esi                                    |
6F3825FD  | 0F84 D5000000            | je game.6F3826D8                                |
6F382603  | 8BCE                     | mov ecx,esi                                     |
6F382605  | E8 96F2CBFF              | call game.6F0418A0                              |
6F38260A  | 85C0                     | test eax,eax                                    |
6F38260C  | 0F84 C6000000            | je game.6F3826D8                                |
6F382612  | F646 20 02               | test byte ptr ds:[esi+20],2                     |
6F382616  | 0F84 BC000000            | je game.6F3826D8                                |
6F38261C  | F786 80020000 00040000   | test dword ptr ds:[esi+280],400                 |
6F382626  | 0F85 AC000000            | jne game.6F3826D8                               |
6F38262C  | 8B06                     | mov eax,dword ptr ds:[esi]                      |
6F38262E  | 8B90 3C010000            | mov edx,dword ptr ds:[eax+13C]                  |
6F382634  | 8BCE                     | mov ecx,esi                                     |
6F382636  | FFD2                     | call edx                                        |
6F382638  | 85C0                     | test eax,eax                                    |
6F38263A  | 0F85 98000000            | jne game.6F3826D8                               |
6F382640  | 8BCE                     | mov ecx,esi                                     |
6F382642  | E8 89AFF2FF              | call game.6F2AD5D0                              |
6F382647  | 85C0                     | test eax,eax                                    |
6F382649  | 0F85 89000000            | jne game.6F3826D8                               |
6F38264F  | 8B06                     | mov eax,dword ptr ds:[esi]                      |
6F382651  | 8B90 00010000            | mov edx,dword ptr ds:[eax+100]                  |
6F382657  | 6A 04                    | push 4                                          |
6F382659  | 6A 00                    | push 0                                          |
6F38265B  | 8BCE                     | mov ecx,esi                                     |
6F38265D  | FFD2                     | call edx                                        |
6F38265F  | 85C0                     | test eax,eax                                    |
6F382661  | 74 75                    | je game.6F3826D8                                |
6F382663  | 8B7C24 1C                | mov edi,dword ptr ss:[esp+1C]                   |
6F382667  | 0FB747 28                | movzx eax,word ptr ds:[edi+28]                  |
6F38266B  | 50                       | push eax                                        |
6F38266C  | 8B06                     | mov eax,dword ptr ds:[esi]                      |
6F38266E  | 8B90 EC000000            | mov edx,dword ptr ds:[eax+EC]                   |
6F382674  | 8BCE                     | mov ecx,esi                                     |
6F382676  | FFD2                     | call edx                                        |
6F382678  | 50                       | push eax                                        |
6F382679  | 8BCF                     | mov ecx,edi                                     |
6F38267B  | E8 B00E0200              | call game.6F3A3530                              |
6F382680  | 85C0                     | test eax,eax                                    |
6F382682  | 75 0A                    | jne game.6F38268E                               |
6F382684  | C74424 20 01000000       | mov dword ptr ss:[esp+20],1                     |
6F38268C  | EB 4A                    | jmp game.6F3826D8                               |
6F38268E  | 33ED                     | xor ebp,ebp                                     |
6F382690  | 396C24 30                | cmp dword ptr ss:[esp+30],ebp                   |
6F382694  | 76 31                    | jbe game.6F3826C7                               |
6F382696  | 8B5C24 34                | mov ebx,dword ptr ss:[esp+34]                   |
6F38269A  | 8D9B 00000000            | lea ebx,dword ptr ds:[ebx]                      |
6F3826A0  | 8B3CAB                   | mov edi,dword ptr ds:[ebx+ebp*4]                |
6F3826A3  | 85FF                     | test edi,edi                                    |
6F3826A5  | 74 1A                    | je game.6F3826C1                                |
6F3826A7  | 8BCF                     | mov ecx,edi                                     |
6F3826A9  | E8 F2F1CBFF              | call game.6F0418A0                              |
6F3826AE  | 85C0                     | test eax,eax                                    |
6F3826B0  | 74 0F                    | je game.6F3826C1                                |
6F3826B2  | 3BFE                     | cmp edi,esi                                     |
6F3826B4  | 74 0B                    | je game.6F3826C1                                |
6F3826B6  | 83C5 01                  | add ebp,1                                       |
6F3826B9  | 3B6C24 30                | cmp ebp,dword ptr ss:[esp+30]                   |
6F3826BD  | 72 E1                    | jb game.6F3826A0                                |
6F3826BF  | EB 06                    | jmp game.6F3826C7                               |
6F3826C1  | 3B6C24 30                | cmp ebp,dword ptr ss:[esp+30]                   |
6F3826C5  | 72 11                    | jb game.6F3826D8                                |
6F3826C7  | 56                       | push esi                                        |
6F3826C8  | 8D4C24 30                | lea ecx,dword ptr ss:[esp+30]                   |
6F3826CC  | E8 5F20D2FF              | call game.6F0A4730                              |
6F3826D1  | 8BC8                     | mov ecx,eax                                     |
6F3826D3  | E8 887ECAFF              | call game.6F02A560                              |
6F3826D8  | 8B4424 18                | mov eax,dword ptr ss:[esp+18]                   |
6F3826DC  | 83C0 01                  | add eax,1                                       |
6F3826DF  | 3B4424 40                | cmp eax,dword ptr ss:[esp+40]                   |
6F3826E3  | 894424 18                | mov dword ptr ss:[esp+18],eax                   |
6F3826E7  | 0F82 03FFFFFF            | jb game.6F3825F0                                |
6F3826ED  | 837C24 30 00             | cmp dword ptr ss:[esp+30],0                     |
6F3826F2  | 75 11                    | jne game.6F382705                               |
6F3826F4  | 8B4424 20                | mov eax,dword ptr ss:[esp+20]                   |
6F3826F8  | 6A 00                    | push 0                                          |
6F3826FA  | F7D8                     | neg eax                                         |
6F3826FC  | 6A 00                    | push 0                                          |
6F3826FE  | 1BC0                     | sbb eax,eax                                     |
6F382700  | E9 D2FDFFFF              | jmp game.6F3824D7                               |
6F382705  | 8B7424 6C                | mov esi,dword ptr ss:[esp+6C]                   |
6F382709  | 8B4C24 28                | mov ecx,dword ptr ss:[esp+28]                   |
6F38270D  | 8D4424 2C                | lea eax,dword ptr ss:[esp+2C]                   |
6F382711  | 50                       | push eax                                        |
6F382712  | 56                       | push esi                                        |
6F382713  | E8 B8D30900              | call game.6F41FAD0                              |
6F382718  | 8D5424 2C                | lea edx,dword ptr ss:[esp+2C]                   |
6F38271C  | 8BCE                     | mov ecx,esi                                     |
6F38271E  | E8 ADD0F4FF              | call game.6F2CF7D0                              |
6F382723  | E9 BCFDFFFF              | jmp game.6F3824E4                               |
6F382728  | 57                       | push edi                                        |
6F382729  | 8BCE                     | mov ecx,esi                                     |
6F38272B  | E8 60BEFFFF              | call game.6F37E590                              |
6F382730  | 85C0                     | test eax,eax                                    |
6F382732  | 0F84 AD020000            | je game.6F3829E5                                |
6F382738  | E8 43273400              | call game.6F6C4E80                              |
6F38273D  | D95C24 28                | fstp dword ptr ss:[esp+28]                      |
6F382741  | 8B4C24 14                | mov ecx,dword ptr ss:[esp+14]                   |
6F382745  | 8BB1 14020000            | mov esi,dword ptr ds:[ecx+214]                  |
6F38274B  | 56                       | push esi                                        |
6F38274C  | E8 AF1AF7FF              | call game.6F2F4200                              |
6F382751  | 85C0                     | test eax,eax                                    |
6F382753  | 75 0C                    | jne game.6F382761                               |
6F382755  | 8B4C24 14                | mov ecx,dword ptr ss:[esp+14]                   |
6F382759  | 6A 01                    | push 1                                          |
6F38275B  | 56                       | push esi                                        |
6F38275C  | E8 7F90F7FF              | call game.6F2FB7E0                              |
6F382761  | 8B5424 14                | mov edx,dword ptr ss:[esp+14]                   |
6F382765  | 8B8A C8030000            | mov ecx,dword ptr ds:[edx+3C8]                  |
6F38276B  | E8 C0F9FBFF              | call game.6F342130                              |
6F382770  | D94424 28                | fld dword ptr ss:[esp+28]                       |
6F382774  | 8B4C24 20                | mov ecx,dword ptr ss:[esp+20]                   |
6F382778  | 3B79 0C                  | cmp edi,dword ptr ds:[ecx+C]                    |
6F38277B  | 75 46                    | jne game.6F3827C3                               |
6F38277D  | D941 10                  | fld dword ptr ds:[ecx+10]                       |
6F382780  | DC05 2051876F            | fadd qword ptr ds:[6F875120]                    |
6F382786  | D8D9                     | fcomp st(1)                                     |
6F382788  | DFE0                     | fnstsw ax                                       |
6F38278A  | F6C4 41                  | test ah,41                                      |
6F38278D  | 75 34                    | jne game.6F3827C3                               |
6F38278F  | 8BCB                     | mov ecx,ebx                                     |
6F382791  | DDD8                     | fstp st(0)                                      |
6F382793  | E8 68D20900              | call game.6F41FA00                              |
6F382798  | 8BF0                     | mov esi,eax                                     |
6F38279A  | 3BF5                     | cmp esi,ebp                                     |
6F38279C  | 74 15                    | je game.6F3827B3                                |
6F38279E  | 8BCE                     | mov ecx,esi                                     |
6F3827A0  | E8 FBF0CBFF              | call game.6F0418A0                              |
6F3827A5  | 85C0                     | test eax,eax                                    |
6F3827A7  | 74 0A                    | je game.6F3827B3                                |
6F3827A9  | 8B4C24 14                | mov ecx,dword ptr ss:[esp+14]                   |
6F3827AD  | 56                       | push esi                                        |
6F3827AE  | E8 3D67F7FF              | call game.6F2F8EF0                              |
6F3827B3  | 8B4424 20                | mov eax,dword ptr ss:[esp+20]                   |
6F3827B7  | C740 0C FFFFFFFF         | mov dword ptr ds:[eax+C],FFFFFFFF               |
6F3827BE  | E9 22020000              | jmp game.6F3829E5                               |
6F3827C3  | D959 10                  | fstp dword ptr ds:[ecx+10]                      |
6F3827C6  | 8979 0C                  | mov dword ptr ds:[ecx+C],edi                    |
6F3827C9  | E9 17020000              | jmp game.6F3829E5                               |
6F3827CE  | E8 8DC91B00              | call game.6F53F160                              | <--- 处理 F6，F7，F8 和 其他按键
6F3827D3  | 85C0                     | test eax,eax                                    |
6F3827D5  | 0F85 0A020000            | jne game.6F3829E5                               |
6F3827DB  | 8B43 10                  | mov eax,dword ptr ds:[ebx+10]                   |
6F3827DE  | 3D 07030000              | cmp eax,307                                     | <--- 是否为 F8
6F3827E3  | 0F84 F1010000            | je game.6F3829DA                                |
6F3827E9  | 3D 00010000              | cmp eax,100                                     |
6F3827EE  | 0F84 E6010000            | je game.6F3829DA                                |
6F3827F4  | 8D4C24 6C                | lea ecx,dword ptr ss:[esp+6C]                   |
6F3827F8  | 51                       | push ecx                                        |
6F3827F9  | 8D5424 18                | lea edx,dword ptr ss:[esp+18]                   |
6F3827FD  | 52                       | push edx                                        |
6F3827FE  | 8D4424 20                | lea eax,dword ptr ss:[esp+20]                   |
6F382802  | 50                       | push eax                                        |
6F382803  | 8D4C24 30                | lea ecx,dword ptr ss:[esp+30]                   |
6F382807  | 51                       | push ecx                                        |
6F382808  | 83EC 20                  | sub esp,20                                      |
6F38280B  | 8BCC                     | mov ecx,esp                                     |
6F38280D  | 896424 58                | mov dword ptr ss:[esp+58],esp                   |
6F382811  | 53                       | push ebx                                        |
6F382812  | E8 8981FDFF              | call game.6F35A9A0                              |
6F382817  | 8B7C24 50                | mov edi,dword ptr ss:[esp+50]                   |
6F38281B  | 8BCF                     | mov ecx,edi                                     |
6F38281D  | C78424 94000000 05000000 | mov dword ptr ss:[esp+94],5                     |
6F382828  | E8 6373FBFF              | call game.6F339B90                              |
6F38282D  | 8B80 C8030000            | mov eax,dword ptr ds:[eax+3C8]                  |
6F382833  | 83CE FF                  | or esi,FFFFFFFF                                 |
6F382836  | 8BC8                     | mov ecx,eax                                     |
6F382838  | 89B424 94000000          | mov dword ptr ss:[esp+94],esi                   |
6F38283F  | E8 4CBBFDFF              | call game.6F35E390                              | <--- 判断是否为技能，并获取技能描述。
6F382844  | 3BC5                     | cmp eax,ebp                                     | <--- 技能和其他操作的判断，
6F382846  | 0F84 16010000            | je game.6F382962                                |
6F38284C  | 8B4424 18                | mov eax,dword ptr ss:[esp+18]                   | <--- 处理技能
6F382850  | 33F6                     | xor esi,esi                                     |
6F382852  | 3B05 DCED926F            | cmp eax,dword ptr ds:[6F92EDDC]                 | <--- 和 0xD0020 比较
6F382858  | 75 0A                    | jne game.6F382864                               |
6F38285A  | E8 91D10B00              | call game.6F43F9F0                              |
6F38285F  | E9 D7000000              | jmp game.6F38293B                               |
6F382864  | 3B05 E0ED926F            | cmp eax,dword ptr ds:[6F92EDE0]                 | 暂停条件(eax==000D001A)
6F38286A  | 0F84 C6000000            | je game.6F382936                                |
6F382870  | 3B05 E4ED926F            | cmp eax,dword ptr ds:[6F92EDE4]                 | 暂停条件(eax==000D001B)
6F382876  | 0F84 BA000000            | je game.6F382936                                |
6F38287C  | 3B05 E8ED926F            | cmp eax,dword ptr ds:[6F92EDE8]                 | 暂停条件(eax==000D001C)
6F382882  | 0F84 AE000000            | je game.6F382936                                |
6F382888  | 3B05 ECED926F            | cmp eax,dword ptr ds:[6F92EDEC]                 | 暂停条件(eax==000D001D)
6F38288E  | 0F84 A2000000            | je game.6F382936                                |
6F382894  | 3B05 F0ED926F            | cmp eax,dword ptr ds:[6F92EDF0]                 | 暂停条件(eax==000D001E)
6F38289A  | 0F84 96000000            | je game.6F382936                                |
6F3828A0  | 3B05 F4ED926F            | cmp eax,dword ptr ds:[6F92EDF4]                 | 暂停条件(eax==00D001F3)
6F3828A6  | 0F84 8A000000            | je game.6F382936                                |
6F3828AC  | 3B05 F8ED926F            | cmp eax,dword ptr ds:[6F92EDF8]                 | 暂停条件(eax==000D003B)
6F3828B2  | 74 78                    | je game.6F38292C                                |
6F3828B4  | 3B05 FCED926F            | cmp eax,dword ptr ds:[6F92EDFC]                 | 暂停条件(eax==000D003C)
6F3828BA  | 74 70                    | je game.6F38292C                                |
6F3828BC  | 3B05 00EE926F            | cmp eax,dword ptr ds:[6F92EE00]                 | 暂停条件(eax==000D003D)
6F3828C2  | 74 68                    | je game.6F38292C                                |
6F3828C4  | 3B05 04EE926F            | cmp eax,dword ptr ds:[6F92EE04]                 | 暂停条件(eax==000D003E)
6F3828CA  | 74 60                    | je game.6F38292C                                |
6F3828CC  | 3B05 08EE926F            | cmp eax,dword ptr ds:[6F92EE08]                 | 暂停条件(eax==000D003F)
6F3828D2  | 74 58                    | je game.6F38292C                                |
6F3828D4  | 3B05 0CEE926F            | cmp eax,dword ptr ds:[6F92EE0C]                 | 暂停条件(eax==000D0040)
6F3828DA  | 74 50                    | je game.6F38292C                                |
6F3828DC  | 3B05 10EE926F            | cmp eax,dword ptr ds:[6F92EE10]                 | 暂停条件(eax==000D0041)
6F3828E2  | 74 48                    | je game.6F38292C                                |
6F3828E4  | 3B05 14EE926F            | cmp eax,dword ptr ds:[6F92EE14]                 | 暂停条件(eax==000D0042)
6F3828EA  | 74 40                    | je game.6F38292C                                |
6F3828EC  | 3B05 18EE926F            | cmp eax,dword ptr ds:[6F92EE18]                 | 暂停条件(eax==000D0043)
6F3828F2  | 74 38                    | je game.6F38292C                                |
6F3828F4  | 3B05 1CEE926F            | cmp eax,dword ptr ds:[6F92EE1C]                 | 暂停条件(eax==000D0044)
6F3828FA  | 74 30                    | je game.6F38292C                                |
6F3828FC  | 3B05 20EE926F            | cmp eax,dword ptr ds:[6F92EE20]                 | 暂停条件(eax==000D0045)
6F382902  | 74 28                    | je game.6F38292C                                |
6F382904  | 3B05 24EE926F            | cmp eax,dword ptr ds:[6F92EE24]                 | 暂停条件(eax==000D0046)
6F38290A  | 74 20                    | je game.6F38292C                                |
6F38290C  | 3B05 28EE926F            | cmp eax,dword ptr ds:[6F92EE28]                 | 暂停条件(eax==000D001EE)
6F382912  | 74 18                    | je game.6F38292C                                |
6F382914  | 3B05 2CEE926F            | cmp eax,dword ptr ds:[6F92EE2C]                 | 暂停条件(eax==000D01EF)
6F38291A  | 74 10                    | je game.6F38292C                                |
6F38291C  | 3B05 30EE926F            | cmp eax,dword ptr ds:[6F92EE30]                 | 暂停条件(eax==000D01F0)
6F382922  | 74 08                    | je game.6F38292C                                |
6F382924  | 3B05 34EE926F            | cmp eax,dword ptr ds:[6F92EE34]                 | 暂停条件(eax==000D01F1)
6F38292A  | 75 0F                    | jne game.6F38293B                               |
6F38292C  | 8B7424 6C                | mov esi,dword ptr ss:[esp+6C]                   |
6F382930  | 896C24 6C                | mov dword ptr ss:[esp+6C],ebp                   |
6F382934  | EB 05                    | jmp game.6F38293B                               |
6F382936  | E8 25D10B00              | call game.6F43FA60                              |
6F38293B  | E8 B0E0FAFF              | call game.6F3309F0                              | <--- 处理声音
6F382940  | 8B5424 6C                | mov edx,dword ptr ss:[esp+6C]                   |
6F382944  | 8B4424 14                | mov eax,dword ptr ss:[esp+14]                   |
6F382948  | 8B4C24 18                | mov ecx,dword ptr ss:[esp+18]                   |
6F38294C  | 55                       | push ebp                                        |
6F38294D  | 56                       | push esi                                        |
6F38294E  | 52                       | push edx                                        |
6F38294F  | 8B5424 30                | mov edx,dword ptr ss:[esp+30]                   |
6F382953  | 50                       | push eax                                        |
6F382954  | 51                       | push ecx                                        |
6F382955  | 52                       | push edx                                        |
6F382956  | 8BCF                     | mov ecx,edi                                     |
6F382958  | E8 C39AFFFF              | call game.6F37C420                              | <--- 关键函数：技能施法
6F38295D  | E9 88000000              | jmp game.6F3829EA                               |
6F382962  | 8D4424 6C                | lea eax,dword ptr ss:[esp+6C]                   | <--- 物品键和其他按键
6F382966  | 50                       | push eax                                        |
6F382967  | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                   |
6F38296B  | 51                       | push ecx                                        |
6F38296C  | 8D5424 20                | lea edx,dword ptr ss:[esp+20]                   |
6F382970  | 52                       | push edx                                        |
6F382971  | 8D4424 30                | lea eax,dword ptr ss:[esp+30]                   |
6F382975  | 50                       | push eax                                        |
6F382976  | 83EC 20                  | sub esp,20                                      |
6F382979  | 8BCC                     | mov ecx,esp                                     |
6F38297B  | 896424 58                | mov dword ptr ss:[esp+58],esp                   |
6F38297F  | 53                       | push ebx                                        |
6F382980  | E8 1B80FDFF              | call game.6F35A9A0                              | <--- 直接返回将影响物品使用
6F382985  | 8BCF                     | mov ecx,edi                                     |
6F382987  | C78424 94000000 06000000 | mov dword ptr ss:[esp+94],6                     |
6F382992  | E8 F971FBFF              | call game.6F339B90                              | <--- 获取 GameUI 对象
6F382997  | 8B80 C4030000            | mov eax,dword ptr ds:[eax+3C4]                  |
6F38299D  | 8B80 48010000            | mov eax,dword ptr ds:[eax+148]                  |
6F3829A3  | 8BC8                     | mov ecx,eax                                     |
6F3829A5  | 89B424 94000000          | mov dword ptr ss:[esp+94],esi                   |
6F3829AC  | E8 1FE3FDFF              | call game.6F360CD0                              | <--- 关键函数：物品使用
6F3829B1  | 3BC5                     | cmp eax,ebp                                     |
6F3829B3  | 74 30                    | je game.6F3829E5                                |
6F3829B5  | E8 36E0FAFF              | call game.6F3309F0                              | <--- 处理操作失败
6F3829BA  | 8B4C24 6C                | mov ecx,dword ptr ss:[esp+6C]                   |
6F3829BE  | 8B5424 14                | mov edx,dword ptr ss:[esp+14]                   |
6F3829C2  | 8B4424 18                | mov eax,dword ptr ss:[esp+18]                   |
6F3829C6  | 6A 01                    | push 1                                          |
6F3829C8  | 55                       | push ebp                                        |
6F3829C9  | 51                       | push ecx                                        |
6F3829CA  | 8B4C24 30                | mov ecx,dword ptr ss:[esp+30]                   |
6F3829CE  | 52                       | push edx                                        |
6F3829CF  | 50                       | push eax                                        |
6F3829D0  | 51                       | push ecx                                        |
6F3829D1  | 8BCF                     | mov ecx,edi                                     |
6F3829D3  | E8 489AFFFF              | call game.6F37C420                              |
6F3829D8  | EB 10                    | jmp game.6F3829EA                               |
6F3829DA  | 8B4C24 14                | mov ecx,dword ptr ss:[esp+14]                   |
6F3829DE  | 6A 01                    | push 1                                          |
6F3829E0  | E8 6B14F7FF              | call game.6F2F3E50                              |
6F3829E5  | B8 01000000              | mov eax,1                                       | <--- 操作成功
6F3829EA  | 8B4C24 5C                | mov ecx,dword ptr ss:[esp+5C]                   |
6F3829EE  | 64:890D 00000000         | mov dword ptr fs:[0],ecx                        |
6F3829F5  | 59                       | pop ecx                                         |
6F3829F6  | 5F                       | pop edi                                         |
6F3829F7  | 5E                       | pop esi                                         |
6F3829F8  | 5D                       | pop ebp                                         |
6F3829F9  | 5B                       | pop ebx                                         |
6F3829FA  | 83C4 54                  | add esp,54                                      |
6F3829FD  | C2 0400                  | ret 4                                           |
6F382A00  | 1223                     | adc ah,byte ptr ds:[ebx]                        |
6F382A02  | 386F F9                  | cmp byte ptr ds:[edi-7],ch                      |
6F382A05  | 2138                     | and dword ptr ds:[eax],edi                      |
6F382A07  | 6F                       | outsd                                           |
6F382A08  | BB 22386FC2              | mov ebx,C26F3822                                |
6F382A0D  | 2238                     | and bh,byte ptr ds:[eax]                        |
6F382A0F  | 6F                       | outsd                                           |
6F382A10  | CC                       | int3                                            |
6F382A11  | 2238                     | and bh,byte ptr ds:[eax]                        |
6F382A13  | 6F                       | outsd                                           |
6F382A14  | D6                       | salc                                            |
6F382A15  | 2238                     | and bh,byte ptr ds:[eax]                        |
6F382A17  | 6F                       | outsd                                           |
6F382A18  | E0 22                    | loopne game.6F382A3C                            |
6F382A1A  | 386F EA                  | cmp byte ptr ds:[edi-16],ch                     |
6F382A1D  | 2238                     | and bh,byte ptr ds:[eax]                        |
6F382A1F  | 6F                       | outsd                                           |
6F382A20  | F4                       | hlt                                             |
6F382A21  | 2238                     | and bh,byte ptr ds:[eax]                        |
6F382A23  | 6F                       | outsd                                           |
6F382A24  | FE                       | ???                                             |
6F382A25  | 2238                     | and bh,byte ptr ds:[eax]                        |
6F382A27  | 6F                       | outsd                                           |
6F382A28  | 0823                     | or byte ptr ds:[ebx],ah                         |
6F382A2A  | 386F 85                  | cmp byte ptr ds:[edi-7B],ch                     |
6F382A2D  | 2338                     | and edi,dword ptr ds:[eax]                      |
6F382A2F  | 6F                       | outsd                                           |
6F382A30  | 000B                     | add byte ptr ds:[ebx],cl                        |
6F382A32  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A34  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A36  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A38  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A3A  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A3C  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A3E  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A40  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A42  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A44  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A46  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A48  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A4A  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A4C  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A4E  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A50  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A52  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A54  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A56  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A58  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A5A  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A5C  | 0B0B                     | or ecx,dword ptr ds:[ebx]                       |
6F382A5E  | 0102                     | add dword ptr ds:[edx],eax                      |
6F382A60  | 030405 06070809          | add eax,dword ptr ds:[eax+9080706]              |
6F382A67  | 0ACC                     | or cl,ah                                        |
```

## 3. C++ 伪代码还原

```cpp
// 键盘消息主处理函数
int __stdcall HighLevelInputHandler(InputMsg* pMsg)
{
    GameUI* pUI = GetGameUI();
    if (!pUI->IsGameRunning()) return 0;

    int keyCode = pMsg->virtualKey; // 获取按键

    // 1. 处理聊天框状态 (代码略，通常在头部检查)
    if (pUI->IsChatBoxOpen()) return HandleChatInput(keyCode);

    // 2. 处理特殊键
    if (IsAltPressed()) {
        HandleAltFunctions(keyCode); // 显血、小地图信号等
        return 1;
    }
    
    if (keyCode == VK_TAB) {
        SwitchSelectedUnitSubgroup();
        return 1;
    }

    if (keyCode >= VK_F1 && keyCode <= VK_F5) {
        SelectHero(keyCode - VK_F1);
        return 1;
    }

    // 3. [优先级 High] 技能施法判定
    // 检查当前选中单位的命令卡(Command Card)中是否有匹配该按键的技能
    Ability* pAbility = FindAbilityByHotkey(pUI->pSelection, keyCode); // 6F35E390
    
    if (pAbility) {
        // 如果是技能键，执行技能逻辑
        CastAbility(pUI->pSelection, pAbility); // 6F37C420
        return 1;
    }

    // 4. [优先级 Low] 物品使用判定 (Fall-through logic)
    // 如果按键没被技能“吃掉”，才轮到物品栏检查
    // 这里调用了我们之前分析的函数 6F360CD0
    bool itemUsed = ProcessInventoryKey(pUI, keyCode); // 6F360CD0
    
    if (itemUsed) {
        return 1;
    }

    return 0; // 未处理的按键
}
```

## 4. 逆向结论与修改建议

### 4.1 输入优先级的启示
代码结构清晰地展示了 **技能 > 物品** 的优先级：
*   代码先调用 `6F35E390` (查技能)。
*   只有当返回 0 (未找到技能) 时，才会 `je game.6F382962` 跳到物品处理。
*   **改建提示**：如果你将物品改建为 `Q`，而当前英雄有一个快捷键也是 `Q` 的技能，按下 `Q` 只会释放技能，物品逻辑永远不会被执行。

### 4.2 完美的 Hook 点
如果你想做一个**全局改建**（即可以拦截所有操作，或者改变优先级），这个函数 `6F382150` 是比 `6F360CD0` 更高层的 Hook 点。

*   **Hook `6F3821C6` 之后**：此时你拿到了 `eax` (按键码)。
    *   你可以将 `Q` 替换为 `Numpad 7`。
    *   **风险**：如果此时 `eax` 被改为 `Numpad 7`，那么**技能判定**也会去查有没有快捷键是 `Numpad 7` 的技能（通常没有），然后判定失败，最后落入物品判定，成功使用物品。
    *   **优势**：这种改建方式非常稳定，且符合游戏原生流程。

### 4.3 技能改建的线索
在 `6F38283F` 调用的 `6F35E390` 是技能判定的核心。如果你想修改技能的快捷键（例如把原本 `W` 的技能改成 `F`），你需要深入分析 `6F35E390`，它内部必然也读取了技能数据结构中的 `Hotkey` 字段（类似于物品的 `0x5AC` 偏移）。
```
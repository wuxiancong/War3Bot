# War3 创建房间后点击列表选项弹出找不到游戏的对话框调试日志

![x64dbg调试截图](https://github.com/wuxiancong/War3Bot/raw/main/debug/images/War3Dialog_NETERROR_JOINGAMENOTFOUND.PNG)


## 问题描述 (Issue Description)

在使用自定义机器人（或通过 Bot 代理的真人玩家）在 PVPGN 服务端创建房间时：
1.  房间可以正常显示在游戏列表中。
2.  **当点击列表选项时显示网络错误的对话框。**
3.  尝试点击“加入游戏”时，客户端弹出错误提示：**"The game you attempted to join could not be found./n/nYou may have entered the name incorrectly or the game creator may have canceled the game."** (您尝试加入的游戏未找到。/n/您输入的名称可能不正确，或者游戏创建者可能已取消了该游戏。)。

**环境 (Environment):**
- **客户端:** Warcraft III (1.26a / Game.dll)
- **服务端:** PVPGN (C++)
- **调试工具:** x64dbg
- **基地址:** 023C0000
- **关键函数地址:** 
    - 加入按钮逻辑: `Game.dll + 5C9650`
    - 列表选中逻辑: `Game.dll + 5B51D0`
    - 状态解析逻辑: `Game.dll + 5BE710`

---

## 1. 逆向分析：为何点击“加入游戏”报错？

**断点位置:** `02A79650` (加入游戏按钮处理函数)

当我们点击加入按钮时，触发了错误弹窗 "The game you attempted to join could not be found./n/nYou may have entered the name incorrectly or the game creator may have canceled the game."。通过反汇编分析，发现是因为**xxx**。

```assembly
02A79650 | 53                       | push ebx                                   |
02A79651 | 56                       | push esi                                   |
02A79652 | 8BF1                     | mov esi,ecx                                |
02A79654 | 85F6                     | test esi,esi                               |
02A79656 | 57                       | push edi                                   |
02A79657 | 8BFA                     | mov edi,edx                                |
02A79659 | 74 52                    | je game.2A796AD                            |
02A7965B | 803E 00                  | cmp byte ptr ds:[esi],0                    |
02A7965E | 74 4D                    | je game.2A796AD                            |
02A79660 | 85FF                     | test edi,edi                               |
02A79662 | 74 49                    | je game.2A796AD                            |
02A79664 | 8B5C24 10                | mov ebx,dword ptr ss:[esp+10]              |
02A79668 | 85DB                     | test ebx,ebx                               |
02A7966A | 74 41                    | je game.2A796AD                            |
02A7966C | 56                       | push esi                                   |
02A7966D | B9 3CD2F702              | mov ecx,game.2F7D23C                       |
02A79672 | C607 00                  | mov byte ptr ds:[edi],0                    |
02A79675 | E8 56F8FFFF              | call game.2A78ED0                          |
02A7967A | 85C0                     | test eax,eax                               |
02A7967C | 74 06                    | je game.2A79684                            |
02A7967E | 8378 1C 00               | cmp dword ptr ds:[eax+1C],0                |
02A79682 | 75 0B                    | jne game.2A7968F                           |
02A79684 | 56                       | push esi                                   |
02A79685 | B9 14D2F702              | mov ecx,game.2F7D214                       |
02A7968A | E8 41F8FFFF              | call game.2A78ED0                          |
02A7968F | 85C0                     | test eax,eax                               |
02A79691 | 74 1A                    | je game.2A796AD                            |
02A79693 | 8B40 1C                  | mov eax,dword ptr ds:[eax+1C]              |
02A79696 | 85C0                     | test eax,eax                               |
02A79698 | 74 13                    | je game.2A796AD                            |
02A7969A | 53                       | push ebx                                   |
02A7969B | 50                       | push eax                                   |
02A7969C | 57                       | push edi                                   |
02A7969D | E8 221F1200              | call <JMP.&Ordinal#501>                    |
02A796A2 | 5F                       | pop edi                                    |
02A796A3 | 5E                       | pop esi                                    |
02A796A4 | B8 01000000              | mov eax,1                                  |
02A796A9 | 5B                       | pop ebx                                    |
02A796AA | C2 0400                  | ret 4                                      |
02A796AD | 5F                       | pop edi                                    |
02A796AE | 5E                       | pop esi                                    |
02A796AF | 33C0                     | xor eax,eax                                |
02A796B1 | 5B                       | pop ebx                                    |
02A796B2 | C2 0400                  | ret 4                                      |
```


```assembly
02A651D0 | 81EC 04020000            | sub esp,204                                |
02A651D6 | A1 40E1F502              | mov eax,dword ptr ds:[2F5E140]             |
02A651DB | 33C4                     | xor eax,esp                                |
02A651DD | 898424 00020000          | mov dword ptr ss:[esp+200],eax             |
02A651E4 | 56                       | push esi                                   |
02A651E5 | 8BF1                     | mov esi,ecx                                |
02A651E7 | 57                       | push edi                                   |
02A651E8 | 6A 00                    | push 0                                     |
02A651EA | 8BD6                     | mov edx,esi                                |
02A651EC | B9 78000940              | mov ecx,40090078                           |
02A651F1 | C786 70010000 00000000   | mov dword ptr ds:[esi+170],0               |
02A651FB | E8 90A1F8FF              | call game.29EF390                          |
02A65200 | 6A 01                    | push 1                                     |
02A65202 | 8BD6                     | mov edx,esi                                |
02A65204 | B9 78000940              | mov ecx,40090078                           |
02A65209 | E8 82A1F8FF              | call game.29EF390                          |
02A6520E | 8B8424 10020000          | mov eax,dword ptr ss:[esp+210]             |
02A65215 | 8B78 14                  | mov edi,dword ptr ds:[eax+14]              |
02A65218 | 8D47 FF                  | lea eax,dword ptr ds:[edi-1]               |
02A6521B | 83F8 2A                  | cmp eax,2A                                 |
02A6521E | 77 75                    | ja game.2A65295                            |
02A65220 | 0FB688 3453A602          | movzx ecx,byte ptr ds:[eax+2A65334]        |
02A65227 | FF248D 0853A602          | jmp dword ptr ds:[ecx*4+2A65308]           |
02A6522E | E8 8D62FFFF              | call game.2A5B4C0                          |
02A65233 | B9 01000000              | mov ecx,1                                  |
02A65238 | C740 18 00000000         | mov dword ptr ds:[eax+18],0                |
02A6523F | C740 14 01000000         | mov dword ptr ds:[eax+14],1                |
02A65246 | E8 F59FF8FF              | call game.29EF240                          |
02A6524B | 6A FE                    | push FFFFFFFE                              |
02A6524D | 8BCE                     | mov ecx,esi                                |
02A6524F | E8 4CCDFDFF              | call game.2A41FA0                          |
02A65254 | EB 52                    | jmp game.2A652A8                           |
02A65256 | B9 6491E102              | mov ecx,game.2E19164                       | <--- "NETERROR_JOINGAMENOTFOUND"
02A6525B | EB 3D                    | jmp game.2A6529A                           | <--- 
02A6525D | B9 188BE102              | mov ecx,game.2E18B18                       | <--- "ERROR_ID_GAMEPORTINUSE"
02A65262 | EB 36                    | jmp game.2A6529A                           | <--- 
02A65264 | B9 7C05E102              | mov ecx,game.2E1057C                       | <--- "ERROR_ID_AUTOCANCEL"
02A65269 | EB 2F                    | jmp game.2A6529A                           | <--- 
02A6526B | B9 381AE102              | mov ecx,game.2E11A38                       | <--- "ERROR_ID_BADPASSWORD"
02A65270 | EB 28                    | jmp game.2A6529A                           | <--- 
02A65272 | B9 3491E102              | mov ecx,game.2E19134                       | <--- "ERROR_ID_GAMEUNJOINABLE"
02A65277 | EB 21                    | jmp game.2A6529A                           | <--- 
02A65279 | B9 8C61E102              | mov ecx,game.2E1618C                       | <--- "ERROR_ID_GAMEFULL"
02A6527E | EB 1A                    | jmp game.2A6529A                           | <--- 
02A65280 | B9 048BE102              | mov ecx,game.2E18B04                       | <--- "ERROR_ID_GAMECLOSED"
02A65285 | EB 13                    | jmp game.2A6529A                           | <--- 
02A65287 | B9 8091E102              | mov ecx,game.2E19180                       | <--- "ERROR_ID_BADCDKEY"
02A6528C | EB 0C                    | jmp game.2A6529A                           | <--- 
02A6528E | B9 4C91E102              | mov ecx,game.2E1914C                       | <--- "ERROR_ID_REQUESTDENIED"
02A65293 | EB 05                    | jmp game.2A6529A                           | <--- 
02A65295 | B9 7461E102              | mov ecx,game.2E16174                       | <--- "NETERROR_JOINGAMEFAILED"
02A6529A | 68 00020000              | push 200                                   |
02A6529F | 8D5424 0C                | lea edx,dword ptr ss:[esp+C]               |
02A652A3 | E8 A8430100              | call game.2A79650                          |
02A652A8 | 83FF 01                  | cmp edi,1                                  |
02A652AB | 74 3C                    | je game.2A652E9                            |
02A652AD | 6A 03                    | push 3                                     |
02A652AF | 8D8E 74010000            | lea ecx,dword ptr ds:[esi+174]             |
02A652B5 | E8 96230600              | call game.2AC7650                          |
02A652BA | 6A 01                    | push 1                                     |
02A652BC | 6A 04                    | push 4                                     |
02A652BE | 6A 00                    | push 0                                     |
02A652C0 | 56                       | push esi                                   |
02A652C1 | 6A 09                    | push 9                                     |
02A652C3 | 8D5424 1C                | lea edx,dword ptr ss:[esp+1C]              |
02A652C7 | B9 01000000              | mov ecx,1                                  |
02A652CC | E8 DF7BFAFF              | call game.2A0CEB0                          |
02A652D1 | 8B96 04020000            | mov edx,dword ptr ds:[esi+204]             |
02A652D7 | 8996 FC010000            | mov dword ptr ds:[esi+1FC],edx             |
02A652DD | 33D2                     | xor edx,edx                                |
02A652DF | 6A 00                    | push 0                                     |
02A652E1 | 8D4A 07                  | lea ecx,dword ptr ds:[edx+7]               |
02A652E4 | E8 F774F9FF              | call game.29FC7E0                          |
02A652E9 | 8B8C24 08020000          | mov ecx,dword ptr ss:[esp+208]             |
02A652F0 | 5F                       | pop edi                                    |
02A652F1 | 5E                       | pop esi                                    |
02A652F2 | 33CC                     | xor ecx,esp                                |
02A652F4 | B8 01000000              | mov eax,1                                  |
02A652F9 | E8 5BBD2200              | call game.2C91059                          |
02A652FE | 81C4 04020000            | add esp,204                                |
02A65304 | C2 0400                  | ret 4                                      |
02A65307 | 90                       | nop                                        |
02A65308 | 2E:52                    | push edx                                   |
02A6530A | A6                       | cmpsb                                      |
02A6530B | 026452 A6                | add ah,byte ptr ds:[edx+edx*2-5A]          |
02A6530F | 0256 52                  | add dl,byte ptr ds:[esi+52]                |
02A65312 | A6                       | cmpsb                                      |
02A65313 | 0272 52                  | add dh,byte ptr ds:[edx+52]                |
02A65316 | A6                       | cmpsb                                      |
02A65317 | 0279 52                  | add bh,byte ptr ds:[ecx+52]                |
02A6531A | A6                       | cmpsb                                      |
02A6531B | 0280 52A6025D            | add al,byte ptr ds:[eax+5D02A652]          |
02A65321 | 52                       | push edx                                   |
02A65322 | A6                       | cmpsb                                      |
02A65323 | 026B 52                  | add ch,byte ptr ds:[ebx+52]                |
02A65326 | A6                       | cmpsb                                      |
02A65327 | 0287 52A6028E            | add al,byte ptr ds:[edi-71FD59AE]          |
02A6532D | 52                       | push edx                                   |
02A6532E | A6                       | cmpsb                                      |
02A6532F | 0295 52A60200            | add dl,byte ptr ss:[ebp+2A652]             |
02A65335 | 010A                     | add dword ptr ds:[edx],ecx                 |
02A65337 | 0A0A                     | or cl,byte ptr ds:[edx]                    |
02A65339 | 0A02                     | or al,byte ptr ds:[edx]                    |
02A6533B | 030405 0202060A          | add eax,dword ptr ds:[eax+A060202]         |
02A65342 | 0A0A                     | or cl,byte ptr ds:[edx]                    |
02A65344 | 0A0A                     | or cl,byte ptr ds:[edx]                    |
02A65346 | 0A0A                     | or cl,byte ptr ds:[edx]                    |
02A65348 | 0A0A                     | or cl,byte ptr ds:[edx]                    |
02A6534A | 0A0A                     | or cl,byte ptr ds:[edx]                    |
02A6534C | 0A0A                     | or cl,byte ptr ds:[edx]                    |
02A6534E | 07                       | pop es                                     |
02A6534F | 080A                     | or byte ptr ds:[edx],cl                    |
02A65351 | 0A0A                     | or cl,byte ptr ds:[edx]                    |
02A65353 | 0A0A                     | or cl,byte ptr ds:[edx]                    |
02A65355 | 0A0A                     | or cl,byte ptr ds:[edx]                    |
02A65357 | 0A0A                     | or cl,byte ptr ds:[edx]                    |
02A65359 | 0A0A                     | or cl,byte ptr ds:[edx]                    |
02A6535B | 0A0A                     | or cl,byte ptr ds:[edx]                    |
02A6535D | 0A09                     | or cl,byte ptr ds:[ecx]                    |
```

```assembly
029DABF0 | 8B4424 04                | mov eax,dword ptr ss:[esp+4]               |
029DABF4 | 56                       | push esi                                   |
029DABF5 | 8BF1                     | mov esi,ecx                                |
029DABF7 | 8B48 08                  | mov ecx,dword ptr ds:[eax+8]               |
029DABFA | 83F9 17                  | cmp ecx,17                                 |
029DABFD | 0F87 B2010000            | ja game.29DADB5                            |
029DAC03 | FF248D BCAD9D02          | jmp dword ptr ds:[ecx*4+29DADBC]           | <--- 处理分支
029DAC0A | 8BCE                     | mov ecx,esi                                |
029DAC0C | E8 8F70FDFF              | call game.29B1CA0                          | <--- 选择列表
029DAC11 | 5E                       | pop esi                                    |
029DAC12 | C2 0400                  | ret 4                                      |
029DAC15 | 8BCE                     | mov ecx,esi                                | <--- 键盘输入
029DAC17 | E8 A472FDFF              | call game.29B1EC0                          |
029DAC1C | 5E                       | pop esi                                    |
029DAC1D | C2 0400                  | ret 4                                      |
029DAC20 | 8BCE                     | mov ecx,esi                                | <--- 创建游戏
029DAC22 | E8 19D0FDFF              | call game.29B7C40                          |
029DAC27 | 5E                       | pop esi                                    |
029DAC28 | C2 0400                  | ret 4                                      |
029DAC2B | 8BCE                     | mov ecx,esi                                | <--- 装载游戏
029DAC2D | E8 FECFFDFF              | call game.29B7C30                          |
029DAC32 | 5E                       | pop esi                                    |
029DAC33 | C2 0400                  | ret 4                                      |
029DAC36 | 6A 00                    | push 0                                     | <--- 加入游戏
029DAC38 | 8BCE                     | mov ecx,esi                                |
029DAC3A | E8 71A3FFFF              | call game.29D4FB0                          |
029DAC3F | 5E                       | pop esi                                    |
029DAC40 | C2 0400                  | ret 4                                      |
029DAC43 | 8BCE                     | mov ecx,esi                                | <--- 返回大厅
029DAC45 | E8 06D0FDFF              | call game.29B7C50                          |
029DAC4A | 5E                       | pop esi                                    |
029DAC4B | C2 0400                  | ret 4                                      |
029DAC4E | 8BCE                     | mov ecx,esi                                |
029DAC50 | E8 9B40FEFF              | call game.29BECF0                          |
029DAC55 | 5E                       | pop esi                                    |
029DAC56 | C2 0400                  | ret 4                                      |
029DAC59 | 8BCE                     | mov ecx,esi                                | <--- 弹出错误对话框 （如：必须选择一个游戏加入）
029DAC5B | E8 30B0FBFF              | call game.2995C90                          |
029DAC60 | 5E                       | pop esi                                    |
029DAC61 | C2 0400                  | ret 4                                      |
029DAC64 | 8BCE                     | mov ecx,esi                                | <--- 设置过滤
029DAC66 | E8 35B5FCFF              | call game.29A61A0                          |
029DAC6B | 5E                       | pop esi                                    |
029DAC6C | C2 0400                  | ret 4                                      |
029DAC6F | 8BCE                     | mov ecx,esi                                | <--- 刷新列表开始
029DAC71 | E8 0A70FDFF              | call game.29B1C80                          |
029DAC76 | 5E                       | pop esi                                    |
029DAC77 | C2 0400                  | ret 4                                      |
029DAC7A | 8BCE                     | mov ecx,esi                                | <--- 设置过滤确认
029DAC7C | E8 DF6FFDFF              | call game.29B1C60                          |
029DAC81 | 5E                       | pop esi                                    |
029DAC82 | C2 0400                  | ret 4                                      |
029DAC85 | 8BCE                     | mov ecx,esi                                | <--- 设置过滤取消
029DAC87 | E8 B4B4FCFF              | call game.29A6140                          |
029DAC8C | 5E                       | pop esi                                    |
029DAC8D | C2 0400                  | ret 4                                      |
029DAC90 | 8BCE                     | mov ecx,esi                                | <--- 搜索游戏名
029DAC92 | E8 B9AFFBFF              | call game.2995C50                          |
029DAC97 | 5E                       | pop esi                                    |
029DAC98 | C2 0400                  | ret 4                                      |
029DAC9B | 50                       | push eax                                   |
029DAC9C | 8BCE                     | mov ecx,esi                                |
029DAC9E | E8 7DE8FFFF              | call game.29D9520                          |
029DACA3 | 5E                       | pop esi                                    |
029DACA4 | C2 0400                  | ret 4                                      |
029DACA7 | 50                       | push eax                                   | <--- 获取列表结束 循环填入数据
029DACA8 | 8BCE                     | mov ecx,esi                                |
029DACAA | E8 01B5FCFF              | call game.29A61B0                          |
029DACAF | 5E                       | pop esi                                    |
029DACB0 | C2 0400                  | ret 4                                      |
029DACB3 | 50                       | push eax                                   | <--- 刷新列表完毕
029DACB4 | 8BCE                     | mov ecx,esi                                |
029DACB6 | E8 85CEFDFF              | call game.29B7B40                          |
029DACBB | 5E                       | pop esi                                    |
029DACBC | C2 0400                  | ret 4                                      |
029DACBF | 50                       | push eax                                   |
029DACC0 | 8BCE                     | mov ecx,esi                                |
029DACC2 | E8 3972FDFF              | call game.29B1F00                          |
029DACC7 | 5E                       | pop esi                                    |
029DACC8 | C2 0400                  | ret 4                                      |
029DACCB | 50                       | push eax                                   | <--- 弹出错误对话框 （如：您尝试加入的游戏未找到。/n/您输入的名称可能不正确，或者游戏创建者可能已取消了该游戏。）
029DACCC | 8BCE                     | mov ecx,esi                                |
029DACCE | E8 FDA4FFFF              | call game.29D51D0                          |
029DACD3 | 5E                       | pop esi                                    |
029DACD4 | C2 0400                  | ret 4                                      |
029DACD7 | 50                       | push eax                                   | <--- 搜索游戏不存在
029DACD8 | 8BCE                     | mov ecx,esi                                |
029DACDA | E8 61A0FFFF              | call game.29D4D40                          |
029DACDF | 5E                       | pop esi                                    |
029DACE0 | C2 0400                  | ret 4                                      |
029DACE3 | 8B06                     | mov eax,dword ptr ds:[esi]                 | <--- 进入自定义游戏
029DACE5 | 8B90 D4000000            | mov edx,dword ptr ds:[eax+D4]              |
029DACEB | 8BCE                     | mov ecx,esi                                |
029DACED | FFD2                     | call edx                                   |
029DACEF | 6A 16                    | push 16                                    |
029DACF1 | 56                       | push esi                                   |
029DACF2 | 6A 00                    | push 0                                     |
029DACF4 | BA 08A2D702              | mov edx,game.2D7A208                       |
029DACF9 | B9 00A2D702              | mov ecx,game.2D7A200                       |
029DACFE | E8 2DF1A4FF              | call game.2429E30                          |
029DAD03 | 51                       | push ecx                                   |
029DAD04 | D91C24                   | fstp dword ptr ss:[esp]                    |
029DAD07 | 56                       | push esi                                   |
029DAD08 | 8D8E B8010000            | lea ecx,dword ptr ds:[esi+1B8]             |
029DAD0E | E8 BD9AD5FF              | call game.27347D0                          |
029DAD13 | B8 01000000              | mov eax,1                                  |
029DAD18 | 5E                       | pop esi                                    |
029DAD19 | C2 0400                  | ret 4                                      |
029DAD1C | 6A 01                    | push 1                                     | <--- 获取列表开始 发起网络请求
029DAD1E | 8BCE                     | mov ecx,esi                                |
029DAD20 | E8 4B6EFDFF              | call game.29B1B70                          |
029DAD25 | 8B8E 04020000            | mov ecx,dword ptr ds:[esi+204]             |
029DAD2B | 6A 00                    | push 0                                     |
029DAD2D | 6A 00                    | push 0                                     |
029DAD2F | 6A 01                    | push 1                                     |
029DAD31 | E8 DA010400              | call game.2A1AF10                          |
029DAD36 | 830D 18A4EB02 01         | or dword ptr ds:[2EBA418],1                |
029DAD3D | B8 01000000              | mov eax,1                                  |
029DAD42 | 5E                       | pop esi                                    |
029DAD43 | C2 0400                  | ret 4                                      |
029DAD46 | 8B06                     | mov eax,dword ptr ds:[esi]                 | <--- 画面切换开始
029DAD48 | 8B90 D0000000            | mov edx,dword ptr ds:[eax+D0]              |
029DAD4E | 8BCE                     | mov ecx,esi                                |
029DAD50 | FFD2                     | call edx                                   |
029DAD52 | 8BCE                     | mov ecx,esi                                |
029DAD54 | E8 A7CFFDFF              | call game.29B7D00                          |
029DAD59 | B8 01000000              | mov eax,1                                  |
029DAD5E | 5E                       | pop esi                                    |
029DAD5F | C2 0400                  | ret 4                                      |
029DAD62 | 830D 18A4EB02 01         | or dword ptr ds:[2EBA418],1                | <--- 画面切换完毕
029DAD69 | 8B86 44020000            | mov eax,dword ptr ds:[esi+244]             |
029DAD6F | 83F8 FE                  | cmp eax,FFFFFFFE                           |
029DAD72 | 8B8E 68010000            | mov ecx,dword ptr ds:[esi+168]             |
029DAD78 | 74 2B                    | je game.29DADA5                            |
029DAD7A | 83F8 FF                  | cmp eax,FFFFFFFF                           |
029DAD7D | 74 16                    | je game.29DAD95                            |
029DAD7F | 8B0485 18FBD702          | mov eax,dword ptr ds:[eax*4+2D7FB18]       |
029DAD86 | 50                       | push eax                                   |
029DAD87 | E8 9482FBFF              | call game.2993020                          |
029DAD8C | B8 01000000              | mov eax,1                                  |
029DAD91 | 5E                       | pop esi                                    |
029DAD92 | C2 0400                  | ret 4                                      |
029DAD95 | 6A 02                    | push 2                                     |
029DAD97 | E8 4483FEFF              | call game.29C30E0                          |
029DAD9C | B8 01000000              | mov eax,1                                  |
029DADA1 | 5E                       | pop esi                                    |
029DADA2 | C2 0400                  | ret 4                                      |
029DADA5 | 6A 0B                    | push B                                     |
029DADA7 | E8 3483FEFF              | call game.29C30E0                          |
029DADAC | B8 01000000              | mov eax,1                                  |
029DADB1 | 5E                       | pop esi                                    |
029DADB2 | C2 0400                  | ret 4                                      |
029DADB5 | 33C0                     | xor eax,eax                                |
029DADB7 | 5E                       | pop esi                                    |
029DADB8 | C2 0400                  | ret 4                                      |
029DADBB | 90                       | nop                                        |
029DADBC | 0AAC9D 0215AC9D          | or ch,byte ptr ss:[ebp+ebx*4-6253EAFE]     |
029DADC3 | 0220                     | add ah,byte ptr ds:[eax]                   |
029DADC5 | AC                       | lodsb                                      |
029DADC6 | 9D                       | popfd                                      |
029DADC7 | 022B                     | add ch,byte ptr ds:[ebx]                   |
029DADC9 | AC                       | lodsb                                      |
029DADCA | 9D                       | popfd                                      |
029DADCB | 0236                     | add dh,byte ptr ds:[esi]                   |
029DADCD | AC                       | lodsb                                      |
029DADCE | 9D                       | popfd                                      |
029DADCF | 0264AC 9D                | add ah,byte ptr ss:[esp+ebp*4-63]          |
029DADD3 | 026F AC                  | add ch,byte ptr ds:[edi-54]                |
029DADD6 | 9D                       | popfd                                      |
029DADD7 | 0243 AC                  | add al,byte ptr ds:[ebx-54]                |
029DADDA | 9D                       | popfd                                      |
029DADDB | 024E AC                  | add cl,byte ptr ds:[esi-54]                |
029DADDE | 9D                       | popfd                                      |
029DADDF | 0259 AC                  | add bl,byte ptr ds:[ecx-54]                |
029DADE2 | 9D                       | popfd                                      |
029DADE3 | 027A AC                  | add bh,byte ptr ds:[edx-54]                |
029DADE6 | 9D                       | popfd                                      |
029DADE7 | 0285 AC9D0290            | add al,byte ptr ss:[ebp-6FFD6254]          |
029DADED | AC                       | lodsb                                      |
029DADEE | 9D                       | popfd                                      |
029DADEF | 029B AC9D02CB            | add bl,byte ptr ds:[ebx-34FD6254]          |
029DADF5 | AC                       | lodsb                                      |
029DADF6 | 9D                       | popfd                                      |
029DADF7 | 02B5 AD9D02A7            | add dh,byte ptr ss:[ebp-58FD6253]          |
029DADFD | AC                       | lodsb                                      |
029DADFE | 9D                       | popfd                                      |
029DADFF | 02B3 AC9D02BF            | add dh,byte ptr ds:[ebx-40FD6254]          |
029DAE05 | AC                       | lodsb                                      |
029DAE06 | 9D                       | popfd                                      |
029DAE07 | 02D7                     | add dl,bh                                  |
029DAE09 | AC                       | lodsb                                      |
029DAE0A | 9D                       | popfd                                      |
029DAE0B | 02E3                     | add ah,bl                                  |
029DAE0D | AC                       | lodsb                                      |
029DAE0E | 9D                       | popfd                                      |
029DAE0F | 0262 AD                  | add ah,byte ptr ds:[edx-53]                |
029DAE12 | 9D                       | popfd                                      |
029DAE13 | 021CAD 9D0246AD          | add bl,byte ptr ds:[ebp*4-52B9FD63]        |
029DAE1A | 9D                       | popfd                                      |
029DAE1B | 02CC                     | add cl,ah                                  |
```
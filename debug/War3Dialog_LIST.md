# War3 机器人主机创建房间后地图信息空白调试日志

![x64dbg调试截图](https://github.com/wuxiancong/War3Bot/raw/main/debug/images/War3Dialog1.PNG)

## 问题描述 (Issue Description)

在使用自定义机器人（或通过 Bot 代理的真人玩家）在 PVPGN 服务端创建房间时：
1.  房间可以正常显示在游戏列表中。
2.  **右侧面板（地图信息/Mappanel）显示为空白。**
3.  尝试点击“加入游戏”时，客户端弹出错误提示：**"You must specify a game to join."** (你必须指定一个要加入的游戏)。

**环境 (Environment):**
- **客户端:** Warcraft III (1.26a / Game.dll)
- **服务端:** PVPGN (C++)
- **调试工具:** x64dbg
- **地址偏移:** game.dll + 575E80

---

### 反汇编代码(game.dll + 575E80)

```assembly
6F575E80 | 81EC 04020000            | sub esp,204                                              |
6F575E86 | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]                          |
6F575E8B | 33C4                     | xor eax,esp                                              |
6F575E8D | 898424 00020000          | mov dword ptr ss:[esp+200],eax                           |
6F575E94 | 56                       | push esi                                                 |
6F575E95 | 8BF1                     | mov esi,ecx                                              |
6F575E97 | 8B8E 04020000            | mov ecx,dword ptr ds:[esi+204]                           |
6F575E9D | E8 FEDF0900              | call game.6F613EA0                                       | <- GetSelectedGameData() - 尝试获取选中房间的信息
6F575EA2 | 85C0                     | test eax,eax                                             | <- 检查返回的指针是否为 NULL
6F575EA4 | 74 20                    | je game.6F575EC6                                         | <- 如果为 NULL，跳转到错误处理
6F575EA6 | 8038 00                  | cmp byte ptr ds:[eax],0                                  | <- 如果长度为 0，跳转到错误处理
6F575EA9 | 74 1B                    | je game.6F575EC6                                         |
6F575EAB | B8 01000000              | mov eax,1                                                |
6F575EB0 | 5E                       | pop esi                                                  |
6F575EB1 | 8B8C24 00020000          | mov ecx,dword ptr ss:[esp+200]                           |
6F575EB8 | 33CC                     | xor ecx,esp                                              |
6F575EBA | E8 9AB12600              | call game.6F7E1059                                       |
6F575EBF | 81C4 04020000            | add esp,204                                              |
6F575EC5 | C3                       | ret                                                      |
6F575EC6 | 68 00020000              | push 200                                                 |
6F575ECB | 8D5424 08                | lea edx,dword ptr ss:[esp+8]                             |
6F575ECF | B9 2814966F              | mov ecx,game.6F961428                                    | <- 加载错误代码 "NETERROR_NOGAMESPECIFIED"
6F575ED4 | E8 77370500              | call game.6F5C9650                                       | <- 转换为文本 "You must specify a game to join"
6F575ED9 | 6A 01                    | push 1                                                   |
6F575EDB | 6A 04                    | push 4                                                   |
6F575EDD | 6A 00                    | push 0                                                   |
6F575EDF | 56                       | push esi                                                 |
6F575EE0 | 6A 09                    | push 9                                                   |
6F575EE2 | 8D5424 18                | lea edx,dword ptr ss:[esp+18]                            |
6F575EE6 | B9 01000000              | mov ecx,1                                                |
6F575EEB | E8 C06FFEFF              | call game.6F55CEB0                                       | <- 弹出 MessageBox
6F575EF0 | 8B86 04020000            | mov eax,dword ptr ds:[esi+204]                           |
6F575EF6 | 8B8C24 04020000          | mov ecx,dword ptr ss:[esp+204]                           |
6F575EFD | 8986 FC010000            | mov dword ptr ds:[esi+1FC],eax                           |
6F575F03 | 5E                       | pop esi                                                  |
6F575F04 | 33CC                     | xor ecx,esp                                              |
6F575F06 | 33C0                     | xor eax,eax                                              |
6F575F08 | E8 4CB12600              | call game.6F7E1059                                       |
6F575F0D | 81C4 04020000            | add esp,204                                              |
6F575F13 | C3                       | ret                                                      |
6F575E80 | sub esp, 204
```

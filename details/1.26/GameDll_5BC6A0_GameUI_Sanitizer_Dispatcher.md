# 魔兽争霸3 聊天颜色清洗逻辑逆向分析报告

**地址**：`029CC6A0`
**偏移**：`game.dll+5BC6A0`

```assembly
029CC6A0  | 81EC 08040000            | sub esp,408                                                                       | 颜色清洗
029CC6A6  | A1 40E1EB02              | mov eax,dword ptr ds:[2EBE140]                                                    |
029CC6AB  | 33C4                     | xor eax,esp                                                                       |
029CC6AD  | 898424 04040000          | mov dword ptr ss:[esp+404],eax                                                    |
029CC6B4  | 56                       | push esi                                                                          |
029CC6B5  | 8BB424 10040000          | mov esi,dword ptr ss:[esp+410]                                                    |
029CC6BC  | 85F6                     | test esi,esi                                                                      |
029CC6BE  | 57                       | push edi                                                                          |
029CC6BF  | 74 11                    | je game.29CC6D2                                                                   |
029CC6C1  | 8BBC24 18040000          | mov edi,dword ptr ss:[esp+418]                                                    |
029CC6C8  | 85FF                     | test edi,edi                                                                      |
029CC6CA  | 74 06                    | je game.29CC6D2                                                                   |
029CC6CC  | 85C9                     | test ecx,ecx                                                                      |
029CC6CE  | 75 1D                    | jne game.29CC6ED                                                                  |
029CC6D0  | 880E                     | mov byte ptr ds:[esi],cl                                                          |
029CC6D2  | 33C0                     | xor eax,eax                                                                       |
029CC6D4  | 8B8C24 0C040000          | mov ecx,dword ptr ss:[esp+40C]                                                    |
029CC6DB  | 5F                       | pop edi                                                                           |
029CC6DC  | 5E                       | pop esi                                                                           |
029CC6DD  | 33CC                     | xor ecx,esp                                                                       |
029CC6DF  | E8 75492200              | call game.2BF1059                                                                 |
029CC6E4  | 81C4 08040000            | add esp,408                                                                       |
029CC6EA  | C2 0800                  | ret 8                                                                             |
029CC6ED  | 53                       | push ebx                                                                          |
029CC6EE  | 8D4424 0C                | lea eax,dword ptr ss:[esp+C]                                                      |
029CC6F2  | 50                       | push eax                                                                          |
029CC6F3  | 57                       | push edi                                                                          |
029CC6F4  | 56                       | push esi                                                                          |
029CC6F5  | 33DB                     | xor ebx,ebx                                                                       |
029CC6F7  | 68 00060000              | push 600                                                                          | ---> 修改为 400
029CC6FC  | 895C24 1C                | mov dword ptr ss:[esp+1C],ebx                                                     |
029CC700  | E8 ABC61F00              | call game.2BC8DB0                                                                 | 搜索敏感字符 (|)
029CC705  | 395C24 0C                | cmp dword ptr ss:[esp+C],ebx                                                      |
029CC709  | 74 3D                    | je game.29CC748                                                                   |
029CC70B  | BB 01000000              | mov ebx,1                                                                         |
029CC710  | 68 00040000              | push 400                                                                          |
029CC715  | 56                       | push esi                                                                          |
029CC716  | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                                                     |
029CC71A  | 51                       | push ecx                                                                          |
029CC71B  | E8 A4EE1200              | call <JMP.&Ordinal#501>                                                           |
029CC720  | 8D5424 0C                | lea edx,dword ptr ss:[esp+C]                                                      |
029CC724  | 52                       | push edx                                                                          |
029CC725  | 57                       | push edi                                                                          |
029CC726  | 56                       | push esi                                                                          |
029CC727  | 68 00060000              | push 600                                                                          |
029CC72C  | 8D4424 20                | lea eax,dword ptr ss:[esp+20]                                                     |
029CC730  | 50                       | push eax                                                                          |
029CC731  | E8 30EF1200              | call <JMP.&Ordinal#506>                                                           |
029CC736  | 8BD0                     | mov edx,eax                                                                       |
029CC738  | 8D4C24 20                | lea ecx,dword ptr ss:[esp+20]                                                     |
029CC73C  | E8 6FC61F00              | call game.2BC8DB0                                                                 |
029CC741  | 837C24 0C 00             | cmp dword ptr ss:[esp+C],0                                                        |
029CC746  | 75 C8                    | jne game.29CC710                                                                  |
029CC748  | 8BC3                     | mov eax,ebx                                                                       |
029CC74A  | 5B                       | pop ebx                                                                           |
029CC74B  | EB 87                    | jmp game.29CC6D4                                                                  |
```

## 1. 核心函数概览

在魔兽争霸3高版本（如 1.27/1.31 及重制版）中，玩家发送的消息会经过一个**多层清洗系统**。该系统不仅检查非法字符，还会对管道符 `|` 进行转义或剔除，以防止玩家伪造系统颜色消息。

*   **分发器函数 (Dispatcher)**: `game.029CC6A0`
    *   负责初始化缓冲区并设置过滤标志（Flags）。
*   **核心清理函数 (Core Cleaner)**: `game.02BC8DB0`
    *   实际执行扫描、字符识别和缓冲区重组的函数。

---

## 2. 关键代码片段分析

### 2.1 过滤器分发逻辑 (`029CC6A0`)
这是进入过滤流程的入口，最重要的逻辑在于**位掩码（Bitmask）**的传递。

```assembly
029CC6ED  | 53                       | push ebx                          ; 保存寄存器
029CC6F7  | 68 00060000              | push 600                          ; 【关键】过滤标志位 (Flags)
029CC700  | E8 ABC61F00              | call game.2BC8DB0                 ; 调用核心清洗函数
029CC705  | 395C24 0C                | cmp dword ptr ss:[esp+C], ebx     ; 检查是否发现了敏感字符
029CC709  | 74 3D                    | je game.29CC748                   ; 如果没发现，跳过清洗流程(放行)
```

*   **Flag `0x600` 的含义**：
    *   `0x200`: 开启管道符 `|` 及其后续颜色指令（c/r/n）的过滤。
    *   `0x400`: 开启常规字符串校验与缓冲区搬运。

### 2.2 核心清理函数逻辑 (`02BC8DB0`)
该函数会遍历 `ESI` 指向的原始输入，根据 `02BC8E2E` 处的跳转表分发处理：
*   **普通路径**：执行 `MOV` 指令将字符拷贝到目标缓冲区 `EDI`。
*   **拦截路径**：识别到 `|c` 后，不执行拷贝，或者将其转义为 `||c`，导致渲染引擎无法解析颜色。

---

## 3. 破解方案：位掩码欺骗 (Bitmask Spoofing)

经过测试，直接在函数头部使用 `ret` 或 `jmp` 会导致缓冲区未被填充，从而出现文字不显示或乱码（如 `jr:`）。**最优雅且稳定的方案是修改过滤标志位。**

### 修改方案对照表

| 修改位置 | 原始指令 | 修改后指令 | 技术原理 |
| :--- | :--- | :--- | :--- |
| **029CC6F7** | `push 600` | **`push 400`** | 屏蔽 `0x200` 位。此时清洗函数仍会正常搬运缓冲区（保证文字显示），但会“失明”从而忽略 `|` 字符，不再进行颜色剔除。 |

---

## 4. 为什么其他方案会失败？

1.  **直接 `ret 8` 失败原因**：
    该函数不仅负责“过滤”，还负责“搬运”。直接返回会导致目标缓冲区为空，渲染引擎读不到数据，显示为残留的内存脏数据。
2.  **跳过整个 `call` 失败原因**：
    父函数 `02A1CBF0` 依赖此函数生成的干净副本。如果跳过，后续的字符串拼接（如拼入玩家名字）会找不到正文内容。
3.  **修改 `0x600` 为 `0x400` 成功原因**：
    它保留了函数的**搬运功能**，但去除了**过滤逻辑**。

---

## 5. 最终效果总结

通过将 `game.dll` 中偏移 `029CC6F7` 处的 `68 00 06 00 00` 修改为 `68 00 04 00 00`：
*   **输入**：`|cffff0000红色文字|r`
*   **结果**：绕过清洗系统，原始代码成功进入渲染引擎 `02D8DC60`。
*   **显示**：聊天框成功显示为 **红色文字**。

**注意**：此修改仅在本地生效。如果要在联机中使用，通常需要发送端和接收端（其他玩家）均进行类似修改，除非利用某些平台未清洗原始封包的漏洞。
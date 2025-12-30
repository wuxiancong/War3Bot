# 魔兽争霸 III (1.26a) GameMain 函数逆向分析

## 1. 函数概览
- **版本**：`1.26a`
- **基址**：`6F000000`
- **地址**：`6F009850`
- **所在模块**：`game.dll`
- **函数名称**：`GameMain`
- **偏移**：'game.dll + 9850'
- **调用约定**：`__stdcall` (由结尾的 `ret 4` 判定)
- **功能总结**：
该函数是 `game.dll` 的核心逻辑入口之一。它负责建立栈帧安全检查、初始化随机数种子、构建日志目录环境、执行启动前检查，并最终进入游戏主循环。

---


```assembly
6F009850  | 81EC 08010000           | sub esp,108                                     |
6F009856  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]                 |
6F00985B  | 33C4                    | xor eax,esp                                     |
6F00985D  | 898424 04010000         | mov dword ptr ss:[esp+104],eax                  |
6F009864  | 56                      | push esi                                        |
6F009865  | 8BB424 10010000         | mov esi,dword ptr ss:[esp+110]                  |
6F00986C  | E8 3F220000             | call game.6F00BAB0                              |
6F009871  | E8 8AB56B00             | call <JMP.&GetTickCount>                        |
6F009876  | 8BC8                    | mov ecx,eax                                     |
6F009878  | E8 23B46B00             | call game.6F6C4CA0                              |
6F00987D  | 8BD6                    | mov edx,esi                                     |
6F00987F  | B9 656D6167             | mov ecx,67616D65                                |
6F009884  | E8 37C06B00             | call game.6F6C58C0                              |
6F009889  | BA 04010000             | mov edx,104                                     |
6F00988E  | 8D4C24 04               | lea ecx,dword ptr ss:[esp+4]                    |
6F009892  | E8 69C76B00             | call game.6F6C6000                              |
6F009897  | 68 04010000             | push 104                                        |
6F00989C  | 68 A860876F             | push game.6F8760A8                              | <--- Logs
6F0098A1  | 8D4424 0C               | lea eax,dword ptr ss:[esp+C]                    |
6F0098A5  | 50                      | push eax                                        |
6F0098A6  | E8 131D6E00             | call <JMP.&Ordinal#503>                         |
6F0098AB  | 8D4C24 04               | lea ecx,dword ptr ss:[esp+4]                    |
6F0098AF  | 51                      | push ecx                                        |
6F0098B0  | E8 A51D6E00             | call <JMP.&Ordinal#585>                         |
6F0098B5  | E8 66FEFFFF             | call game.6F009720                              |
6F0098BA  | 85C0                    | test eax,eax                                    |
6F0098BC  | 5E                      | pop esi                                         |
6F0098BD  | 74 14                   | je game.6F0098D3                                |
6F0098BF  | E8 0C136200             | call game.6F62ABD0                              |
6F0098C4  | E8 37E2FFFF             | call game.6F007B00                              |
6F0098C9  | B9 656D6167             | mov ecx,67616D65                                |
6F0098CE  | E8 3DC06B00             | call game.6F6C5910                              |
6F0098D3  | 8B8C24 04010000         | mov ecx,dword ptr ss:[esp+104]                  |
6F0098DA  | 33CC                    | xor ecx,esp                                     |
6F0098DC  | 33C0                    | xor eax,eax                                     |
6F0098DE  | E8 76777D00             | call game.6F7E1059                              |
6F0098E3  | 81C4 08010000           | add esp,108                                     |
6F0098E9  | C2 0400                 | ret 4                                           |
```

## 2. 详细代码逻辑分析

### 2.1 栈帧建立与安全检查 (Stack Frame & Security Cookie)
```assembly
6F009850  | sub esp,108             ; 分配 0x108 (264) 字节的局部变量空间
6F009856  | mov eax,dword ptr ds:[6FAAE140] ; 读取全局安全 Cookie
6F00985B  | xor eax,esp             ; 将 Cookie 与当前栈指针做异或
6F00985D  | mov dword ptr ss:[esp+104],eax ; 将处理后的 Cookie 存入栈底
```
*   **分析**：这是典型的 MSVC 编译器 `/GS` 选项生成的代码，用于防止栈缓冲区溢出（Stack Buffer Overflow）。
*   **目的**：保护函数内部定义的局部变量（主要是后续用于存储路径字符串的 `char buffer[260]`）。

### 2.2 参数获取与基础初始化
```assembly
6F009864  | push esi                ; 保存 ESI 寄存器
6F009865  | mov esi,dword ptr ss:[esp+110] ; 获取函数第一个参数 (Arg1)
6F00986C  | call game.6F00BAB0      ; [Call 1] 内部子系统初始化
6F009871  | call <JMP.&GetTickCount>; 获取系统启动毫秒数
6F009876  | mov ecx,eax             ; 将时间戳传入 ECX
6F009878  | call game.6F6C4CA0      ; [Call 2] 初始化随机数种子 (srand类似机制)
```
*   **参数**：`esp+110` 对应栈上的第一个参数（通常是 `HINSTANCE` 或命令行结构体指针）。
*   **随机化**：使用 `GetTickCount` 确保每次启动游戏的随机序列（如掉落、伤害浮动）不同。

### 2.3 字符串构建与日志环境 (Logging Setup)
```assembly
6F00987F  | mov ecx,67616D65        ; 将 "game" (ASCII: 0x67616D65) 存入 ECX
6F009884  | call game.6F6C58C0      ; [Call 3] 注册/初始化名为 "game" 的模块上下文
...
6F00989C  | push game.6F8760A8      ; 压入字符串指针 "Logs"
6F0098A6  | call <JMP.&Ordinal#503> ; 调用 Storm.dll #503 (SStrPrintf)
6F0098B0  | call <JMP.&Ordinal#585> ; 调用 Storm.dll #585 (SFileCreateDir)
```
*   **关键标识**：`67616D65` 是小端序的 **"game"** 字符串。
*   **Storm.dll 调用**：
    *   **Ordinal #503**：`SStrPrintf`，用于格式化字符串。这里是在拼接路径，类似于 `%s\Logs`。
    *   **Ordinal #585**：通常对应 `SFileCreateDir` 或类似功能。
*   **功能**：确保游戏根目录下的 `Logs` 文件夹存在，以便后续写入错误报告或运行日志。

### 2.4 核心启动检查与主循环 (The Game Loop)
```assembly
6F0098B5  | call game.6F009720      ; [关键 Check] 检查游戏是否允许启动
6F0098BA  | test eax,eax            ; 检查返回值
6F0098BD  | je game.6F0098D3        ; 如果返回 0 (False)，跳过主循环直接清理退出
```
*   **前置检查 (`6F009720`)**：这通常检查单例进程（防止双开）、光盘验证（旧版）、或关键 MPQ 文件是否加载成功。

```assembly
; --- 检查通过，进入游戏 ---
6F0098BF  | call game.6F62ABD0      ; [Call 4] 媒体/渲染子系统初始化
6F0098C4  | call game.6F007B00      ; [核心] GameLoop - 游戏主循环
6F0098C9  | mov ecx,67616D65        ; 再次加载 "game" 标识
6F0098CE  | call game.6F6C5910      ; [Call 5] 模块清理/释放资源
```
*   **主循环 (`6F007B00`)**：这是函数中**最重要**的调用。程序会在此处停留绝大部分时间，处理窗口消息、渲染画面、运行游戏逻辑。直到玩家点击退出，该函数才会返回。

### 2.5 清理与返回
```assembly
6F0098D3  | mov ecx,dword ptr ss:[esp+104] ; 取回保存的 Security Cookie
6F0098DA  | xor ecx,esp             ; 校验 Cookie 完整性
6F0098DE  | call game.6F7E1059      ; Security Check (如果不匹配则触发异常)
6F0098E3  | add esp,108             ; 释放栈空间
6F0098E9  | ret 4                   ; 返回并平栈 4 字节
```

---

## 3. C++ 伪代码还原

为了便于理解，将上述汇编逻辑还原为 C++ 代码：

```cpp
// 定义：stdcall 调用约定，返回 void，带一个参数
void __stdcall GameMain(void* hInstance) 
{
    // 1. 栈安全检查初始化 (__security_prologue)
    __security_cookie_init(); 
    
    char logPathBuffer[260]; // 对应 esp+4 的缓冲区

    // 2. 基础初始化
    InitInternalSubsystem(hInstance); // 6F00BAB0
    
    // 3. 随机数种子初始化
    DWORD currentTime = GetTickCount();
    InitRandomSeed(currentTime);      // 6F6C4CA0

    // 4. 初始化 "game" 模块上下文
    InitModuleContext("game");        // 6F6C58C0

    // 5. 准备日志目录 (调用 Storm.dll)
    // 相当于: sprintf(logPathBuffer, "%s\\Logs", GetGameDir());
    Storm_SStrPrintf(logPathBuffer, ... , "Logs"); 
    Storm_SFileCreateDir(logPathBuffer);

    // 6. 启动前检查 (Pre-flight Check)
    if (CheckGameCanStart()) // 6F009720
    {
        // 初始化视频/音频等
        InitMediaSystems();  // 6F62ABD0
        
        // ------------------------------------------------
        // 进入游戏主循环 (在此处阻塞，直到游戏退出)
        // ------------------------------------------------
        RunGameLoop();       // 6F007B00

        // 7. 游戏结束后的清理
        CleanupModuleContext("game"); // 6F6C5910
    }

    // 8. 栈安全校验与返回 (__security_epilogue)
    __security_check_cookie();
    return;
}
```

## 4. 总结与关键点

1.  **崩溃排查线索**：如果游戏启动时在创建日志目录阶段崩溃，问题通常出在文件系统权限或 `Storm.dll` 版本不匹配。
2.  **修改入口**：如果你在制作 MOD 或注入器，`6F007B00` (Main Loop) 是注入钩子（Hook）以每帧执行代码的最佳位置。
3.  **随机性**：代码明确展示了魔兽争霸利用 `GetTickCount` 进行伪随机数生成器的初始化，这意味着如果在同一毫秒内启动多个实例（理论上），它们的随机序列可能相同。
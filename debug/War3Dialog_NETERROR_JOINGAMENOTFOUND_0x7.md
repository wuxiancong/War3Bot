# War3 创建房间后点击列表选项弹出找不到游戏的对话框调试日志
**Error Code: 0x07 (NETERROR_JOINGAMENOTFOUND)**

![x64dbg调试截图](https://github.com/wuxiancong/War3Bot/raw/main/debug/images/War3Dialog_NETERROR_JOINGAMENOTFOUND_0x7.PNG)

## 1. 现象描述 (Symptom)
在 Hook 拦截到目标房间名并尝试执行 `JoinGame` 后，游戏弹出 **"无法找到游戏"** 的错误提示窗口。

### UI 层错误判断逻辑
在函数 `6F5B4D40` (JoinGame 流程) 中，程序根据返回的错误码决定弹窗类型。

```assembly
; 检查错误码
6F5B4D70  | 8B6F 14                 | mov ebp,dword ptr ds:[edi+14]    ; 读取错误码，此时 EBP = 7
6F5B4D73  | 8D45 FF                 | lea eax,dword ptr ss:[ebp-1]     ; EAX = 6
6F5B4D76  | 83F8 2A                 | cmp eax,2A                       ; Switch 分支判断
; ...
; 错误码导致跳转到 NETERROR_JOINGAMENOTFOUND
6F5B4EDE  | B9 6491966F             | mov ecx,game.6F969164            ; 字符串: "NETERROR_JOINGAMENOTFOUND"
6F5B4EE3  | EB 21                   | jmp game.6F5B4F06                ; 执行弹窗
```

---

## 2. 错误码溯源 (Root Cause Trace)
通过在栈指针和内存下硬件写入断点，追踪到错误码 `7` 的产生源头。

### 错误码写入点
错误码来源于 `CNetEventGameFind` 结构的处理过程。

```assembly
; 函数: game.6F652690
6F652697  | E8 9406E7FF             | call game.6F4C2D30               ; 内部写入逻辑
...
; 内存数据 Dump (EngineNet->Recycle->Event->Data)
1B1E0DFC  6F6D1B1E  ..mo  
1B1E0E00  00000007  ....  <-- 错误码 7 被写入此处
```

### 关键调用栈
错误码是由底层的状态检查函数返回的。

```assembly
; 调用层级
6F546806  | E8 85BE1000             | call game.6F652690  <-- 写入错误码
...
6F65B9C3  | E8 38D8FFFF             | call game.6F659200
...
6F660A6C  | FFD2                    | call edx            <-- 动态调用返回 7
```

---

## 3. 核心原因：状态机冲突 (State Machine Conflict)
这是导致加入失败的根本原因。反汇编显示，游戏引擎在执行加入逻辑前，检查了当前的**网络状态**。

### 状态检查逻辑
位于函数 `6F660990` 中：

```assembly
6F660A23  | E8 78790700             | call game.6F6D83A0             ; 获取状态相关对象
6F660A28  | 83BE E8060000 05        | cmp dword ptr ds:[esi+6E8], 5  ; 检查当前状态是否为 5
6F660A2F  | 75 42                   | jne game.6F660A73              ; 如果不是 5，跳转到正常逻辑
```

*   **`[esi+6E8]`**: 网络引擎当前状态 (Network State)。
*   **状态 `5`**: **GAME_STATE_LISTING** (正在请求/接收/解析游戏列表)。
*   **逻辑解读**:
    *   如果当前状态是 **5** (正在刷列表)，程序**不跳转**，继续向下执行。
    *   向下执行会触发 `call edx` (`6F660A6C`)。
    *   该 Call 返回了错误码 **7** (以及 `lea eax, [edi+7]`)。
    *   这意味着：**在列表刷新未完成（状态=5）时强行尝试 Join，会被引擎拒绝。**

### 错误分支代码
进入状态 5 的处理分支后，直接生成错误：

```assembly
6F660A56  | 8D47 07                 | lea eax,dword ptr ds:[edi+7]   ; 准备错误码 7
6F660A59  | 8B16                    | mov edx,dword ptr ds:[esi]
6F660A5B  | 8B92 9C000000           | mov edx,dword ptr ds:[edx+9C]
...
6F660A6C  | FFD2                    | call edx                       ; 触发错误处理回调
```

---

## 4. 结论与修复方案 (Conclusion)

### 结论
Hook 逻辑 (`FindMyRoom`) 是在游戏列表刷新循环中被触发的。此时，游戏网络引擎的状态机正处于 **State 5 (Listing)**。
如果在 Hook 中立即调用 `JoinGame`，实际上是在 State 5 的状态下请求进入 State 6 (Connecting)。游戏引擎的状态机逻辑禁止这种转换（必须先回到 State 0 Idle），从而抛出错误码 7。

### 修复方案
必须等待网络引擎从 State 5 退出，回到 State 0 (Idle) 后，才能安全调用 `JoinGame`。

1.  **Hook 策略**: 在 `FindMyRoom` 中匹配成功后，**返回 0**，让列表刷新循环正常结束。
2.  **状态注入**: 提前将房间名写入 UI 输入框 (`ForceSetJoinGameInputText`)。
3.  **异步执行**: 启动一个线程，**Sleep(500~1000ms)**，等待主线程完成列表刷新并重置状态。
4.  **延迟调用**: 线程苏醒后，调用 `JoinGame(1)`。此时状态为 0，加入成功。
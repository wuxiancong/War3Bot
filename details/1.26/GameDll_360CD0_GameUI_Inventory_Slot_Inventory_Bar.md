# 魔兽争霸 III (v1.26) 物品栏按键响应机制逆向分析

## 1. 函数概览
- **基址**：`6F000000`
- **地址**：`6F360CD0` (以及调用它的输入分发器 `6F382150`)
- **所在模块**：`game.dll`
- **函数功能**：**物品使用判定与执行**
- **功能总结**：
该函数是物品栏快捷键响应的核心逻辑。当玩家按键通过顶层输入处理器 (`6F382150`) 筛选后，会调用此函数。它遍历当前的物品槽位，通过对比**按键码 (Key Code)** 与**物品UI对象中存储的快捷键设置**，来决定是否激活某个物品。

---

## 2. 核心反汇编代码 (物品按键遍历)

```assembly
; --- 函数入口：6F360CD0 ---
; 参数：ECX = 物品栏 UI 对象指针
; 栈参数 [esp+24] = 玩家按下的虚拟键码 (Virtual Key Code)

6F360CD0  | 53                       | push ebx                                        |
6F360CD1  | 55                       | push ebp                                        |
6F360CD2  | 56                       | push esi                                        |
6F360CD3  | 8BB1 2C010000            | mov esi,dword ptr ds:[ecx+12C]                  | 获取物品栏槽位数 (通常为 6)
6F360CD9  | 85F6                     | test esi,esi                                    |
6F360CDB  | 57                       | push edi                                        |
6F360CDC  | 74 54                    | je game.6F360D32                                | 若无槽位直接退出
6F360CDE  | 8B81 30010000            | mov eax,dword ptr ds:[ecx+130]                  | 获取 InventoryBar 对象数组
6F360CE4  | 8B5C24 24                | mov ebx,dword ptr ss:[esp+24]                   | [关键] 获取按下的键码 (Key Code)
6F360CE8  | 8B2D 9CED926F            | mov ebp,dword ptr ds:[6F92ED9C]                 | 加载内部状态/校验码
6F360CEE  | 8D7CF0 04                | lea edi,dword ptr ds:[eax+esi*8+4]              | 循环指针计算

; --- 循环开始：遍历每个物品槽 ---
6F360CF2  | 8B47 F8                  | mov eax,dword ptr ds:[edi-8]                    | 获取当前槽位的物品对象指针
6F360CF5  | 83EF 08                  | sub edi,8                                       | 指针递减
6F360CF8  | 83EE 01                  | sub esi,1                                       | 循环计数器递减
6F360CFB  | 85C0                     | test eax,eax                                    |
6F360CFD  | 74 2F                    | je game.6F360D2E                                | 空槽位跳过
6F360CFF  | 83B8 94000000 00         | cmp dword ptr ds:[eax+94],0                     | 检查物品是否存在/为空
6F360D06  | 74 26                    | je game.6F360D2E                                |
6F360D08  | 83B8 38010000 00         | cmp dword ptr ds:[eax+138],0                    | 检查物品是否被禁用 (Cooldown/Disable)
6F360D0F  | 74 1D                    | je game.6F360D2E                                |
6F360D11  | 8B88 90010000            | mov ecx,dword ptr ds:[eax+190]                  | 获取物品 UI 描述结构 (TipStrings/Hotkeys)

; --- [核心判定] 改建的关键点 ---
6F360D17  | 3999 AC050000            | cmp dword ptr ds:[ecx+5AC],ebx                  | 比较：[物品定义的快捷键] vs [当前按键]
6F360D1D  | 75 0F                    | jne game.6F360D2E                               | 不匹配则跳转继续循环，匹配则向下执行

; --- 物品激活逻辑 ---
6F360D1F  | 8B51 08                  | mov edx,dword ptr ds:[ecx+8]                    |
6F360D22  | 3BD5                     | cmp edx,ebp                                     |
6F360D24  | 74 08                    | je game.6F360D2E                                |
6F360D26  | 8B49 10                  | mov ecx,dword ptr ds:[ecx+10]                   |
6F360D29  | 83F9 40                  | cmp ecx,40                                      |
6F360D2C  | 75 0D                    | jne game.6F360D3B                               | 跳转到使用物品的具体执行代码
...
```

## 3. 详细逻辑分析

### 3.1 调用栈上下文
函数 `6F360CD0` 并非独立运行，而是由 **高级输入响应器 (High-Level Input Handler)** 在 `6F382150` 处调用。
*   **输入捕获**：`6F382150` 捕获所有键盘消息。
*   **按键过滤**：在 `6F3821C9` 附近，游戏判断按键是否在保留范围（如技能键、F1-F12等）。
*   **分发调用**：如果不是硬编码的功能键，流程最终到达 `6F3829AC`，调用本函数 (`6F360CD0`) 尝试作为物品键处理。

### 3.2 数据驱动的键位匹配
与传统的 `switch(key)` 结构不同，Warcraft III 使用了一种**数据驱动**的方法：
1.  **获取输入**：`mov ebx, [esp+24]` 获取玩家按下的键（例如按下了 'Q'，则 ebx=0x51）。
2.  **获取定义**：`mov ecx, [eax+190]` 获取物品的 UI 数据，其中偏移 `0x5AC` 存储了该物品当前绑定的触发键（例如小键盘7，则为 0x67）。
3.  **动态对比**：`cmp [ecx+5AC], ebx`。
    *   这意味着游戏并不硬性规定 "Slot 1 必须是小键盘 7"。
    *   它是看 "Slot 1 里的这个物品，它自己认为它应该由什么键触发"。

### 3.3 循环机制
代码使用 `lea edi, [eax+esi*8+4]` 和 `sub esi, 1` 进行倒序或正序遍历（取决于编译器优化），检查物品栏的全部 6 个槽位。只要有一个槽位的快捷键匹配当前按键，循环就会中止并尝试使用该物品。

---

## 4. C++ 伪代码还原

```cpp
// 物品使用处理函数 (Address: 6F360CD0)
// 返回值: Bool (是否成功处理了按键)
bool __stdcall ProcessInventoryKey(void* pGameUI, int inputKeyCode)
{
    // pGameUI = ecx
    // inputKeyCode = [esp+24] (ebx)
    
    int slotCount = pGameUI->inventory.slotCount; // [ecx+12C]
    if (slotCount == 0) return false;

    ItemSlot* slots = pGameUI->inventory.slots;   // [ecx+130]

    // 遍历所有物品槽
    for (int i = slotCount - 1; i >= 0; i--)
    {
        Item* pItem = slots[i].pItem;
        
        // 1. 基础有效性检查
        if (!pItem) continue;                   // 空槽位 [eax+94]
        if (pItem->isDisabled) continue;        // 冷却中或被禁用 [eax+138]

        // 2. 获取物品 UI 配置数据
        ItemUIData* pUIData = pItem->pUIData;   // [eax+190]

        // 3. [关键] 检查按键是否匹配
        // 比较物品内部绑定的快捷键与玩家输入的按键
        if (pUIData->boundKey == inputKeyCode)  // cmp [ecx+5AC], ebx
        {
            // 4. 尝试使用物品
            if (CanUseItem(pItem))
            {
                ExecuteItemUsage(pItem);        // 6F360D3B
                return true; // 成功处理
            }
        }
    }

    return false; // 未匹配到任何物品
}
```

## 5. 逆向结论与改建启示

1.  **改建原理验证**：
    *   **外部改建（模拟按键）**：将 `Q` 映射为 `Numpad 7`。游戏接收到 `Numpad 7`，进入循环，发现 Slot 1 的 `boundKey` 是 `Numpad 7`，匹配成功。
    *   **内存改建（Internal Hack）**：
        *   **方法 A (Hook)**：Hook 函数入口 `6F360CD0`，当传入 `Q` 时，将其篡改为 `Numpad 7` 再交给原函数。
        *   **方法 B (Memory Patch)**：遍历内存中的物品对象，直接修改 `[ecx+5AC]` 的值为 `Q` 的键码。这样无需 Hook，游戏原生逻辑就会认为该物品由 `Q` 触发。

2.  **关键偏移**：
    *   `GameUI + 0x3C4 -> + 0x148`: 获取 InventoryBar。
    *   `Item + 0x190`: 获取 UI Data。
    *   `UI Data + 0x5AC`: **物品快捷键定义**。
```
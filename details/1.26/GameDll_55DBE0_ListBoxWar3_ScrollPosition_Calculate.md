# War3 1.26a 函数逆向分析报告：列表框滚动计算

**函数地址**：`Game.dll + 0x55DBE0` (内存地址 `6F55DBE0`)
**功能概述**：计算 ListBox（列表框）当前的**起始显示索引 (Start Index)**。
**调用场景**：UI 刷新、绘制列表项、滚动条拖动时调用。它根据当前的滚动值（Float）和房间总数（Int），计算出列表应该从第几个房间开始绘制。

---

## 1. 核心寄存器与结构

| 寄存器 | 对应数据含义 | 备注 |
| :--- | :--- | :--- |
| **ECX / EDI** | **CListBox 指针** | 列表框控件对象本身 (this指针) |
| **ESI** | **CLayout / CFrameDef 指针** | UI 布局定义对象，来源 `[edi+0x238]` |
| **EAX** | **返回值 (int)** | **起始索引** (即列表第一行显示第几个房间) |

## 2. 关键内存偏移

*   **`[EDI + 0x238]`**：指向 UI 布局定义结构体 (`CFrameDef`)。
*   **`[EDI + 0x1F0]`**：**房间总数 (Total Items)**（整数）。
*   **`[ESI + 0xB0]`**：**UI 状态标志位 (Status Flags)**（位掩码）。
*   **`[ESI + 0x1F0]`**：**当前滚动位置 (Current Scroll Value)**（浮点数 Float）。
*   **`[ESI + 0x1F4]`**：**列表可视高度/容量因子**（浮点数 Float）。

---

## 3. 反汇编详细注释

```assembly
; =====================================================
; 函数：CListBox_CalculateScrollIndex
; 地址：6F55DBE0
; =====================================================

6F55DBE0  | 83EC 0C                 | sub esp, 0xC                     ; 开辟栈空间（用于浮点转换临时存储）
6F55DBE3  | 56                      | push esi                         ; 保存寄存器
6F55DBE4  | 57                      | push edi                         ; 保存寄存器
6F55DBE5  | 8BF9                    | mov edi, ecx                     ; 将 this 指针 (ListBox) 存入 EDI

; [步骤 1] 获取 UI 布局定义对象 (FrameDef)
6F55DBE7  | 8BB7 38020000           | mov esi, dword ptr ds:[edi+238]  ; 获取 Layout 指针 -> ESI
6F55DBED  | 33C0                    | xor eax, eax                     ; EAX 清零 (默认返回值)

; [步骤 2] 安全检查与状态位检测
6F55DBEF  | 85F6                    | test esi, esi                    ; 检查 Layout 对象是否存在
6F55DBF1  | 74 50                   | je game.6F55DC43                 ; 如果为空，直接返回 0
6F55DBF3  | F686 B0000000 03        | test byte ptr ds:[esi+B0], 3     ; 【关键】检查状态位 (Bit 0 和 Bit 1)
                                                                       ; 0x01: Hidden/Updating (隐藏/更新中)
                                                                       ; 0x02: Disabled/NotReady (未就绪)
6F55DBFA  | 75 47                   | jne game.6F55DC43                ; 如果处于不可用状态 (如 2D, 2F)，直接返回 0

; [步骤 3] 计算一页能显示多少项 (PageSize)
6F55DBFC  | D986 F4010000           | fld dword ptr ds:[esi+1F4]       ; 加载可视高度因子 (Float)
6F55DC02  | E8 D9352800             | call game.6F7E11E0               ; 调用函数计算每页容量 (返回 EAX)
6F55DC07  | 8BC8                    | mov ecx, eax                     ; ECX = PageSize (一页能显示几行)

; [步骤 4] 判断是否需要滚动
6F55DC09  | 3B8F F0010000           | cmp ecx, dword ptr ds:[edi+1F0]  ; 比较 PageSize vs 房间总数
6F55DC0F  | 7C 02                   | jl game.6F55DC13                 ; 如果 (容量 < 总数)，跳转继续计算
6F55DC11  | 33C9                    | xor ecx, ecx                     ; 如果 (容量 >= 总数)，无需偏移，修正因子设为 0

; [步骤 5] 获取当前滚动条数值 (Float 转 Int)
6F55DC13  | D986 F0010000           | fld dword ptr ds:[esi+1F0]       ; 加载当前滚动值 (Float)
6F55DC19  | D97C24 0A               | fnstcw word ptr ss:[esp+A]       ; --- FPU 浮点转整型 标准操作开始 ---
6F55DC1D  | 0FB74424 0A             | movzx eax, word ptr ss:[esp+A]   ; 获取 FPU 控制字
6F55DC22  | 0D 000C0000             | or eax, C00                      ; 设置截断模式 (Round to Integer)
6F55DC27  | 894424 0C               | mov dword ptr ss:[esp+C], eax    ; 
6F55DC2B  | D96C24 0C               | fldcw word ptr ss:[esp+C]        ; 加载新控制字
6F55DC2F  | DF7C24 0C               | fistp qword ptr ss:[esp+C]       ; 将浮点数转为整数存入栈 [esp+C]
6F55DC33  | 8B4424 0C               | mov eax, dword ptr ss:[esp+C]    ; EAX = 转换后的滚动索引 (ScrollIndex)
6F55DC37  | 3BC8                    | cmp ecx, eax                     ; 比较 修正因子 vs 滚动索引
6F55DC39  | D96C24 0A               | fldcw word ptr ss:[esp+A]        ; 恢复 FPU 控制字
                                                                       ; --- FPU 操作结束 ---

; [步骤 6] 边界修正与返回
6F55DC3D  | 7E 02                   | jle game.6F55DC41                ; 逻辑分支处理
6F55DC3F  | 8BC8                    | mov ecx, eax                     ; 修正逻辑
6F55DC41  | 2BC1                    | sub eax, ecx                     ; 计算最终起始索引
6F55DC43  | 5F                      | pop edi                          ; 恢复环境
6F55DC44  | 5E                      | pop esi                          ; 
6F55DC45  | 83C4 0C                 | add esp, C                       ; 
6F55DC48  | C3                      | ret                              ; 返回 EAX (起始索引)
```

---

## 4. 关键逻辑总结

1.  **状态拦截**：
    代码首先检查 `[esi+0xB0]`。如果该值为 `0x2D` (刷新中) 或 `0x2F` (删除中)，函数立即返回 `0`。只有当值为 `0x2C` (或其他低两位为0的值) 时，才执行计算。

2.  **数据转换**：
    UI 系统内部使用 **浮点数 (Float)** 来平滑处理滚动条位置（位于 `[esi+1F0]`），但具体的房间列表数组是整数索引。因此代码中有一大段 `fld / fistp` 指令，用于将浮点数滚动值转换为整数索引。

3.  **计算公式 (推导)**：
    ```cpp
    int CalculateStartIndex(CListBox* box) {
        CFrameDef* layout = box->FrameDef; // [edi+238]
        if (!layout || (layout->Flags & 3)) return 0; // 检查0xB0状态

        int pageSize = CalculatePageSize(layout->ItemHeight); // call 6F7E11E0
        int totalItems = box->TotalCount; // [edi+1F0]
        
        // 浮点转整型
        int scrollPos = (int)layout->CurrentScrollValue; // [esi+1F0]

        // 这里的 sub eax, ecx 是为了防止滚动越界
        // 核心目的是返回：当前应该从列表的第几项开始画
        return AdjustIndex(scrollPos, pageSize, totalItems);
    }
    ```

## 5. 逆向应用建议

如果你想通过 Hook 此函数来修改游戏逻辑：

*   **修改返回值 (EAX)**：可以强制列表锁定在某一行（例如锁定在第一行返回 0，或者锁定在最后一行）。
*   **强制执行**：如果在 Hook 中想强制计算，需要将 `byte ptr [esi+0xB0]` 临时修正为 `0x2C`，但需注意线程安全和空指针风险。
*   **不可在此添加数据**：此函数只负责“读”数据并计算显示位置，**不负责**“写”数据或管理房间内存。

## 6. 函数取名建议

根据对汇编代码逻辑的深度分析，这个函数是 **CListBox** 类的一个成员函数，没有任何栈参数，仅依赖 `ECX` (this指针)。

以下是我为你构思的几个名字，按不同侧重点分类：

### 选项 1：直观描述 (推荐)
**名字**：`GetScrollStartIndex`
**理由**：函数最终返回的是列表框当前应该从第几行开始绘制（Start Index）。这个名字最朴素，直接描述了返回值的含义。

### 选项 2：UI 功能视角
**名字**：`GetFirstVisibleItemIndex`
**理由**：从用户界面（UI）的角度来看，这个函数计算的是“第一个可见项目的索引”。如果你是在做 UI 渲染相关的 Hook，这个名字最贴切。

### 选项 3：底层逻辑视角
**名字**：`CalcScrollPosition`
**理由**：函数内部进行了大量的浮点数转整数运算（`fld`, `fistp`），实际上是在“计算”滚动条的逻辑位置。

### 选项 4：简短风格
**名字**：`GetTopIndex`
**理由**：MFC 或 Win32 API 中常将列表框最顶端显示的索引称为 TopIndex。符合 Windows 编程习惯。

---

### 函数原型 (C/C++ Hook 写法)

由于该函数是类成员函数 (`__thiscall`)，在 C++ 代码中 Hook 或调用它时，有几种写法。

#### 1. 标准 C++ 类成员写法 (最正规)
如果你已经还原了 `CListBox` 的类结构：

```cpp
// 假设在类定义中
class CListBox {
public:
    // ... 偏移填充 ...
    
    // 地址: 0x6F55DBE0
    // 功能: 根据 FrameDef 状态计算列表显示的起始索引
    int __thiscall GetScrollStartIndex(); 
};
```

#### 2. 用于 Hook 的函数指针写法 (通用)
在写 Hook 代码（如 Detours 或 MinHook）时，由于编译器很难直接声明 `__thiscall` 的函数指针，通常使用 `__fastcall` 来模拟（因为 `__fastcall` 的第一个参数也是 `ECX`，第二个参数 `EDX` 在这个函数里没用到，可以占位）。

```cpp
// 定义函数指针类型
// 参数1 (ECX): pListBox (this指针)
// 参数2 (EDX): _unused (占位符，原函数不使用栈参数，ret也没有退栈，所以用fastcall模拟)
typedef int (__fastcall *pGetScrollStartIndex)(void* pListBox, void* _unused);

// 声明原函数指针
pGetScrollStartIndex Original_GetScrollStartIndex = (pGetScrollStartIndex)(0x6F55DBE0);

// 你的 Hook 函数
int __fastcall My_GetScrollStartIndex(void* pListBox, void* _unused) {
    // 1. 可以在这里读取 pListBox + 0x238 (FrameDef) 来判断状态
    // 2. 调用原函数
    int startIndex = Original_GetScrollStartIndex(pListBox, _unused);
    
    // 3. 可以在这里修改返回值，例如强制 startIndex = 0
    return startIndex;
}
```

### 总结
*   **输入 (ECX)**: `void* pListBox` (列表框对象指针)
*   **返回值 (EAX)**: `int` (当前显示的起始行索引，0 代表从第一行开始)
*   **参数**: 无栈参数 (直接 `ret`，没有 `ret n`)

建议选用 **`GetScrollStartIndex`**，既准确又好记。
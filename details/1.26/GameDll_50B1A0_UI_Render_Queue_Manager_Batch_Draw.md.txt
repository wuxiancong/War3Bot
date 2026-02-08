

**函数地址**：`Game.dll + 0x50B1A0` (内存地址 `6F50B1A0`)  
**功能概述**：UI 渲染队列管理器（UI Render Queue Manager）  
**功能详述**：该函数负责收集当前帧所有需要显示的 UI 元素，通过 `qsort` 进行深度排序，并循环调用绘制函数将它们渲染到屏幕上。鼠标通常作为队列的最后一个元素进行绘制。

---

### 1. 核心汇编分析 (Key Assembly Fragments)

#### A. 获取全局队列与元素总数
```asm
6F50B1A0  | 8B0D E0BBAC6F  | mov ecx, dword ptr ds:[6FACBBE0]  ; [6FACBBE0] = UI元素总数
6F50B1AC  | 33FF           | xor edi, edi                      ; 初始化计数器
6F50B1AE  | 0F84 D8010000  | je game.6F50B38C                  ; 如果总数为0，直接跳过渲染
6F50B1BA  | 8B15 E4BBAC6F  | mov edx, dword ptr ds:[6FACBBE4]  ; [6FACBBE4] = 原始对象缓冲区
```

#### B. 调用 qsort 进行深度排序 (Z-Order)
```asm
6F50B1F3  | 68 7086506F    | push game.6F508670                ; 参数: 深度比较回调函数
6F50B1F8  | 6A 04          | push 4                            ; 参数: 每个指针的大小(4字节)
6F50B1FA  | 51             | push ecx                          ; 参数: 元素总数
6F50B1FB  | 68 881FAC6F    | push game.6FAC1F88                ; 参数: 待排序的对象指针列表
6F50B200  | FF15 C0D3866F  | call dword ptr ds:[<qsort>]       ; 执行排序
```

#### C. 遍历渲染循环与绘制执行
```asm
; --- 循环开始 ---
6F50B230  | 833D 3483AB6F 00 | cmp dword ptr ds:[6FAB8334], 0
6F50B23B  | 8B2C8D 881FAC6F  | mov ebp, dword ptr ds:[ecx*4+6FAC1F88] ; 取出排序后的对象实例
...
6F50B32C  | 8B4D 08          | mov ecx, dword ptr ss:[ebp+8]          ; 准备实例参数
6F50B330  | E8 ABFBFFFF      | call game.6F50AEE0                     ; 调用单体渲染出口 (核心绘制)
; --- 循环结束 ---
```

---

### 2. 关键内存变量 (Key Global Variables)

| 地址 | 类型 | 说明 |
| :--- | :--- | :--- |
| `6FACBBE0` | `uint32` | **RenderCount**: 当前帧排队渲染的 UI 元素数量。 |
| `6FAC1F88` | `Pointer[]` | **SortedList**: 经过 `qsort` 排序后的对象指针数组，最后一个通常是鼠标。 |
| `6FACC7B4` | `Pointer` | **GlobalUIContext**: UI 系统总上下文指针。 |
| `6FAC1F88` | `Array` | **ObjectBuffer**: `qsort` 的操作区域。 |

---

### 3. 渲染链路追踪

通过对该函数内 `6F50B330` 的深入追踪，得出以下渲染链路：
1.  **HTexture** (纹理管理) -> `+0x0C`
2.  **CSurfaceD3D** (表面包装) -> `+0x34`
3.  **IDirect3DTexture8** (D3D 原生对象)

---

### 4. 逆向结论与实验数据

-   **鼠标隐藏实验**：在 `6F50B1A0` 处直接执行 `ret`，会导致除 3D 环境（雪花等）外的所有 UI 消失。
-   **鼠标特征判定**：在 `6F50B330` 处，若传入的 `ebp` 对象所持有的纹理句柄为 `0x161C0080` (HumanCursor.blp)，则该次调用负责绘制鼠标。
-   **排序机制**：War3 界面之所以不会出现鼠标被窗口遮挡，是因为在该函数的 `qsort` 阶段，鼠标被赋予了最高的权重，始终排在列表的末尾。

---

### 5. 关联函数
- `6F50AEE0`：渲染单个 UI 精灵（Sprites）的核心实现。
- `6F508670`：Z-Order 排序比较算法。
- `6F5260E0`：UI 顶点数据提交。
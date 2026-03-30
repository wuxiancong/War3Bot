这是对 **魔兽争霸 III** 内部 **文本格式化解析器 (Text Formatter / Color Parser)** 函数的逆向分析总结。

# 魔兽争霸 III 文本格式化与颜色解析器逆向分析报告

该函数是游戏 UI 渲染引擎的核心组件，负责将包含转义字符（如 `|c`、`|r`、`|n`）的原始字符串解析为带样式的文本。它是实现“彩色文字”显示的**最终执行地**。

## 1. 函数概览
- **基址/偏移**：`02DDDC60` (通常位于 `game.dll+7BDC60` 或重制版主模块)
- **父级/关联函数**：`02AE82F0` (UTF-8 字符解码器)
- **功能总结**：
  遍历字符串字节流，识别特殊引导符 `|` (0x7C)，并解析其后的指令（颜色代码、换行或恢复默认设置）。

---

## 2. 核心逻辑流程 (反汇编分析)

### 2.1 字符循环与控制符识别
函数首先调用解码器获取当前字符的 Unicode 编码（EAX），并识别回车与管道符：
```assembly
02DDDC92  | cmp eax,A                | 检查是否为 '\n' (换行)
02DDDCA8  | cmp eax,D                | 检查是否为 '\r' (回车)
02DDDCDD  | cmp eax,7C               | [核心判定] 检查是否为 '|' (管道符/转义引导符)
02DDDCE0  | jne game.2DDDE96         | 若非管道符，作为普通文字渲染
```

### 2.2 指令分发 (Jump Table)
当识别到 `|` 后，函数会读取紧随其后的一个字符，并通过一个偏移计算来决定后续动作：
```assembly
02DDDD08  | movsx eax,byte ptr [esi] | 读取 '|' 后的字符 (如 'c', 'r', 'n')
02DDDD0B  | add eax,FFFFFFBD         | 算法：EAX = EAX - 0x43 ('C')
02DDDD1E  | jmp dword ptr [ecx*4+...] | 基于跳转表分发逻辑：
                                     | - Index 0 (C/c): 进入颜色设置
                                     | - 其他: 进入换行或重置逻辑
```

### 2.3 颜色代码解析 (Hex to Binary)
若识别为 `|c`，进入颜色处理分支。此时函数预期后面跟着 8 位十六进制字符（AARRGGBB）：
```assembly
02DDDDE0  | mov al,byte ptr [esi]    | 循环读取 16 进制字符
02DDDE03  | call <strtol>            | [关键] 调用标准库函数，将字符串转为长整型数字
                                     | 将 "ffff0000" 转换为 0xFF0000 (纯红)
```

### 2.4 颜色状态应用 (Color State Apply)
解析后的数值被拆解并存入渲染上下文的颜色缓冲区中：
```assembly
02DDDE43  | movzx edx,byte ptr [esp+25] | 获取 Alpha/Red
02DDDE53  | or edx,FFFFFF00             | 合成 32 位颜色值
02DDDE62  | mov dword ptr [ecx],edx     ; 将当前渲染色更新为解析出的颜色
```

---

## 3. 逻辑架构还原 (C++ 伪代码)

```cpp
// 文本渲染流解析器
void RenderTextParser(const char* utf8String) {
    while (*utf8String) {
        uint32_t charCode = DecodeUTF8(&utf8String);

        if (charCode == '|') { // 识别到引导符
            char cmd = *utf8String++; 
            switch (cmd) {
                case 'c':
                case 'C': { // 设置颜色
                    char hexCode[9];
                    memcpy(hexCode, utf8String, 8);
                    uint32_t color = strtol(hexCode, nullptr, 16); // 16进制转换
                    SetCurrentRenderColor(color);
                    utf8String += 8;
                    break;
                }
                case 'r':
                case 'R': // 恢复颜色
                    ResetDefaultColor();
                    break;
                case 'n':
                case 'N': // 强制换行
                    AddNewLine();
                    break;
            }
        } else {
            // 输出普通文字
            DrawGlyph(charCode, GetCurrentRenderColor());
        }
    }
}
```

---

## 4. 逆向结论：为何你的文字无法变色？

基于对 `02DDDC60` 的分析，我们可以得出以下技术结论：

1.  **解析器是健全的**：这个函数能够完美解析 `|cffff0000`。如果你在内存里直接修改系统公告，颜色会生效。
2.  **乱码的根源**：
    - 在调用此函数前的 `02DDDC73 (call 2AE82F0)` 是 UTF-8 解码。
    - 如果你发送的是 **GBK 编码**，解码器会识别失败并返回 `0xFFFD` ()。
3.  **过滤器拦截**：
    - 对于玩家的聊天输入，游戏在将字符串传给这个解析器**之前**，有一个“清洗函数”。
    - 该清洗函数会扫描 `0x7C` (|)，如果发现是玩家输入的，会将其剔除或转义。
4.  **改色建议**：
    - **Hook 点**：不要改这个函数，而是找到**调用它的地方**，或者找到**聊天框提交字符串的地方**。
    - **绕过技巧**：在提交字符串时，如果将 `|` 替换为其他未被过滤的字符（如 `#`），并在 `02DDDCDD` 处修改 `cmp eax, 7C` 为 `cmp eax, 23` (#)，则可绕过过滤系统实现彩色聊天。
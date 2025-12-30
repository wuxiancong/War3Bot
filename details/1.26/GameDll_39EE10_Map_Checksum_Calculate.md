# 魔兽争霸 III (1.26a) 地图校验算法逆向核心文档
**Warcraft III Map Checksum Algorithm Reverse Engineering**

*   **目标模块**: `Game.dll` (v1.26.0.6401)
*   **分析基址**: `0x6F000000`
*   **核心入口**: `Game.dll + 0x39EE10` (`0x6F39EE10`)

---

graph TD
    Level1[Layer 1: 主控逻辑 6F39EE10] --> Init[SHA1_Init]
    Level1 --> LoadEnv[Layer 2: 环境校验 6F3B1D00]
    
    subgraph Layer2_Environment [Layer 2: 环境与盐值]
        LoadEnv --> Salt[输入 Salt: 0x03F1379E]
        LoadEnv --> HashCommon[Hash: common.j]
        LoadEnv --> HashBlizz[Hash: blizzard.j]
        LoadEnv --> HashScript[Hash: war3map.j]
    end

    Level1 --> LoopComp[Layer 1: 组件遍历 6F39ED00]
    
    subgraph Layer1_Loop [组件遍历]
        LoopComp --> ExtCheck{检查后缀}
        ExtCheck --> HashW3E[.w3e 地形]
        ExtCheck --> HashWPM[.wpm 路径]
        ExtCheck --> HashDOO[.doo 装饰物]
        ExtCheck --> HashW3U[.w3u 单位]
        ExtCheck --> HashRest[... 其他数据文件]
    end

    LoopComp --> Helper[Layer 2: 组件读取 6F39EC70]
    Helper --> SHA1Upd[Layer 2: SHA1_Update 6F00B890]
    SHA1Upd --> SHA1Trans[Layer 3: SHA1_Transform 6F00B3A0]
    
    Level1 --> Final[Layer 1: SHA1_Final 6F00B9B0]
	
---

## 1. 核心架构与偏移概览 (Architecture & Offsets)

为了方便调试器（如 OllyDbg / x64dbg）快速跳转，请参考以下核心偏移表：

| 逻辑层级 | 功能描述 | 绝对地址 | **基址偏移 (Offset)** |
| :--- | :--- | :--- | :--- |
| **Control** | **算法主入口 (Main)** | `6F39EE10` | **`+ 39EE10`** |
| **Control** | 组件遍历循环 (Loop) | `6F39ED00` | **`+ 39ED00`** |
| **Business**| **环境与加盐 (Env & Salt)**| `6F3B1D00` | **`+ 3B1D00`** |
| **Business**| 单组件处理 (Process) | `6F39EC70` | **`+ 39EC70`** |
| **Kernel** | SHA1_Init | `6F00B850` | **`+ 00B850`** |
| **Kernel** | SHA1_Update | `6F00B890` | **`+ 00B890`** |
| **Kernel** | SHA1_Final | `6F00B9B0` | **`+ 00B9B0`** |
| **Kernel** | SHA1_Transform | `6F00B3A0` | **`+ 00B3A0`** |
| **Data** | **Salt (盐值 0x03F1379E)** | `6F3B1DBB` | **`+ 3B1DBB`** |

---

## 2. 第一层：控制层 (The Controller)

### 2.1 主函数 `CalculateMapSHA1`
*   **Location**: `Game.dll + 0x39EE10`
*   **功能**: 算法总控，负责初始化 SHA1 Context，调用环境加载，并启动组件遍历。

```asm
; 函数头部：栈帧建立与 SHA1 初始化
6F39EE10 | 81EC 68010000    | sub esp, 168              ; [Offset +39EE10] 开辟栈空间
6F39EE16 | A1 40E1AA6F      | mov eax, [6FAAE140]       ; [Offset +AAE140] 获取 Security Cookie
...
6F39EE2E | E8 1DCAC6FF      | call game.6F00B850        ; [Offset +00B850] CALL SHA1_Init(&ctx)

; 路径处理
6F39EE33 | 68 04010000      | push 104                  ; Push 260 (MAX_PATH)
6F39EE3E | E8 2D2FC7FF      | call game.6F011D70        ; [Offset +011D70] PathNormalize()
6F39EE43 | 85C0             | test eax, eax             ; 检查路径是否有效
6F39EE45 | 74 4A            | je game.6F39EE91          ; [Offset +39EE91] 无效则退出

; 版本标志位处理 (Legacy Flag)
6F39EE52 | BE AB170000      | mov esi, 17AB             ; ESI = 6059 (v1.21b Build Number)
6F39EE57 | 8B8424 74010000  | mov eax, [esp+174]        ; 读取参数
...

; [关键调用 1] 进入业务层：加载环境、盐值和脚本
6F39EE69 | E8 F22F0100      | call game.6F3B1E60        ; [Offset +3B1E60] -> 跳转至 EnvLoader
                                                        ; 注意：此函数内部会调用 +3B1D00

; [关键调用 2] 进入控制层：遍历地图组件 (.w3e, .w3u 等)
6F39EE79 | 8D4C24 08        | lea ecx, [esp+8]          ; ECX = Context 指针
6F39EE7D | E8 7EFEFFFF      | call game.6F39ED00        ; [Offset +39ED00] CALL ComponentLoop

; 结算输出
6F39EE87 | E8 24CBC6FF      | call game.6F00B9B0        ; [Offset +00B9B0] CALL SHA1_Final
```

### 2.2 组件遍历循环 `ComponentLoop`
*   **Location**: `Game.dll + 0x39ED00`
*   **功能**: 按硬编码顺序检查文件扩展名。

```asm
6F39ED00 | 56               | push esi                  ; [Offset +39ED00] 函数入口
6F39ED08 | B9 2C6A876F      | mov ecx, game.6F876A2C    ; [Offset +876A2C] String: ".w3e" (地形)
6F39ED0D | E8 5EFFFFFF      | call game.6F39EC70        ; [Offset +39EC70] ProcessComponent()

6F39ED14 | B9 246A876F      | mov ecx, game.6F876A24    ; [Offset +876A24] String: ".wpm" (路径)
6F39ED19 | E8 52FFFFFF      | call game.6F39EC70        ; [Offset +39EC70] ProcessComponent()

6F39ED20 | B9 2819946F      | mov ecx, game.6F941928    ; [Offset +941928] String: ".doo" (装饰物)
6F39ED25 | E8 46FFFFFF      | call game.6F39EC70

; ... 后续依次为 .w3u (单位), .w3b (可破坏), .w3d (定义), .w3a (技能), .w3q (科技)
```

---

## 3. 第二层：业务逻辑层 (The Business Logic)

### 3.1 环境与加盐 `EnvLoader`
*   **Location**: `Game.dll + 0x3B1D00` (由 `+3B1E60` 内部调用)
*   **功能**: **反作弊核心**。注入环境文件和魔数盐值。

```asm
; 加载 common.j
6F3B1D4E | BE 1831936F      | mov esi, game.6F933118    ; [Offset +933118] String: "common.j"
6F3B1D53 | E8 18FCFFFF      | call game.6F3B1970        ; [Offset +3B1970] ScriptLoader
6F3B1D73 | E8 189BC5FF      | call game.6F00B890        ; [Offset +00B890] SHA1_Update(common.j)

; 加载 blizzard.j
6F3B1D82 | BE C825946F      | mov esi, game.6F9425C8    ; [Offset +9425C8] String: "blizzard.j"
6F3B1D8F | E8 DCFBFFFF      | call game.6F3B1970        ; [Offset +3B1970] ScriptLoader
6F3B1DAD | E8 DE9AC5FF      | call game.6F00B890        ; [Offset +00B890] SHA1_Update(blizzard.j)

; [核心] 注入 Salt (盐值)
; 这里的 3F1379E 是立即数，硬编码在指令中
6F3B1DBB | C74424 18 9E37F103 | mov [esp+18], 3F1379E   ; [Offset +3B1DBB] Value: 0x03F1379E
6F3B1DC3 | E8 C89AC5FF        | call game.6F00B890      ; [Offset +00B890] SHA1_Update(&Salt, 4)

; 加载 war3map.j (地图核心脚本)
6F3B1DEF | E8 9C9AC5FF      | call game.6F00B890        ; [Offset +00B890] SHA1_Update(war3map.j)
```

### 3.2 单组件处理 `ProcessComponent`
*   **Location**: `Game.dll + 0x39EC70`
*   **功能**: 拼接文件名 -> MPQ查找 -> 读取 -> Hash。

```asm
6F39EC86 | 68 CC50876F      | push game.6F8750CC        ; [Offset +8750CC] String: "war3map"
6F39EC8B | 68 F0C6926F      | push game.6F92C6F0        ; [Offset +92C6F0] String: "%s%s"
...
6F39ECBB | E8 90281200      | call game.6F4C1550        ; [Offset +4C1550] MPQ_OpenFile
6F39ECD0 | E8 BBCBC6FF      | call game.6F00B890        ; [Offset +00B890] SHA1_Update
```

---

## 4. 第三层：内核与IO层 (Kernel & IO)

### 4.1 SHA-1 核心变换 `SHA1_Transform`
*   **Location**: `Game.dll + 0x00B3A0`
*   **功能**: 标准 FIPS 180-1 压缩函数。

**关键常数验证 (Magic Constants Verification)**:
*   `+00B4CC`: `0x5A827999` (K0)
*   `+00B592`: `0x6ED9EBA1` (K1)
*   `+00B668`: `0x8F1BBCDC` (K2) - 汇编中显示为 `-70E44324`
*   `+00B75F`: `0xCA62C1D6` (K3) - 汇编中显示为 `-359D3E2A`

### 4.2 MPQ 接口 `MPQ_Interface`
*   **Location**: `Game.dll + 0x4C1550`
*   **功能**: 封装了 Storm.dll 的调用，包含内存缓存 (`FileCache`)。

```asm
6F4C157F | E8 4CDBFFFF      | call game.6F4BF0D0        ; [Offset +4BF0D0] CheckFileCache
...
6F4C1658 | E8 AF9F2200      | call <JMP.&Ordinal#279>   ; Storm.SFileOpenFileEx
6F4C15F3 | E8 BA9F2200      | call <JMP.&Ordinal#401>   ; Storm.SMemAlloc
```

---

## 5. 逻辑还原 (Detailed Reconstruction)

基于上述基址偏移的详细分析，我们可以构建出精确的逻辑模型：

```cpp
// Game.dll Base: 0x6F000000
// Logic reconstruction based on offset analysis

void CalculateMapChecksum_126a(const char* mapPath) {
    SHA1_CTX ctx;
    
    // [Offset +00B850]
    SHA1_Init(&ctx); 

    // --- Phase 1: Environment & Salt (Offset +3B1D00) ---
    
    // 1. Common.j
    // String at +933118
    void* pCommon = LoadFile("common.j"); 
    SHA1_Update(&ctx, pCommon, size);
    
    // 2. Blizzard.j
    // String at +9425C8
    void* pBlizz = LoadFile("blizzard.j");
    SHA1_Update(&ctx, pBlizz, size);
    
    // 3. The Salt (Magic Number)
    // Code at +3B1DBB
    DWORD salt = 0x03F1379E; 
    SHA1_Update(&ctx, &salt, 4);

    // 4. Map Script
    // Code at +3B1DEF
    void* pMapScript = LoadFileFromMPQ(mapPath, "war3map.j");
    SHA1_Update(&ctx, pMapScript, size);

    // --- Phase 2: Component Loop (Offset +39ED00) ---
    
    const char* extList[] = {
        ".w3e", // [+876A2C]
        ".wpm", // [+876A24]
        ".doo", // [+941928]
        ".w3u", // [+92C560]
        ".w3b", // [+92C650]
        ".w3d", // [+92C680]
        ".w3a", // [+92C5C0]
        ".w3q"  // [+92C620]
    };

    for (const char* ext : extList) {
        // [Offset +39EC70]
        char filename[32];
        sprintf(filename, "war3map%s", ext); // String format at +92C6F0
        
        if (FileExistsInMPQ(mapPath, filename)) {
            void* data = LoadFileFromMPQ(mapPath, filename);
            SHA1_Update(&ctx, data, size);
        }
    }

    // --- Phase 3: Finalize (Offset +00B9B0) ---
    BYTE checksum[20];
    SHA1_Final(checksum, &ctx);
}
```

## 6. 总结 (Conclusion)

通过将基址锁定在 `0x6F000000`，您可以直接在调试器中使用 `Ctrl+G` 跳转到上述 **Offset** 指向的位置。

*   **调试建议**:
    *   在 `6F3B1DBB` (`+3B1DBB`) 下断点，可以观察到 Salt 被推入栈的瞬间。
    *   在 `6F39ED00` (`+39ED00`) 下断点，可以单步跟踪它如何跳过不存在的文件（例如只修改了贴图的地图，此循环流程将完全一致）。
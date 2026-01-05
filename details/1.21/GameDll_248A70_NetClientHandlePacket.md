# Warcraft III (1.21 Legacy) - Create Account (Salt Bypass) Analysis

本文档分析了 `Game.dll` (v1.21) 中创建账号请求时的密码盐值（Salt）生成逻辑，并提供了绕过 SRP 安全验证的补丁方案。

## 📍 基础信息 (Metadata)

- **目标模块**: `Game.dll`
- **游戏版本**: `1.21.0.6263` (Legacy)
- **关键地址**: `0x6F231840` (相对基址 `Game.dll + 2317E0`)
- **功能目标**: 禁用客户端生成随机 Salt 的过程，允许发送未加密/弱加密的账号创建请求（常用于 PVPGN 私服或免 CD 环境）。

---

```assembly
6F2317E0  | 55                      | push ebp                                     |
6F2317E1  | 8BEC                    | mov ebp,esp                                  |
6F2317E3  | 81EC 00020000           | sub esp,200                                  |
6F2317E9  | 53                      | push ebx                                     |
6F2317EA  | 56                      | push esi                                     |
6F2317EB  | 8BF1                    | mov esi,ecx                                  |
6F2317ED  | 57                      | push edi                                     |
6F2317EE  | 6A 00                   | push 0                                       |
6F2317F0  | 8BD6                    | mov edx,esi                                  |
6F2317F2  | B9 CB000940             | mov ecx,400900CB                             |
6F2317F7  | E8 A4260600             | call game.6F293EA0                           |
6F2317FC  | 8B4D 08                 | mov ecx,dword ptr ss:[ebp+8]                 |
6F2317FF  | C786 D8010000 00000000  | mov dword ptr ds:[esi+1D8],0                 |
6F231809  | 8B79 18                 | mov edi,dword ptr ds:[ecx+18]                |
6F23180C  | 8D47 FF                 | lea eax,dword ptr ds:[edi-1]                 |
6F23180F  | 83F8 3C                 | cmp eax,3C                                   |
6F231812  | 0F87 E5010000           | ja game.6F2319FD                             |
6F231818  | 0FB680 901A236F         | movzx eax,byte ptr ds:[eax+6F231A90]         |
6F23181F  | FF2485 6C1A236F         | jmp dword ptr ds:[eax*4+6F231A6C]            |
6F231826  | 83FF 3C                 | cmp edi,3C                                   |
6F231829  | 75 0A                   | jne game.6F231835                            |
6F23182B  | C786 E8010000 01000000  | mov dword ptr ds:[esi+1E8],1                 |
6F231835  | E8 D6EFFFFF             | call game.6F230810                           |
6F23183A  | 8B8E F8010000           | mov ecx,dword ptr ds:[esi+1F8]               |
6F231840  | 6A 10                   | push 10                                      | <--- 在这里修改
6F231842  | 8BF8                    | mov edi,eax                                  |
6F231844  | E8 871C4200             | call game.6F6534D0                           |
6F231849  | 50                      | push eax                                     |
6F23184A  | 8D8F 1C020000           | lea ecx,dword ptr ds:[edi+21C]               |
6F231850  | 51                      | push ecx                                     |
6F231851  | E8 BA5F1600             | call <JMP.&Ordinal#501>                      |
6F231856  | E8 F511DDFF             | call game.6F002A50                           |
6F23185B  | 8D55 08                 | lea edx,dword ptr ss:[ebp+8]                 |
6F23185E  | 52                      | push edx                                     |
6F23185F  | 6A 2D                   | push 2D                                      |
6F231861  | 8BC8                    | mov ecx,eax                                  |
6F231863  | E8 1813DDFF             | call game.6F002B80                           |
6F231868  | 8B4D 08                 | mov ecx,dword ptr ss:[ebp+8]                 |
6F23186B  | E8 70834A00             | call game.6F6D9BE0                           |
6F231870  | 8B9F C8030000           | mov ebx,dword ptr ds:[edi+3C8]               |
6F231876  | 68 FF000000             | push FF                                      |
6F23187B  | 8BCB                    | mov ecx,ebx                                  |
6F23187D  | E8 1ECCFFFF             | call game.6F22E4A0                           |
6F231882  | 8BF8                    | mov edi,eax                                  |
6F231884  | 83FF 24                 | cmp edi,24                                   |
6F231887  | 0F87 8E000000           | ja game.6F23191B                             |
6F23188D  | 0FB687 E81A236F         | movzx eax,byte ptr ds:[edi+6F231AE8]         |
6F231894  | FF2485 D01A236F         | jmp dword ptr ds:[eax*4+6F231AD0]            |
6F23189B  | 56                      | push esi                                     |
6F23189C  | 6A 13                   | push 13                                      |
6F23189E  | 8BCB                    | mov ecx,ebx                                  |
6F2318A0  | E8 7BD1FFFF             | call game.6F22EA20                           |
6F2318A5  | 5F                      | pop edi                                      |
6F2318A6  | 5E                      | pop esi                                      |
6F2318A7  | B8 01000000             | mov eax,1                                    |
6F2318AC  | 5B                      | pop ebx                                      |
6F2318AD  | 8BE5                    | mov esp,ebp                                  |
6F2318AF  | 5D                      | pop ebp                                      |
6F2318B0  | C2 0400                 | ret 4                                        |
6F2318B3  | 68 00020000             | push 200                                     |
6F2318B8  | 8D95 00FEFFFF           | lea edx,dword ptr ss:[ebp-200]               |
6F2318BE  | B9 7036806F             | mov ecx,game.6F803670                        | <--- ERROR_ID_INVALIDPARAMS
6F2318C3  | E8 181AE9FF             | call game.6F0C32E0                           |
6F2318C8  | E9 68010000             | jmp game.6F231A35                            |
6F2318CD  | 68 00020000             | push 200                                     |
6F2318D2  | 8D95 00FEFFFF           | lea edx,dword ptr ss:[ebp-200]               |
6F2318D8  | B9 B0CD806F             | mov ecx,game.6F80CDB0                        | <--- ERROR_ID_NOTLOGGEDON
6F2318DD  | E8 FE19E9FF             | call game.6F0C32E0                           |
6F2318E2  | E9 4E010000             | jmp game.6F231A35                            |
6F2318E7  | 68 00020000             | push 200                                     |
6F2318EC  | 8D95 00FEFFFF           | lea edx,dword ptr ss:[ebp-200]               |
6F2318F2  | B9 30DB806F             | mov ecx,game.6F80DB30                        | <--- ERROR_ID_NOTCONNECTED
6F2318F7  | E8 E419E9FF             | call game.6F0C32E0                           |
6F2318FC  | E9 34010000             | jmp game.6F231A35                            |
6F231901  | 68 00020000             | push 200                                     |
6F231906  | 8D95 00FEFFFF           | lea edx,dword ptr ss:[ebp-200]               |
6F23190C  | B9 9436806F             | mov ecx,game.6F803694                        | <--- ERROR_ID_NOTINITIALIZED
6F231911  | E8 CA19E9FF             | call game.6F0C32E0                           |
6F231916  | E9 1A010000             | jmp game.6F231A35                            |
6F23191B  | 68 00020000             | push 200                                     |
6F231920  | 8D95 00FEFFFF           | lea edx,dword ptr ss:[ebp-200]               |
6F231926  | B9 AC36806F             | mov ecx,game.6F8036AC                        | <--- NETERROR_DEFAULTERROR
6F23192B  | E8 B019E9FF             | call game.6F0C32E0                           |
6F231930  | E9 FB000000             | jmp game.6F231A30                            |
6F231935  | B9 30DB806F             | mov ecx,game.6F80DB30                        | <--- ERROR_ID_NOTCONNECTED
6F23193A  | EB 16                   | jmp game.6F231952                            |
6F23193C  | B9 4CEA806F             | mov ecx,game.6F80EA4C                        | <--- ERROR_ID_BADSERVER
6F231941  | E9 BC000000             | jmp game.6F231A02                            |
6F231946  | B9 30EA806F             | mov ecx,game.6F80EA30                        | <--- ERROR_ID_UNSUPPORTEDACCOUNT
6F23194B  | EB 5D                   | jmp game.6F2319AA                            |
6F23194D  | B9 18EA806F             | mov ecx,game.6F80EA18                        | <--- ERROR_ID_UNKNOWNACCOUNT
6F231952  | 68 00020000             | push 200                                     |
6F231957  | 8D95 00FEFFFF           | lea edx,dword ptr ss:[ebp-200]               |
6F23195D  | E8 7E19E9FF             | call game.6F0C32E0                           |
6F231962  | 8B8E F8010000           | mov ecx,dword ptr ds:[esi+1F8]               |
6F231968  | 6A 01                   | push 1                                       |
6F23196A  | 68 2CC5846F             | push game.6F84C52C                           |
6F23196F  | E8 DC1A4200             | call game.6F653450                           |
6F231974  | 8B8E F8010000           | mov ecx,dword ptr ds:[esi+1F8]               |
6F23197A  | 898E D0010000           | mov dword ptr ds:[esi+1D0],ecx               |
6F231980  | E9 AB000000             | jmp game.6F231A30                            |
6F231985  | 8D41 1C                 | lea eax,dword ptr ds:[ecx+1C]                |
6F231988  | 85C0                    | test eax,eax                                 |
6F23198A  | 74 19                   | je game.6F2319A5                             |
6F23198C  | 8038 00                 | cmp byte ptr ds:[eax],0                      |
6F23198F  | 74 14                   | je game.6F2319A5                             |
6F231991  | 68 00020000             | push 200                                     |
6F231996  | 50                      | push eax                                     |
6F231997  | 8D95 00FEFFFF           | lea edx,dword ptr ss:[ebp-200]               |
6F23199D  | 52                      | push edx                                     |
6F23199E  | E8 6D5E1600             | call <JMP.&Ordinal#501>                      |
6F2319A3  | EB 15                   | jmp game.6F2319BA                            |
6F2319A5  | B9 18EA806F             | mov ecx,game.6F80EA18                        | <--- ERROR_ID_UNKNOWNACCOUNT
6F2319AA  | 68 00020000             | push 200                                     |
6F2319AF  | 8D95 00FEFFFF           | lea edx,dword ptr ss:[ebp-200]               |
6F2319B5  | E8 2619E9FF             | call game.6F0C32E0                           |
6F2319BA  | 8B8E F8010000           | mov ecx,dword ptr ds:[esi+1F8]               |
6F2319C0  | 6A 01                   | push 1                                       |
6F2319C2  | 68 2CC5846F             | push game.6F84C52C                           |
6F2319C7  | E8 841A4200             | call game.6F653450                           |
6F2319CC  | 8B86 F8010000           | mov eax,dword ptr ds:[esi+1F8]               |
6F2319D2  | 8986 D0010000           | mov dword ptr ds:[esi+1D0],eax               |
6F2319D8  | EB 56                   | jmp game.6F231A30                            |
6F2319DA  | 68 00020000             | push 200                                     |
6F2319DF  | 8D95 00FEFFFF           | lea edx,dword ptr ss:[ebp-200]               |
6F2319E5  | B9 ACCF806F             | mov ecx,game.6F80CFAC                        | <--- ERROR_ID_BADPASSWORD
6F2319EA  | E8 F118E9FF             | call game.6F0C32E0                           |
6F2319EF  | 8B8E FC010000           | mov ecx,dword ptr ds:[esi+1FC]               |
6F2319F5  | 898E D0010000           | mov dword ptr ds:[esi+1D0],ecx               |
6F2319FB  | EB 33                   | jmp game.6F231A30                            |
6F2319FD  | B9 AC36806F             | mov ecx,game.6F8036AC                        | <--- NETERROR_DEFAULTERROR
6F231A02  | 8D95 00FEFFFF           | lea edx,dword ptr ss:[ebp-200]               |
6F231A08  | 68 00020000             | push 200                                     |
6F231A0D  | E8 CE18E9FF             | call game.6F0C32E0                           |
6F231A12  | 8B8E F8010000           | mov ecx,dword ptr ds:[esi+1F8]               |
6F231A18  | 6A 01                   | push 1                                       |
6F231A1A  | 68 2CC5846F             | push game.6F84C52C                           |
6F231A1F  | E8 2C1A4200             | call game.6F653450                           |
6F231A24  | 8B96 F8010000           | mov edx,dword ptr ds:[esi+1F8]               |
6F231A2A  | 8996 D0010000           | mov dword ptr ds:[esi+1D0],edx               |
6F231A30  | 83FF 01                 | cmp edi,1                                    |
6F231A33  | 74 26                   | je game.6F231A5B                             |
6F231A35  | 6A 01                   | push 1                                       |
6F231A37  | 8D8E EC010000           | lea ecx,dword ptr ds:[esi+1EC]               |
6F231A3D  | E8 AE2B4300             | call game.6F6645F0                           |
6F231A42  | 6A 01                   | push 1                                       |
6F231A44  | 6A 04                   | push 4                                       |
6F231A46  | 6A 00                   | push 0                                       |
6F231A48  | 56                      | push esi                                     |
6F231A49  | 6A 0C                   | push C                                       |
6F231A4B  | 8D95 00FEFFFF           | lea edx,dword ptr ss:[ebp-200]               |
6F231A51  | B9 01000000             | mov ecx,1                                    |
6F231A56  | E8 B552FDFF             | call game.6F206D10                           |
6F231A5B  | 5F                      | pop edi                                      |
6F231A5C  | 5E                      | pop esi                                      |
6F231A5D  | B8 01000000             | mov eax,1                                    |
6F231A62  | 5B                      | pop ebx                                      |
6F231A63  | 8BE5                    | mov esp,ebp                                  |
6F231A65  | 5D                      | pop ebp                                      |
6F231A66  | C2 0400                 | ret 4                                        |
6F231A69  | 8D49 00                 | lea ecx,dword ptr ds:[ecx]                   |
6F231A6C  | 26:1823                 | sbb byte ptr es:[ebx],ah                     |
6F231A6F  | 6F                      | outsd                                        |
6F231A70  | 5B                      | pop ebx                                      |
6F231A71  | 1A23                    | sbb ah,byte ptr ds:[ebx]                     |
6F231A73  | 6F                      | outsd                                        |
6F231A74  | 35 19236F3C             | xor eax,3C6F2319                             |
6F231A79  | 1923                    | sbb dword ptr ds:[ebx],esp                   |
6F231A7B  | 6F                      | outsd                                        |
6F231A7C  | 4D                      | dec ebp                                      |
6F231A7D  | 1923                    | sbb dword ptr ds:[ebx],esp                   |
6F231A7F  | 6F                      | outsd                                        |
6F231A80  | 46                      | inc esi                                      |
6F231A81  | 1923                    | sbb dword ptr ds:[ebx],esp                   |
6F231A83  | 6F                      | outsd                                        |
6F231A84  | DA19                    | ficomp dword ptr ds:[ecx]                    |
6F231A86  | 236F 85                 | and ebp,dword ptr ds:[edi-7B]                |
6F231A89  | 1923                    | sbb dword ptr ds:[ebx],esp                   |
6F231A8B  | 6F                      | outsd                                        |
6F231A8C  | FD                      | std                                          |
6F231A8D  | 1923                    | sbb dword ptr ds:[ebx],esp                   |
6F231A8F  | 6F                      | outsd                                        |
6F231A90  | 0008                    | add byte ptr ds:[eax],cl                     |
6F231A92  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231A94  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231A96  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231A98  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231A9A  | 0801                    | or byte ptr ds:[ecx],al                      |
6F231A9C  | 0802                    | or byte ptr ds:[edx],al                      |
6F231A9E  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AA0  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AA2  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AA4  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AA6  | 0803                    | or byte ptr ds:[ebx],al                      |
6F231AA8  | 04 05                   | add al,5                                     |
6F231AAA  | 06                      | push es                                      |
6F231AAB  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AAD  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AAF  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AB1  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AB3  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AB5  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AB7  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AB9  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231ABB  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231ABD  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231ABF  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AC1  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AC3  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AC5  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AC7  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231AC9  | 0808                    | or byte ptr ds:[eax],cl                      |
6F231ACB  | 0007                    | add byte ptr ds:[edi],al                     |
6F231ACD  | 8D49 00                 | lea ecx,dword ptr ds:[ecx]                   |
6F231AD0  | B3 18                   | mov bl,18                                    |
6F231AD2  | 236F 9B                 | and ebp,dword ptr ds:[edi-65]                |
6F231AD5  | 1823                    | sbb byte ptr ds:[ebx],ah                     |
6F231AD7  | 6F                      | outsd                                        |
6F231AD8  | 0119                    | add dword ptr ds:[ecx],ebx                   |
6F231ADA  | 236F E7                 | and ebp,dword ptr ds:[edi-19]                |
6F231ADD  | 1823                    | sbb byte ptr ds:[ebx],ah                     |
6F231ADF  | 6F                      | outsd                                        |
6F231AE0  | CD 18                   | int 18                                       |
6F231AE2  | 236F 1B                 | and ebp,dword ptr ds:[edi+1B]                |
6F231AE5  | 1923                    | sbb dword ptr ds:[ebx],esp                   |
6F231AE7  | 6F                      | outsd                                        |
6F231AE8  | 0001                    | add byte ptr ds:[ecx],al                     |
6F231AEA  | 05 05050205             | add eax,5020505                              |
6F231AEF  | 05 05050505             | add eax,5050505                              |
6F231AF4  | 05 05030505             | add eax,5050305                              |
6F231AF9  | 05 05050505             | add eax,5050505                              |
6F231AFE  | 05 05050505             | add eax,5050505                              |
6F231B03  | 05 05050505             | add eax,5050505                              |
6F231B08  | 05 05050504             | add eax,4050505                              |
```

## 🔍 代码逻辑流分析

该逻辑位于客户端准备向服务器发送 `SID_AUTH_ACCOUNTCREATE` 数据包的函数中。在发送之前，客户端通常需要生成一个随机的 Salt（盐值）用于 SRP 协议的密码验证。

### 1. 原始汇编代码 (Original Code)
根据调试器反汇编结果：

```assembly
; 上下文：eax 此时指向分配好的缓冲区，用于存放 Salt
6F231840  | 6A 10           | push 10           ; ★ 压入参数：Size = 16 (0x10) 字节
6F231842  | 8BF8            | mov edi,eax       ; 保存缓冲区指针到 edi
6F231844  | E8 871C4200     | call game.6F6534D0 ; ★ 调用生成函数：GenerateRandom(buffer, size)
6F231849  | 50              | push eax          ; 将 Salt 缓冲区指针压栈，准备后续使用
```

### 2. 逻辑解析
*   **`push 10`**: 指定要生成的随机数长度为 16 字节。
*   **`call game.6F6534D0`**: 这是一个随机数生成函数（类似 `CSHA1::Update` 或 `RandomBytes`），它会用随机数据填充缓冲区。
*   **SRP 协议**: 在标准战网中，这个 Salt 会结合用户密码生成 Verifier 发送给服务器。

### 3. 补丁意图 (Patch Logic)
为了兼容不支持完整 SRP 验证的私服，或者简化登录流程，我们需要**阻止客户端生成这个随机 Salt**。
这就意味着我们要让那个 `call` 不执行，或者让它不做任何事。

---

## 🛠️ 补丁方案 (Patch Solution)

我们采用 **NOP (No Operation)** 填充的方式来移除该逻辑。

### ⚠️ 关键注意事项 (Stack Balance)
我们不能仅仅 NOP 掉 `call` 指令，因为 `call` 前面有一个 `push 10`。
*   如果只 NOP `call`，`push` 压入栈的 4 字节数据将无人清理，导致函数返回时堆栈不平衡（Crash）。
*   **必须同时 NOP 掉 `push 10`**。

同时，中间夹着一条 `mov edi, eax` 指令，这条指令**必须保留**，因为它保存了重要的指针，后续代码（如 `lea ecx, [edi+...]`）依赖 `edi` 的值。

### 修改方案
*   **地址 `6F231840` (2字节)**: `6A 10` -> `90 90` (NOP 掉参数)
*   **地址 `6F231842` (2字节)**: `8B F8` -> `8B F8` (保留原指令，或者 Patch 时写回去)
*   **地址 `6F231844` (5字节)**: `E8 ...` -> `90 90 90 90 90` (NOP 掉生成函数)

### 最终效果代码
修改后的指令流如下：
```assembly
6F231840  | 90              | nop               ; (原 push 10) 已移除
6F231841  | 90              | nop               ; 
6F231842  | 8BF8            | mov edi,eax       ; ★ 这里的指针保存逻辑被保留
6F231844  | 90              | nop               ; (原 call Generate) 已移除
6F231845  | 90              | nop
6F231846  | 90              | nop
6F231847  | 90              | nop
6F231848  | 90              | nop
6F231849  | 50              | push eax          ; 继续执行后续逻辑...
```

---

## 💻 C++ 实现代码

```cpp
/* [Create Account 1] Legacy 1.21 Signature */
unsigned char legacy_create_account1_SignData[] = {
    // 6A 10             | push 10
    0xFF, 0x6A, 0xFF, 0x10,

    // 8B F8             | mov edi, eax
    0xFF, 0x8B, 0xFF, 0xF8,

    // E8 XX XX XX XX    | call game.6F6534D0 (Generate Salt)
    // 这里的偏移量是通配符，适配不同小版本
    0xFF, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // 50                | push eax (Anchor)
    0xFF, 0x50
};

t_sign legacy_create_account1_Sign = { 
    10, 
    legacy_create_account1_SignData, 
    0, 
    "create_account1_legacy" 
};

// Patch Data:
// 1. NOP (push 10)
// 2. Rewrite (mov edi, eax) - 保持原逻辑
// 3. NOP (call Generate)
unsigned char legacy_create_account1_PatchData[] = { 
    0x90, 0x90,             // NOP push
    0x8B, 0xF8,             // MOV edi, eax (Restore)
    0x90, 0x90, 0x90, 0x90, 0x90 // NOP call
};

t_patch legacy_create_account1_Patch = { 
    9, 
    legacy_create_account1_PatchData, 
    "create_account1_legacy" 
};
```
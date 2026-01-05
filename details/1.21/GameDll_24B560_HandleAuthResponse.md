# Warcraft III (1.21 Legacy) - Auth Response Analysis

本文档分析了 `Game.dll` (v1.21) 中处理服务器认证响应（Auth Response / CDKey Verification）的关键函数。

## 📍 基础信息 (Metadata)

- **目标模块**: `Game.dll`
- **游戏版本**: `1.21.0.6263` (Legacy)
- **函数基址**: `0x6F24B560` (相对基址 `Game.dll + 4B560`)
- **关键逻辑**: 处理战网返回的 CDKey 验证结果、版本检查结果以及下载状态。

---

```assembly
6F24B560  | 55                      | push ebp                                     |
6F24B561  | 8BEC                    | mov ebp,esp                                  |
6F24B563  | 81EC 04070000           | sub esp,704                                  |
6F24B569  | 53                      | push ebx                                     |
6F24B56A  | 56                      | push esi                                     |
6F24B56B  | 8BF1                    | mov esi,ecx                                  |
6F24B56D  | 57                      | push edi                                     |
6F24B56E  | 6A 00                   | push 0                                       |
6F24B570  | 8BD6                    | mov edx,esi                                  |
6F24B572  | B9 C8000940             | mov ecx,400900C8                             |
6F24B577  | E8 24890400             | call game.6F293EA0                           |
6F24B57C  | 8B7D 08                 | mov edi,dword ptr ss:[ebp+8]                 |
6F24B57F  | 8B47 18                 | mov eax,dword ptr ds:[edi+18]                |
6F24B582  | 8945 FC                 | mov dword ptr ss:[ebp-4],eax                 |
6F24B585  | 48                      | dec eax                                      |
6F24B586  | 83F8 34                 | cmp eax,34                                   |
6F24B589  | 0F87 08030000           | ja game.6F24B897                             |
6F24B58F  | 0FB680 1CB9246F         | movzx eax,byte ptr ds:[eax+6F24B91C]         |
6F24B596  | FF2485 F4B8246F         | jmp dword ptr ds:[eax*4+6F24B8F4]            |
6F24B59D  | 68 7C10816F             | push game.6F81107C                           | <--- BNET_CONNECT_DOWNLOAD
6F24B5A2  | 8BCE                    | mov ecx,esi                                  |
6F24B5A4  | E8 87FEFFFF             | call game.6F24B430                           |
6F24B5A9  | E8 6252FEFF             | call game.6F230810                           |
6F24B5AE  | 68 FFFFFF7F             | push 7FFFFFFF                                |
6F24B5B3  | 68 2CC5846F             | push game.6F84C52C                           |
6F24B5B8  | 8D5F 24                 | lea ebx,dword ptr ds:[edi+24]                |
6F24B5BB  | 53                      | push ebx                                     |
6F24B5BC  | 8945 08                 | mov dword ptr ss:[ebp+8],eax                 |
6F24B5BF  | E8 FAC21400             | call <JMP.&Ordinal#508>                      |
6F24B5C4  | 85C0                    | test eax,eax                                 |
6F24B5C6  | 74 52                   | je game.6F24B61A                             |
6F24B5C8  | 8B8E A0040000           | mov ecx,dword ptr ds:[esi+4A0]               |
6F24B5CE  | 53                      | push ebx                                     |
6F24B5CF  | E8 AC26FEFF             | call game.6F22DC80                           |
6F24B5D4  | 85C0                    | test eax,eax                                 |
6F24B5D6  | 74 42                   | je game.6F24B61A                             |
6F24B5D8  | 8B8E A0040000           | mov ecx,dword ptr ds:[esi+4A0]               |
6F24B5DE  | 68 00010000             | push 100                                     |
6F24B5E3  | E8 7826FEFF             | call game.6F22DC60                           |
6F24B5E8  | 50                      | push eax                                     |
6F24B5E9  | E8 5226FEFF             | call game.6F22DC40                           |
6F24B5EE  | 8B4D 08                 | mov ecx,dword ptr ss:[ebp+8]                 |
6F24B5F1  | 05 04010000             | add eax,104                                  |
6F24B5F6  | 50                      | push eax                                     |
6F24B5F7  | 81C1 1C010000           | add ecx,11C                                  |
6F24B5FD  | 51                      | push ecx                                     |
6F24B5FE  | E8 0DC21400             | call <JMP.&Ordinal#501>                      |
6F24B603  | 8B8E A0040000           | mov ecx,dword ptr ds:[esi+4A0]               |
6F24B609  | 68 B0E9806F             | push game.6F80E9B0                           | <--- Battle.net Gateways
6F24B60E  | 68 047C7E6F             | push game.6F7E7C04                           | <--- Warcraft III
6F24B613  | E8 A81EFEFF             | call game.6F22D4C0                           |
6F24B618  | EB 15                   | jmp game.6F24B62F                            |
6F24B61A  | 8B55 08                 | mov edx,dword ptr ss:[ebp+8]                 |
6F24B61D  | 68 00010000             | push 100                                     |
6F24B622  | 53                      | push ebx                                     |
6F24B623  | 81C2 1C010000           | add edx,11C                                  |
6F24B629  | 52                      | push edx                                     |
6F24B62A  | E8 E1C11400             | call <JMP.&Ordinal#501>                      |
6F24B62F  | 8B45 08                 | mov eax,dword ptr ss:[ebp+8]                 |
6F24B632  | 68 00010000             | push 100                                     |
6F24B637  | 8D78 1C                 | lea edi,dword ptr ds:[eax+1C]                |
6F24B63A  | 53                      | push ebx                                     |
6F24B63B  | 57                      | push edi                                     |
6F24B63C  | E8 CFC11400             | call <JMP.&Ordinal#501>                      |
6F24B641  | B2 2E                   | mov dl,2E                                    |
6F24B643  | 8BCF                    | mov ecx,edi                                  |
6F24B645  | E8 AAC21400             | call <JMP.&Ordinal#569>                      |
6F24B64A  | 85C0                    | test eax,eax                                 |
6F24B64C  | 74 03                   | je game.6F24B651                             |
6F24B64E  | C600 00                 | mov byte ptr ds:[eax],0                      |
6F24B651  | 6A 01                   | push 1                                       |
6F24B653  | 8BCE                    | mov ecx,esi                                  |
6F24B655  | C786 DC050000 0B000000  | mov dword ptr ds:[esi+5DC],B                 |
6F24B65F  | C786 D4050000 00000000  | mov dword ptr ds:[esi+5D4],0                 |
6F24B669  | E8 A2080000             | call game.6F24BF10                           |
6F24B66E  | 85C0                    | test eax,eax                                 |
6F24B670  | 74 1A                   | je game.6F24B68C                             |
6F24B672  | 6A 02                   | push 2                                       |
6F24B674  | 8BCE                    | mov ecx,esi                                  |
6F24B676  | E8 95080000             | call game.6F24BF10                           |
6F24B67B  | 85C0                    | test eax,eax                                 |
6F24B67D  | 74 0D                   | je game.6F24B68C                             |
6F24B67F  | 6A 08                   | push 8                                       |
6F24B681  | 8BCE                    | mov ecx,esi                                  |
6F24B683  | E8 88080000             | call game.6F24BF10                           |
6F24B688  | 85C0                    | test eax,eax                                 |
6F24B68A  | 75 1A                   | jne game.6F24B6A6                            |
6F24B68C  | 68 00020000             | push 200                                     |
6F24B691  | 8D95 FCFCFFFF           | lea edx,dword ptr ss:[ebp-304]               |
6F24B697  | B9 6410816F             | mov ecx,game.6F811064                        | <--- ERROR_ID_DOWNLOADFAILED
6F24B69C  | E8 3F7CE7FF             | call game.6F0C32E0                           |
6F24B6A1  | E9 0C020000             | jmp game.6F24B8B2                            |
6F24B6A6  | 8B4D 08                 | mov ecx,dword ptr ss:[ebp+8]                 |
6F24B6A9  | E8 524AFEFF             | call game.6F230100                           |
6F24B6AE  | 85C0                    | test eax,eax                                 |
6F24B6B0  | 8986 EC050000           | mov dword ptr ds:[esi+5EC],eax               |
6F24B6B6  | 0F84 F0010000           | je game.6F24B8AC                             |
6F24B6BC  | 838E DC050000 04        | or dword ptr ds:[esi+5DC],4                  |
6F24B6C3  | E9 E4010000             | jmp game.6F24B8AC                            |
6F24B6C8  | B9 4810816F             | mov ecx,game.6F811048                        | <--- NETERROR_CANTCONNECTBNET
6F24B6CD  | E9 CA010000             | jmp game.6F24B89C                            |
6F24B6D2  | 68 FFFFFF7F             | push 7FFFFFFF                                |
6F24B6D7  | 68 2CC5846F             | push game.6F84C52C                           |
6F24B6DC  | 8D5F 24                 | lea ebx,dword ptr ds:[edi+24]                |
6F24B6DF  | 53                      | push ebx                                     |
6F24B6E0  | E8 D9C11400             | call <JMP.&Ordinal#508>                      |
6F24B6E5  | 85C0                    | test eax,eax                                 |
6F24B6E7  | 74 0C                   | je game.6F24B6F5                             |
6F24B6E9  | 8B8E A0040000           | mov ecx,dword ptr ds:[esi+4A0]               |
6F24B6EF  | 53                      | push ebx                                     |
6F24B6F0  | E8 8B25FEFF             | call game.6F22DC80                           |
6F24B6F5  | 8B8E A0040000           | mov ecx,dword ptr ds:[esi+4A0]               |
6F24B6FB  | 68 B0E9806F             | push game.6F80E9B0                           | <--- Battle.net Gateways
6F24B700  | 68 047C7E6F             | push game.6F7E7C04                           | <--- Warcraft III
6F24B705  | E8 B61DFEFF             | call game.6F22D4C0                           |
6F24B70A  | 8BCE                    | mov ecx,esi                                  |
6F24B70C  | E8 6FFCFFFF             | call game.6F24B380                           |
6F24B711  | 57                      | push edi                                     |
6F24B712  | 8BCE                    | mov ecx,esi                                  |
6F24B714  | E8 17090000             | call game.6F24C030                           |
6F24B719  | 5F                      | pop edi                                      |
6F24B71A  | 5E                      | pop esi                                      |
6F24B71B  | B8 01000000             | mov eax,1                                    |
6F24B720  | 5B                      | pop ebx                                      |
6F24B721  | 8BE5                    | mov esp,ebp                                  |
6F24B723  | 5D                      | pop ebp                                      |
6F24B724  | C2 0400                 | ret 4                                        |
6F24B727  | B9 3010816F             | mov ecx,game.6F811030                        | <--- ERROR_ID_VERSION_BAD
6F24B72C  | E9 6B010000             | jmp game.6F24B89C                            |
6F24B731  | B9 1810816F             | mov ecx,game.6F811018                        | <--- ERROR_ID_CDKEY_INVALID
6F24B736  | E9 61010000             | jmp game.6F24B89C                            |
6F24B73B  | 8D9F 28020000           | lea ebx,dword ptr ds:[edi+228]               |
6F24B741  | 8BBF 68020000           | mov edi,dword ptr ds:[edi+268]               |
6F24B747  | 68 80000000             | push 80                                      |
6F24B74C  | 8D95 FCFEFFFF           | lea edx,dword ptr ss:[ebp-104]               |
6F24B752  | 8BCF                    | mov ecx,edi                                  |
6F24B754  | E8 8753FEFF             | call game.6F230AE0                           |
6F24B759  | 85C0                    | test eax,eax                                 |
6F24B75B  | 74 22                   | je game.6F24B77F                             |
6F24B75D  | 8D8D FCFEFFFF           | lea ecx,dword ptr ss:[ebp-104]               |
6F24B763  | 51                      | push ecx                                     |
6F24B764  | 68 1010816F             | push game.6F811010                           |
6F24B769  | 8D95 7CFFFFFF           | lea edx,dword ptr ss:[ebp-84]                |
6F24B76F  | 68 80000000             | push 80                                      |
6F24B774  | 52                      | push edx                                     |
6F24B775  | E8 78C01400             | call <JMP.&Ordinal#578>                      |
6F24B77A  | 83C4 10                 | add esp,10                                   |
6F24B77D  | EB 07                   | jmp game.6F24B786                            |
6F24B77F  | C685 7CFFFFFF 00        | mov byte ptr ss:[ebp-84],0                   |
6F24B786  | 837D FC 14              | cmp dword ptr ss:[ebp-4],14                  |
6F24B78A  | 8D95 FCFAFFFF           | lea edx,dword ptr ss:[ebp-504]               |
6F24B790  | 68 00020000             | push 200                                     |
6F24B795  | 75 50                   | jne game.6F24B7E7                            |
6F24B797  | B9 F80F816F             | mov ecx,game.6F810FF8                        | <--- ERROR_ID_CDKEY_INUSE
6F24B79C  | E8 3F7BE7FF             | call game.6F0C32E0                           |
6F24B7A1  | 68 00020000             | push 200                                     |
6F24B7A6  | 8D95 FCF8FFFF           | lea edx,dword ptr ss:[ebp-704]               |
6F24B7AC  | B9 E02A806F             | mov ecx,game.6F802AE0                        | <--- UNKNOWN
6F24B7B1  | E8 2A7BE7FF             | call game.6F0C32E0                           |
6F24B7B6  | 85DB                    | test ebx,ebx                                 |
6F24B7B8  | 74 05                   | je game.6F24B7BF                             |
6F24B7BA  | 803B 00                 | cmp byte ptr ds:[ebx],0                      |
6F24B7BD  | 75 06                   | jne game.6F24B7C5                            |
6F24B7BF  | 8D9D FCF8FFFF           | lea ebx,dword ptr ss:[ebp-704]               |
6F24B7C5  | 57                      | push edi                                     |
6F24B7C6  | 53                      | push ebx                                     |
6F24B7C7  | 8D85 FCFAFFFF           | lea eax,dword ptr ss:[ebp-504]               |
6F24B7CD  | 50                      | push eax                                     |
6F24B7CE  | 8D8D FCFCFFFF           | lea ecx,dword ptr ss:[ebp-304]               |
6F24B7D4  | 68 00020000             | push 200                                     |
6F24B7D9  | 51                      | push ecx                                     |
6F24B7DA  | E8 13C01400             | call <JMP.&Ordinal#578>                      |
6F24B7DF  | 83C4 14                 | add esp,14                                   |
6F24B7E2  | E9 88000000             | jmp game.6F24B86F                            |
6F24B7E7  | B9 E00F816F             | mov ecx,game.6F810FE0                        | <--- ERROR_ID_CDKEY_DISABLED
6F24B7EC  | E8 EF7AE7FF             | call game.6F0C32E0                           |
6F24B7F1  | 8D95 FCFAFFFF           | lea edx,dword ptr ss:[ebp-504]               |
6F24B7F7  | EB 60                   | jmp game.6F24B859                            |
6F24B7F9  | 8BBF 68020000           | mov edi,dword ptr ds:[edi+268]               |
6F24B7FF  | 68 80000000             | push 80                                      |
6F24B804  | 8D95 FCFEFFFF           | lea edx,dword ptr ss:[ebp-104]               |
6F24B80A  | 8BCF                    | mov ecx,edi                                  |
6F24B80C  | E8 CF52FEFF             | call game.6F230AE0                           |
6F24B811  | 85C0                    | test eax,eax                                 |
6F24B813  | 74 22                   | je game.6F24B837                             |
6F24B815  | 8D85 FCFEFFFF           | lea eax,dword ptr ss:[ebp-104]               |
6F24B81B  | 50                      | push eax                                     |
6F24B81C  | 68 1010816F             | push game.6F811010                           |
6F24B821  | 8D8D 7CFFFFFF           | lea ecx,dword ptr ss:[ebp-84]                |
6F24B827  | 68 80000000             | push 80                                      |
6F24B82C  | 51                      | push ecx                                     |
6F24B82D  | E8 C0BF1400             | call <JMP.&Ordinal#578>                      |
6F24B832  | 83C4 10                 | add esp,10                                   |
6F24B835  | EB 07                   | jmp game.6F24B83E                            |
6F24B837  | C685 7CFFFFFF 00        | mov byte ptr ss:[ebp-84],0                   |
6F24B83E  | 68 00020000             | push 200                                     |
6F24B843  | 8D95 FCF8FFFF           | lea edx,dword ptr ss:[ebp-704]               |
6F24B849  | B9 C80F816F             | mov ecx,game.6F810FC8                        | <--- ERROR_ID_CDKEY_LANGUAGE
6F24B84E  | E8 8D7AE7FF             | call game.6F0C32E0                           |
6F24B853  | 8D95 FCF8FFFF           | lea edx,dword ptr ss:[ebp-704]               |
6F24B859  | 57                      | push edi                                     |
6F24B85A  | 52                      | push edx                                     |
6F24B85B  | 8D85 FCFCFFFF           | lea eax,dword ptr ss:[ebp-304]               |
6F24B861  | 68 00020000             | push 200                                     |
6F24B866  | 50                      | push eax                                     |
6F24B867  | E8 86BF1400             | call <JMP.&Ordinal#578>                      |
6F24B86C  | 83C4 10                 | add esp,10                                   |
6F24B86F  | 68 00020000             | push 200                                     |
6F24B874  | 8D8D 7CFFFFFF           | lea ecx,dword ptr ss:[ebp-84]                |
6F24B87A  | 51                      | push ecx                                     |
6F24B87B  | 8D95 FCFCFFFF           | lea edx,dword ptr ss:[ebp-304]               |
6F24B881  | 52                      | push edx                                     |
6F24B882  | E8 95BF1400             | call <JMP.&Ordinal#503>                      |
6F24B887  | EB 23                   | jmp game.6F24B8AC                            |
6F24B889  | B9 B00F816F             | mov ecx,game.6F810FB0                        | <--- ERROR_ID_CDKEY_MISMATCH
6F24B88E  | EB 0C                   | jmp game.6F24B89C                            |
6F24B890  | B9 900F816F             | mov ecx,game.6F810F90                        | <--- ERROR_ID_DOWNLOADFAILED_WRITE
6F24B895  | EB 05                   | jmp game.6F24B89C                            |
6F24B897  | B9 AC36806F             | mov ecx,game.6F8036AC                        | <--- NETERROR_DEFAULTERROR
6F24B89C  | 8D95 FCFCFFFF           | lea edx,dword ptr ss:[ebp-304]               |
6F24B8A2  | 68 00020000             | push 200                                     |
6F24B8A7  | E8 347AE7FF             | call game.6F0C32E0                           |
6F24B8AC  | 837D FC 01              | cmp dword ptr ss:[ebp-4],1                   |
6F24B8B0  | 74 32                   | je game.6F24B8E4                             |
6F24B8B2  | E8 6950FEFF             | call game.6F230920                           |
6F24B8B7  | 8BCE                    | mov ecx,esi                                  |
6F24B8B9  | E8 C2FAFFFF             | call game.6F24B380                           |
6F24B8BE  | E8 5D6E0400             | call game.6F292720                           |
6F24B8C3  | 8BCE                    | mov ecx,esi                                  |
6F24B8C5  | E8 56D0FFFF             | call game.6F248920                           |
6F24B8CA  | 6A 01                   | push 1                                       |
6F24B8CC  | 6A 04                   | push 4                                       |
6F24B8CE  | 6A 00                   | push 0                                       |
6F24B8D0  | 6A 00                   | push 0                                       |
6F24B8D2  | 6A 00                   | push 0                                       |
6F24B8D4  | 8D95 FCFCFFFF           | lea edx,dword ptr ss:[ebp-304]               |
6F24B8DA  | B9 01000000             | mov ecx,1                                    |
6F24B8DF  | E8 2CB4FBFF             | call game.6F206D10                           |
6F24B8E4  | 5F                      | pop edi                                      |
6F24B8E5  | 5E                      | pop esi                                      |
6F24B8E6  | B8 01000000             | mov eax,1                                    |
6F24B8EB  | 5B                      | pop ebx                                      |
6F24B8EC  | 8BE5                    | mov esp,ebp                                  |
6F24B8EE  | 5D                      | pop ebp                                      |
6F24B8EF  | C2 0400                 | ret 4                                        |
6F24B8F2  | 8BFF                    | mov edi,edi                                  |
6F24B8F4  | 9D                      | popfd                                        |
6F24B8F5  | B5 24                   | mov ch,24                                    |
6F24B8F7  | 6F                      | outsd                                        |
6F24B8F8  | C8 B624 6F              | enter 24B6,6F                                |
6F24B8FC  | D2B6 246F27B7           | shl byte ptr ds:[esi-48D890DC],cl            |
6F24B902  | 24 6F                   | and al,6F                                    |
6F24B904  | 31B7 246F3BB7           | xor dword ptr ds:[edi-48C490DC],esi          |
6F24B90A  | 24 6F                   | and al,6F                                    |
6F24B90C  | 89B8 246FF9B7           | mov dword ptr ds:[eax-480690DC],edi          |
6F24B912  | 24 6F                   | and al,6F                                    |
6F24B914  | 90                      | nop                                          |
6F24B915  | B8 246F97B8             | mov eax,B8976F24                             |
6F24B91A  | 24 6F                   | and al,6F                                    |
6F24B91C  | 0009                    | add byte ptr ds:[ecx],cl                     |
6F24B91E  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B920  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B922  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B924  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B926  | 0109                    | add dword ptr ds:[ecx],ecx                   |
6F24B928  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B92A  | 0902                    | or dword ptr ds:[edx],eax                    |
6F24B92C  | 0309                    | add ecx,dword ptr ds:[ecx]                   |
6F24B92E  | 04 05                   | add al,5                                     |
6F24B930  | 05 06070909             | add eax,9090706                              |
6F24B935  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B937  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B939  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B93B  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B93D  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B93F  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B941  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B943  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B945  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B947  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B949  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B94B  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B94D  | 0909                    | or dword ptr ds:[ecx],ecx                    |
6F24B94F  | 0908                    | or dword ptr ds:[eax],ecx                    |
6F24B951  | 90                      | nop                                          |
6F24B952  | 90                      | nop                                          |
6F24B953  | 90                      | nop                                          |
6F24B954  | 90                      | nop                                          |
6F24B955  | 90                      | nop                                          |
6F24B956  | 90                      | nop                                          |
6F24B957  | 90                      | nop                                          |
6F24B958  | 90                      | nop                                          |
6F24B959  | 90                      | nop                                          |
6F24B95A  | 90                      | nop                                          |
6F24B95B  | 90                      | nop                                          |
6F24B95C  | 90                      | nop                                          |
6F24B95D  | 90                      | nop                                          |
6F24B95E  | 90                      | nop                                          |
6F24B95F  | 90                      | nop                                          |
6F24B960  | 56                      | push esi                                     |
6F24B961  | 8BF1                    | mov esi,ecx                                  |
6F24B963  | 6A 00                   | push 0                                       |
6F24B965  | 8BD6                    | mov edx,esi                                  |
6F24B967  | B9 C8000940             | mov ecx,400900C8                             |
6F24B96C  | E8 2F850400             | call game.6F293EA0                           |
6F24B971  | 6A 00                   | push 0                                       |
6F24B973  | 8BD6                    | mov edx,esi                                  |
6F24B975  | B9 E2000940             | mov ecx,400900E2                             |
6F24B97A  | E8 21850400             | call game.6F293EA0                           |
6F24B97F  | 6A 00                   | push 0                                       |
6F24B981  | 8BD6                    | mov edx,esi                                  |
6F24B983  | B9 CD000940             | mov ecx,400900CD                             |
6F24B988  | E8 13850400             | call game.6F293EA0                           |
6F24B98D  | 6A 00                   | push 0                                       |
6F24B98F  | 8BD6                    | mov edx,esi                                  |
6F24B991  | B9 CE000940             | mov ecx,400900CE                             |
6F24B996  | E8 05850400             | call game.6F293EA0                           |
6F24B99B  | E8 704EFEFF             | call game.6F230810                           |
6F24B9A0  | 8B88 C8030000           | mov ecx,dword ptr ds:[eax+3C8]               |
6F24B9A6  | 56                      | push esi                                     |
6F24B9A7  | E8 9430FEFF             | call game.6F22EA40                           |
6F24B9AC  | E8 6F6D0400             | call game.6F292720                           |
6F24B9B1  | 8BCE                    | mov ecx,esi                                  |
6F24B9B3  | E8 C8F9FFFF             | call game.6F24B380                           |
6F24B9B8  | 8BCE                    | mov ecx,esi                                  |
6F24B9BA  | E8 61CFFFFF             | call game.6F248920                           |
6F24B9BF  | B8 01000000             | mov eax,1                                    |
6F24B9C4  | 5E                      | pop esi                                      |
6F24B9C5  | C3                      | ret                                          |
```
---

## 🔍 代码逻辑流分析

该函数是一个典型的网络包处理或状态机处理函数，包含大量的错误码分支判断。

### 1. 初始化与状态分发
函数开头初始化栈帧，并通过跳转表（Switch Case）根据 `eax` 的值跳转到不同的处理逻辑。
```assembly
6F24B560  | 55               | push ebp
...
6F24B586  | 83F8 34          | cmp eax,34            ; 检查状态码/包类型
6F24B589  | 0F87 08030000    | ja game.6F24B897      ; Default Error
6F24B596  | FF2485 F4B8246F  | jmp dword ptr ...     ; Switch Jump Table
```

### 2. 错误码处理 (Error Strings)
代码中间部分包含了大量的错误字符串引用，这证实了该函数负责向用户展示认证失败的原因。

*   **`6F24B731`**: `ERROR_ID_CDKEY_INVALID` (CDKey 无效)
*   **`6F24B797`**: `ERROR_ID_CDKEY_INUSE` (CDKey 已被使用)
*   **`6F24B7E7`**: `ERROR_ID_CDKEY_DISABLED` (CDKey 被封禁)
*   **`6F24B849`**: `ERROR_ID_CDKEY_LANGUAGE` (语言版本不匹配)
*   **`6F24B889`**: `ERROR_ID_CDKEY_MISMATCH` (CDKey 不匹配)
*   **`6F24B727`**: `ERROR_ID_VERSION_BAD` (版本错误)

所有这些错误分支最终都会汇聚到错误弹窗显示逻辑，或者直接导致函数返回失败。

### 3. 核心判定点 (The Critical Check) ⭐
在函数的末尾，逻辑汇聚到一处，检查服务器返回的最终状态码。

```assembly
; [ebp-4] 存储了服务器返回的验证结果
; 通常：1 = 成功, 其他值 = 各种错误
6F24B8AC  | 837D FC 01       | cmp dword ptr ss:[ebp-4],1  ; ★ 比较结果是否为 1 (成功)
6F24B8B0  | 74 32            | je game.6F24B8E4            ; ★ 关键跳转：如果成功，跳过报错逻辑
```

### 4. 失败分支 (Failure Path)
如果 `[ebp-4] != 1`，程序继续向下执行，调用显示错误对话框和断开连接的函数。
```assembly
6F24B8B2  | E8 6950FEFF      | call game.6F230920          ; 显示错误/断开连接
...
6F24B8DF  | E8 2CB4FBFF      | call game.6F206D10          ; 错误处理子程序
```

### 5. 成功分支 (Success Path)
如果跳转成功，程序执行清理工作并返回 `1` (True)。
```assembly
6F24B8E4  | 5F               | pop edi
6F24B8E5  | 5E               | pop esi
6F24B8E6  | B8 01000000      | mov eax,1                   ; 返回 1 (成功)
6F24B8EB  | 5B               | pop ebx
6F24B8EC  | 8BE5             | mov esp,ebp
6F24B8EE  | 5D               | pop ebp
6F24B8EF  | C2 0400          | ret 4
```

---

## 🛠️ 补丁方案 (Patch Solution)

为了绕过 CDKey 验证或模拟认证成功，我们需要修改 **核心判定点** 的跳转逻辑。

### 目标地址
**`0x6F24B8B0`** (对应偏移 `+24B8B0`)

### 原始指令
```assembly
74 32   ; JE 6F24B8E4 (Jump if Equal) - 只有当结果等于 1 时才跳转
```

### 修改后指令 (Bypass)
```assembly
EB 32   ; JMP 6F24B8E4 (Jump Short)   - 强制跳转，忽略比较结果
```

### 特征码提取 (Signature)
为了兼容性，选取 `cmp` 指令及其后的 `call` 序列作为特征，并屏蔽偏移量。

**Code:**
```cpp
/* [Auth Req] Legacy 1.21 Signature */
unsigned char legacy_auth_req_SignData[] = {
    // 83 7D FC 01       | cmp dword ptr ss:[ebp-4], 1
    0xFF,0x83, 0xFF,0x7D, 0xFF,0xFC, 0xFF,0x01,
    
    // 74 XX             | je ... (目标跳转)
    0xFF,0x74, 0x00,0x00,
    
    // E8 XX XX XX XX    | call game.6F230920 (错误处理函数)
    0xFF,0xE8, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    
    // 8B CE             | mov ecx, esi
    0xFF,0x8B, 0xFF,0xCE,
    
    // E8 XX XX XX XX    | call game.6F24B380
    0xFF,0xE8, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00
};

// Patch: JE (74) -> JMP (EB)
unsigned char legacy_auth_req_PatchData[] = { 0xEB };
```

---

## ✅ 总结

*   **机制**: 游戏将服务器返回的验证状态存储在 `[ebp-4]`。
*   **验证**: 在函数末尾对比该值是否为 `1`。
*   **破解**: 将条件跳转 `JE` 修改为强制跳转 `JMP`，使得游戏在收到任何服务器响应（即使是错误码）时，都执行“认证成功”的代码路径。
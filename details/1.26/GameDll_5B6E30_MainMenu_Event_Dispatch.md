# 魔兽争霸3主菜单事件处理分支

*   **目标模块**: `Game.dll` (v1.26.0.6401)
*   **函数地址**: `0x6F5B6E30`
*   **偏移地址**: `game.dll + 5B6E30`

```assembly
6F5B6E30  | 8B4424 04                | mov eax,dword ptr ss:[esp+4]                                                      |
6F5B6E34  | 56                       | push esi                                                                          |
6F5B6E35  | 8BF1                     | mov esi,ecx                                                                       |
6F5B6E37  | 8B48 08                  | mov ecx,dword ptr ds:[eax+8]                                                      |
6F5B6E3A  | 83F9 21                  | cmp ecx,21                                                                        |
6F5B6E3D  | 77 50                    | ja game.6F5B6E8F                                                                  |
6F5B6E3F  | FF248D A4715B6F          | jmp dword ptr ds:[ecx*4+6F5B71A4]                                                 |
6F5B6E46  | 8BCE                     | mov ecx,esi                                                                       |
6F5B6E48  | E8 136AFCFF              | call game.6F57D860                                                                |
6F5B6E4D  | 8B8E 1C020000            | mov ecx,dword ptr ds:[esi+21C]                                                    |
6F5B6E53  | 8B01                     | mov eax,dword ptr ds:[ecx]                                                        |
6F5B6E55  | 8B90 D4000000            | mov edx,dword ptr ds:[eax+D4]                                                     |
6F5B6E5B  | FFD2                     | call edx                                                                          |
6F5B6E5D  | 6A 00                    | push 0                                                                            |
6F5B6E5F  | BA 08A2956F              | mov edx,game.6F95A208                                                             |
6F5B6E64  | B9 00A2956F              | mov ecx,game.6F95A200                                                             |
6F5B6E69  | E8 C22FA5FF              | call game.6F009E30                                                                |
6F5B6E6E  | D95C24 08                | fstp dword ptr ss:[esp+8]                                                         |
6F5B6E72  | D94424 08                | fld dword ptr ss:[esp+8]                                                          |
6F5B6E76  | 8B86 1C020000            | mov eax,dword ptr ds:[esi+21C]                                                    |
6F5B6E7C  | 6A 1A                    | push 1A                                                                           |
6F5B6E7E  | 56                       | push esi                                                                          |
6F5B6E7F  | 51                       | push ecx                                                                          |
6F5B6E80  | D91C24                   | fstp dword ptr ss:[esp]                                                           |
6F5B6E83  | 50                       | push eax                                                                          |
6F5B6E84  | 8D8E 68010000            | lea ecx,dword ptr ds:[esi+168]                                                    |
6F5B6E8A  | E8 41D9D5FF              | call game.6F3147D0                                                                |
6F5B6E8F  | B8 01000000              | mov eax,1                                                                         |
6F5B6E94  | 5E                       | pop esi                                                                           |
6F5B6E95  | C2 0400                  | ret 4                                                                             |
6F5B6E98  | 8B8E 1C020000            | mov ecx,dword ptr ds:[esi+21C]                                                    |
6F5B6E9E  | 8B11                     | mov edx,dword ptr ds:[ecx]                                                        |
6F5B6EA0  | 8B82 D0000000            | mov eax,dword ptr ds:[edx+D0]                                                     |
6F5B6EA6  | FFD0                     | call eax                                                                          |
6F5B6EA8  | 8BCE                     | mov ecx,esi                                                                       |
6F5B6EAA  | E8 11D9FDFF              | call game.6F5947C0                                                                |
6F5B6EAF  | B8 01000000              | mov eax,1                                                                         |
6F5B6EB4  | 5E                       | pop esi                                                                           |
6F5B6EB5  | C2 0400                  | ret 4                                                                             |
6F5B6EB8  | 8D8E B0040000            | lea ecx,dword ptr ds:[esi+4B0]                                                    |
6F5B6EBE  | E8 7DF6FDFF              | call game.6F596540                                                                |
6F5B6EC3  | 6A 00                    | push 0                                                                            |
6F5B6EC5  | C700 01000000            | mov dword ptr ds:[eax],1                                                          |
6F5B6ECB  | 8B8E 20020000            | mov ecx,dword ptr ds:[esi+220]                                                    |
6F5B6ED1  | 6A 00                    | push 0                                                                            |
6F5B6ED3  | E8 E86F0400              | call game.6F5FDEC0                                                                |
6F5B6ED8  | 8B8E 20020000            | mov ecx,dword ptr ds:[esi+220]                                                    |
6F5B6EDE  | 8B11                     | mov edx,dword ptr ds:[ecx]                                                        |
6F5B6EE0  | 8B82 D4000000            | mov eax,dword ptr ds:[edx+D4]                                                     |
6F5B6EE6  | FFD0                     | call eax                                                                          |
6F5B6EE8  | 6A 00                    | push 0                                                                            |
6F5B6EEA  | BA 08A2956F              | mov edx,game.6F95A208                                                             |
6F5B6EEF  | B9 00A2956F              | mov ecx,game.6F95A200                                                             |
6F5B6EF4  | E8 372FA5FF              | call game.6F009E30                                                                |
6F5B6EF9  | D95C24 08                | fstp dword ptr ss:[esp+8]                                                         |
6F5B6EFD  | D94424 08                | fld dword ptr ss:[esp+8]                                                          |
6F5B6F01  | 6A 1A                    | push 1A                                                                           |
6F5B6F03  | 56                       | push esi                                                                          |
6F5B6F04  | 51                       | push ecx                                                                          |
6F5B6F05  | 8B8E 20020000            | mov ecx,dword ptr ds:[esi+220]                                                    |
6F5B6F0B  | D91C24                   | fstp dword ptr ss:[esp]                                                           |
6F5B6F0E  | 51                       | push ecx                                                                          |
6F5B6F0F  | 8D8E AC010000            | lea ecx,dword ptr ds:[esi+1AC]                                                    |
6F5B6F15  | E8 B6D8D5FF              | call game.6F3147D0                                                                |
6F5B6F1A  | B8 01000000              | mov eax,1                                                                         |
6F5B6F1F  | 5E                       | pop esi                                                                           |
6F5B6F20  | C2 0400                  | ret 4                                                                             |
6F5B6F23  | 8B96 B4040000            | mov edx,dword ptr ds:[esi+4B4]                                                    |
6F5B6F29  | 83EA 01                  | sub edx,1                                                                         |
6F5B6F2C  | 52                       | push edx                                                                          |
6F5B6F2D  | 8D8E B0040000            | lea ecx,dword ptr ds:[esi+4B0]                                                    |
6F5B6F33  | E8 58F6FDFF              | call game.6F596590                                                                |
6F5B6F38  | 8B8E 20020000            | mov ecx,dword ptr ds:[esi+220]                                                    |
6F5B6F3E  | 8B01                     | mov eax,dword ptr ds:[ecx]                                                        |
6F5B6F40  | 8B90 D0000000            | mov edx,dword ptr ds:[eax+D0]                                                     |
6F5B6F46  | FFD2                     | call edx                                                                          |
6F5B6F48  | 83BE AC040000 00         | cmp dword ptr ds:[esi+4AC],0                                                      |
6F5B6F4F  | 0F85 3AFFFFFF            | jne game.6F5B6E8F                                                                 |
6F5B6F55  | 8BCE                     | mov ecx,esi                                                                       |
6F5B6F57  | E8 8468FCFF              | call game.6F57D7E0                                                                |
6F5B6F5C  | B8 01000000              | mov eax,1                                                                         |
6F5B6F61  | 5E                       | pop esi                                                                           |
6F5B6F62  | C2 0400                  | ret 4                                                                             |
6F5B6F65  | 6A 00                    | push 0                                                                            |
6F5B6F67  | 8BCE                     | mov ecx,esi                                                                       |
6F5B6F69  | E8 82D7FDFF              | call game.6F5946F0                                                                |
6F5B6F6E  | 83BE AC040000 00         | cmp dword ptr ds:[esi+4AC],0                                                      |
6F5B6F75  | 0F85 14FFFFFF            | jne game.6F5B6E8F                                                                 |
6F5B6F7B  | 6A 01                    | push 1                                                                            |
6F5B6F7D  | 8BCE                     | mov ecx,esi                                                                       |
6F5B6F7F  | E8 0C53FDFF              | call game.6F58C290                                                                |
6F5B6F84  | B8 01000000              | mov eax,1                                                                         |
6F5B6F89  | 5E                       | pop esi                                                                           |
6F5B6F8A  | C2 0400                  | ret 4                                                                             |
6F5B6F8D  | 8BCE                     | mov ecx,esi                                                                       |
6F5B6F8F  | E8 4C68FCFF              | call game.6F57D7E0                                                                |
6F5B6F94  | 83BE 50020000 0F         | cmp dword ptr ds:[esi+250],F                                                      |
6F5B6F9B  | 75 42                    | jne game.6F5B6FDF                                                                 |
6F5B6F9D  | 8B8E A0040000            | mov ecx,dword ptr ds:[esi+4A0]                                                    |
6F5B6FA3  | 57                       | push edi                                                                          |
6F5B6FA4  | 68 1865966F              | push game.6F966518                                                                |
6F5B6FA9  | 68 244F876F              | push game.6F874F24                                                                |
6F5B6FAE  | E8 1DB2FDFF              | call game.6F5921D0                                                                |
6F5B6FB3  | E8 0845FFFF              | call game.6F5AB4C0                                                                |
6F5B6FB8  | 8BF8                     | mov edi,eax                                                                       |
6F5B6FBA  | 8B86 A0040000            | mov eax,dword ptr ds:[esi+4A0]                                                    |
6F5B6FC0  | 50                       | push eax                                                                          |
6F5B6FC1  | 8BCF                     | mov ecx,edi                                                                       |
6F5B6FC3  | E8 98B7FDFF              | call game.6F592760                                                                |
6F5B6FC8  | 68 4C6D966F              | push game.6F966D4C                                                                | 6F966D4C:"BattleNet\\bnserver-WAR3.ini"
6F5B6FCD  | 8BCF                     | mov ecx,edi                                                                       |
6F5B6FCF  | C786 A0040000 00000000   | mov dword ptr ds:[esi+4A0],0                                                      |
6F5B6FD9  | E8 9200FCFF              | call game.6F577070                                                                |
6F5B6FDE  | 5F                       | pop edi                                                                           |
6F5B6FDF  | 8B8E 50020000            | mov ecx,dword ptr ds:[esi+250]                                                    |
6F5B6FE5  | 83F9 01                  | cmp ecx,1                                                                         |
6F5B6FE8  | 74 07                    | je game.6F5B6FF1                                                                  |
6F5B6FEA  | 33D2                     | xor edx,edx                                                                       |
6F5B6FEC  | E8 FFC4FDFF              | call game.6F5934F0                                                                |
6F5B6FF1  | 33C9                     | xor ecx,ecx                                                                       |
6F5B6FF3  | E8 4882F8FF              | call game.6F53F240                                                                |
6F5B6FF8  | B8 01000000              | mov eax,1                                                                         |
6F5B6FFD  | 5E                       | pop esi                                                                           |
6F5B6FFE  | C2 0400                  | ret 4                                                                             |
6F5B7001  | 6A 04                    | push 4                                                                            |
6F5B7003  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7005  | E8 E699FEFF              | call game.6F5A09F0                                                                |
6F5B700A  | B8 01000000              | mov eax,1                                                                         |
6F5B700F  | 5E                       | pop esi                                                                           |
6F5B7010  | C2 0400                  | ret 4                                                                             |
6F5B7013  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7015  | E8 468DFFFF              | call game.6F5AFD60                                                                |
6F5B701A  | B8 01000000              | mov eax,1                                                                         |
6F5B701F  | 5E                       | pop esi                                                                           |
6F5B7020  | C2 0400                  | ret 4                                                                             |
6F5B7023  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7025  | E8 864BFDFF              | call game.6F58BBB0                                                                |
6F5B702A  | B8 01000000              | mov eax,1                                                                         |
6F5B702F  | 5E                       | pop esi                                                                           |
6F5B7030  | C2 0400                  | ret 4                                                                             |
6F5B7033  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7035  | E8 8699FEFF              | call game.6F5A09C0                                                                |
6F5B703A  | B8 01000000              | mov eax,1                                                                         |
6F5B703F  | 5E                       | pop esi                                                                           |
6F5B7040  | C2 0400                  | ret 4                                                                             |
6F5B7043  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7045  | E8 2699FEFF              | call game.6F5A0970                                                                |
6F5B704A  | B8 01000000              | mov eax,1                                                                         |
6F5B704F  | 5E                       | pop esi                                                                           |
6F5B7050  | C2 0400                  | ret 4                                                                             |
6F5B7053  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7055  | E8 266BFCFF              | call game.6F57DB80                                                                |
6F5B705A  | B8 01000000              | mov eax,1                                                                         |
6F5B705F  | 5E                       | pop esi                                                                           |
6F5B7060  | C2 0400                  | ret 4                                                                             |
6F5B7063  | 6A 03                    | push 3                                                                            |
6F5B7065  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7067  | E8 8499FEFF              | call game.6F5A09F0                                                                |
6F5B706C  | B8 01000000              | mov eax,1                                                                         |
6F5B7071  | 5E                       | pop esi                                                                           |
6F5B7072  | C2 0400                  | ret 4                                                                             |
6F5B7075  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7077  | E8 F446FDFF              | call game.6F58B770                                                                |
6F5B707C  | B8 01000000              | mov eax,1                                                                         |
6F5B7081  | 5E                       | pop esi                                                                           |
6F5B7082  | C2 0400                  | ret 4                                                                             |
6F5B7085  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7087  | E8 644CFDFF              | call game.6F58BCF0                                                                |
6F5B708C  | B8 01000000              | mov eax,1                                                                         |
6F5B7091  | 5E                       | pop esi                                                                           |
6F5B7092  | C2 0400                  | ret 4                                                                             |
6F5B7095  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7097  | E8 74D0FEFF              | call game.6F5A4110                                                                |
6F5B709C  | B8 01000000              | mov eax,1                                                                         |
6F5B70A1  | 5E                       | pop esi                                                                           |
6F5B70A2  | C2 0400                  | ret 4                                                                             |
6F5B70A5  | 8BCE                     | mov ecx,esi                                                                       |
6F5B70A7  | E8 646AFCFF              | call game.6F57DB10                                                                |
6F5B70AC  | B8 01000000              | mov eax,1                                                                         |
6F5B70B1  | 5E                       | pop esi                                                                           |
6F5B70B2  | C2 0400                  | ret 4                                                                             |
6F5B70B5  | 8BCE                     | mov ecx,esi                                                                       |
6F5B70B7  | E8 444CFDFF              | call game.6F58BD00                                                                |
6F5B70BC  | B8 01000000              | mov eax,1                                                                         |
6F5B70C1  | 5E                       | pop esi                                                                           |
6F5B70C2  | C2 0400                  | ret 4                                                                             |
6F5B70C5  | 8BCE                     | mov ecx,esi                                                                       |
6F5B70C7  | E8 546AFCFF              | call game.6F57DB20                                                                |
6F5B70CC  | B8 01000000              | mov eax,1                                                                         |
6F5B70D1  | 5E                       | pop esi                                                                           |
6F5B70D2  | C2 0400                  | ret 4                                                                             |
6F5B70D5  | E8 96490700              | call game.6F62BA70                                                                |
6F5B70DA  | B8 01000000              | mov eax,1                                                                         |
6F5B70DF  | 5E                       | pop esi                                                                           |
6F5B70E0  | C2 0400                  | ret 4                                                                             |
6F5B70E3  | 8BCE                     | mov ecx,esi                                                                       |
6F5B70E5  | E8 466AFCFF              | call game.6F57DB30                                                                |
6F5B70EA  | B8 01000000              | mov eax,1                                                                         |
6F5B70EF  | 5E                       | pop esi                                                                           |
6F5B70F0  | C2 0400                  | ret 4                                                                             |
6F5B70F3  | 50                       | push eax                                                                          |
6F5B70F4  | 8D8E 70030000            | lea ecx,dword ptr ds:[esi+370]                                                    |
6F5B70FA  | E8 E161FEFF              | call game.6F59D2E0                                                                |
6F5B70FF  | B8 01000000              | mov eax,1                                                                         |
6F5B7104  | 5E                       | pop esi                                                                           |
6F5B7105  | C2 0400                  | ret 4                                                                             |
6F5B7108  | 50                       | push eax                                                                          |
6F5B7109  | 8D8E 70030000            | lea ecx,dword ptr ds:[esi+370]                                                    |
6F5B710F  | E8 0C97FCFF              | call game.6F580820                                                                |
6F5B7114  | B8 01000000              | mov eax,1                                                                         |
6F5B7119  | 5E                       | pop esi                                                                           |
6F5B711A  | C2 0400                  | ret 4                                                                             |
6F5B711D  | 50                       | push eax                                                                          |
6F5B711E  | 8D8E 70030000            | lea ecx,dword ptr ds:[esi+370]                                                    |
6F5B7124  | E8 F797FCFF              | call game.6F580920                                                                |
6F5B7129  | 6A 01                    | push 1                                                                            |
6F5B712B  | 8BCE                     | mov ecx,esi                                                                       |
6F5B712D  | C705 00B1A86F 02000000   | mov dword ptr ds:[6FA8B100],2                                                     |
6F5B7137  | E8 B498FEFF              | call game.6F5A09F0                                                                |
6F5B713C  | B8 01000000              | mov eax,1                                                                         |
6F5B7141  | 5E                       | pop esi                                                                           |
6F5B7142  | C2 0400                  | ret 4                                                                             |
6F5B7145  | 50                       | push eax                                                                          |
6F5B7146  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7148  | E8 138DFFFF              | call game.6F5AFE60                                                                |
6F5B714D  | 5E                       | pop esi                                                                           |
6F5B714E  | C2 0400                  | ret 4                                                                             |
6F5B7151  | 50                       | push eax                                                                          |
6F5B7152  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7154  | E8 9799FEFF              | call game.6F5A0AF0                                                                |
6F5B7159  | 5E                       | pop esi                                                                           |
6F5B715A  | C2 0400                  | ret 4                                                                             |
6F5B715D  | 50                       | push eax                                                                          |
6F5B715E  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7160  | E8 DB6CFCFF              | call game.6F57DE40                                                                |
6F5B7165  | 5E                       | pop esi                                                                           |
6F5B7166  | C2 0400                  | ret 4                                                                             |
6F5B7169  | 50                       | push eax                                                                          |
6F5B716A  | 8BCE                     | mov ecx,esi                                                                       |
6F5B716C  | E8 6F91FFFF              | call game.6F5B02E0                                                                |
6F5B7171  | 5E                       | pop esi                                                                           |
6F5B7172  | C2 0400                  | ret 4                                                                             |
6F5B7175  | 50                       | push eax                                                                          |
6F5B7176  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7178  | E8 B357FDFF              | call game.6F58C930                                                                |
6F5B717D  | 5E                       | pop esi                                                                           |
6F5B717E  | C2 0400                  | ret 4                                                                             |
6F5B7181  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7183  | E8 8855FDFF              | call game.6F58C710                                                                |
6F5B7188  | 5E                       | pop esi                                                                           |
6F5B7189  | C2 0400                  | ret 4                                                                             |
6F5B718C  | 8BCE                     | mov ecx,esi                                                                       |
6F5B718E  | E8 DD90FFFF              | call game.6F5B0270                                                                |
6F5B7193  | 5E                       | pop esi                                                                           |
6F5B7194  | C2 0400                  | ret 4                                                                             |
6F5B7197  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7199  | E8 0245FDFF              | call game.6F58B6A0                                                                |
6F5B719E  | 5E                       | pop esi                                                                           |
6F5B719F  | C2 0400                  | ret 4                                                                             |
6F5B71A2  | 8BFF                     | mov edi,edi                                                                       |
6F5B71A4  | 0170 5B                  | add dword ptr ds:[eax+5B],esi                                                     |
6F5B71A7  | 6F                       | outsd                                                                             |
6F5B71A8  | 1370 5B                  | adc esi,dword ptr ds:[eax+5B]                                                     |
6F5B71AB  | 6F                       | outsd                                                                             |
6F5B71AC  | 43                       | inc ebx                                                                           |
6F5B71AD  | 70 5B                    | jo game.6F5B720A                                                                  |
6F5B71AF  | 6F                       | outsd                                                                             |
6F5B71B0  | 53                       | push ebx                                                                          |
6F5B71B1  | 70 5B                    | jo game.6F5B720E                                                                  |
6F5B71B3  | 6F                       | outsd                                                                             |
6F5B71B4  | 6370 5B                  | arpl word ptr ds:[eax+5B],si                                                      |
6F5B71B7  | 6F                       | outsd                                                                             |
6F5B71B8  | 97                       | xchg edi,eax                                                                      |
6F5B71B9  | 71 5B                    | jno game.6F5B7216                                                                 |
6F5B71BB  | 6F                       | outsd                                                                             |
6F5B71BC  | 75 70                    | jne game.6F5B722E                                                                 |
6F5B71BE  | 5B                       | pop ebx                                                                           |
6F5B71BF  | 6F                       | outsd                                                                             |
6F5B71C0  | 8570 5B                  | test dword ptr ds:[eax+5B],esi                                                    |
6F5B71C3  | 6F                       | outsd                                                                             |
6F5B71C4  | 95                       | xchg ebp,eax                                                                      |
6F5B71C5  | 70 5B                    | jo game.6F5B7222                                                                  |
6F5B71C7  | 6F                       | outsd                                                                             |
6F5B71C8  | A5                       | movsd                                                                             |
6F5B71C9  | 70 5B                    | jo game.6F5B7226                                                                  |
6F5B71CB  | 6F                       | outsd                                                                             |
6F5B71CC  | B5 70                    | mov ch,70                                                                         |
6F5B71CE  | 5B                       | pop ebx                                                                           |
6F5B71CF  | 6F                       | outsd                                                                             |
6F5B71D0  | C570 5B                  | lds esi,fword ptr ds:[eax+5B]                                                     |
6F5B71D3  | 6F                       | outsd                                                                             |
6F5B71D4  | D5 70                    | aad 70                                                                            |
6F5B71D6  | 5B                       | pop ebx                                                                           |
6F5B71D7  | 6F                       | outsd                                                                             |
6F5B71D8  | E3 70                    | jecxz game.6F5B724A                                                               |
6F5B71DA  | 5B                       | pop ebx                                                                           |
6F5B71DB  | 6F                       | outsd                                                                             |
6F5B71DC  | B8 6E5B6F23              | mov eax,236F5B6E                                                                  |
6F5B71E1  | 6F                       | outsd                                                                             |
6F5B71E2  | 5B                       | pop ebx                                                                           |
6F5B71E3  | 6F                       | outsd                                                                             |
6F5B71E4  | 65:6F                    | outsd                                                                             |
6F5B71E6  | 5B                       | pop ebx                                                                           |
6F5B71E7  | 6F                       | outsd                                                                             |
6F5B71E8  | 2370 5B                  | and esi,dword ptr ds:[eax+5B]                                                     |
6F5B71EB  | 6F                       | outsd                                                                             |
6F5B71EC  | 43                       | inc ebx                                                                           |
6F5B71ED  | 70 5B                    | jo game.6F5B724A                                                                  |
6F5B71EF  | 6F                       | outsd                                                                             |
6F5B71F0  | 3370 5B                  | xor esi,dword ptr ds:[eax+5B]                                                     |
6F5B71F3  | 6F                       | outsd                                                                             |
6F5B71F4  | F3:70 5B                 | jo game.6F5B7252                                                                  |
6F5B71F7  | 6F                       | outsd                                                                             |
6F5B71F8  | 0871 5B                  | or byte ptr ds:[ecx+5B],dh                                                        |
6F5B71FB  | 6F                       | outsd                                                                             |
6F5B71FC  | 1D 715B6F98              | sbb eax,986F5B71                                                                  |
6F5B7201  | 6E                       | outsb                                                                             |
6F5B7202  | 5B                       | pop ebx                                                                           |
6F5B7203  | 6F                       | outsd                                                                             |
6F5B7204  | 46                       | inc esi                                                                           |
6F5B7205  | 6E                       | outsb                                                                             |
6F5B7206  | 5B                       | pop ebx                                                                           |
6F5B7207  | 6F                       | outsd                                                                             |
6F5B7208  | 8D6F 5B                  | lea ebp,dword ptr ds:[edi+5B]                                                     |
6F5B720B  | 6F                       | outsd                                                                             |
6F5B720C  | 55                       | push ebp                                                                          |
6F5B720D  | 6F                       | outsd                                                                             |
6F5B720E  | 5B                       | pop ebx                                                                           |
6F5B720F  | 6F                       | outsd                                                                             |
6F5B7210  | 45                       | inc ebp                                                                           |
6F5B7211  | 71 5B                    | jno game.6F5B726E                                                                 |
6F5B7213  | 6F                       | outsd                                                                             |
6F5B7214  | 51                       | push ecx                                                                          |
6F5B7215  | 71 5B                    | jno game.6F5B7272                                                                 |
6F5B7217  | 6F                       | outsd                                                                             |
6F5B7218  | 5D                       | pop ebp                                                                           |
6F5B7219  | 71 5B                    | jno game.6F5B7276                                                                 |
6F5B721B  | 6F                       | outsd                                                                             |
6F5B721C  | 75 71                    | jne game.6F5B728F                                                                 |
6F5B721E  | 5B                       | pop ebx                                                                           |
6F5B721F  | 6F                       | outsd                                                                             |
6F5B7220  | 6971 5B 6F8C715B         | imul esi,dword ptr ds:[ecx+5B],nvgpucomp32.5B718C6F                               |
6F5B7227  | 6F                       | outsd                                                                             |
6F5B7228  | 8171 5B 6FCCCCCC         | xor dword ptr ds:[ecx+5B],CCCCCC6F                                                |
6F5B722F  | CC                       | int3                                                                              |
6F5B7230  | 8B4424 04                | mov eax,dword ptr ss:[esp+4]                                                      |
6F5B7234  | 8178 10 00020000         | cmp dword ptr ds:[eax+10],200                                                     |
6F5B723B  | 56                       | push esi                                                                          |
6F5B723C  | 8BF1                     | mov esi,ecx                                                                       |
6F5B723E  | 75 7D                    | jne game.6F5B72BD                                                                 |
6F5B7240  | 83BE D8050000 00         | cmp dword ptr ds:[esi+5D8],0                                                      |
6F5B7247  | 74 15                    | je game.6F5B725E                                                                  |
6F5B7249  | E8 9297D7FF              | call game.6F3309E0                                                                |
6F5B724E  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7250  | E8 BB54FDFF              | call game.6F58C710                                                                |
6F5B7255  | B8 01000000              | mov eax,1                                                                         |
6F5B725A  | 5E                       | pop esi                                                                           |
6F5B725B  | C2 0400                  | ret 4                                                                             |
6F5B725E  | 8B86 B4040000            | mov eax,dword ptr ds:[esi+4B4]                                                    |
6F5B7264  | 85C0                     | test eax,eax                                                                      |
6F5B7266  | 74 0C                    | je game.6F5B7274                                                                  |
6F5B7268  | 8B8E B8040000            | mov ecx,dword ptr ds:[esi+4B8]                                                    |
6F5B726E  | 8D4481 FC                | lea eax,dword ptr ds:[ecx+eax*4-4]                                                |
6F5B7272  | EB 02                    | jmp game.6F5B7276                                                                 |
6F5B7274  | 33C0                     | xor eax,eax                                                                       |
6F5B7276  | 8B00                     | mov eax,dword ptr ds:[eax]                                                        |
6F5B7278  | 83E8 00                  | sub eax,0                                                                         |
6F5B727B  | 74 2D                    | je game.6F5B72AA                                                                  |
6F5B727D  | 83E8 01                  | sub eax,1                                                                         |
6F5B7280  | 8BCE                     | mov ecx,esi                                                                       |
6F5B7282  | 74 13                    | je game.6F5B7297                                                                  |
6F5B7284  | E8 E78FFFFF              | call game.6F5B0270                                                                |
6F5B7289  | E8 5297D7FF              | call game.6F3309E0                                                                |
6F5B728E  | B8 01000000              | mov eax,1                                                                         |
6F5B7293  | 5E                       | pop esi                                                                           |
6F5B7294  | C2 0400                  | ret 4                                                                             |
6F5B7297  | E8 2497FEFF              | call game.6F5A09C0                                                                |
6F5B729C  | E8 3F97D7FF              | call game.6F3309E0                                                                |
6F5B72A1  | B8 01000000              | mov eax,1                                                                         |
6F5B72A6  | 5E                       | pop esi                                                                           |
6F5B72A7  | C2 0400                  | ret 4                                                                             |
6F5B72AA  | E8 C1470700              | call game.6F62BA70                                                                |
6F5B72AF  | E8 2C97D7FF              | call game.6F3309E0                                                                |
6F5B72B4  | B8 01000000              | mov eax,1                                                                         |
6F5B72B9  | 5E                       | pop esi                                                                           |
6F5B72BA  | C2 0400                  | ret 4                                                                             |
6F5B72BD  | 33C0                     | xor eax,eax                                                                       |
6F5B72BF  | 5E                       | pop esi                                                                           |
6F5B72C0  | C2 0400                  | ret 4                                                                             |
```
## 子函数分析（ecx=0x01）
```assembly
6F5AFD60  | 81EC 08010000            | sub esp,108                                                                       |
6F5AFD66  | A1 40E1AA6F              | mov eax,dword ptr ds:[6FAAE140]                                                   |
6F5AFD6B  | 33C4                     | xor eax,esp                                                                       |
6F5AFD6D  | 898424 04010000          | mov dword ptr ss:[esp+104],eax                                                    |
6F5AFD74  | 56                       | push esi                                                                          |
6F5AFD75  | 8BF1                     | mov esi,ecx                                                                       |
6F5AFD77  | 8B8E 48020000            | mov ecx,dword ptr ds:[esi+248]                                                    |
6F5AFD7D  | 8B01                     | mov eax,dword ptr ds:[ecx]                                                        |
6F5AFD7F  | 8B90 48010000            | mov edx,dword ptr ds:[eax+148]                                                    |
6F5AFD85  | 57                       | push edi                                                                          |
6F5AFD86  | FFD2                     | call edx                                                                          |
6F5AFD88  | 8BF8                     | mov edi,eax                                                                       |
6F5AFD8A  | E8 31B7FFFF              | call game.6F5AB4C0                                                                |
6F5AFD8F  | 8BCE                     | mov ecx,esi                                                                       |
6F5AFD91  | E8 1AAEFEFF              | call game.6F59ABB0                                                                |
6F5AFD96  | 83FF FF                  | cmp edi,FFFFFFFF                                                                  |
6F5AFD99  | 75 46                    | jne game.6F5AFDE1                                                                 |
6F5AFD9B  | E8 A04E1100              | call game.6F6C4C40                                                                |
6F5AFDA0  | 8B8E A0040000            | mov ecx,dword ptr ds:[esi+4A0]                                                    |
6F5AFDA6  | 894424 08                | mov dword ptr ss:[esp+8],eax                                                      |
6F5AFDAA  | E8 C16AFDFF              | call game.6F586870                                                                |
6F5AFDAF  | 8B8E A0040000            | mov ecx,dword ptr ds:[esi+4A0]                                                    |
6F5AFDB5  | 50                       | push eax                                                                          |
6F5AFDB6  | E8 456AFDFF              | call game.6F586800                                                                |
6F5AFDBB  | 83C0 04                  | add eax,4                                                                         |
6F5AFDBE  | 50                       | push eax                                                                          |
6F5AFDBF  | 8D4424 0C                | lea eax,dword ptr ss:[esp+C]                                                      |
6F5AFDC3  | 50                       | push eax                                                                          |
6F5AFDC4  | 68 5088966F              | push game.6F968850                                                                | 6F968850:"%s.WAR3.battle.net;%s"
6F5AFDC9  | 8D4C24 18                | lea ecx,dword ptr ss:[esp+18]                                                     |
6F5AFDCD  | 68 00010000              | push 100                                                                          |
6F5AFDD2  | 51                       | push ecx                                                                          |
6F5AFDD3  | E8 CEB71300              | call <JMP.&Ordinal#578>                                                           |
6F5AFDD8  | 83C4 14                  | add esp,14                                                                        |
6F5AFDDB  | 8D7C24 0C                | lea edi,dword ptr ss:[esp+C]                                                      |
6F5AFDDF  | EB 1D                    | jmp game.6F5AFDFE                                                                 |
6F5AFDE1  | 8B8E A0040000            | mov ecx,dword ptr ds:[esi+4A0]                                                    |
6F5AFDE7  | 57                       | push edi                                                                          |
6F5AFDE8  | E8 7371FCFF              | call game.6F576F60                                                                |
6F5AFDED  | 8B8E A0040000            | mov ecx,dword ptr ds:[esi+4A0]                                                    |
6F5AFDF3  | 57                       | push edi                                                                          |
6F5AFDF4  | E8 076AFDFF              | call game.6F586800                                                                |
6F5AFDF9  | 8BF8                     | mov edi,eax                                                                       |
6F5AFDFB  | 83C7 04                  | add edi,4                                                                         |
6F5AFDFE  | 68 3C88966F              | push game.6F96883C                                                                | 6F96883C:"BNET_CONNECT_INIT"
6F5AFE03  | 8BCE                     | mov ecx,esi                                                                       |
6F5AFE05  | E8 D6DFFCFF              | call game.6F57DDE0                                                                |
6F5AFE0A  | 6A 00                    | push 0                                                                            |
6F5AFE0C  | 56                       | push esi                                                                          |
6F5AFE0D  | BA 1B000000              | mov edx,1B                                                                        |
6F5AFE12  | B9 C8000940              | mov ecx,400900C8                                                                  |
6F5AFE17  | E8 34F5F8FF              | call game.6F53F350                                                                |
6F5AFE1C  | 8BD7                     | mov edx,edi                                                                       |
6F5AFE1E  | B9 54454E42              | mov ecx,424E4554                                                                  |
6F5AFE23  | E8 88F3F9FF              | call game.6F54F1B0                                                                |
6F5AFE28  | 8BCE                     | mov ecx,esi                                                                       |
6F5AFE2A  | E8 F1D9FCFF              | call game.6F57D820                                                                |
6F5AFE2F  | 830D 18A4A96F 01         | or dword ptr ds:[6FA9A418],1                                                      |
6F5AFE36  | BA 00020000              | mov edx,200                                                                       |
6F5AFE3B  | 8BCE                     | mov ecx,esi                                                                       |
6F5AFE3D  | E8 8E110500              | call game.6F600FD0                                                                |
6F5AFE42  | 8B8C24 0C010000          | mov ecx,dword ptr ss:[esp+10C]                                                    |
6F5AFE49  | 5F                       | pop edi                                                                           |
6F5AFE4A  | 5E                       | pop esi                                                                           |
6F5AFE4B  | 33CC                     | xor ecx,esp                                                                       |
6F5AFE4D  | B8 01000000              | mov eax,1                                                                         |
6F5AFE52  | E8 02122300              | call game.6F7E1059                                                                |
6F5AFE57  | 81C4 08010000            | add esp,108                                                                       |
6F5AFE5D  | C3                       | ret                                                                               |
```
这段汇编代码是 **Warcraft III (War3) 处理“进入 Battle.net” (战网) 按钮点击**的核心逻辑。

当用户在主菜单点击“Battle.net”按钮时，程序会进入这个分支。它的主要任务是：**确定当前选中的网关（Gateway/Server），并启动连接流程。**

以下是详细的分步分析：

### 1. 获取当前选中的网关索引
```assembly
6F5AFD77  | 8B8E 48020000            | mov ecx,dword ptr ds:[esi+248]   ; 获取 UI 列表/控件指针
6F5AFD7D  | 8B01                     | mov eax,dword ptr ds:[ecx]       ; 虚表
6F5AFD7F  | 8B90 48010000            | mov edx,dword ptr ds:[eax+148]   ; 获取“获取当前选中项”的虚函数
6F5AFD86  | FFD2                     | call edx                         ; 调用
6F5AFD88  | 8BF8                     | mov edi,eax                      ; edi = 选中的索引 (0, 1, 2...)
```
这段代码访问了 UI 对象，调用虚函数来获取在网关选择列表里点选的是哪一个（例如：美国、欧洲、亚洲、或者是私服）。返回的索引存放在 `edi` 中。

---

### 2. 逻辑分支：处理“未选中”或“默认”网关
```assembly
6F5AFD96  | 83FF FF                  | cmp edi,FFFFFFFF                 ; 检查索引是否为 -1 (没选)
6F5AFD99  | 75 46                    | jne game.6F5AFDE1                ; 如果选了，跳转到 6F5AFDE1
```

#### 情况 A：如果没有有效索引 (edi == -1)
程序会构造一个默认的连接字符串：
- **关键字符串：** `%s.WAR3.battle.net;%s` (位于 `6F968850`)。
- 它会调用 `6F6C4C40` 获取一些系统环境信息，然后使用 `sprintf` 类的函数（`Ordinal#578`）拼接出默认的战网地址。

#### 情况 B：如果选中了某个网关 (edi != -1)
```assembly
6F5AFDE1  | 8B8E A0040000            | mov ecx,dword ptr ds:[esi+4A0]   ; 网关管理器指针
6F5AFDE7  | 57                       | push edi                         ; 压入选中的索引
6F5AFDE8  | E8 7371FCFF              | call game.6F576F60               ; 设置当前活动网关
6F5AFDF4  | E8 076AFDFF              | call game.6F586800               ; 获取该网关对应的服务器地址字符串
6F5AFDF9  | 8BF8                     | mov edi,eax                      ; edi = 服务器地址字符串指针
```
这里是正常逻辑：根据索引从配置中提取具体的服务器 IP 或域名。

---

### 3. 启动连接状态机
```assembly
6F5AFDFE  | 68 3C88966F              | push game.6F96883C               ; "BNET_CONNECT_INIT"
6F5AFE03  | 8BCE                     | mov ecx,esi                      ; 界面控制器
6F5AFE05  | E8 D6DFFCFF              | call game.6F57DDE0               ; 切换 UI 状态到“正在连接”
```
这里向 UI 系统发送了 `BNET_CONNECT_INIT` 指令，这会导致屏幕上出现“正在连接到 Battle.net...”的对话框。

---

### 4. 发起网络请求 (关键点)
```assembly
6F5AFE1E  | B9 54454E42              | mov ecx,424E4554                 ; 'BNET' (ASCII 转换后是 BNET)
6F5AFE23  | E8 88F3F9FF              | call game.6F54F1B0               ; 核心网络连接函数
```
这是最重要的部分。`424E4554` 是十六进制，转换成字符就是 **"BNET"**。
- `6F54F1B0` 是 War3 处理网络协议切换的函数。
- 通过传递 "BNET" 标志和之前获取的服务器地址（`edx=edi`），War3 开始建立 TCP 连接并准备进行战网握手。

---

### 5. 后续处理
```assembly
6F5AFE2F  | 830D 18A4A96F 01         | or dword ptr ds:[6FA9A418],1     ; 设置全局标志位，表示正在尝试战网连接
6F5AFE3D  | E8 8E110500              | call game.6F600FD0               ; 可能是在播放点击声或刷新界面
```
最后设置一个全局状态位，并清理堆栈返回。

### 总结
**`ecx=0x1` 对应的这个函数 `6F5AFD60` 是“战网登录启动器”。**

它的逻辑链路是：
1. **询问 UI**：“玩家选了哪个服务器？”
2. **查找地址**：“这个服务器的 IP/域名是什么？”
3. **通知 UI**：“显示‘正在连接’窗口。”
4. **通知网络层**：“使用 BNET 协议连接到这个 IP。”

### 分析 6F57D7E0 这个函数的作用
```assembly
6F5B6F55  | 8BCE                     | mov ecx,esi                                                                       | ecx==0x1A
6F5B6F57  | E8 8468FCFF              | call game.6F57D7E0                                                                |
6F5B6F5C  | B8 01000000              | mov eax,1                                                                         |
6F5B6F61  | 5E                       | pop esi                                                                           |
6F5B6F62  | C2 0400                  | ret 4                                                                             |
```
```assembly
6F57D7E0  | 56                       | push esi                                                                          |
6F57D7E1  | 8BF1                     | mov esi,ecx                                                                       |
6F57D7E3  | 8B86 C4040000            | mov eax,dword ptr ds:[esi+4C4]                                                    |
6F57D7E9  | 85C0                     | test eax,eax                                                                      |
6F57D7EB  | 7E 2A                    | jle game.6F57D817                                                                 |
6F57D7ED  | 83C0 FF                  | add eax,FFFFFFFF                                                                  |
6F57D7F0  | 6A 01                    | push 1                                                                            |
6F57D7F2  | 8D8E 10020000            | lea ecx,dword ptr ds:[esi+210]                                                    |
6F57D7F8  | 8986 C4040000            | mov dword ptr ds:[esi+4C4],eax                                                    |
6F57D7FE  | E8 4D9E0900              | call game.6F617650                                                                |
6F57D803  | 830D 18A4A96F 01         | or dword ptr ds:[6FA9A418],1                                                      |
6F57D80A  | 8BCE                     | mov ecx,esi                                                                       |
6F57D80C  | BA 00020000              | mov edx,200                                                                       |
6F57D811  | 5E                       | pop esi                                                                           |
6F57D812  | E9 B9370800              | jmp game.6F600FD0                                                                 |
6F57D817  | 5E                       | pop esi                                                                           |
6F57D818  | C3                       | ret                                                                               |
```

如果在分析主菜单逻辑，这个函数就是点击“Battle.net”后的第一步动作。后续的包处理（如验证账号密码）会在 `6F54F1B0` 触发的网络回调中异步进行。

经过对提供的代码片段进行分析，这三个步骤（0x11 -> 0x18 -> 0x1A）构成了 War3 UI 框架中**从一个菜单状态切换/回退到主菜单的“渲染与状态机稳定”过程**。

以下是详细分析以及的 Hook 方案建议：

### 1. 流程深度分析

#### **第一步：ecx=0x11 (`6F58BBB0`) —— UI 文本与网关刷新**
*   **功能**：这是“准备阶段”。
*   **逻辑**：它从 `[esi+248]` 获取当前的网关索引（edi），然后根据索引去查找字符串（如 `BNET_REALM_SELECT_TIP_GATEWAY`）。
*   **作用**：当退回主菜单时，它需要重新计算并显示“当前选中的战网服务器”文字。它确保了主菜单上的“Battle.net”按钮下方的提示文字是正确的。

#### **第二步：ecx=0x18 (`6F5B6E48`) —— 界面淡入动画**
*   **功能**：这是“表现阶段”。
*   **逻辑**：代码中出现了 `ControlFadeDuration` 和 `Glue`（胶水层，War3 UI 的代号）。它调用了 `6F3147D0`，这是一个典型的 UI 动画控制函数。
*   **作用**：执行主菜单按钮的“淡入”或者“滑入”效果。此时界面虽然在显示，但可能还在动画过程中，不一定能立刻响应点击。

#### **第三步：ecx=0x1A (`6F5B6F57`) —— 状态机归位/激活**
*   **功能**：这是“最后确认阶段”。
*   **核心调用**：`6F57D7E0`。
    *   它修改了 `[esi+4C4]`（通常是 UI 堆栈深度或状态计数）。
    *   执行了 `or dword ptr ds:[6FA9A418], 1`。这是一个全局标志位，告诉游戏“界面已更新，现在可以接受输入了”。
    *   最后 `jmp game.6F600FD0` 是刷新 UI 渲染器的调用。
*   **作用**：**这是最关心的位置。** 当 0x1A 执行完毕，意味着主菜单已经完全加载、动画播放完成、逻辑状态已经处于“空闲（Idle）”并等待玩家操作。

---

### 2. Hook 方案建议：实现自动连接战网

想在 0x1A 执行完后直接跳转到 0x1（即执行 `6F5AFD60`）。这在逻辑上是完全正确的，因为此时 UI 最稳定。

#### **Hook 位置建议**
不要直接在跳转表里改，最稳妥的方法是 Hook **0x1A 分支的结束处**。

**目标位置：**
```assembly
6F5B6F5C  | B8 01000000              | mov eax,1
6F5B6F61  | 5E                       | pop esi
6F5B6F62  | C2 0400                  | ret 4
```

#### **注入逻辑（伪代码/汇编思路）**

可以把 `6F5B6F5C` 处的 `mov eax, 1` 改为一个 `jmp` 到的补丁代码：

```assembly
// 的补丁代码 (Codecave)
push ecx             ; 保护寄存器
mov ecx, esi         ; 此时 esi 仍然指向 UI 对象的指针
call 6F5AFD60        ; 调用“点击战网按钮”的函数 (即分析的第一个函数)
pop ecx              ; 恢复寄存器
mov eax, 1           ; 还原被覆盖的原始代码
pop esi              ; 还原被覆盖的原始代码
ret 4                ; 返回
```

#### **关键注意事项：**

1.  **防止无限循环**：
    如果在 0x1A 后面直接调用 0x1，而 0x1 执行失败又退回主菜单，会再次触发 0x1A，导致程序死循环。
    *   **解决**：需要设置一个全局变量（Flag），例如 `isAutoLogging`。
    *   在 Hook 代码里判断：`if (isAutoLogging == 0) { isAutoLogging = 1; call 6F5AFD60; }`。
    *   当玩家手动点击“取消”或者连接彻底失败时，再把 `isAutoLogging` 置回 0。

2.  **ESI 指针的有效性**：
    在 `6F5B6F5C` 处，`esi` 指针依然是有效的。但在 `6F5B6F61` 执行 `pop esi` 之后，`esi` 就失效了。所以 Hook 必须发生在 `pop esi` 之前。

3.  **自动登录逻辑**：
    执行完 `6F5AFD60` 只是进入了“连接中”状态。
    *   如果需要自动输入密码，需要关注 `BNET_CONNECT_INIT` 之后的下一个状态机。
    *   通常是在收到战网服务器的 `SID_AUTH_CHECK` 成功包后，UI 会切换到登录界面（Account Login），那时需要再 hook 一次，往那个界面的输入框里填入字符串并模拟点击“Logon”。

### 总结
的分析非常准确。**0x1A 是整个主菜单加载序列的终点。** 只要在此处“推一把”，让程序以为玩家按下了战网按钮，就能完美衔接自动连接流程。建议直接在 `6F5B6F5C` 处写跳转。
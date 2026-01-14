6F551D80  | 6A FF                   | push FFFFFFFF                                     |
6F551D82  | 68 A774836F             | push game.6F8374A7                                |
6F551D87  | 64:A1 00000000          | mov eax,dword ptr fs:[0]                          |
6F551D8D  | 50                      | push eax                                          |
6F551D8E  | 81EC E8020000           | sub esp,2E8                                       |
6F551D94  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]                   |
6F551D99  | 33C4                    | xor eax,esp                                       |
6F551D9B  | 898424 E4020000         | mov dword ptr ss:[esp+2E4],eax                    |
6F551DA2  | 53                      | push ebx                                          |
6F551DA3  | 55                      | push ebp                                          |
6F551DA4  | 56                      | push esi                                          |
6F551DA5  | 57                      | push edi                                          |
6F551DA6  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]                   |
6F551DAB  | 33C4                    | xor eax,esp                                       |
6F551DAD  | 50                      | push eax                                          |
6F551DAE  | 8D8424 FC020000         | lea eax,dword ptr ss:[esp+2FC]                    |
6F551DB5  | 64:A3 00000000          | mov dword ptr fs:[0],eax                          |
6F551DBB  | 8B8424 0C030000         | mov eax,dword ptr ss:[esp+30C]                    |
6F551DC2  | 8B50 08                 | mov edx,dword ptr ds:[eax+8]                      |
6F551DC5  | 8BF1                    | mov esi,ecx                                       |
6F551DC7  | 8B48 0C                 | mov ecx,dword ptr ds:[eax+C]                      |
6F551DCA  | 33DB                    | xor ebx,ebx                                       |
6F551DCC  | C74424 14 082C936F      | mov dword ptr ss:[esp+14],game.6F932C08           |
6F551DD4  | 895424 18               | mov dword ptr ss:[esp+18],edx                     |
6F551DD8  | 895C24 1C               | mov dword ptr ss:[esp+1C],ebx                     |
6F551DDC  | C74424 20 FFFFFFFF      | mov dword ptr ss:[esp+20],FFFFFFFF                |
6F551DE4  | 894C24 24               | mov dword ptr ss:[esp+24],ecx                     |
6F551DE8  | 895C24 28               | mov dword ptr ss:[esp+28],ebx                     |
6F551DEC  | 8A48 14                 | mov cl,byte ptr ds:[eax+14]                       |
6F551DEF  | 80F9 40                 | cmp cl,40                                         |
6F551DF2  | 899C24 04030000         | mov dword ptr ss:[esp+304],ebx                    |
6F551DF9  | 73 09                   | jae game.6F551E04                                 |
6F551DFB  | 0FB6C9                  | movzx ecx,cl                                      |
6F551DFE  | 894C24 30               | mov dword ptr ss:[esp+30],ecx                     |
6F551E02  | EB 08                   | jmp game.6F551E0C                                 |
6F551E04  | C74424 30 32000000      | mov dword ptr ss:[esp+30],32                      |
6F551E0C  | 3858 15                 | cmp byte ptr ds:[eax+15],bl                       |
6F551E0F  | 74 08                   | je game.6F551E19                                  |
6F551E11  | 8BBE 10060000           | mov edi,dword ptr ds:[esi+610]                    |
6F551E17  | EB 02                   | jmp game.6F551E1B                                 |
6F551E19  | 33FF                    | xor edi,edi                                       |
6F551E1B  | 3BBE 10060000           | cmp edi,dword ptr ds:[esi+610]                    |
6F551E21  | 0F85 5B020000           | jne game.6F552082                                 |
6F551E27  | 8B4424 30               | mov eax,dword ptr ss:[esp+30]                     |
6F551E2B  | 83F8 10                 | cmp eax,10                                        |
6F551E2E  | 0F84 A5010000           | je game.6F551FD9                                  |
6F551E34  | 83F8 15                 | cmp eax,15                                        |
6F551E37  | 0F84 1E010000           | je game.6F551F5B                                  |
6F551E3D  | 83F8 1B                 | cmp eax,1B                                        |
6F551E40  | 0F85 3C020000           | jne game.6F552082                                 |
6F551E46  | 8BD7                    | mov edx,edi                                       |
6F551E48  | 69D2 04030000           | imul edx,edx,304                                  |
6F551E4E  | 83BC32 78020000 01      | cmp dword ptr ds:[edx+esi+278],1                  |
6F551E56  | 8D2C32                  | lea ebp,dword ptr ds:[edx+esi]                    |
6F551E59  | 0F85 23020000           | jne game.6F552082                                 |
6F551E5F  | 3BFB                    | cmp edi,ebx                                       |
6F551E61  | 8BCE                    | mov ecx,esi                                       |
6F551E63  | 75 18                   | jne game.6F551E7D                                 |
6F551E65  | 6A 01                   | push 1                                            |
6F551E67  | E8 445FFEFF             | call game.6F537DB0                                |
6F551E6C  | 399E E00B0000           | cmp dword ptr ds:[esi+BE0],ebx                    |
6F551E72  | 0F85 0A020000           | jne game.6F552082                                 |
6F551E78  | E9 82000000             | jmp game.6F551EFF                                 |
6F551E7D  | 399D 88020000           | cmp dword ptr ss:[ebp+288],ebx                    |
6F551E83  | 74 3F                   | je game.6F551EC4                                  |
6F551E85  | 6A 02                   | push 2                                            |
6F551E87  | E8 945EFEFF             | call game.6F537D20                                |
6F551E8C  | 899E C80A0000           | mov dword ptr ds:[esi+AC8],ebx                    |
6F551E92  | 83BD D8020000 01        | cmp dword ptr ss:[ebp+2D8],1                      |
6F551E99  | 75 1B                   | jne game.6F551EB6                                 |
6F551E9B  | 8B86 28060000           | mov eax,dword ptr ds:[esi+628]                    |
6F551EA1  | 8BCE                    | mov ecx,esi                                       |
6F551EA3  | 8986 CC0A0000           | mov dword ptr ds:[esi+ACC],eax                    |
6F551EA9  | E8 52CCFFFF             | call game.6F54EB00                                |
6F551EAE  | 899E D40A0000           | mov dword ptr ds:[esi+AD4],ebx                    |
6F551EB4  | EB 32                   | jmp game.6F551EE8                                 |
6F551EB6  | 899E CC0A0000           | mov dword ptr ds:[esi+ACC],ebx                    |
6F551EBC  | 899E D40A0000           | mov dword ptr ds:[esi+AD4],ebx                    |
6F551EC2  | EB 24                   | jmp game.6F551EE8                                 |
6F551EC4  | 6A 03                   | push 3                                            |
6F551EC6  | E8 555EFEFF             | call game.6F537D20                                |
6F551ECB  | 8B86 18060000           | mov eax,dword ptr ds:[esi+618]                    |
6F551ED1  | 8B50 1C                 | mov edx,dword ptr ds:[eax+1C]                     |
6F551ED4  | 8D8E 18060000           | lea ecx,dword ptr ds:[esi+618]                    |
6F551EDA  | FFD2                    | call edx                                          |
6F551EDC  | 899E C80A0000           | mov dword ptr ds:[esi+AC8],ebx                    |
6F551EE2  | 899E CC0A0000           | mov dword ptr ds:[esi+ACC],ebx                    |
6F551EE8  | 399E 70220000           | cmp dword ptr ds:[esi+2270],ebx                   |
6F551EEE  | 899E D00A0000           | mov dword ptr ds:[esi+AD0],ebx                    |
6F551EF4  | 74 1A                   | je game.6F551F10                                  |
6F551EF6  | 6A 01                   | push 1                                            |
6F551EF8  | 8BCE                    | mov ecx,esi                                       |
6F551EFA  | E8 B15EFEFF             | call game.6F537DB0                                |
6F551EFF  | 8B86 E80B0000           | mov eax,dword ptr ds:[esi+BE8]                    |
6F551F05  | 8B50 1C                 | mov edx,dword ptr ds:[eax+1C]                     |
6F551F08  | 8D8E E80B0000           | lea ecx,dword ptr ds:[esi+BE8]                    |
6F551F0E  | FFD2                    | call edx                                          |
6F551F10  | 399E E00B0000           | cmp dword ptr ds:[esi+BE0],ebx                    |
6F551F16  | 0F85 66010000           | jne game.6F552082                                 |
6F551F1C  | 8BCE                    | mov ecx,esi                                       |
6F551F1E  | E8 FD5BFEFF             | call game.6F537B20                                |
6F551F23  | 8BC8                    | mov ecx,eax                                       |
6F551F25  | 3BCB                    | cmp ecx,ebx                                       |
6F551F27  | 0F84 55010000           | je game.6F552082                                  |
6F551F2D  | 8B86 741C0000           | mov eax,dword ptr ds:[esi+1C74]                   |
6F551F33  | F7D8                    | neg eax                                           |
6F551F35  | 1BC0                    | sbb eax,eax                                       |
6F551F37  | 25 00000080             | and eax,80000000                                  |
6F551F3C  | 0D AB170000             | or eax,17AB                                       |
6F551F41  | 50                      | push eax                                          |
6F551F42  | 6A 1A                   | push 1A                                           |
6F551F44  | E8 A75FFEFF             | call game.6F537EF0                                |
6F551F49  | 8B15 4491A86F           | mov edx,dword ptr ds:[6FA89144]                   |
6F551F4F  | 50                      | push eax                                          |
6F551F50  | 52                      | push edx                                          |
6F551F51  | E8 5A2AFEFF             | call game.6F5349B0                                |
6F551F56  | E9 27010000             | jmp game.6F552082                                 |
6F551F5B  | 8BC7                    | mov eax,edi                                       |
6F551F5D  | 69C0 04030000           | imul eax,eax,304                                  |
6F551F63  | 83BC30 78020000 01      | cmp dword ptr ds:[eax+esi+278],1                  |
6F551F6B  | 0F8D 11010000           | jge game.6F552082                                 |
6F551F71  | 8D9424 40010000         | lea edx,dword ptr ss:[esp+140]                    |
6F551F78  | 8D4C24 14               | lea ecx,dword ptr ss:[esp+14]                     |
6F551F7C  | 889C24 45010000         | mov byte ptr ss:[esp+145],bl                      |
6F551F83  | 889C24 55010000         | mov byte ptr ss:[esp+155],bl                      |
6F551F8A  | 889C24 60010000         | mov byte ptr ss:[esp+160],bl                      |
6F551F91  | 889C24 80010000         | mov byte ptr ss:[esp+180],bl                      |
6F551F98  | 889C24 90010000         | mov byte ptr ss:[esp+190],bl                      |
6F551F9F  | E8 BC051000             | call game.6F652560                                |
6F551FA4  | 8B4C24 28               | mov ecx,dword ptr ss:[esp+28]                     |
6F551FA8  | 3B4C24 24               | cmp ecx,dword ptr ss:[esp+24]                     |
6F551FAC  | 77 7E                   | ja game.6F55202C                                  |
6F551FAE  | 8BCE                    | mov ecx,esi                                       |
6F551FB0  | 899E 10060000           | mov dword ptr ds:[esi+610],ebx                    |
6F551FB6  | 33FF                    | xor edi,edi                                       |
6F551FB8  | E8 D357FEFF             | call game.6F537790                                |
6F551FBD  | F78424 14020000 0001000 | test dword ptr ss:[esp+214],100                   |
6F551FC8  | 8986 741C0000           | mov dword ptr ds:[esi+1C74],eax                   |
6F551FCE  | 0F84 AA000000           | je game.6F55207E                                  |
6F551FD4  | E9 9A000000             | jmp game.6F552073                                 |
6F551FD9  | 8BD7                    | mov edx,edi                                       |
6F551FDB  | 69D2 04030000           | imul edx,edx,304                                  |
6F551FE1  | 83BC32 78020000 01      | cmp dword ptr ds:[edx+esi+278],1                  |
6F551FE9  | 0F8D 93000000           | jge game.6F552082                                 |
6F551FEF  | 8D9424 1C020000         | lea edx,dword ptr ss:[esp+21C]                    |
6F551FF6  | 8D4C24 14               | lea ecx,dword ptr ss:[esp+14]                     |
6F551FFA  | 889C24 21020000         | mov byte ptr ss:[esp+221],bl                      |
6F552001  | 889C24 31020000         | mov byte ptr ss:[esp+231],bl                      |
6F552008  | 889C24 3C020000         | mov byte ptr ss:[esp+23C],bl                      |
6F55200F  | 889C24 5C020000         | mov byte ptr ss:[esp+25C],bl                      |
6F552016  | 889C24 6C020000         | mov byte ptr ss:[esp+26C],bl                      |
6F55201D  | E8 3E051000             | call game.6F652560                                |
6F552022  | 8B4424 28               | mov eax,dword ptr ss:[esp+28]                     |
6F552026  | 3B4424 24               | cmp eax,dword ptr ss:[esp+24]                     |
6F55202A  | 76 19                   | jbe game.6F552045                                 |
6F55202C  | 8D4C24 14               | lea ecx,dword ptr ss:[esp+14]                     |
6F552030  | C78424 04030000 FFFFFFF | mov dword ptr ss:[esp+304],FFFFFFFF               |
6F55203B  | E8 D069D7FF             | call game.6F2C8A10                                |
6F552040  | E9 670F0000             | jmp game.6F552FAC                                 |
6F552045  | 8BCE                    | mov ecx,esi                                       |
6F552047  | 899E 10060000           | mov dword ptr ds:[esi+610],ebx                    |
6F55204D  | 33FF                    | xor edi,edi                                       |
6F55204F  | E8 3C57FEFF             | call game.6F537790                                |
6F552054  | F78424 F0020000 0001000 | test dword ptr ss:[esp+2F0],100                   |
6F55205F  | 8986 741C0000           | mov dword ptr ds:[esi+1C74],eax                   |
6F552065  | 74 17                   | je game.6F55207E                                  |
6F552067  | 8B8E C40A0000           | mov ecx,dword ptr ds:[esi+AC4]                    |
6F55206D  | 898E 741C0000           | mov dword ptr ds:[esi+1C74],ecx                   |
6F552073  | BF 01000000             | mov edi,1                                         |
6F552078  | 89BE 10060000           | mov dword ptr ds:[esi+610],edi                    |
6F55207E  | 895C24 28               | mov dword ptr ss:[esp+28],ebx                     |
6F552082  | 3BFB                    | cmp edi,ebx                                       |
6F552084  | 8B6C24 30               | mov ebp,dword ptr ss:[esp+30]                     |
6F552088  | 895C24 34               | mov dword ptr ss:[esp+34],ebx                     |
6F55208C  | 75 3B                   | jne game.6F5520C9                                 |
6F55208E  | 83BE E40B0000 01        | cmp dword ptr ds:[esi+BE4],1                      |
6F552095  | 75 32                   | jne game.6F5520C9                                 |
6F552097  | 8D45 E9                 | lea eax,dword ptr ss:[ebp-17]                     |
6F55209A  | 83F8 18                 | cmp eax,18                                        |
6F55209D  | 77 2A                   | ja game.6F5520C9                                  |
6F55209F  | 0FB690 F02F556F         | movzx edx,byte ptr ds:[eax+6F552FF0]              |
6F5520A6  | FF2495 D82F556F         | jmp dword ptr ds:[edx*4+6F552FD8]                 |
6F5520AD  | 8D4424 14               | lea eax,dword ptr ss:[esp+14]                     |
6F5520B1  | 50                      | push eax                                          |
6F5520B2  | EB 0D                   | jmp game.6F5520C1                                 |
6F5520B4  | C74424 34 01000000      | mov dword ptr ss:[esp+34],1                       |
6F5520BC  | 8D4C24 14               | lea ecx,dword ptr ss:[esp+14]                     |
6F5520C0  | 51                      | push ecx                                          |
6F5520C1  | 55                      | push ebp                                          |
6F5520C2  | 8BCE                    | mov ecx,esi                                       |
6F5520C4  | E8 D759FEFF             | call game.6F537AA0                                |
6F5520C9  | 3BFB                    | cmp edi,ebx                                       |
6F5520CB  | C786 6C1C0000 02000000  | mov dword ptr ds:[esi+1C6C],2                     |
6F5520D5  | 75 36                   | jne game.6F55210D                                 |
6F5520D7  | 83FD 1E                 | cmp ebp,1E                                        |
6F5520DA  | 74 14                   | je game.6F5520F0                                  |
6F5520DC  | 83FD 1F                 | cmp ebp,1F                                        |
6F5520DF  | 74 0F                   | je game.6F5520F0                                  |
6F5520E1  | 83FD 21                 | cmp ebp,21                                        |
6F5520E4  | 74 0A                   | je game.6F5520F0                                  |
6F5520E6  | 83FD 17                 | cmp ebp,17                                        |
6F5520E9  | 74 05                   | je game.6F5520F0                                  |
6F5520EB  | 83FD 2F                 | cmp ebp,2F                                        |
6F5520EE  | 75 1D                   | jne game.6F55210D                                 |
6F5520F0  | B8 01000000             | mov eax,1                                         |
6F5520F5  | EB 18                   | jmp game.6F55210F                                 |
6F5520F7  | 8D5424 14               | lea edx,dword ptr ss:[esp+14]                     |
6F5520FB  | 52                      | push edx                                          |
6F5520FC  | EB C3                   | jmp game.6F5520C1                                 |
6F5520FE  | 399E E00B0000           | cmp dword ptr ds:[esi+BE0],ebx                    |
6F552104  | 75 C3                   | jne game.6F5520C9                                 |
6F552106  | 8D4424 14               | lea eax,dword ptr ss:[esp+14]                     |
6F55210A  | 50                      | push eax                                          |
6F55210B  | EB B4                   | jmp game.6F5520C1                                 |
6F55210D  | 33C0                    | xor eax,eax                                       |
6F55210F  | 8D4D FF                 | lea ecx,dword ptr ss:[ebp-1]                      |
6F552112  | 83F9 30                 | cmp ecx,30                                        |
6F552115  | 0F87 570D0000           | ja game.6F552E72                                  |
6F55211B  | FF248D 0C30556F         | jmp dword ptr ds:[ecx*4+6F55300C]                 |
6F552122  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552128  | 74 0B                   | je game.6F552135                                  |
6F55212A  | 3BC3                    | cmp eax,ebx                                       |
6F55212C  | 74 07                   | je game.6F552135                                  |
6F55212E  | B8 01000000             | mov eax,1                                         |
6F552133  | EB 02                   | jmp game.6F552137                                 |
6F552135  | 33C0                    | xor eax,eax                                       |
6F552137  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F55213D  | 53                      | push ebx                                          |
6F55213E  | 52                      | push edx                                          |
6F55213F  | 50                      | push eax                                          |
6F552140  | 8BCF                    | mov ecx,edi                                       |
6F552142  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552148  | 53                      | push ebx                                          |
6F552149  | 53                      | push ebx                                          |
6F55214A  | 68 E495956F             | push game.6F9595E4                                |
6F55214F  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552153  | 50                      | push eax                                          |
6F552154  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552158  | 8BD7                    | mov edx,edi                                       |
6F55215A  | E8 D168FEFF             | call game.6F538A30                                |
6F55215F  | E9 0E0D0000             | jmp game.6F552E72                                 |
6F552164  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F55216A  | 74 0B                   | je game.6F552177                                  |
6F55216C  | 3BC3                    | cmp eax,ebx                                       |
6F55216E  | 74 07                   | je game.6F552177                                  |
6F552170  | B8 01000000             | mov eax,1                                         |
6F552175  | EB 02                   | jmp game.6F552179                                 |
6F552177  | 33C0                    | xor eax,eax                                       |
6F552179  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F55217F  | 53                      | push ebx                                          |
6F552180  | 52                      | push edx                                          |
6F552181  | 50                      | push eax                                          |
6F552182  | 8BCF                    | mov ecx,edi                                       |
6F552184  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F55218A  | 53                      | push ebx                                          |
6F55218B  | 53                      | push ebx                                          |
6F55218C  | 68 D095956F             | push game.6F9595D0                                |
6F552191  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552195  | 50                      | push eax                                          |
6F552196  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F55219A  | 8BD7                    | mov edx,edi                                       |
6F55219C  | E8 7F69FEFF             | call game.6F538B20                                |
6F5521A1  | E9 CC0C0000             | jmp game.6F552E72                                 |
6F5521A6  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F5521AC  | 74 0B                   | je game.6F5521B9                                  |
6F5521AE  | 3BC3                    | cmp eax,ebx                                       |
6F5521B0  | 74 07                   | je game.6F5521B9                                  |
6F5521B2  | B8 01000000             | mov eax,1                                         |
6F5521B7  | EB 02                   | jmp game.6F5521BB                                 |
6F5521B9  | 33C0                    | xor eax,eax                                       |
6F5521BB  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F5521C1  | 53                      | push ebx                                          |
6F5521C2  | 52                      | push edx                                          |
6F5521C3  | 50                      | push eax                                          |
6F5521C4  | 8BCF                    | mov ecx,edi                                       |
6F5521C6  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F5521CC  | 53                      | push ebx                                          |
6F5521CD  | 53                      | push ebx                                          |
6F5521CE  | 68 B895956F             | push game.6F9595B8                                |
6F5521D3  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F5521D7  | 50                      | push eax                                          |
6F5521D8  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F5521DC  | 8BD7                    | mov edx,edi                                       |
6F5521DE  | E8 2D6AFEFF             | call game.6F538C10                                |
6F5521E3  | E9 8A0C0000             | jmp game.6F552E72                                 |
6F5521E8  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F5521EE  | 74 0B                   | je game.6F5521FB                                  |
6F5521F0  | 3BC3                    | cmp eax,ebx                                       |
6F5521F2  | 74 07                   | je game.6F5521FB                                  |
6F5521F4  | B8 01000000             | mov eax,1                                         |
6F5521F9  | EB 02                   | jmp game.6F5521FD                                 |
6F5521FB  | 33C0                    | xor eax,eax                                       |
6F5521FD  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552203  | 53                      | push ebx                                          |
6F552204  | 52                      | push edx                                          |
6F552205  | 50                      | push eax                                          |
6F552206  | 8BCF                    | mov ecx,edi                                       |
6F552208  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F55220E  | 53                      | push ebx                                          |
6F55220F  | 53                      | push ebx                                          |
6F552210  | 68 A095956F             | push game.6F9595A0                                |
6F552215  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552219  | 50                      | push eax                                          |
6F55221A  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F55221E  | 8BD7                    | mov edx,edi                                       |
6F552220  | E8 DB6AFEFF             | call game.6F538D00                                |
6F552225  | E9 480C0000             | jmp game.6F552E72                                 |
6F55222A  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552230  | 74 0B                   | je game.6F55223D                                  |
6F552232  | 3BC3                    | cmp eax,ebx                                       |
6F552234  | 74 07                   | je game.6F55223D                                  |
6F552236  | B8 01000000             | mov eax,1                                         |
6F55223B  | EB 02                   | jmp game.6F55223F                                 |
6F55223D  | 33C0                    | xor eax,eax                                       |
6F55223F  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552245  | 53                      | push ebx                                          |
6F552246  | 52                      | push edx                                          |
6F552247  | 50                      | push eax                                          |
6F552248  | 8BCF                    | mov ecx,edi                                       |
6F55224A  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552250  | 53                      | push ebx                                          |
6F552251  | 53                      | push ebx                                          |
6F552252  | 68 8895956F             | push game.6F959588                                |
6F552257  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F55225B  | 50                      | push eax                                          |
6F55225C  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552260  | 8BD7                    | mov edx,edi                                       |
6F552262  | E8 896BFEFF             | call game.6F538DF0                                |
6F552267  | E9 060C0000             | jmp game.6F552E72                                 |
6F55226C  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552272  | 74 0B                   | je game.6F55227F                                  |
6F552274  | 3BC3                    | cmp eax,ebx                                       |
6F552276  | 74 07                   | je game.6F55227F                                  |
6F552278  | B8 01000000             | mov eax,1                                         |
6F55227D  | EB 02                   | jmp game.6F552281                                 |
6F55227F  | 33C0                    | xor eax,eax                                       |
6F552281  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552287  | 53                      | push ebx                                          |
6F552288  | 52                      | push edx                                          |
6F552289  | 50                      | push eax                                          |
6F55228A  | 8BCF                    | mov ecx,edi                                       |
6F55228C  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552292  | 53                      | push ebx                                          |
6F552293  | 53                      | push ebx                                          |
6F552294  | 68 7095956F             | push game.6F959570                                |
6F552299  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F55229D  | 50                      | push eax                                          |
6F55229E  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F5522A2  | 8BD7                    | mov edx,edi                                       |
6F5522A4  | E8 E77AFFFF             | call game.6F549D90                                |
6F5522A9  | E9 C40B0000             | jmp game.6F552E72                                 |
6F5522AE  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F5522B4  | 74 0B                   | je game.6F5522C1                                  |
6F5522B6  | 3BC3                    | cmp eax,ebx                                       |
6F5522B8  | 74 07                   | je game.6F5522C1                                  |
6F5522BA  | B8 01000000             | mov eax,1                                         |
6F5522BF  | EB 02                   | jmp game.6F5522C3                                 |
6F5522C1  | 33C0                    | xor eax,eax                                       |
6F5522C3  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F5522C9  | 53                      | push ebx                                          |
6F5522CA  | 52                      | push edx                                          |
6F5522CB  | 50                      | push eax                                          |
6F5522CC  | 8BCF                    | mov ecx,edi                                       |
6F5522CE  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F5522D4  | 53                      | push ebx                                          |
6F5522D5  | 53                      | push ebx                                          |
6F5522D6  | 68 5895956F             | push game.6F959558                                |
6F5522DB  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F5522DF  | 50                      | push eax                                          |
6F5522E0  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F5522E4  | 8BD7                    | mov edx,edi                                       |
6F5522E6  | E8 F56BFEFF             | call game.6F538EE0                                |
6F5522EB  | E9 820B0000             | jmp game.6F552E72                                 |
6F5522F0  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F5522F6  | 74 0B                   | je game.6F552303                                  |
6F5522F8  | 3BC3                    | cmp eax,ebx                                       |
6F5522FA  | 74 07                   | je game.6F552303                                  |
6F5522FC  | B8 01000000             | mov eax,1                                         |
6F552301  | EB 02                   | jmp game.6F552305                                 |
6F552303  | 33C0                    | xor eax,eax                                       |
6F552305  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F55230B  | 53                      | push ebx                                          |
6F55230C  | 52                      | push edx                                          |
6F55230D  | 50                      | push eax                                          |
6F55230E  | 8BCF                    | mov ecx,edi                                       |
6F552310  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552316  | 53                      | push ebx                                          |
6F552317  | 53                      | push ebx                                          |
6F552318  | 68 4095956F             | push game.6F959540                                |
6F55231D  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552321  | 50                      | push eax                                          |
6F552322  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552326  | 8BD7                    | mov edx,edi                                       |
6F552328  | E8 A36CFEFF             | call game.6F538FD0                                |
6F55232D  | E9 400B0000             | jmp game.6F552E72                                 |
6F552332  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552338  | 74 0B                   | je game.6F552345                                  |
6F55233A  | 3BC3                    | cmp eax,ebx                                       |
6F55233C  | 74 07                   | je game.6F552345                                  |
6F55233E  | B8 01000000             | mov eax,1                                         |
6F552343  | EB 02                   | jmp game.6F552347                                 |
6F552345  | 33C0                    | xor eax,eax                                       |
6F552347  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F55234D  | 53                      | push ebx                                          |
6F55234E  | 52                      | push edx                                          |
6F55234F  | 50                      | push eax                                          |
6F552350  | 8BCF                    | mov ecx,edi                                       |
6F552352  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552358  | 53                      | push ebx                                          |
6F552359  | 53                      | push ebx                                          |
6F55235A  | 68 2495956F             | push game.6F959524                                |
6F55235F  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552363  | 50                      | push eax                                          |
6F552364  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552368  | 8BD7                    | mov edx,edi                                       |
6F55236A  | E8 516DFEFF             | call game.6F5390C0                                |
6F55236F  | E9 FE0A0000             | jmp game.6F552E72                                 |
6F552374  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F55237A  | 74 0B                   | je game.6F552387                                  |
6F55237C  | 3BC3                    | cmp eax,ebx                                       |
6F55237E  | 74 07                   | je game.6F552387                                  |
6F552380  | B8 01000000             | mov eax,1                                         |
6F552385  | EB 02                   | jmp game.6F552389                                 |
6F552387  | 33C0                    | xor eax,eax                                       |
6F552389  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F55238F  | 53                      | push ebx                                          |
6F552390  | 52                      | push edx                                          |
6F552391  | 50                      | push eax                                          |
6F552392  | 8BCF                    | mov ecx,edi                                       |
6F552394  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F55239A  | 53                      | push ebx                                          |
6F55239B  | 53                      | push ebx                                          |
6F55239C  | 68 0895956F             | push game.6F959508                                |
6F5523A1  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F5523A5  | 50                      | push eax                                          |
6F5523A6  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F5523AA  | 8BD7                    | mov edx,edi                                       |
6F5523AC  | E8 FF6DFEFF             | call game.6F5391B0                                |
6F5523B1  | E9 BC0A0000             | jmp game.6F552E72                                 |
6F5523B6  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F5523BC  | 74 0B                   | je game.6F5523C9                                  |
6F5523BE  | 3BC3                    | cmp eax,ebx                                       |
6F5523C0  | 74 07                   | je game.6F5523C9                                  |
6F5523C2  | B8 01000000             | mov eax,1                                         |
6F5523C7  | EB 02                   | jmp game.6F5523CB                                 |
6F5523C9  | 33C0                    | xor eax,eax                                       |
6F5523CB  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F5523D1  | 53                      | push ebx                                          |
6F5523D2  | 52                      | push edx                                          |
6F5523D3  | 50                      | push eax                                          |
6F5523D4  | 8BCF                    | mov ecx,edi                                       |
6F5523D6  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F5523DC  | 53                      | push ebx                                          |
6F5523DD  | 53                      | push ebx                                          |
6F5523DE  | 68 EC94956F             | push game.6F9594EC                                |
6F5523E3  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F5523E7  | 50                      | push eax                                          |
6F5523E8  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F5523EC  | 8BD7                    | mov edx,edi                                       |
6F5523EE  | E8 CD7AFFFF             | call game.6F549EC0                                |
6F5523F3  | E9 7A0A0000             | jmp game.6F552E72                                 |
6F5523F8  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F5523FE  | 74 0B                   | je game.6F55240B                                  |
6F552400  | 3BC3                    | cmp eax,ebx                                       |
6F552402  | 74 07                   | je game.6F55240B                                  |
6F552404  | B8 01000000             | mov eax,1                                         |
6F552409  | EB 02                   | jmp game.6F55240D                                 |
6F55240B  | 33C0                    | xor eax,eax                                       |
6F55240D  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552413  | 53                      | push ebx                                          |
6F552414  | 52                      | push edx                                          |
6F552415  | 50                      | push eax                                          |
6F552416  | 8BCF                    | mov ecx,edi                                       |
6F552418  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F55241E  | 53                      | push ebx                                          |
6F55241F  | 53                      | push ebx                                          |
6F552420  | 68 D094956F             | push game.6F9594D0                                |
6F552425  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552429  | 50                      | push eax                                          |
6F55242A  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F55242E  | 8BD7                    | mov edx,edi                                       |
6F552430  | E8 8B7BFFFF             | call game.6F549FC0                                |
6F552435  | E9 380A0000             | jmp game.6F552E72                                 |
6F55243A  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552440  | 74 0B                   | je game.6F55244D                                  |
6F552442  | 3BC3                    | cmp eax,ebx                                       |
6F552444  | 74 07                   | je game.6F55244D                                  |
6F552446  | B8 01000000             | mov eax,1                                         |
6F55244B  | EB 02                   | jmp game.6F55244F                                 |
6F55244D  | 33C0                    | xor eax,eax                                       |
6F55244F  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552455  | 53                      | push ebx                                          |
6F552456  | 52                      | push edx                                          |
6F552457  | 50                      | push eax                                          |
6F552458  | 8BCF                    | mov ecx,edi                                       |
6F55245A  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552460  | 53                      | push ebx                                          |
6F552461  | 53                      | push ebx                                          |
6F552462  | 68 B494956F             | push game.6F9594B4                                |
6F552467  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F55246B  | 50                      | push eax                                          |
6F55246C  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552470  | 8BD7                    | mov edx,edi                                       |
6F552472  | E8 296EFEFF             | call game.6F5392A0                                |
6F552477  | E9 F6090000             | jmp game.6F552E72                                 |
6F55247C  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552482  | 74 0B                   | je game.6F55248F                                  |
6F552484  | 3BC3                    | cmp eax,ebx                                       |
6F552486  | 74 07                   | je game.6F55248F                                  |
6F552488  | B8 01000000             | mov eax,1                                         |
6F55248D  | EB 02                   | jmp game.6F552491                                 |
6F55248F  | 33C0                    | xor eax,eax                                       |
6F552491  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552497  | 53                      | push ebx                                          |
6F552498  | 52                      | push edx                                          |
6F552499  | 50                      | push eax                                          |
6F55249A  | 8BCF                    | mov ecx,edi                                       |
6F55249C  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F5524A2  | 53                      | push ebx                                          |
6F5524A3  | 53                      | push ebx                                          |
6F5524A4  | 68 9C94956F             | push game.6F95949C                                |
6F5524A9  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F5524AD  | 50                      | push eax                                          |
6F5524AE  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F5524B2  | 8BD7                    | mov edx,edi                                       |
6F5524B4  | E8 D76EFEFF             | call game.6F539390                                |
6F5524B9  | E9 B4090000             | jmp game.6F552E72                                 |
6F5524BE  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F5524C4  | 74 0B                   | je game.6F5524D1                                  |
6F5524C6  | 3BC3                    | cmp eax,ebx                                       |
6F5524C8  | 74 07                   | je game.6F5524D1                                  |
6F5524CA  | B8 01000000             | mov eax,1                                         |
6F5524CF  | EB 02                   | jmp game.6F5524D3                                 |
6F5524D1  | 33C0                    | xor eax,eax                                       |
6F5524D3  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F5524D9  | 53                      | push ebx                                          |
6F5524DA  | 52                      | push edx                                          |
6F5524DB  | 50                      | push eax                                          |
6F5524DC  | 8BCF                    | mov ecx,edi                                       |
6F5524DE  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F5524E4  | 53                      | push ebx                                          |
6F5524E5  | 53                      | push ebx                                          |
6F5524E6  | 68 8494956F             | push game.6F959484                                |
6F5524EB  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F5524EF  | 50                      | push eax                                          |
6F5524F0  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F5524F4  | 8BD7                    | mov edx,edi                                       |
6F5524F6  | E8 856FFEFF             | call game.6F539480                                |
6F5524FB  | E9 72090000             | jmp game.6F552E72                                 |
6F552500  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552506  | 74 0B                   | je game.6F552513                                  |
6F552508  | 3BC3                    | cmp eax,ebx                                       |
6F55250A  | 74 07                   | je game.6F552513                                  |
6F55250C  | B8 01000000             | mov eax,1                                         |
6F552511  | EB 02                   | jmp game.6F552515                                 |
6F552513  | 33C0                    | xor eax,eax                                       |
6F552515  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F55251B  | 53                      | push ebx                                          |
6F55251C  | 52                      | push edx                                          |
6F55251D  | 50                      | push eax                                          |
6F55251E  | 8BCF                    | mov ecx,edi                                       |
6F552520  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552526  | 53                      | push ebx                                          |
6F552527  | 68 801B556F             | push game.6F551B80                                |
6F55252C  | 68 7094956F             | push game.6F959470                                |
6F552531  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552535  | 50                      | push eax                                          |
6F552536  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F55253A  | 8BD7                    | mov edx,edi                                       |
6F55253C  | E8 7F7BFFFF             | call game.6F54A0C0                                |
6F552541  | E9 2C090000             | jmp game.6F552E72                                 |
6F552546  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F55254C  | 74 0B                   | je game.6F552559                                  |
6F55254E  | 3BC3                    | cmp eax,ebx                                       |
6F552550  | 74 07                   | je game.6F552559                                  |
6F552552  | B8 01000000             | mov eax,1                                         |
6F552557  | EB 02                   | jmp game.6F55255B                                 |
6F552559  | 33C0                    | xor eax,eax                                       |
6F55255B  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552561  | 53                      | push ebx                                          |
6F552562  | 52                      | push edx                                          |
6F552563  | 50                      | push eax                                          |
6F552564  | 8BCF                    | mov ecx,edi                                       |
6F552566  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F55256C  | 53                      | push ebx                                          |
6F55256D  | 53                      | push ebx                                          |
6F55256E  | 68 6094956F             | push game.6F959460                                |
6F552573  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552577  | 50                      | push eax                                          |
6F552578  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F55257C  | 8BD7                    | mov edx,edi                                       |
6F55257E  | E8 AD3FFFFF             | call game.6F546530                                |
6F552583  | E9 EA080000             | jmp game.6F552E72                                 |
6F552588  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F55258E  | 74 0B                   | je game.6F55259B                                  |
6F552590  | 3BC3                    | cmp eax,ebx                                       |
6F552592  | 74 07                   | je game.6F55259B                                  |
6F552594  | B8 01000000             | mov eax,1                                         |
6F552599  | EB 02                   | jmp game.6F55259D                                 |
6F55259B  | 33C0                    | xor eax,eax                                       |
6F55259D  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F5525A3  | 53                      | push ebx                                          |
6F5525A4  | 52                      | push edx                                          |
6F5525A5  | 50                      | push eax                                          |
6F5525A6  | 8BCF                    | mov ecx,edi                                       |
6F5525A8  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F5525AE  | 53                      | push ebx                                          |
6F5525AF  | 53                      | push ebx                                          |
6F5525B0  | 68 4C94956F             | push game.6F95944C                                |
6F5525B5  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F5525B9  | 50                      | push eax                                          |
6F5525BA  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F5525BE  | 8BD7                    | mov edx,edi                                       |
6F5525C0  | E8 9B40FFFF             | call game.6F546660                                |
6F5525C5  | E9 A8080000             | jmp game.6F552E72                                 |
6F5525CA  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F5525D0  | 74 0B                   | je game.6F5525DD                                  |
6F5525D2  | 3BC3                    | cmp eax,ebx                                       |
6F5525D4  | 74 07                   | je game.6F5525DD                                  |
6F5525D6  | B8 01000000             | mov eax,1                                         |
6F5525DB  | EB 02                   | jmp game.6F5525DF                                 |
6F5525DD  | 33C0                    | xor eax,eax                                       |
6F5525DF  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F5525E5  | 53                      | push ebx                                          |
6F5525E6  | 52                      | push edx                                          |
6F5525E7  | 50                      | push eax                                          |
6F5525E8  | 8BCF                    | mov ecx,edi                                       |
6F5525EA  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F5525F0  | 53                      | push ebx                                          |
6F5525F1  | 53                      | push ebx                                          |
6F5525F2  | 68 3894956F             | push game.6F959438                                |
6F5525F7  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F5525FB  | 50                      | push eax                                          |
6F5525FC  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552600  | 8BD7                    | mov edx,edi                                       |
6F552602  | E8 696FFEFF             | call game.6F539570                                |
6F552607  | E9 66080000             | jmp game.6F552E72                                 |
6F55260C  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552612  | 74 0B                   | je game.6F55261F                                  |
6F552614  | 3BC3                    | cmp eax,ebx                                       |
6F552616  | 74 07                   | je game.6F55261F                                  |
6F552618  | B8 01000000             | mov eax,1                                         |
6F55261D  | EB 02                   | jmp game.6F552621                                 |
6F55261F  | 33C0                    | xor eax,eax                                       |
6F552621  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552627  | 53                      | push ebx                                          |
6F552628  | 52                      | push edx                                          |
6F552629  | 50                      | push eax                                          |
6F55262A  | 8BCF                    | mov ecx,edi                                       |
6F55262C  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552632  | 53                      | push ebx                                          |
6F552633  | 53                      | push ebx                                          |
6F552634  | 68 2494956F             | push game.6F959424                                |
6F552639  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F55263D  | 50                      | push eax                                          |
6F55263E  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552642  | 8BD7                    | mov edx,edi                                       |
6F552644  | E8 2741FFFF             | call game.6F546770                                |
6F552649  | E9 24080000             | jmp game.6F552E72                                 |
6F55264E  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552654  | 74 0B                   | je game.6F552661                                  |
6F552656  | 3BC3                    | cmp eax,ebx                                       |
6F552658  | 74 07                   | je game.6F552661                                  |
6F55265A  | B8 01000000             | mov eax,1                                         |
6F55265F  | EB 02                   | jmp game.6F552663                                 |
6F552661  | 33C0                    | xor eax,eax                                       |
6F552663  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552669  | 53                      | push ebx                                          |
6F55266A  | 52                      | push edx                                          |
6F55266B  | 50                      | push eax                                          |
6F55266C  | 8BCF                    | mov ecx,edi                                       |
6F55266E  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552674  | 53                      | push ebx                                          |
6F552675  | 68 201C556F             | push game.6F551C20                                |
6F55267A  | 68 1094956F             | push game.6F959410                                |
6F55267F  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552683  | 50                      | push eax                                          |
6F552684  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552688  | 8BD7                    | mov edx,edi                                       |
6F55268A  | E8 1142FFFF             | call game.6F5468A0                                |
6F55268F  | E9 DE070000             | jmp game.6F552E72                                 |
6F552694  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F55269A  | 74 0B                   | je game.6F5526A7                                  |
6F55269C  | 3BC3                    | cmp eax,ebx                                       |
6F55269E  | 74 07                   | je game.6F5526A7                                  |
6F5526A0  | B8 01000000             | mov eax,1                                         |
6F5526A5  | EB 02                   | jmp game.6F5526A9                                 |
6F5526A7  | 33C0                    | xor eax,eax                                       |
6F5526A9  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F5526AF  | 53                      | push ebx                                          |
6F5526B0  | 52                      | push edx                                          |
6F5526B1  | 50                      | push eax                                          |
6F5526B2  | 8BCF                    | mov ecx,edi                                       |
6F5526B4  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F5526BA  | 53                      | push ebx                                          |
6F5526BB  | 68 B01C556F             | push game.6F551CB0                                |
6F5526C0  | 68 FC93956F             | push game.6F9593FC                                |
6F5526C5  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F5526C9  | 50                      | push eax                                          |
6F5526CA  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F5526CE  | 8BD7                    | mov edx,edi                                       |
6F5526D0  | E8 0B43FFFF             | call game.6F5469E0                                |
6F5526D5  | E9 98070000             | jmp game.6F552E72                                 |
6F5526DA  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F5526E0  | 74 0B                   | je game.6F5526ED                                  |
6F5526E2  | 3BC3                    | cmp eax,ebx                                       |
6F5526E4  | 74 07                   | je game.6F5526ED                                  |
6F5526E6  | B8 01000000             | mov eax,1                                         |
6F5526EB  | EB 02                   | jmp game.6F5526EF                                 |
6F5526ED  | 33C0                    | xor eax,eax                                       |
6F5526EF  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F5526F5  | 53                      | push ebx                                          |
6F5526F6  | 52                      | push edx                                          |
6F5526F7  | 50                      | push eax                                          |
6F5526F8  | 8BCF                    | mov ecx,edi                                       |
6F5526FA  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552700  | 68 F0FD546F             | push game.6F54FDF0                                |
6F552705  | 53                      | push ebx                                          |
6F552706  | 68 E493956F             | push game.6F9593E4                                |
6F55270B  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F55270F  | 50                      | push eax                                          |
6F552710  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552714  | 8BD7                    | mov edx,edi                                       |
6F552716  | E8 456FFEFF             | call game.6F539660                                |
6F55271B  | E9 52070000             | jmp game.6F552E72                                 |
6F552720  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552726  | 74 0B                   | je game.6F552733                                  |
6F552728  | 3BC3                    | cmp eax,ebx                                       |
6F55272A  | 74 07                   | je game.6F552733                                  |
6F55272C  | B8 01000000             | mov eax,1                                         |
6F552731  | EB 02                   | jmp game.6F552735                                 |
6F552733  | 33C0                    | xor eax,eax                                       |
6F552735  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F55273B  | 53                      | push ebx                                          |
6F55273C  | 52                      | push edx                                          |
6F55273D  | 50                      | push eax                                          |
6F55273E  | 8BCF                    | mov ecx,edi                                       |
6F552740  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552746  | 53                      | push ebx                                          |
6F552747  | 53                      | push ebx                                          |
6F552748  | 68 CC93956F             | push game.6F9593CC                                |
6F55274D  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552751  | 50                      | push eax                                          |
6F552752  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552756  | 8BD7                    | mov edx,edi                                       |
6F552758  | E8 F36FFEFF             | call game.6F539750                                |
6F55275D  | E9 10070000             | jmp game.6F552E72                                 |
6F552762  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552768  | 74 0B                   | je game.6F552775                                  |
6F55276A  | 3BC3                    | cmp eax,ebx                                       |
6F55276C  | 74 07                   | je game.6F552775                                  |
6F55276E  | B8 01000000             | mov eax,1                                         |
6F552773  | EB 02                   | jmp game.6F552777                                 |
6F552775  | 33C0                    | xor eax,eax                                       |
6F552777  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F55277D  | 53                      | push ebx                                          |
6F55277E  | 52                      | push edx                                          |
6F55277F  | 50                      | push eax                                          |
6F552780  | 8BCF                    | mov ecx,edi                                       |
6F552782  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552788  | 53                      | push ebx                                          |
6F552789  | 68 5053546F             | push game.6F545350                                |
6F55278E  | 68 B893956F             | push game.6F9593B8                                |
6F552793  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552797  | 50                      | push eax                                          |
6F552798  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F55279C  | 8BD7                    | mov edx,edi                                       |
6F55279E  | E8 3D43FFFF             | call game.6F546AE0                                |
6F5527A3  | E9 CA060000             | jmp game.6F552E72                                 |
6F5527A8  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F5527AE  | 74 0B                   | je game.6F5527BB                                  |
6F5527B0  | 3BC3                    | cmp eax,ebx                                       |
6F5527B2  | 74 07                   | je game.6F5527BB                                  |
6F5527B4  | B8 01000000             | mov eax,1                                         |
6F5527B9  | EB 02                   | jmp game.6F5527BD                                 |
6F5527BB  | 33C0                    | xor eax,eax                                       |
6F5527BD  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F5527C3  | 53                      | push ebx                                          |
6F5527C4  | 52                      | push edx                                          |
6F5527C5  | 50                      | push eax                                          |
6F5527C6  | 8BCF                    | mov ecx,edi                                       |
6F5527C8  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F5527CE  | 53                      | push ebx                                          |
6F5527CF  | 53                      | push ebx                                          |
6F5527D0  | 68 A493956F             | push game.6F9593A4                                |
6F5527D5  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F5527D9  | 50                      | push eax                                          |
6F5527DA  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F5527DE  | 8BD7                    | mov edx,edi                                       |
6F5527E0  | E8 5B70FEFF             | call game.6F539840                                |
6F5527E5  | E9 88060000             | jmp game.6F552E72                                 |
6F5527EA  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F5527F0  | 74 0B                   | je game.6F5527FD                                  |
6F5527F2  | 3BC3                    | cmp eax,ebx                                       |
6F5527F4  | 74 07                   | je game.6F5527FD                                  |
6F5527F6  | B8 01000000             | mov eax,1                                         |
6F5527FB  | EB 02                   | jmp game.6F5527FF                                 |
6F5527FD  | 33C0                    | xor eax,eax                                       |
6F5527FF  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552805  | 53                      | push ebx                                          |
6F552806  | 52                      | push edx                                          |
6F552807  | 50                      | push eax                                          |
6F552808  | 8BCF                    | mov ecx,edi                                       |
6F55280A  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552810  | 53                      | push ebx                                          |
6F552811  | 68 70E1536F             | push game.6F53E170                                |
6F552816  | 68 9093956F             | push game.6F959390                                |
6F55281B  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F55281F  | 50                      | push eax                                          |
6F552820  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552824  | 8BD7                    | mov edx,edi                                       |
6F552826  | E8 0571FEFF             | call game.6F539930                                |
6F55282B  | E9 42060000             | jmp game.6F552E72                                 |
6F552830  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552836  | 74 0B                   | je game.6F552843                                  |
6F552838  | 3BC3                    | cmp eax,ebx                                       |
6F55283A  | 74 07                   | je game.6F552843                                  |
6F55283C  | B8 01000000             | mov eax,1                                         |
6F552841  | EB 02                   | jmp game.6F552845                                 |
6F552843  | 33C0                    | xor eax,eax                                       |
6F552845  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F55284B  | 53                      | push ebx                                          |
6F55284C  | 52                      | push edx                                          |
6F55284D  | 50                      | push eax                                          |
6F55284E  | 8BCF                    | mov ecx,edi                                       |
6F552850  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552856  | 53                      | push ebx                                          |
6F552857  | 68 B0E1536F             | push game.6F53E1B0                                |
6F55285C  | 68 7C93956F             | push game.6F95937C                                |
6F552861  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552865  | 50                      | push eax                                          |
6F552866  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F55286A  | 8BD7                    | mov edx,edi                                       |
6F55286C  | E8 AF71FEFF             | call game.6F539A20                                |
6F552871  | E9 FC050000             | jmp game.6F552E72                                 |
6F552876  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F55287C  | 74 0B                   | je game.6F552889                                  |
6F55287E  | 3BC3                    | cmp eax,ebx                                       |
6F552880  | 74 07                   | je game.6F552889                                  |
6F552882  | B8 01000000             | mov eax,1                                         |
6F552887  | EB 02                   | jmp game.6F55288B                                 |
6F552889  | 33C0                    | xor eax,eax                                       |
6F55288B  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552891  | 53                      | push ebx                                          |
6F552892  | 52                      | push edx                                          |
6F552893  | 50                      | push eax                                          |
6F552894  | 8BCF                    | mov ecx,edi                                       |
6F552896  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F55289C  | 53                      | push ebx                                          |
6F55289D  | 53                      | push ebx                                          |
6F55289E  | 68 6493956F             | push game.6F959364                                |
6F5528A3  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F5528A7  | 50                      | push eax                                          |
6F5528A8  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F5528AC  | 8BD7                    | mov edx,edi                                       |
6F5528AE  | E8 5D72FEFF             | call game.6F539B10                                |
6F5528B3  | E9 BA050000             | jmp game.6F552E72                                 |
6F5528B8  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F5528BE  | 74 0B                   | je game.6F5528CB                                  |
6F5528C0  | 3BC3                    | cmp eax,ebx                                       |
6F5528C2  | 74 07                   | je game.6F5528CB                                  |
6F5528C4  | B8 01000000             | mov eax,1                                         |
6F5528C9  | EB 02                   | jmp game.6F5528CD                                 |
6F5528CB  | 33C0                    | xor eax,eax                                       |
6F5528CD  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F5528D3  | 53                      | push ebx                                          |
6F5528D4  | 52                      | push edx                                          |
6F5528D5  | 50                      | push eax                                          |
6F5528D6  | 8BCF                    | mov ecx,edi                                       |
6F5528D8  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F5528DE  | 53                      | push ebx                                          |
6F5528DF  | 68 4093546F             | push game.6F549340                                |
6F5528E4  | 68 4C93956F             | push game.6F95934C                                |
6F5528E9  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F5528ED  | 50                      | push eax                                          |
6F5528EE  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F5528F2  | 8BD7                    | mov edx,edi                                       |
6F5528F4  | E8 0779FFFF             | call game.6F54A200                                |
6F5528F9  | E9 74050000             | jmp game.6F552E72                                 |
6F5528FE  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552904  | 74 0B                   | je game.6F552911                                  |
6F552906  | 3BC3                    | cmp eax,ebx                                       |
6F552908  | 74 07                   | je game.6F552911                                  |
6F55290A  | B8 01000000             | mov eax,1                                         |
6F55290F  | EB 02                   | jmp game.6F552913                                 |
6F552911  | 33C0                    | xor eax,eax                                       |
6F552913  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552919  | 53                      | push ebx                                          |
6F55291A  | 52                      | push edx                                          |
6F55291B  | 50                      | push eax                                          |
6F55291C  | 8BCF                    | mov ecx,edi                                       |
6F55291E  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552924  | 68 A093546F             | push game.6F5493A0                                |
6F552929  | 53                      | push ebx                                          |
6F55292A  | 68 3493956F             | push game.6F959334                                |
6F55292F  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552933  | 50                      | push eax                                          |
6F552934  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552938  | 8BD7                    | mov edx,edi                                       |
6F55293A  | E8 C172FEFF             | call game.6F539C00                                |
6F55293F  | E9 2E050000             | jmp game.6F552E72                                 |
6F552944  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F55294A  | 74 0B                   | je game.6F552957                                  |
6F55294C  | 3BC3                    | cmp eax,ebx                                       |
6F55294E  | 74 07                   | je game.6F552957                                  |
6F552950  | B8 01000000             | mov eax,1                                         |
6F552955  | EB 02                   | jmp game.6F552959                                 |
6F552957  | 33C0                    | xor eax,eax                                       |
6F552959  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F55295F  | 53                      | push ebx                                          |
6F552960  | 52                      | push edx                                          |
6F552961  | 50                      | push eax                                          |
6F552962  | 8BCF                    | mov ecx,edi                                       |
6F552964  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F55296A  | 53                      | push ebx                                          |
6F55296B  | 68 20E2536F             | push game.6F53E220                                |
6F552970  | 68 1493956F             | push game.6F959314                                |
6F552975  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552979  | 50                      | push eax                                          |
6F55297A  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F55297E  | 8BD7                    | mov edx,edi                                       |
6F552980  | E8 6B73FEFF             | call game.6F539CF0                                |
6F552985  | E9 E8040000             | jmp game.6F552E72                                 |
6F55298A  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552990  | 74 0B                   | je game.6F55299D                                  |
6F552992  | 3BC3                    | cmp eax,ebx                                       |
6F552994  | 74 07                   | je game.6F55299D                                  |
6F552996  | B8 01000000             | mov eax,1                                         |
6F55299B  | EB 02                   | jmp game.6F55299F                                 |
6F55299D  | 33C0                    | xor eax,eax                                       |
6F55299F  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F5529A5  | 53                      | push ebx                                          |
6F5529A6  | 52                      | push edx                                          |
6F5529A7  | 50                      | push eax                                          |
6F5529A8  | 8BCF                    | mov ecx,edi                                       |
6F5529AA  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F5529B0  | 53                      | push ebx                                          |
6F5529B1  | 53                      | push ebx                                          |
6F5529B2  | 68 F492956F             | push game.6F9592F4                                |
6F5529B7  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F5529BB  | 50                      | push eax                                          |
6F5529BC  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F5529C0  | 8BD7                    | mov edx,edi                                       |
6F5529C2  | E8 1974FEFF             | call game.6F539DE0                                |
6F5529C7  | E9 A6040000             | jmp game.6F552E72                                 |
6F5529CC  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F5529D2  | 74 0B                   | je game.6F5529DF                                  |
6F5529D4  | 3BC3                    | cmp eax,ebx                                       |
6F5529D6  | 74 07                   | je game.6F5529DF                                  |
6F5529D8  | B8 01000000             | mov eax,1                                         |
6F5529DD  | EB 02                   | jmp game.6F5529E1                                 |
6F5529DF  | 33C0                    | xor eax,eax                                       |
6F5529E1  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F5529E7  | 53                      | push ebx                                          |
6F5529E8  | 52                      | push edx                                          |
6F5529E9  | 50                      | push eax                                          |
6F5529EA  | 8BCF                    | mov ecx,edi                                       |
6F5529EC  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F5529F2  | 68 60E2536F             | push game.6F53E260                                |
6F5529F7  | 53                      | push ebx                                          |
6F5529F8  | 68 D892956F             | push game.6F9592D8                                |
6F5529FD  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552A01  | 50                      | push eax                                          |
6F552A02  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552A06  | 8BD7                    | mov edx,edi                                       |
6F552A08  | E8 C374FEFF             | call game.6F539ED0                                |
6F552A0D  | E9 60040000             | jmp game.6F552E72                                 |
6F552A12  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552A18  | 74 0B                   | je game.6F552A25                                  |
6F552A1A  | 3BC3                    | cmp eax,ebx                                       |
6F552A1C  | 74 07                   | je game.6F552A25                                  |
6F552A1E  | B8 01000000             | mov eax,1                                         |
6F552A23  | EB 02                   | jmp game.6F552A27                                 |
6F552A25  | 33C0                    | xor eax,eax                                       |
6F552A27  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552A2D  | 53                      | push ebx                                          |
6F552A2E  | 52                      | push edx                                          |
6F552A2F  | 50                      | push eax                                          |
6F552A30  | 8BCF                    | mov ecx,edi                                       |
6F552A32  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552A38  | 53                      | push ebx                                          |
6F552A39  | 53                      | push ebx                                          |
6F552A3A  | 68 BC92956F             | push game.6F9592BC                                |
6F552A3F  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552A43  | 50                      | push eax                                          |
6F552A44  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552A48  | 8BD7                    | mov edx,edi                                       |
6F552A4A  | E8 7175FEFF             | call game.6F539FC0                                |
6F552A4F  | E9 1E040000             | jmp game.6F552E72                                 |
6F552A54  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552A5A  | 74 0B                   | je game.6F552A67                                  |
6F552A5C  | 3BC3                    | cmp eax,ebx                                       |
6F552A5E  | 74 07                   | je game.6F552A67                                  |
6F552A60  | B8 01000000             | mov eax,1                                         |
6F552A65  | EB 02                   | jmp game.6F552A69                                 |
6F552A67  | 33C0                    | xor eax,eax                                       |
6F552A69  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552A6F  | 53                      | push ebx                                          |
6F552A70  | 52                      | push edx                                          |
6F552A71  | 50                      | push eax                                          |
6F552A72  | 8BCF                    | mov ecx,edi                                       |
6F552A74  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552A7A  | 53                      | push ebx                                          |
6F552A7B  | 53                      | push ebx                                          |
6F552A7C  | 68 A092956F             | push game.6F9592A0                                |
6F552A81  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552A85  | 50                      | push eax                                          |
6F552A86  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552A8A  | 8BD7                    | mov edx,edi                                       |
6F552A8C  | E8 1F76FEFF             | call game.6F53A0B0                                |
6F552A91  | E9 DC030000             | jmp game.6F552E72                                 |
6F552A96  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552A9C  | 74 0B                   | je game.6F552AA9                                  |
6F552A9E  | 3BC3                    | cmp eax,ebx                                       |
6F552AA0  | 74 07                   | je game.6F552AA9                                  |
6F552AA2  | B8 01000000             | mov eax,1                                         |
6F552AA7  | EB 02                   | jmp game.6F552AAB                                 |
6F552AA9  | 33C0                    | xor eax,eax                                       |
6F552AAB  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552AB1  | 53                      | push ebx                                          |
6F552AB2  | 52                      | push edx                                          |
6F552AB3  | 50                      | push eax                                          |
6F552AB4  | 8BCF                    | mov ecx,edi                                       |
6F552AB6  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552ABC  | 53                      | push ebx                                          |
6F552ABD  | 53                      | push ebx                                          |
6F552ABE  | 68 8892956F             | push game.6F959288                                |
6F552AC3  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552AC7  | 50                      | push eax                                          |
6F552AC8  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552ACC  | 8BD7                    | mov edx,edi                                       |
6F552ACE  | E8 2D41FFFF             | call game.6F546C00                                |
6F552AD3  | E9 9A030000             | jmp game.6F552E72                                 |
6F552AD8  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552ADE  | 74 0B                   | je game.6F552AEB                                  |
6F552AE0  | 3BC3                    | cmp eax,ebx                                       |
6F552AE2  | 74 07                   | je game.6F552AEB                                  |
6F552AE4  | B8 01000000             | mov eax,1                                         |
6F552AE9  | EB 02                   | jmp game.6F552AED                                 |
6F552AEB  | 33C0                    | xor eax,eax                                       |
6F552AED  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552AF3  | 53                      | push ebx                                          |
6F552AF4  | 52                      | push edx                                          |
6F552AF5  | 50                      | push eax                                          |
6F552AF6  | 8BCF                    | mov ecx,edi                                       |
6F552AF8  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552AFE  | 53                      | push ebx                                          |
6F552AFF  | 53                      | push ebx                                          |
6F552B00  | 68 6C92956F             | push game.6F95926C                                |
6F552B05  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552B09  | 50                      | push eax                                          |
6F552B0A  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552B0E  | 8BD7                    | mov edx,edi                                       |
6F552B10  | E8 EB77FFFF             | call game.6F54A300                                |
6F552B15  | E9 58030000             | jmp game.6F552E72                                 |
6F552B1A  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552B20  | 74 0B                   | je game.6F552B2D                                  |
6F552B22  | 3BC3                    | cmp eax,ebx                                       |
6F552B24  | 74 07                   | je game.6F552B2D                                  |
6F552B26  | B8 01000000             | mov eax,1                                         |
6F552B2B  | EB 02                   | jmp game.6F552B2F                                 |
6F552B2D  | 33C0                    | xor eax,eax                                       |
6F552B2F  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552B35  | 53                      | push ebx                                          |
6F552B36  | 52                      | push edx                                          |
6F552B37  | 50                      | push eax                                          |
6F552B38  | 8BCF                    | mov ecx,edi                                       |
6F552B3A  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552B40  | 53                      | push ebx                                          |
6F552B41  | 53                      | push ebx                                          |
6F552B42  | 68 5092956F             | push game.6F959250                                |
6F552B47  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552B4B  | 50                      | push eax                                          |
6F552B4C  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552B50  | 8BD7                    | mov edx,edi                                       |
6F552B52  | E8 C941FFFF             | call game.6F546D20                                |
6F552B57  | E9 16030000             | jmp game.6F552E72                                 |
6F552B5C  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552B62  | 74 0B                   | je game.6F552B6F                                  |
6F552B64  | 3BC3                    | cmp eax,ebx                                       |
6F552B66  | 74 07                   | je game.6F552B6F                                  |
6F552B68  | B8 01000000             | mov eax,1                                         |
6F552B6D  | EB 02                   | jmp game.6F552B71                                 |
6F552B6F  | 33C0                    | xor eax,eax                                       |
6F552B71  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552B77  | 53                      | push ebx                                          |
6F552B78  | 52                      | push edx                                          |
6F552B79  | 50                      | push eax                                          |
6F552B7A  | 8BCF                    | mov ecx,edi                                       |
6F552B7C  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552B82  | 53                      | push ebx                                          |
6F552B83  | 68 50FD546F             | push game.6F54FD50                                |
6F552B88  | 68 3492956F             | push game.6F959234                                |
6F552B8D  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552B91  | 50                      | push eax                                          |
6F552B92  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552B96  | 8BD7                    | mov edx,edi                                       |
6F552B98  | E8 6378FFFF             | call game.6F54A400                                |
6F552B9D  | E9 D0020000             | jmp game.6F552E72                                 |
6F552BA2  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552BA8  | 74 0B                   | je game.6F552BB5                                  |
6F552BAA  | 3BC3                    | cmp eax,ebx                                       |
6F552BAC  | 74 07                   | je game.6F552BB5                                  |
6F552BAE  | B8 01000000             | mov eax,1                                         |
6F552BB3  | EB 02                   | jmp game.6F552BB7                                 |
6F552BB5  | 33C0                    | xor eax,eax                                       |
6F552BB7  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552BBD  | 53                      | push ebx                                          |
6F552BBE  | 52                      | push edx                                          |
6F552BBF  | 50                      | push eax                                          |
6F552BC0  | 8BCF                    | mov ecx,edi                                       |
6F552BC2  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552BC8  | 53                      | push ebx                                          |
6F552BC9  | 68 F0E1536F             | push game.6F53E1F0                                |
6F552BCE  | 68 1892956F             | push game.6F959218                                |
6F552BD3  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552BD7  | 50                      | push eax                                          |
6F552BD8  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552BDC  | 8BD7                    | mov edx,edi                                       |
6F552BDE  | E8 BD75FEFF             | call game.6F53A1A0                                |
6F552BE3  | E9 8A020000             | jmp game.6F552E72                                 |
6F552BE8  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552BEE  | 74 0B                   | je game.6F552BFB                                  |
6F552BF0  | 3BC3                    | cmp eax,ebx                                       |
6F552BF2  | 74 07                   | je game.6F552BFB                                  |
6F552BF4  | B8 01000000             | mov eax,1                                         |
6F552BF9  | EB 02                   | jmp game.6F552BFD                                 |
6F552BFB  | 33C0                    | xor eax,eax                                       |
6F552BFD  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552C03  | 53                      | push ebx                                          |
6F552C04  | 52                      | push edx                                          |
6F552C05  | 50                      | push eax                                          |
6F552C06  | 8BCF                    | mov ecx,edi                                       |
6F552C08  | 69C9 04030000           | imul ecx,ecx,304                                  |
6F552C0E  | 53                      | push ebx                                          |
6F552C0F  | 53                      | push ebx                                          |
6F552C10  | 68 0492956F             | push game.6F959204                                |
6F552C15  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552C19  | 50                      | push eax                                          |
6F552C1A  | 8D4C31 08               | lea ecx,dword ptr ds:[ecx+esi+8]                  |
6F552C1E  | 8BD7                    | mov edx,edi                                       |
6F552C20  | E8 6B76FEFF             | call game.6F53A290                                |
6F552C25  | E9 48020000             | jmp game.6F552E72                                 |
6F552C2A  | 53                      | push ebx                                          |
6F552C2B  | 57                      | push edi                                          |
6F552C2C  | 8D5424 1C               | lea edx,dword ptr ss:[esp+1C]                     |
6F552C30  | 52                      | push edx                                          |
6F552C31  | 8BCE                    | mov ecx,esi                                       |
6F552C33  | E8 A8EAFFFF             | call game.6F5516E0                                |
6F552C38  | E9 35020000             | jmp game.6F552E72                                 |
6F552C3D  | 6A 01                   | push 1                                            |
6F552C3F  | 57                      | push edi                                          |
6F552C40  | 8D4424 1C               | lea eax,dword ptr ss:[esp+1C]                     |
6F552C44  | 50                      | push eax                                          |
6F552C45  | 8BCE                    | mov ecx,esi                                       |
6F552C47  | E8 94EAFFFF             | call game.6F5516E0                                |
6F552C4C  | E9 21020000             | jmp game.6F552E72                                 |
6F552C51  | 57                      | push edi                                          |
6F552C52  | 8D4C24 18               | lea ecx,dword ptr ss:[esp+18]                     |
6F552C56  | 51                      | push ecx                                          |
6F552C57  | 8BCE                    | mov ecx,esi                                       |
6F552C59  | 899E 6C1C0000           | mov dword ptr ds:[esi+1C6C],ebx                   |
6F552C5F  | E8 FCBBFFFF             | call game.6F54E860                                |
6F552C64  | E9 09020000             | jmp game.6F552E72                                 |
6F552C69  | 8D5424 50               | lea edx,dword ptr ss:[esp+50]                     |
6F552C6D  | 8D4C24 14               | lea ecx,dword ptr ss:[esp+14]                     |
6F552C71  | 885C24 50               | mov byte ptr ss:[esp+50],bl                       |
6F552C75  | E8 F6FB0F00             | call game.6F652870                                |
6F552C7A  | 8B5424 28               | mov edx,dword ptr ss:[esp+28]                     |
6F552C7E  | 3B5424 24               | cmp edx,dword ptr ss:[esp+24]                     |
6F552C82  | 74 0F                   | je game.6F552C93                                  |
6F552C84  | B8 F091956F             | mov eax,game.6F9591F0                             |
6F552C89  | E8 124CFEFF             | call game.6F5378A0                                |
6F552C8E  | E9 DF010000             | jmp game.6F552E72                                 |
6F552C93  | 3BFB                    | cmp edi,ebx                                       |
6F552C95  | 0F85 D7010000           | jne game.6F552E72                                 |
6F552C9B  | 399E 10060000           | cmp dword ptr ds:[esi+610],ebx                    |
6F552CA1  | 0F84 84000000           | je game.6F552D2B                                  |
6F552CA7  | 399E 70220000           | cmp dword ptr ds:[esi+2270],ebx                   |
6F552CAD  | 0F85 BF010000           | jne game.6F552E72                                 |
6F552CB3  | 399E 6C220000           | cmp dword ptr ds:[esi+226C],ebx                   |
6F552CB9  | 0F85 B3010000           | jne game.6F552E72                                 |
6F552CBF  | 0FB64424 50             | movzx eax,byte ptr ss:[esp+50]                    |
6F552CC4  | 50                      | push eax                                          |
6F552CC5  | 8D4C24 55               | lea ecx,dword ptr ss:[esp+55]                     |
6F552CC9  | 51                      | push ecx                                          |
6F552CCA  | 8D4C24 40               | lea ecx,dword ptr ss:[esp+40]                     |
6F552CCE  | 895C24 34               | mov dword ptr ss:[esp+34],ebx                     |
6F552CD2  | E8 C907EFFF             | call game.6F4434A0                                |
6F552CD7  | 8D5424 2C               | lea edx,dword ptr ss:[esp+2C]                     |
6F552CDB  | 8D4C24 38               | lea ecx,dword ptr ss:[esp+38]                     |
6F552CDF  | C68424 04030000 01      | mov byte ptr ss:[esp+304],1                       |
6F552CE7  | E8 444BFEFF             | call game.6F537830                                |
6F552CEC  | 8D4424 2C               | lea eax,dword ptr ss:[esp+2C]                     |
6F552CF0  | 8D96 641C0000           | lea edx,dword ptr ds:[esi+1C64]                   |
6F552CF6  | 8BE8                    | mov ebp,eax                                       |
6F552CF8  | B9 04000000             | mov ecx,4                                         |
6F552CFD  | 2BD5                    | sub edx,ebp                                       |
6F552CFF  | 90                      | nop                                               |
6F552D00  | 8B2C02                  | mov ebp,dword ptr ds:[edx+eax]                    |
6F552D03  | 3B28                    | cmp ebp,dword ptr ds:[eax]                        |
6F552D05  | 75 0B                   | jne game.6F552D12                                 |
6F552D07  | 83E9 04                 | sub ecx,4                                         |
6F552D0A  | 83C0 04                 | add eax,4                                         |
6F552D0D  | 83F9 04                 | cmp ecx,4                                         |
6F552D10  | 73 EE                   | jae game.6F552D00                                 |
6F552D12  | 8D4C24 38               | lea ecx,dword ptr ss:[esp+38]                     |
6F552D16  | 889C24 04030000         | mov byte ptr ss:[esp+304],bl                      |
6F552D1D  | E8 EE5CD7FF             | call game.6F2C8A10                                |
6F552D22  | 8B6C24 30               | mov ebp,dword ptr ss:[esp+30]                     |
6F552D26  | E9 47010000             | jmp game.6F552E72                                 |
6F552D2B  | 8B96 681C0000           | mov edx,dword ptr ds:[esi+1C68]                   |
6F552D31  | 52                      | push edx                                          |
6F552D32  | 68 DC91956F             | push game.6F9591DC                                |
6F552D37  | E8 248FCBFF             | call game.6F20BC60                                |
6F552D3C  | 53                      | push ebx                                          |
6F552D3D  | 68 CC91956F             | push game.6F9591CC                                |
6F552D42  | E8 198FCBFF             | call game.6F20BC60                                |
6F552D47  | 83C4 10                 | add esp,10                                        |
6F552D4A  | E9 23010000             | jmp game.6F552E72                                 |
6F552D4F  | 885C24 54               | mov byte ptr ss:[esp+54],bl                       |
6F552D53  | 899C24 B8000000         | mov dword ptr ss:[esp+B8],ebx                     |
6F552D5A  | 899C24 BC000000         | mov dword ptr ss:[esp+BC],ebx                     |
6F552D61  | 899C24 C0000000         | mov dword ptr ss:[esp+C0],ebx                     |
6F552D68  | 8D5424 50               | lea edx,dword ptr ss:[esp+50]                     |
6F552D6C  | 8D4C24 14               | lea ecx,dword ptr ss:[esp+14]                     |
6F552D70  | C68424 04030000 02      | mov byte ptr ss:[esp+304],2                       |
6F552D78  | E8 D3081000             | call game.6F653650                                |
6F552D7D  | 8B4424 28               | mov eax,dword ptr ss:[esp+28]                     |
6F552D81  | 3B4424 24               | cmp eax,dword ptr ss:[esp+24]                     |
6F552D85  | 74 0A                   | je game.6F552D91                                  |
6F552D87  | B8 B091956F             | mov eax,game.6F9591B0                             |
6F552D8C  | E8 0F4BFEFF             | call game.6F5378A0                                |
6F552D91  | 0FB64C24 54             | movzx ecx,byte ptr ss:[esp+54]                    |
6F552D96  | 51                      | push ecx                                          |
6F552D97  | 8D5424 59               | lea edx,dword ptr ss:[esp+59]                     |
6F552D9B  | 52                      | push edx                                          |
6F552D9C  | 8D4C24 40               | lea ecx,dword ptr ss:[esp+40]                     |
6F552DA0  | E8 FB06EFFF             | call game.6F4434A0                                |
6F552DA5  | 8D5424 2C               | lea edx,dword ptr ss:[esp+2C]                     |
6F552DA9  | 8D4C24 38               | lea ecx,dword ptr ss:[esp+38]                     |
6F552DAD  | C68424 04030000 03      | mov byte ptr ss:[esp+304],3                       |
6F552DB5  | E8 764AFEFF             | call game.6F537830                                |
6F552DBA  | 8D4C24 38               | lea ecx,dword ptr ss:[esp+38]                     |
6F552DBE  | C68424 04030000 02      | mov byte ptr ss:[esp+304],2                       |
6F552DC6  | E8 455CD7FF             | call game.6F2C8A10                                |
6F552DCB  | 8D4C24 50               | lea ecx,dword ptr ss:[esp+50]                     |
6F552DCF  | 889C24 04030000         | mov byte ptr ss:[esp+304],bl                      |
6F552DD6  | E8 7524FFFF             | call game.6F545250                                |
6F552DDB  | E9 92000000             | jmp game.6F552E72                                 |
6F552DE0  | 83FF 01                 | cmp edi,1                                         |
6F552DE3  | 0F85 89000000           | jne game.6F552E72                                 |
6F552DE9  | 399E BC0A0000           | cmp dword ptr ds:[esi+ABC],ebx                    |
6F552DEF  | 0F84 7D000000           | je game.6F552E72                                  |
6F552DF5  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552DFB  | 74 08                   | je game.6F552E05                                  |
6F552DFD  | 3BC3                    | cmp eax,ebx                                       |
6F552DFF  | 74 04                   | je game.6F552E05                                  |
6F552E01  | 8BC7                    | mov eax,edi                                       |
6F552E03  | EB 02                   | jmp game.6F552E07                                 |
6F552E05  | 33C0                    | xor eax,eax                                       |
6F552E07  | 8B8E 681C0000           | mov ecx,dword ptr ds:[esi+1C68]                   |
6F552E0D  | 53                      | push ebx                                          |
6F552E0E  | 51                      | push ecx                                          |
6F552E0F  | 50                      | push eax                                          |
6F552E10  | 53                      | push ebx                                          |
6F552E11  | 53                      | push ebx                                          |
6F552E12  | 68 9891956F             | push game.6F959198                                |
6F552E17  | 8D5424 2C               | lea edx,dword ptr ss:[esp+2C]                     |
6F552E1B  | 52                      | push edx                                          |
6F552E1C  | 8D8E 0C030000           | lea ecx,dword ptr ds:[esi+30C]                    |
6F552E22  | BA 01000000             | mov edx,1                                         |
6F552E27  | E8 A484FFFF             | call game.6F54B2D0                                |
6F552E2C  | EB 44                   | jmp game.6F552E72                                 |
6F552E2E  | 83FF 01                 | cmp edi,1                                         |
6F552E31  | 75 3F                   | jne game.6F552E72                                 |
6F552E33  | 399E BC0A0000           | cmp dword ptr ds:[esi+ABC],ebx                    |
6F552E39  | 74 37                   | je game.6F552E72                                  |
6F552E3B  | 399E 74220000           | cmp dword ptr ds:[esi+2274],ebx                   |
6F552E41  | 74 08                   | je game.6F552E4B                                  |
6F552E43  | 3BC3                    | cmp eax,ebx                                       |
6F552E45  | 74 04                   | je game.6F552E4B                                  |
6F552E47  | 8BC7                    | mov eax,edi                                       |
6F552E49  | EB 02                   | jmp game.6F552E4D                                 |
6F552E4B  | 33C0                    | xor eax,eax                                       |
6F552E4D  | 8B8E 681C0000           | mov ecx,dword ptr ds:[esi+1C68]                   |
6F552E53  | 53                      | push ebx                                          |
6F552E54  | 51                      | push ecx                                          |
6F552E55  | 50                      | push eax                                          |
6F552E56  | 53                      | push ebx                                          |
6F552E57  | 53                      | push ebx                                          |
6F552E58  | 68 8091956F             | push game.6F959180                                |
6F552E5D  | 8D5424 2C               | lea edx,dword ptr ss:[esp+2C]                     |
6F552E61  | 52                      | push edx                                          |
6F552E62  | 8D8E 0C030000           | lea ecx,dword ptr ds:[esi+30C]                    |
6F552E68  | BA 01000000             | mov edx,1                                         |
6F552E6D  | E8 DE3FFFFF             | call game.6F546E50                                |
6F552E72  | 3BFB                    | cmp edi,ebx                                       |
6F552E74  | 899E 6C1C0000           | mov dword ptr ds:[esi+1C6C],ebx                   |
6F552E7A  | 75 2A                   | jne game.6F552EA6                                 |
6F552E7C  | 83BE E40B0000 01        | cmp dword ptr ds:[esi+BE4],1                      |
6F552E83  | 75 21                   | jne game.6F552EA6                                 |
6F552E85  | 83FD 1B                 | cmp ebp,1B                                        |
6F552E88  | 75 1C                   | jne game.6F552EA6                                 |
6F552E8A  | 399E E00B0000           | cmp dword ptr ds:[esi+BE0],ebx                    |
6F552E90  | 75 14                   | jne game.6F552EA6                                 |
6F552E92  | 8BCE                    | mov ecx,esi                                       |
6F552E94  | E8 0725FFFF             | call game.6F5453A0                                |
6F552E99  | 8D4424 14               | lea eax,dword ptr ss:[esp+14]                     |
6F552E9D  | 50                      | push eax                                          |
6F552E9E  | 55                      | push ebp                                          |
6F552E9F  | 8BCE                    | mov ecx,esi                                       |
6F552EA1  | E8 FA4BFEFF             | call game.6F537AA0                                |
6F552EA6  | 395C24 34               | cmp dword ptr ss:[esp+34],ebx                     |
6F552EAA  | 0F84 8E000000           | je game.6F552F3E                                  |
6F552EB0  | 8D8C24 C4000000         | lea ecx,dword ptr ss:[esp+C4]                     |
6F552EB7  | 885C24 50               | mov byte ptr ss:[esp+50],bl                       |
6F552EBB  | E8 F0CBFEFF             | call game.6F53FAB0                                |
6F552EC0  | 8D96 641C0000           | lea edx,dword ptr ds:[esi+1C64]                   |
6F552EC6  | 8D8C24 C4000000         | lea ecx,dword ptr ss:[esp+C4]                     |
6F552ECD  | C68424 04030000 04      | mov byte ptr ss:[esp+304],4                       |
6F552ED5  | E8 4649FEFF             | call game.6F537820                                |
6F552EDA  | 8B8424 C4000000         | mov eax,dword ptr ss:[esp+C4]                     |
6F552EE1  | 53                      | push ebx                                          |
6F552EE2  | 8D4C24 38               | lea ecx,dword ptr ss:[esp+38]                     |
6F552EE6  | 51                      | push ecx                                          |
6F552EE7  | 8D5424 34               | lea edx,dword ptr ss:[esp+34]                     |
6F552EEB  | 52                      | push edx                                          |
6F552EEC  | 8B50 24                 | mov edx,dword ptr ds:[eax+24]                     |
6F552EEF  | 8D8C24 D0000000         | lea ecx,dword ptr ss:[esp+D0]                     |
6F552EF6  | 899C24 E4000000         | mov dword ptr ss:[esp+E4],ebx                     |
6F552EFD  | FFD2                    | call edx                                          |
6F552EFF  | 8A4424 34               | mov al,byte ptr ss:[esp+34]                       |
6F552F03  | 8B4C24 2C               | mov ecx,dword ptr ss:[esp+2C]                     |
6F552F07  | 884424 50               | mov byte ptr ss:[esp+50],al                       |
6F552F0B  | 0FB6C0                  | movzx eax,al                                      |
6F552F0E  | 50                      | push eax                                          |
6F552F0F  | 51                      | push ecx                                          |
6F552F10  | 8D5424 59               | lea edx,dword ptr ss:[esp+59]                     |
6F552F14  | 52                      | push edx                                          |
6F552F15  | E8 D2E62800             | call <JMP.&memcpy>                                |
6F552F1A  | 83C4 0C                 | add esp,C                                         |
6F552F1D  | 8D4424 50               | lea eax,dword ptr ss:[esp+50]                     |
6F552F21  | 50                      | push eax                                          |
6F552F22  | 6A 22                   | push 22                                           |
6F552F24  | 8BCE                    | mov ecx,esi                                       |
6F552F26  | E8 550AFFFF             | call game.6F543980                                |
6F552F2B  | 8D8C24 C4000000         | lea ecx,dword ptr ss:[esp+C4]                     |
6F552F32  | 889C24 04030000         | mov byte ptr ss:[esp+304],bl                      |
6F552F39  | E8 D2CDFEFF             | call game.6F53FD10                                |
6F552F3E  | 399E 441B0000           | cmp dword ptr ds:[esi+1B44],ebx                   |
6F552F44  | 74 35                   | je game.6F552F7B                                  |
6F552F46  | 8BCE                    | mov ecx,esi                                       |
6F552F48  | 899E 441B0000           | mov dword ptr ds:[esi+1B44],ebx                   |
6F552F4E  | E8 FD4BFEFF             | call game.6F537B50                                |
6F552F53  | 69FF 04030000           | imul edi,edi,304                                  |
6F552F59  | 8B8E 681C0000           | mov ecx,dword ptr ds:[esi+1C68]                   |
6F552F5F  | 8B96 74220000           | mov edx,dword ptr ds:[esi+2274]                   |
6F552F65  | 51                      | push ecx                                          |
6F552F66  | 52                      | push edx                                          |
6F552F67  | 68 6C91956F             | push game.6F95916C                                |
6F552F6C  | 8D96 481B0000           | lea edx,dword ptr ds:[esi+1B48]                   |
6F552F72  | 8D4C37 08               | lea ecx,dword ptr ds:[edi+esi+8]                  |
6F552F76  | E8 0574FEFF             | call game.6F53A380                                |
6F552F7B  | 83C8 FF                 | or eax,FFFFFFFF                                   |
6F552F7E  | 394424 20               | cmp dword ptr ss:[esp+20],eax                     |
6F552F82  | 898424 04030000         | mov dword ptr ss:[esp+304],eax                    |
6F552F89  | C74424 14 082C936F      | mov dword ptr ss:[esp+14],game.6F932C08           |
6F552F91  | 74 19                   | je game.6F552FAC                                  |
6F552F93  | 8D4424 20               | lea eax,dword ptr ss:[esp+20]                     |
6F552F97  | 50                      | push eax                                          |
6F552F98  | 8D4C24 20               | lea ecx,dword ptr ss:[esp+20]                     |
6F552F9C  | 51                      | push ecx                                          |
6F552F9D  | 8D5424 20               | lea edx,dword ptr ss:[esp+20]                     |
6F552FA1  | 52                      | push edx                                          |
6F552FA2  | 8D4C24 20               | lea ecx,dword ptr ss:[esp+20]                     |
6F552FA6  | FF15 0C2C936F           | call dword ptr ds:[6F932C0C]                      |
6F552FAC  | 8B8C24 FC020000         | mov ecx,dword ptr ss:[esp+2FC]                    |
6F552FB3  | 64:890D 00000000        | mov dword ptr fs:[0],ecx                          |
6F552FBA  | 59                      | pop ecx                                           |
6F552FBB  | 5F                      | pop edi                                           |
6F552FBC  | 5E                      | pop esi                                           |
6F552FBD  | 5D                      | pop ebp                                           |
6F552FBE  | 5B                      | pop ebx                                           |
6F552FBF  | 8B8C24 E4020000         | mov ecx,dword ptr ss:[esp+2E4]                    |
6F552FC6  | 33CC                    | xor ecx,esp                                       |
6F552FC8  | E8 8CE02800             | call game.6F7E1059                                |
6F552FCD  | 81C4 F4020000           | add esp,2F4                                       |
6F552FD3  | C2 0400                 | ret 4                                             |
6F552FD6  | 8BFF                    | mov edi,edi                                       |
6F552FD8  | BC 20556FFE             | mov esp,FE6F5520                                  |
6F552FDD  | 2055 6F                 | and byte ptr ss:[ebp+6F],dl                       |
6F552FE0  | AD                      | lodsd                                             |
6F552FE1  | 2055 6F                 | and byte ptr ss:[ebp+6F],dl                       |
6F552FE4  | B4 20                   | mov ah,20                                         |
6F552FE6  | 55                      | push ebp                                          |
6F552FE7  | 6F                      | outsd                                             |
6F552FE8  | F720                    | mul dword ptr ds:[eax]                            |
6F552FEA  | 55                      | push ebp                                          |
6F552FEB  | 6F                      | outsd                                             |
6F552FEC  | C9                      | leave                                             |
6F552FED  | 2055 6F                 | and byte ptr ss:[ebp+6F],dl                       |
6F552FF0  | 0005 05050501           | add byte ptr ds:[1050505],al                      |
6F552FF6  | 05 02030400             | add eax,40302                                     |
6F552FFB  | 05 00050505             | add eax,5050500                                   |
6F553000  | 05 05050505             | add eax,5050505                                   |
6F553005  | 05 0505008D             | add eax,8D000505                                  |
6F55300A  | 49                      | dec ecx                                           |
6F55300B  | 0022                    | add byte ptr ds:[edx],ah                          |
6F55300D  | 2155 6F                 | and dword ptr ss:[ebp+6F],edx                     |
6F553010  | 64:2155 6F              | and dword ptr fs:[ebp+6F],edx                     |
6F553014  | A6                      | cmpsb                                             |
6F553015  | 2155 6F                 | and dword ptr ss:[ebp+6F],edx                     |
6F553018  | E8 21556F2A             | call 99C4853E                                     |
6F55301D  | 2255 6F                 | and dl,byte ptr ss:[ebp+6F]                       |
6F553020  | 6C                      | insb                                              |
6F553021  | 2255 6F                 | and dl,byte ptr ss:[ebp+6F]                       |
6F553024  | AE                      | scasb                                             |
6F553025  | 2255 6F                 | and dl,byte ptr ss:[ebp+6F]                       |
6F553028  | F0                      | ???                                               |
6F553029  | 2255 6F                 | and dl,byte ptr ss:[ebp+6F]                       |
6F55302C  | 3223                    | xor ah,byte ptr ds:[ebx]                          |
6F55302E  | 55                      | push ebp                                          |
6F55302F  | 6F                      | outsd                                             |
6F553030  | 74 23                   | je game.6F553055                                  |
6F553032  | 55                      | push ebp                                          |
6F553033  | 6F                      | outsd                                             |
6F553034  | B6 23                   | mov dh,23                                         |
6F553036  | 55                      | push ebp                                          |
6F553037  | 6F                      | outsd                                             |
6F553038  | F8                      | clc                                               |
6F553039  | 2355 6F                 | and edx,dword ptr ss:[ebp+6F]                     |
6F55303C  | 3A2455 6F7C2455         | cmp ah,byte ptr ds:[edx*2+55247C6F]               |
6F553043  | 6F                      | outsd                                             |
6F553044  | BE 24556F00             | mov esi,6F5524                                    |
6F553049  | 25 556F4625             | and eax,25466F55                                  |
6F55304E  | 55                      | push ebp                                          |
6F55304F  | 6F                      | outsd                                             |
6F553050  | 8825 556FCA25           | mov byte ptr ds:[25CA6F55],ah                     |
6F553056  | 55                      | push ebp                                          |
6F553057  | 6F                      | outsd                                             |
6F553058  | 0C 26                   | or al,26                                          |
6F55305A  | 55                      | push ebp                                          |
6F55305B  | 6F                      | outsd                                             |
6F55305C  | 4E                      | dec esi                                           |
6F55305D  | 26:55                   | push ebp                                          |
6F55305F  | 6F                      | outsd                                             |
6F553060  | 94                      | xchg esp,eax                                      |
6F553061  | 26:55                   | push ebp                                          |
6F553063  | 6F                      | outsd                                             |
6F553064  | DA26                    | fisub dword ptr ds:[esi]                          |
6F553066  | 55                      | push ebp                                          |
6F553067  | 6F                      | outsd                                             |
6F553068  | 2027                    | and byte ptr ds:[edi],ah                          |
6F55306A  | 55                      | push ebp                                          |
6F55306B  | 6F                      | outsd                                             |
6F55306C  | 6227                    | bound esp,qword ptr ds:[edi]                      |
6F55306E  | 55                      | push ebp                                          |
6F55306F  | 6F                      | outsd                                             |
6F553070  | A8 27                   | test al,27                                        |
6F553072  | 55                      | push ebp                                          |
6F553073  | 6F                      | outsd                                             |
6F553074  | EA 27556F30 2855        | jmp far 5528:306F5527                             |
6F55307B  | 6F                      | outsd                                             |
6F55307C  | 76 28                   | jbe game.6F5530A6                                 |
6F55307E  | 55                      | push ebp                                          |
6F55307F  | 6F                      | outsd                                             |
6F553080  | 2A2C55 6F3D2C55         | sub ch,byte ptr ds:[edx*2+552C3D6F]               |
6F553087  | 6F                      | outsd                                             |
6F553088  | 51                      | push ecx                                          |
6F553089  | 2C 55                   | sub al,55                                         |
6F55308B  | 6F                      | outsd                                             |
6F55308C  | A2 2B556F69             | mov byte ptr ds:[696F552B],al                     |
6F553091  | 2C 55                   | sub al,55                                         |
6F553093  | 6F                      | outsd                                             |
6F553094  | 4F                      | dec edi                                           |
6F553095  | 2D 556FB828             | sub eax,28B86F55                                  |
6F55309A  | 55                      | push ebp                                          |
6F55309B  | 6F                      | outsd                                             |
6F55309C  | FE                      | ???                                               |
6F55309D  | 2855 6F                 | sub byte ptr ss:[ebp+6F],dl                       |
6F5530A0  | 44                      | inc esp                                           |
6F5530A1  | 2955 6F                 | sub dword ptr ss:[ebp+6F],edx                     |
6F5530A4  | 8A29                    | mov ch,byte ptr ds:[ecx]                          |
6F5530A6  | 55                      | push ebp                                          |
6F5530A7  | 6F                      | outsd                                             |
6F5530A8  | CC                      | int3                                              |
6F5530A9  | 2955 6F                 | sub dword ptr ss:[ebp+6F],edx                     |
6F5530AC  | 122A                    | adc ch,byte ptr ds:[edx]                          |
6F5530AE  | 55                      | push ebp                                          |
6F5530AF  | 6F                      | outsd                                             |
6F5530B0  | 54                      | push esp                                          |
6F5530B1  | 2A55 6F                 | sub dl,byte ptr ss:[ebp+6F]                       |
6F5530B4  | 96                      | xchg esi,eax                                      |
6F5530B5  | 2A55 6F                 | sub dl,byte ptr ss:[ebp+6F]                       |
6F5530B8  | D82A                    | fsubr dword ptr ds:[edx]                          |
6F5530BA  | 55                      | push ebp                                          |
6F5530BB  | 6F                      | outsd                                             |
6F5530BC  | 1A2B                    | sbb ch,byte ptr ds:[ebx]                          |
6F5530BE  | 55                      | push ebp                                          |
6F5530BF  | 6F                      | outsd                                             |
6F5530C0  | 5C                      | pop esp                                           |
6F5530C1  | 2B55 6F                 | sub edx,dword ptr ss:[ebp+6F]                     |
6F5530C4  | E8 2B556FE0             | call 4FC485F4                                     |
6F5530C9  | 2D 556F2E2E             | sub eax,2E2E6F55                                  |
6F5530CE  | 55                      | push ebp                                          |
6F5530CF  | 6F                      | outsd                                             |
6F5530D0  | 6A FF                   | push FFFFFFFF                                     |
6F5530D2  | 68 F874836F             | push game.6F8374F8                                |
6F5530D7  | 64:A1 00000000          | mov eax,dword ptr fs:[0]                          |
6F5530DD  | 50                      | push eax                                          |
6F5530DE  | 83EC 5C                 | sub esp,5C                                        |
6F5530E1  | 53                      | push ebx                                          |
6F5530E2  | 55                      | push ebp                                          |
6F5530E3  | 56                      | push esi                                          |
6F5530E4  | 57                      | push edi                                          |
6F5530E5  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]                   |
6F5530EA  | 33C4                    | xor eax,esp                                       |
6F5530EC  | 50                      | push eax                                          |
6F5530ED  | 8D4424 70               | lea eax,dword ptr ss:[esp+70]                     |
6F5530F1  | 64:A3 00000000          | mov dword ptr fs:[0],eax                          |
6F5530F7  | 8BF9                    | mov edi,ecx                                       |
6F5530F9  | 8BB7 400F0000           | mov esi,dword ptr ds:[edi+F40]                    |
6F5530FF  | 33DB                    | xor ebx,ebx                                       |
6F553101  | 3BF3                    | cmp esi,ebx                                       |
6F553103  | 895C24 20               | mov dword ptr ss:[esp+20],ebx                     |
6F553107  | 0F8E DD020000           | jle game.6F5533EA                                 |
6F55310D  | 8BC6                    | mov eax,esi                                       |
6F55310F  | 75 06                   | jne game.6F553117                                 |
6F553111  | 8D87 3C0F0000           | lea eax,dword ptr ds:[edi+F3C]                    |
6F553117  | 8B40 04                 | mov eax,dword ptr ds:[eax+4]                      |
6F55311A  | 33C9                    | xor ecx,ecx                                       |
6F55311C  | 3BC3                    | cmp eax,ebx                                       |
6F55311E  | 0F9EC1                  | setle cl                                          |
6F553121  | 83E9 01                 | sub ecx,1                                         |
6F553124  | 23C1                    | and eax,ecx                                       |
6F553126  | 399F 3C170000           | cmp dword ptr ds:[edi+173C],ebx                   |
6F55312C  | 8987 401B0000           | mov dword ptr ds:[edi+1B40],eax                   |
6F553132  | 74 11                   | je game.6F553145                                  |
6F553134  | 0FB656 14               | movzx edx,byte ptr ds:[esi+14]                    |
6F553138  | 399C97 44170000         | cmp dword ptr ds:[edi+edx*4+1744],ebx             |
6F55313F  | 0F84 E7020000           | je game.6F55342C                                  |
6F553145  | 8A46 14                 | mov al,byte ptr ds:[esi+14]                       |
6F553148  | 3C 40                   | cmp al,40                                         |
6F55314A  | 0F83 BA020000           | jae game.6F55340A                                 |
6F553150  | 33C9                    | xor ecx,ecx                                       |
6F553152  | 399F 10060000           | cmp dword ptr ds:[edi+610],ebx                    |
6F553158  | 0FB6E8                  | movzx ebp,al                                      |
6F55315B  | 0F94C1                  | sete cl                                           |
6F55315E  | 33D2                    | xor edx,edx                                       |
6F553160  | 3856 15                 | cmp byte ptr ds:[esi+15],dl                       |
6F553163  | 895C24 1C               | mov dword ptr ss:[esp+1C],ebx                     |
6F553167  | 0F95C2                  | setne dl                                          |
6F55316A  | 896C24 18               | mov dword ptr ss:[esp+18],ebp                     |
6F55316E  | 3BD1                    | cmp edx,ecx                                       |
6F553170  | 0F85 4A020000           | jne game.6F5533C0                                 |
6F553176  | 83BF 78020000 04        | cmp dword ptr ds:[edi+278],4                      |
6F55317D  | 0F8C 3D020000           | jl game.6F5533C0                                  |
6F553183  | 3C 40                   | cmp al,40                                         |
6F553185  | 0F83 35020000           | jae game.6F5533C0                                 |
6F55318B  | 0FB6C8                  | movzx ecx,al                                      |
6F55318E  | 83F9 1E                 | cmp ecx,1E                                        |
6F553191  | 0F84 70010000           | je game.6F553307                                  |
6F553197  | 3C 40                   | cmp al,40                                         |
6F553199  | 0F83 21020000           | jae game.6F5533C0                                 |
6F55319F  | 0FB6C8                  | movzx ecx,al                                      |
6F5531A2  | 83F9 1F                 | cmp ecx,1F                                        |
6F5531A5  | 0F84 5C010000           | je game.6F553307                                  |
6F5531AB  | 3C 40                   | cmp al,40                                         |
6F5531AD  | 0F83 0D020000           | jae game.6F5533C0                                 |
6F5531B3  | 0FB6C8                  | movzx ecx,al                                      |
6F5531B6  | 83F9 17                 | cmp ecx,17                                        |
6F5531B9  | 75 74                   | jne game.6F55322F                                 |
6F5531BB  | 8B46 0C                 | mov eax,dword ptr ds:[esi+C]                      |
6F5531BE  | 8B4E 08                 | mov ecx,dword ptr ds:[esi+8]                      |
6F5531C1  | 83CD FF                 | or ebp,FFFFFFFF                                   |
6F5531C4  | C74424 40 082C936F      | mov dword ptr ss:[esp+40],game.6F932C08           |
6F5531CC  | 894C24 44               | mov dword ptr ss:[esp+44],ecx                     |
6F5531D0  | 895C24 48               | mov dword ptr ss:[esp+48],ebx                     |
6F5531D4  | 896C24 4C               | mov dword ptr ss:[esp+4C],ebp                     |
6F5531D8  | 894424 50               | mov dword ptr ss:[esp+50],eax                     |
6F5531DC  | 895C24 54               | mov dword ptr ss:[esp+54],ebx                     |
6F5531E0  | 8D5424 30               | lea edx,dword ptr ss:[esp+30]                     |
6F5531E4  | 8D4C24 40               | lea ecx,dword ptr ss:[esp+40]                     |
6F5531E8  | 895C24 78               | mov dword ptr ss:[esp+78],ebx                     |
6F5531EC  | E8 BFE30F00             | call game.6F6515B0                                |
6F5531F1  | 8B4C24 30               | mov ecx,dword ptr ss:[esp+30]                     |
6F5531F5  | 8D5424 18               | lea edx,dword ptr ss:[esp+18]                     |
6F5531F9  | E8 92D9D2FF             | call game.6F280B90                                |
6F5531FE  | 0FB64C24 34             | movzx ecx,byte ptr ss:[esp+34]                    |
6F553203  | 8D5424 18               | lea edx,dword ptr ss:[esp+18]                     |
6F553207  | E8 84D9D2FF             | call game.6F280B90                                |
6F55320C  | 8B4C24 38               | mov ecx,dword ptr ss:[esp+38]                     |
6F553210  | 8D5424 18               | lea edx,dword ptr ss:[esp+18]                     |
6F553214  | E8 77D9D2FF             | call game.6F280B90                                |
6F553219  | 8D4C24 40               | lea ecx,dword ptr ss:[esp+40]                     |
6F55321D  | 896C24 78               | mov dword ptr ss:[esp+78],ebp                     |
6F553221  | E8 EA57D7FF             | call game.6F2C8A10                                |
6F553226  | 8B4C24 18               | mov ecx,dword ptr ss:[esp+18]                     |
6F55322A  | E9 36010000             | jmp game.6F553365                                 |
6F55322F  | 3C 40                   | cmp al,40                                         |
6F553231  | 0F83 89010000           | jae game.6F5533C0                                 |
6F553237  | 0FB6C8                  | movzx ecx,al                                      |
6F55323A  | 83F9 21                 | cmp ecx,21                                        |
6F55323D  | 75 5D                   | jne game.6F55329C                                 |
6F55323F  | 8B46 0C                 | mov eax,dword ptr ds:[esi+C]                      |
6F553242  | 8B4E 08                 | mov ecx,dword ptr ds:[esi+8]                      |
6F553245  | 50                      | push eax                                          |
6F553246  | 51                      | push ecx                                          |
6F553247  | 8D4C24 48               | lea ecx,dword ptr ss:[esp+48]                     |
6F55324B  | E8 5002EFFF             | call game.6F4434A0                                |
6F553250  | 8D5424 14               | lea edx,dword ptr ss:[esp+14]                     |
6F553254  | 8D4C24 40               | lea ecx,dword ptr ss:[esp+40]                     |
6F553258  | C74424 78 01000000      | mov dword ptr ss:[esp+78],1                       |
6F553260  | E8 8BE40F00             | call game.6F6516F0                                |
6F553265  | 66:8B5C24 14            | mov bx,word ptr ss:[esp+14]                       |
6F55326A  | 0FB6CB                  | movzx ecx,bl                                      |
6F55326D  | 8D5424 18               | lea edx,dword ptr ss:[esp+18]                     |
6F553271  | E8 1AD9D2FF             | call game.6F280B90                                |
6F553276  | 0FB6CF                  | movzx ecx,bh                                      |
6F553279  | 8D5424 18               | lea edx,dword ptr ss:[esp+18]                     |
6F55327D  | E8 0ED9D2FF             | call game.6F280B90                                |
6F553282  | 8D4C24 40               | lea ecx,dword ptr ss:[esp+40]                     |
6F553286  | C74424 78 FFFFFFFF      | mov dword ptr ss:[esp+78],FFFFFFFF                |
6F55328E  | E8 7D57D7FF             | call game.6F2C8A10                                |
6F553293  | 8B4C24 18               | mov ecx,dword ptr ss:[esp+18]                     |
6F553297  | E9 C7000000             | jmp game.6F553363                                 |
6F55329C  | 3C 40                   | cmp al,40                                         |
6F55329E  | 0F83 1C010000           | jae game.6F5533C0                                 |
6F5532A4  | 0FB6C0                  | movzx eax,al                                      |
6F5532A7  | 83F8 2F                 | cmp eax,2F                                        |
6F5532AA  | 0F85 10010000           | jne game.6F5533C0                                 |
6F5532B0  | 8B46 0C                 | mov eax,dword ptr ds:[esi+C]                      |
6F5532B3  | 8B4E 08                 | mov ecx,dword ptr ds:[esi+8]                      |
6F5532B6  | 50                      | push eax                                          |
6F5532B7  | 51                      | push ecx                                          |
6F5532B8  | 8D4C24 60               | lea ecx,dword ptr ss:[esp+60]                     |
6F5532BC  | E8 DF01EFFF             | call game.6F4434A0                                |
6F5532C1  | 8D5424 28               | lea edx,dword ptr ss:[esp+28]                     |
6F5532C5  | 8D4C24 58               | lea ecx,dword ptr ss:[esp+58]                     |
6F5532C9  | C74424 78 02000000      | mov dword ptr ss:[esp+78],2                       |
6F5532D1  | E8 BAE40F00             | call game.6F651790                                |
6F5532D6  | 8B4C24 28               | mov ecx,dword ptr ss:[esp+28]                     |
6F5532DA  | 8D5424 18               | lea edx,dword ptr ss:[esp+18]                     |
6F5532DE  | E8 ADD8D2FF             | call game.6F280B90                                |
6F5532E3  | 8B4C24 2C               | mov ecx,dword ptr ss:[esp+2C]                     |
6F5532E7  | 8D5424 18               | lea edx,dword ptr ss:[esp+18]                     |
6F5532EB  | E8 A0D8D2FF             | call game.6F280B90                                |
6F5532F0  | 8D4C24 58               | lea ecx,dword ptr ss:[esp+58]                     |
6F5532F4  | C74424 78 FFFFFFFF      | mov dword ptr ss:[esp+78],FFFFFFFF                |
6F5532FC  | E8 0F57D7FF             | call game.6F2C8A10                                |
6F553301  | 8B4C24 18               | mov ecx,dword ptr ss:[esp+18]                     |
6F553305  | EB 5E                   | jmp game.6F553365                                 |
6F553307  | 8B56 0C                 | mov edx,dword ptr ds:[esi+C]                      |
6F55330A  | 8B4E 08                 | mov ecx,dword ptr ds:[esi+8]                      |
6F55330D  | C74424 1C 01000000      | mov dword ptr ss:[esp+1C],1                       |
6F553315  | E8 A6B2E4FF             | call game.6F39E5C0                                |
6F55331A  | 0FB6DC                  | movzx ebx,ah                                      |
6F55331D  | 8BC8                    | mov ecx,eax                                       |
6F55331F  | 0FB6C0                  | movzx eax,al                                      |
6F553322  | 895C24 24               | mov dword ptr ss:[esp+24],ebx                     |
6F553326  | 69DB 2D7A0000           | imul ebx,ebx,7A2D                                 |
6F55332C  | 335C24 24               | xor ebx,dword ptr ss:[esp+24]                     |
6F553330  | 894424 18               | mov dword ptr ss:[esp+18],eax                     |
6F553334  | 69C0 2D7A0000           | imul eax,eax,7A2D                                 |
6F55333A  | 334424 18               | xor eax,dword ptr ss:[esp+18]                     |
6F55333E  | C1E9 10                 | shr ecx,10                                        |
6F553341  | 0FB6D5                  | movzx edx,ch                                      |
6F553344  | 0FB6C9                  | movzx ecx,cl                                      |
6F553347  | 03C3                    | add eax,ebx                                       |
6F553349  | 8BD9                    | mov ebx,ecx                                       |
6F55334B  | 69DB 2D7A0000           | imul ebx,ebx,7A2D                                 |
6F553351  | 33D9                    | xor ebx,ecx                                       |
6F553353  | 8BCA                    | mov ecx,edx                                       |
6F553355  | 69C9 2D7A0000           | imul ecx,ecx,7A2D                                 |
6F55335B  | 33CA                    | xor ecx,edx                                       |
6F55335D  | 03C3                    | add eax,ebx                                       |
6F55335F  | 03CD                    | add ecx,ebp                                       |
6F553361  | 03C8                    | add ecx,eax                                       |
6F553363  | 33DB                    | xor ebx,ebx                                       |
6F553365  | 399C24 80000000         | cmp dword ptr ss:[esp+80],ebx                     |
6F55336C  | 74 7C                   | je game.6F5533EA                                  |
6F55336E  | 8BC1                    | mov eax,ecx                                       |
6F553370  | C1E8 10                 | shr eax,10                                        |
6F553373  | 0FB6D4                  | movzx edx,ah                                      |
6F553376  | 8BEA                    | mov ebp,edx                                       |
6F553378  | 69ED 2D7A0000           | imul ebp,ebp,7A2D                                 |
6F55337E  | 33EA                    | xor ebp,edx                                       |
6F553380  | 01AF 7C220000           | add dword ptr ds:[edi+227C],ebp                   |
6F553386  | 8B97 7C220000           | mov edx,dword ptr ds:[edi+227C]                   |
6F55338C  | 0FB6C0                  | movzx eax,al                                      |
6F55338F  | 8BE8                    | mov ebp,eax                                       |
6F553391  | 69ED 2D7A0000           | imul ebp,ebp,7A2D                                 |
6F553397  | 33E8                    | xor ebp,eax                                       |
6F553399  | 8D042A                  | lea eax,dword ptr ds:[edx+ebp]                    |
6F55339C  | 0FB6D5                  | movzx edx,ch                                      |
6F55339F  | 0FB6C9                  | movzx ecx,cl                                      |
6F5533A2  | 8BE9                    | mov ebp,ecx                                       |
6F5533A4  | 69ED 2D7A0000           | imul ebp,ebp,7A2D                                 |
6F5533AA  | 33E9                    | xor ebp,ecx                                       |
6F5533AC  | 8BCA                    | mov ecx,edx                                       |
6F5533AE  | 69C9 2D7A0000           | imul ecx,ecx,7A2D                                 |
6F5533B4  | 33CA                    | xor ecx,edx                                       |
6F5533B6  | 03E8                    | add ebp,eax                                       |
6F5533B8  | 03CD                    | add ecx,ebp                                       |
6F5533BA  | 898F 7C220000           | mov dword ptr ds:[edi+227C],ecx                   |
6F5533C0  | 56                      | push esi                                          |
6F5533C1  | 8D8F 380F0000           | lea ecx,dword ptr ds:[edi+F38]                    |
6F5533C7  | E8 B45DFFFF             | call game.6F549180                                |
6F5533CC  | 56                      | push esi                                          |
6F5533CD  | 8BCF                    | mov ecx,edi                                       |
6F5533CF  | E8 ACE9FFFF             | call game.6F551D80                                |
6F5533D4  | 56                      | push esi                                          |
6F5533D5  | 8BCF                    | mov ecx,edi                                       |
6F5533D7  | E8 3484FFFF             | call game.6F54B810                                |
6F5533DC  | 395C24 1C               | cmp dword ptr ss:[esp+1C],ebx                     |
6F5533E0  | 74 4A                   | je game.6F55342C                                  |
6F5533E2  | C74424 20 01000000      | mov dword ptr ss:[esp+20],1                       |
6F5533EA  | 899F 401B0000           | mov dword ptr ds:[edi+1B40],ebx                   |
6F5533F0  | 8B4424 20               | mov eax,dword ptr ss:[esp+20]                     |
6F5533F4  | 8B4C24 70               | mov ecx,dword ptr ss:[esp+70]                     |
6F5533F8  | 64:890D 00000000        | mov dword ptr fs:[0],ecx                          |
6F5533FF  | 59                      | pop ecx                                           |
6F553400  | 5F                      | pop edi                                           |
6F553401  | 5E                      | pop esi                                           |
6F553402  | 5D                      | pop ebp                                           |
6F553403  | 5B                      | pop ebx                                           |
6F553404  | 83C4 68                 | add esp,68                                        |
6F553407  | C2 0400                 | ret 4                                             |
6F55340A  | 2C 40                   | sub al,40                                         |
6F55340C  | 3C BE                   | cmp al,BE                                         |
6F55340E  | 77 1C                   | ja game.6F55342C                                  |
6F553410  | 56                      | push esi                                          |
6F553411  | 8D8F 380F0000           | lea ecx,dword ptr ds:[edi+F38]                    |
6F553417  | E8 645DFFFF             | call game.6F549180                                |
6F55341C  | 56                      | push esi                                          |
6F55341D  | 8BCF                    | mov ecx,edi                                       |
6F55341F  | E8 1C99FFFF             | call game.6F54CD40                                |
6F553424  | 56                      | push esi                                          |
6F553425  | 8BCF                    | mov ecx,edi                                       |
6F553427  | E8 E483FFFF             | call game.6F54B810                                |
6F55342C  | 8BB7 401B0000           | mov esi,dword ptr ds:[edi+1B40]                   |
6F553432  | 3BF3                    | cmp esi,ebx                                       |
6F553434  | 0F85 D3FCFFFF           | jne game.6F55310D                                 |
6F55343A  | EB AE                   | jmp game.6F5533EA                                 |
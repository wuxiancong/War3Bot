6F352290  | 8B4424 04               | mov eax,dword ptr ss:[esp+4]                      |
6F352294  | 8378 08 00              | cmp dword ptr ds:[eax+8],0                        |
6F352298  | 74 09                   | je game.6F3522A3                                  |
6F35229A  | 894424 04               | mov dword ptr ss:[esp+4],eax                      |
6F35229E  | E9 CD892B00             | jmp game.6F60AC70                                 |
6F3522A3  | E8 A8FFFFFF             | call game.6F352250                                |
6F3522A8  | B8 01000000             | mov eax,1                                         |
6F3522AD  | C2 0400                 | ret 4                                             |
6F3522B0  | 81EC 88000000           | sub esp,88                                        |
6F3522B6  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]                   |
6F3522BB  | 33C4                    | xor eax,esp                                       |
6F3522BD  | 898424 84000000         | mov dword ptr ss:[esp+84],eax                     |
6F3522C4  | 53                      | push ebx                                          |
6F3522C5  | 8B9C24 94000000         | mov ebx,dword ptr ss:[esp+94]                     |
6F3522CC  | 55                      | push ebp                                          |
6F3522CD  | 56                      | push esi                                          |
6F3522CE  | 8BF1                    | mov esi,ecx                                       |
6F3522D0  | 8B0D F465AB6F           | mov ecx,dword ptr ds:[6FAB65F4]                   |
6F3522D6  | 33C0                    | xor eax,eax                                       |
6F3522D8  | 57                      | push edi                                          |
6F3522D9  | 33FF                    | xor edi,edi                                       |
6F3522DB  | 33ED                    | xor ebp,ebp                                       |
6F3522DD  | 3BC8                    | cmp ecx,eax                                       |
6F3522DF  | 894424 10               | mov dword ptr ss:[esp+10],eax                     |
6F3522E3  | 74 14                   | je game.6F3522F9                                  |
6F3522E5  | 6A 01                   | push 1                                            |
6F3522E7  | E8 745D0500             | call game.6F3A8060                                |
6F3522EC  | 8BC8                    | mov ecx,eax                                       |
6F3522EE  | E8 DD5B0D00             | call game.6F427ED0                                |
6F3522F3  | F7D8                    | neg eax                                           |
6F3522F5  | 1BC0                    | sbb eax,eax                                       |
6F3522F7  | F7D8                    | neg eax                                           |
6F3522F9  | 8B8C24 9C000000         | mov ecx,dword ptr ss:[esp+9C]                     |
6F352300  | 83F9 03                 | cmp ecx,3                                         |
6F352303  | 77 4B                   | ja game.6F352350                                  |
6F352305  | FF248D A425356F         | jmp dword ptr ds:[ecx*4+6F3525A4]                 |
6F35230C  | 8BAC24 A8000000         | mov ebp,dword ptr ss:[esp+A8]                     |
6F352313  | BF 74DE936F             | mov edi,game.6F93DE74                             | <-- GAMEOVER_VICTORY
6F352318  | EB 36                   | jmp game.6F352350                                 |
6F35231A  | 8B8424 A8000000         | mov eax,dword ptr ss:[esp+A8]                     |
6F352321  | BF 64DE936F             | mov edi,game.6F93DE64                             | <-- GAMEOVER_DEFEAT
6F352326  | 33ED                    | xor ebp,ebp                                       |
6F352328  | 894424 10               | mov dword ptr ss:[esp+10],eax                     |
6F35232C  | EB 22                   | jmp game.6F352350                                 |
6F35232E  | 8B8424 A8000000         | mov eax,dword ptr ss:[esp+A8]                     |
6F352335  | BF 54DE936F             | mov edi,game.6F93DE54                             | <-- GAMEOVER_TIE
6F35233A  | 8BE8                    | mov ebp,eax                                       |
6F35233C  | 894424 10               | mov dword ptr ss:[esp+10],eax                     |
6F352340  | EB 0E                   | jmp game.6F352350                                 |
6F352342  | 85C0                    | test eax,eax                                      |
6F352344  | BF 3CDE936F             | mov edi,game.6F93DE3C                             | <-- GAMEOVER_DISCONNECTED
6F352349  | 75 05                   | jne game.6F352350                                 |
6F35234B  | BF 28DE936F             | mov edi,game.6F93DE28                             | <-- GAMEOVER_NEUTRAL
6F352350  | 68 80000000             | push 80                                           |
6F352355  | 8D5424 18               | lea edx,dword ptr ss:[esp+18]                     |
6F352359  | 8BCF                    | mov ecx,edi                                       |
6F35235B  | E8 F0722700             | call game.6F5C9650                                | <--- 调用 Dialog 显示 'You were disconnected'
6F352360  | 85DB                    | test ebx,ebx                                      |
6F352362  | 75 04                   | jne game.6F352368                                 |
6F352364  | 8D5C24 14               | lea ebx,dword ptr ss:[esp+14]                     |
6F352368  | 8B8E 6C010000           | mov ecx,dword ptr ds:[esi+16C]                    |
6F35236E  | 53                      | push ebx                                          |
6F35236F  | E8 CCF92B00             | call game.6F611D40                                |
6F352374  | 85ED                    | test ebp,ebp                                      |
6F352376  | 0F84 99000000           | je game.6F352415                                  |
6F35237C  | 83BC24 A4000000 00      | cmp dword ptr ss:[esp+A4],0                       |
6F352384  | B9 14DE936F             | mov ecx,game.6F93DE14                             | <--- CONTINUE_PLAYING
6F352389  | 75 05                   | jne game.6F352390                                 |
6F35238B  | B9 10DE936F             | mov ecx,game.6F93DE10                             | <--- OK
6F352390  | 68 80000000             | push 80                                           |
6F352395  | 8D5424 18               | lea edx,dword ptr ss:[esp+18]                     |
6F352399  | E8 B2722700             | call game.6F5C9650                                |
6F35239E  | 8B8E 70010000           | mov ecx,dword ptr ds:[esi+170]                    |
6F3523A4  | 6A 01                   | push 1                                            |
6F3523A6  | 81C1 B4000000           | add ecx,B4                                        |
6F3523AC  | E8 BF3E2B00             | call game.6F606270                                |
6F3523B1  | 8B86 6C010000           | mov eax,dword ptr ds:[esi+16C]                    |
6F3523B7  | 85C0                    | test eax,eax                                      |
6F3523B9  | 74 07                   | je game.6F3523C2                                  |
6F3523BB  | 05 B4000000             | add eax,B4                                        |
6F3523C0  | EB 02                   | jmp game.6F3523C4                                 |
6F3523C2  | 33C0                    | xor eax,eax                                       |
6F3523C4  | D905 8861936F           | fld dword ptr ds:[6F936188]                       |
6F3523CA  | 8B8E 70010000           | mov ecx,dword ptr ds:[esi+170]                    |
6F3523D0  | 6A 01                   | push 1                                            |
6F3523D2  | 83EC 08                 | sub esp,8                                         |
6F3523D5  | D95C24 04               | fstp dword ptr ss:[esp+4]                         |
6F3523D9  | 81C1 B4000000           | add ecx,B4                                        |
6F3523DF  | D9EE                    | fldz                                              |
6F3523E1  | D91C24                  | fstp dword ptr ss:[esp]                           |
6F3523E4  | 6A 07                   | push 7                                            |
6F3523E6  | 50                      | push eax                                          |
6F3523E7  | 6A 01                   | push 1                                            |
6F3523E9  | E8 82432B00             | call game.6F606770                                |
6F3523EE  | 8D4C24 14               | lea ecx,dword ptr ss:[esp+14]                     |
6F3523F2  | 51                      | push ecx                                          |
6F3523F3  | 8B8E 70010000           | mov ecx,dword ptr ds:[esi+170]                    |
6F3523F9  | E8 22C42C00             | call game.6F61E820                                |
6F3523FE  | 8BC8                    | mov ecx,eax                                       |
6F352400  | E8 3BF92B00             | call game.6F611D40                                |
6F352405  | 8B8E 70010000           | mov ecx,dword ptr ds:[esi+170]                    |
6F35240B  | 8B11                    | mov edx,dword ptr ds:[ecx]                        |
6F35240D  | 8B82 D4000000           | mov eax,dword ptr ds:[edx+D4]                     |
6F352413  | EB 0E                   | jmp game.6F352423                                 |
6F352415  | 8B8E 70010000           | mov ecx,dword ptr ds:[esi+170]                    |
6F35241B  | 8B11                    | mov edx,dword ptr ds:[ecx]                        |
6F35241D  | 8B82 D0000000           | mov eax,dword ptr ds:[edx+D0]                     | <--- FFQ|Ruit Campaign
6F352423  | FFD0                    | call eax                                          |
6F352425  | 8B7C24 10               | mov edi,dword ptr ss:[esp+10]                     |
6F352429  | 85FF                    | test edi,edi                                      |
6F35242B  | 0F84 96000000           | je game.6F3524C7                                  |
6F352431  | 68 80000000             | push 80                                           |
6F352436  | 8D5424 18               | lea edx,dword ptr ss:[esp+18]                     |
6F35243A  | B9 00DE936F             | mov ecx,game.6F93DE00                             | <--- RESTART_MISSION
6F35243F  | E8 0C722700             | call game.6F5C9650                                |
6F352444  | 8D4C24 14               | lea ecx,dword ptr ss:[esp+14]                     |
6F352448  | 51                      | push ecx                                          |
6F352449  | 8B8E 74010000           | mov ecx,dword ptr ds:[esi+174]                    |
6F35244F  | E8 CCC32C00             | call game.6F61E820                                |
6F352454  | 8BC8                    | mov ecx,eax                                       |
6F352456  | E8 E5F82B00             | call game.6F611D40                                |
6F35245B  | 8B8E 74010000           | mov ecx,dword ptr ds:[esi+174]                    |
6F352461  | 6A 01                   | push 1                                            |
6F352463  | 81C1 B4000000           | add ecx,B4                                        |
6F352469  | E8 023E2B00             | call game.6F606270                                |
6F35246E  | 85ED                    | test ebp,ebp                                      |
6F352470  | 74 08                   | je game.6F35247A                                  |
6F352472  | 8B86 70010000           | mov eax,dword ptr ds:[esi+170]                    |
6F352478  | EB 06                   | jmp game.6F352480                                 |
6F35247A  | 8B86 6C010000           | mov eax,dword ptr ds:[esi+16C]                    |
6F352480  | 85C0                    | test eax,eax                                      |
6F352482  | 74 07                   | je game.6F35248B                                  |
6F352484  | 05 B4000000             | add eax,B4                                        |
6F352489  | EB 02                   | jmp game.6F35248D                                 |
6F35248B  | 33C0                    | xor eax,eax                                       |
6F35248D  | D905 8861936F           | fld dword ptr ds:[6F936188]                       |
6F352493  | 8B8E 74010000           | mov ecx,dword ptr ds:[esi+174]                    |
6F352499  | 6A 01                   | push 1                                            |
6F35249B  | 83EC 08                 | sub esp,8                                         |
6F35249E  | D95C24 04               | fstp dword ptr ss:[esp+4]                         |
6F3524A2  | 81C1 B4000000           | add ecx,B4                                        |
6F3524A8  | D9EE                    | fldz                                              |
6F3524AA  | D91C24                  | fstp dword ptr ss:[esp]                           |
6F3524AD  | 6A 07                   | push 7                                            |
6F3524AF  | 50                      | push eax                                          |
6F3524B0  | 6A 01                   | push 1                                            |
6F3524B2  | E8 B9422B00             | call game.6F606770                                |
6F3524B7  | 8B8E 74010000           | mov ecx,dword ptr ds:[esi+174]                    |
6F3524BD  | 8B11                    | mov edx,dword ptr ds:[ecx]                        |
6F3524BF  | 8B82 D4000000           | mov eax,dword ptr ds:[edx+D4]                     |
6F3524C5  | EB 0E                   | jmp game.6F3524D5                                 |
6F3524C7  | 8B8E 74010000           | mov ecx,dword ptr ds:[esi+174]                    |
6F3524CD  | 8B11                    | mov edx,dword ptr ds:[ecx]                        |
6F3524CF  | 8B82 D0000000           | mov eax,dword ptr ds:[edx+D0]                     | <--- FFQ|Ruit Campaign
6F3524D5  | FFD0                    | call eax                                          |
6F3524D7  | 8B8E 78010000           | mov ecx,dword ptr ds:[esi+178]                    |
6F3524DD  | 6A 01                   | push 1                                            |
6F3524DF  | 81C1 B4000000           | add ecx,B4                                        |
6F3524E5  | E8 863D2B00             | call game.6F606270                                |
6F3524EA  | 85FF                    | test edi,edi                                      |
6F3524EC  | 74 08                   | je game.6F3524F6                                  |
6F3524EE  | 8B86 74010000           | mov eax,dword ptr ds:[esi+174]                    |
6F3524F4  | EB 12                   | jmp game.6F352508                                 |
6F3524F6  | 85ED                    | test ebp,ebp                                      |
6F3524F8  | 74 08                   | je game.6F352502                                  |
6F3524FA  | 8B86 70010000           | mov eax,dword ptr ds:[esi+170]                    |
6F352500  | EB 06                   | jmp game.6F352508                                 |
6F352502  | 8B86 6C010000           | mov eax,dword ptr ds:[esi+16C]                    |
6F352508  | 85C0                    | test eax,eax                                      |
6F35250A  | 74 07                   | je game.6F352513                                  |
6F35250C  | 05 B4000000             | add eax,B4                                        |
6F352511  | EB 02                   | jmp game.6F352515                                 |
6F352513  | 33C0                    | xor eax,eax                                       |
6F352515  | D905 8861936F           | fld dword ptr ds:[6F936188]                       |
6F35251B  | 8B8E 78010000           | mov ecx,dword ptr ds:[esi+178]                    |
6F352521  | 6A 01                   | push 1                                            |
6F352523  | 83EC 08                 | sub esp,8                                         |
6F352526  | D95C24 04               | fstp dword ptr ss:[esp+4]                         |
6F35252A  | 81C1 B4000000           | add ecx,B4                                        |
6F352530  | D9EE                    | fldz                                              |
6F352532  | D91C24                  | fstp dword ptr ss:[esp]                           |
6F352535  | 6A 07                   | push 7                                            |
6F352537  | 50                      | push eax                                          |
6F352538  | 6A 01                   | push 1                                            |
6F35253A  | E8 31422B00             | call game.6F606770                                |
6F35253F  | 8B8E 78010000           | mov ecx,dword ptr ds:[esi+178]                    |
6F352545  | 8B11                    | mov edx,dword ptr ds:[ecx]                        |
6F352547  | 8B82 D4000000           | mov eax,dword ptr ds:[edx+D4]                     |
6F35254D  | FFD0                    | call eax                                          |
6F35254F  | 03FD                    | add edi,ebp                                       |
6F352551  | 897C24 10               | mov dword ptr ss:[esp+10],edi                     |
6F352555  | DB4424 10               | fild dword ptr ss:[esp+10]                        |
6F352559  | 51                      | push ecx                                          |
6F35255A  | 81C6 B4000000           | add esi,B4                                        |
6F352560  | 8BCE                    | mov ecx,esi                                       |
6F352562  | DC0D F8DD936F           | fmul qword ptr ds:[6F93DDF8]                      |
6F352568  | DC05 F0DD936F           | fadd qword ptr ds:[6F93DDF0]                      |
6F35256E  | D95C24 14               | fstp dword ptr ss:[esp+14]                        |
6F352572  | D94424 14               | fld dword ptr ss:[esp+14]                         |
6F352576  | D91C24                  | fstp dword ptr ss:[esp]                           |
6F352579  | E8 32382B00             | call game.6F605DB0                                |
6F35257E  | 6A 01                   | push 1                                            |
6F352580  | 8BCE                    | mov ecx,esi                                       |
6F352582  | E8 39372B00             | call game.6F605CC0                                |
6F352587  | 8B8C24 94000000         | mov ecx,dword ptr ss:[esp+94]                     |
6F35258E  | 5F                      | pop edi                                           |
6F35258F  | 5E                      | pop esi                                           |
6F352590  | 5D                      | pop ebp                                           |
6F352591  | 5B                      | pop ebx                                           |
6F352592  | 33CC                    | xor ecx,esp                                       |
6F352594  | E8 C0EA4800             | call game.6F7E1059                                |
6F352599  | 81C4 88000000           | add esp,88                                        |
6F35259F  | C2 1000                 | ret 10                                            |
6F3525A2  | 8BFF                    | mov edi,edi                                       |
6F3525A4  | 0C 23                   | or al,23                                          |
6F3525A6  | 35 6F1A2335             | xor eax,35231A6F                                  |
6F3525AB  | 6F                      | outsd                                             |
6F3525AC  | 2E:2335 6F422335        | and esi,dword ptr cs:[3523426F]                   |
6F3525B3  | 6F                      | outsd                                             |
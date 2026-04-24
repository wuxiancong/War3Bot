# 魔兽争霸3战网用户界面事件处理分支

*   **目标模块**: `Game.dll` (v1.26.0.6401)
*   **函数地址**: `0x6F5BB910`
*   **偏移地址**: `game.dll + 5BB910`

```assembly
6F5BB910  | 8B4424 04                | mov eax,dword ptr ss:[esp+4]                                                      |
6F5BB914  | 56                       | push esi                                                                          |
6F5BB915  | 8BF1                     | mov esi,ecx                                                                       |
6F5BB917  | 8B48 08                  | mov ecx,dword ptr ds:[eax+8]                                                      |
6F5BB91A  | 83F9 19                  | cmp ecx,19                                                                        |
6F5BB91D  | 0F87 2F010000            | ja game.6F5BBA52                                                                  |
6F5BB923  | FF248D 50BB5B6F          | jmp dword ptr ds:[ecx*4+6F5BBB50]                                                 | Bnet 事件处理
6F5BB92A  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB92C  | E8 7FBAFCFF              | call game.6F5873B0                                                                |
6F5BB931  | 5E                       | pop esi                                                                           |
6F5BB932  | C2 0400                  | ret 4                                                                             |
6F5BB935  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB937  | E8 94BAFCFF              | call game.6F5873D0                                                                |
6F5BB93C  | 5E                       | pop esi                                                                           |
6F5BB93D  | C2 0400                  | ret 4                                                                             |
6F5BB940  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB942  | E8 B9BAFCFF              | call game.6F587400                                                                |
6F5BB947  | 5E                       | pop esi                                                                           |
6F5BB948  | C2 0400                  | ret 4                                                                             |
6F5BB94B  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB94D  | E8 0EBBFCFF              | call game.6F587460                                                                |
6F5BB952  | 5E                       | pop esi                                                                           |
6F5BB953  | C2 0400                  | ret 4                                                                             |
6F5BB956  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB958  | E8 63BBFCFF              | call game.6F5874C0                                                                |
6F5BB95D  | 5E                       | pop esi                                                                           |
6F5BB95E  | C2 0400                  | ret 4                                                                             |
6F5BB961  | 8BCE                     | mov ecx,esi                                                                       | ---> ecx=0x05
6F5BB963  | E8 38BEFBFF              | call game.6F5777A0                                                                |
6F5BB968  | 5E                       | pop esi                                                                           |
6F5BB969  | C2 0400                  | ret 4                                                                             |
6F5BB96C  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB96E  | E8 3D6EFDFF              | call game.6F5927B0                                                                |
6F5BB973  | 5E                       | pop esi                                                                           |
6F5BB974  | C2 0400                  | ret 4                                                                             |
6F5BB977  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB979  | E8 32DEFFFF              | call game.6F5B97B0                                                                |
6F5BB97E  | 5E                       | pop esi                                                                           |
6F5BB97F  | C2 0400                  | ret 4                                                                             |
6F5BB982  | 50                       | push eax                                                                          |
6F5BB983  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB985  | E8 96BCFBFF              | call game.6F577620                                                                |
6F5BB98A  | 5E                       | pop esi                                                                           |
6F5BB98B  | C2 0400                  | ret 4                                                                             |
6F5BB98E  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB990  | E8 3BF6FFFF              | call game.6F5BAFD0                                                                |
6F5BB995  | 5E                       | pop esi                                                                           |
6F5BB996  | C2 0400                  | ret 4                                                                             |
6F5BB999  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB99B  | E8 309DFFFF              | call game.6F5B56D0                                                                |
6F5BB9A0  | 5E                       | pop esi                                                                           |
6F5BB9A1  | C2 0400                  | ret 4                                                                             |
6F5BB9A4  | 50                       | push eax                                                                          |
6F5BB9A5  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB9A7  | E8 A49DFFFF              | call game.6F5B5750                                                                |
6F5BB9AC  | 5E                       | pop esi                                                                           |
6F5BB9AD  | C2 0400                  | ret 4                                                                             |
6F5BB9B0  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB9B2  | E8 49C1FBFF              | call game.6F577B00                                                                |
6F5BB9B7  | 5E                       | pop esi                                                                           |
6F5BB9B8  | C2 0400                  | ret 4                                                                             |
6F5BB9BB  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB9BD  | E8 0EBCFCFF              | call game.6F5875D0                                                                |
6F5BB9C2  | 5E                       | pop esi                                                                           |
6F5BB9C3  | C2 0400                  | ret 4                                                                             |
6F5BB9C6  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB9C8  | E8 23BCFCFF              | call game.6F5875F0                                                                |
6F5BB9CD  | 5E                       | pop esi                                                                           |
6F5BB9CE  | C2 0400                  | ret 4                                                                             |
6F5BB9D1  | 50                       | push eax                                                                          | ---> ecx=0x06
6F5BB9D2  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB9D4  | E8 57FCFEFF              | call game.6F5AB630                                                                |
6F5BB9D9  | 5E                       | pop esi                                                                           |
6F5BB9DA  | C2 0400                  | ret 4                                                                             |
6F5BB9DD  | 50                       | push eax                                                                          |
6F5BB9DE  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB9E0  | E8 DBC4FBFF              | call game.6F577EC0                                                                |
6F5BB9E5  | 5E                       | pop esi                                                                           |
6F5BB9E6  | C2 0400                  | ret 4                                                                             |
6F5BB9E9  | 50                       | push eax                                                                          |
6F5BB9EA  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB9EC  | E8 DFC6FBFF              | call game.6F5780D0                                                                |
6F5BB9F1  | 5E                       | pop esi                                                                           |
6F5BB9F2  | C2 0400                  | ret 4                                                                             |
6F5BB9F5  | 50                       | push eax                                                                          |
6F5BB9F6  | 8BCE                     | mov ecx,esi                                                                       |
6F5BB9F8  | E8 A3BBFBFF              | call game.6F5775A0                                                                |
6F5BB9FD  | 5E                       | pop esi                                                                           |
6F5BB9FE  | C2 0400                  | ret 4                                                                             |
6F5BBA01  | 50                       | push eax                                                                          | ---> ecx=0x13
6F5BBA02  | 8BCE                     | mov ecx,esi                                                                       |
6F5BBA04  | E8 879CFFFF              | call game.6F5B5690                                                                |
6F5BBA09  | 5E                       | pop esi                                                                           |
6F5BBA0A  | C2 0400                  | ret 4                                                                             |
6F5BBA0D  | 830D 18A4A96F 01         | or dword ptr ds:[6FA9A418],1                                                      |
6F5BBA14  | 8B8E 9C020000            | mov ecx,dword ptr ds:[esi+29C]                                                    |
6F5BBA1A  | 8B01                     | mov eax,dword ptr ds:[ecx]                                                        |
6F5BBA1C  | 8B90 0C010000            | mov edx,dword ptr ds:[eax+10C]                                                    |
6F5BBA22  | 6A 01                    | push 1                                                                            |
6F5BBA24  | FFD2                     | call edx                                                                          |
6F5BBA26  | 6A 01                    | push 1                                                                            |
6F5BBA28  | 8D8E EC010000            | lea ecx,dword ptr ds:[esi+1EC]                                                    |
6F5BBA2E  | E8 1DBC0500              | call game.6F617650                                                                |
6F5BBA33  | 8B8E D0010000            | mov ecx,dword ptr ds:[esi+1D0]                                                    |
6F5BBA39  | 85C9                     | test ecx,ecx                                                                      |
6F5BBA3B  | 74 15                    | je game.6F5BBA52                                                                  |
6F5BBA3D  | 6A 00                    | push 0                                                                            |
6F5BBA3F  | 6A 00                    | push 0                                                                            |
6F5BBA41  | 6A 01                    | push 1                                                                            |
6F5BBA43  | E8 C8F40300              | call game.6F5FAF10                                                                |
6F5BBA48  | C786 D0010000 00000000   | mov dword ptr ds:[esi+1D0],0                                                      |
6F5BBA52  | 33C0                     | xor eax,eax                                                                       |
6F5BBA54  | 5E                       | pop esi                                                                           |
6F5BBA55  | C2 0400                  | ret 4                                                                             |
6F5BBA58  | 8B86 A4020000            | mov eax,dword ptr ds:[esi+2A4]                                                    |
6F5BBA5E  | 50                       | push eax                                                                          |
6F5BBA5F  | 8BCE                     | mov ecx,esi                                                                       |
6F5BBA61  | E8 DAC8FBFF              | call game.6F578340                                                                |
6F5BBA66  | BA 00020000              | mov edx,200                                                                       |
6F5BBA6B  | 8BCE                     | mov ecx,esi                                                                       |
6F5BBA6D  | E8 5E550400              | call game.6F600FD0                                                                |
6F5BBA72  | 8BCE                     | mov ecx,esi                                                                       |
6F5BBA74  | E8 D7CCFBFF              | call game.6F578750                                                                |
6F5BBA79  | 33C0                     | xor eax,eax                                                                       |
6F5BBA7B  | 5E                       | pop esi                                                                           |
6F5BBA7C  | C2 0400                  | ret 4                                                                             |
6F5BBA7F  | 33D2                     | xor edx,edx                                                                       | ---> ecx=0x18
6F5BBA81  | 33C9                     | xor ecx,ecx                                                                       |
6F5BBA83  | E8 387EFDFF              | call game.6F5938C0                                                                |
6F5BBA88  | 8B8E D4010000            | mov ecx,dword ptr ds:[esi+1D4]                                                    |
6F5BBA8E  | 8B11                     | mov edx,dword ptr ds:[ecx]                                                        |
6F5BBA90  | 8B82 D4000000            | mov eax,dword ptr ds:[edx+D4]                                                     |
6F5BBA96  | FFD0                     | call eax                                                                          |
6F5BBA98  | 6A 00                    | push 0                                                                            |
6F5BBA9A  | BA 08A2956F              | mov edx,game.6F95A208                                                             |
6F5BBA9F  | B9 00A2956F              | mov ecx,game.6F95A200                                                             |
6F5BBAA4  | E8 87E3A4FF              | call game.6F009E30                                                                |
6F5BBAA9  | D95C24 08                | fstp dword ptr ss:[esp+8]                                                         |
6F5BBAAD  | D94424 08                | fld dword ptr ss:[esp+8]                                                          |
6F5BBAB1  | 6A 17                    | push 17                                                                           |
6F5BBAB3  | 56                       | push esi                                                                          |
6F5BBAB4  | 51                       | push ecx                                                                          |
6F5BBAB5  | 8B8E D4010000            | mov ecx,dword ptr ds:[esi+1D4]                                                    |
6F5BBABB  | D91C24                   | fstp dword ptr ss:[esp]                                                           |
6F5BBABE  | 51                       | push ecx                                                                          |
6F5BBABF  | 8D8E 88010000            | lea ecx,dword ptr ds:[esi+188]                                                    |
6F5BBAC5  | E8 068DD5FF              | call game.6F3147D0                                                                |
6F5BBACA  | B8 01000000              | mov eax,1                                                                         |
6F5BBACF  | 5E                       | pop esi                                                                           |
6F5BBAD0  | C2 0400                  | ret 4                                                                             |
6F5BBAD3  | 830D 18A4A96F 01         | or dword ptr ds:[6FA9A418],1                                                      | ---> ecx=0x17
6F5BBADA  | 8B8E F8010000            | mov ecx,dword ptr ds:[esi+1F8]                                                    |
6F5BBAE0  | E8 BB830500              | call game.6F613EA0                                                                |
6F5BBAE5  | 85C0                     | test eax,eax                                                                      |
6F5BBAE7  | 74 0D                    | je game.6F5BBAF6                                                                  |
6F5BBAE9  | 8038 00                  | cmp byte ptr ds:[eax],0                                                           |
6F5BBAEC  | 74 08                    | je game.6F5BBAF6                                                                  |
6F5BBAEE  | 8B8E FC010000            | mov ecx,dword ptr ds:[esi+1FC]                                                    |
6F5BBAF4  | EB 06                    | jmp game.6F5BBAFC                                                                 |
6F5BBAF6  | 8B8E F8010000            | mov ecx,dword ptr ds:[esi+1F8]                                                    |
6F5BBAFC  | 6A 00                    | push 0                                                                            |
6F5BBAFE  | 6A 00                    | push 0                                                                            |
6F5BBB00  | 6A 01                    | push 1                                                                            |
6F5BBB02  | E8 09F40300              | call game.6F5FAF10                                                                |
6F5BBB07  | B8 01000000              | mov eax,1                                                                         |
6F5BBB0C  | 5E                       | pop esi                                                                           |
6F5BBB0D  | C2 0400                  | ret 4                                                                             |
6F5BBB10  | 8B8E D4010000            | mov ecx,dword ptr ds:[esi+1D4]                                                    | ---> ecx=0x16
6F5BBB16  | 8B11                     | mov edx,dword ptr ds:[ecx]                                                        |
6F5BBB18  | 8B82 D0000000            | mov eax,dword ptr ds:[edx+D0]                                                     |
6F5BBB1E  | FFD0                     | call eax                                                                          |
6F5BBB20  | 8BCE                     | mov ecx,esi                                                                       |
6F5BBB22  | E8 29C9FDFF              | call game.6F598450                                                                |
6F5BBB27  | B8 01000000              | mov eax,1                                                                         |
6F5BBB2C  | 5E                       | pop esi                                                                           |
6F5BBB2D  | C2 0400                  | ret 4                                                                             |
6F5BBB30  | 830D 18A4A96F 01         | or dword ptr ds:[6FA9A418],1                                                      | ---> ecx=0x19
6F5BBB37  | 8B8E 68010000            | mov ecx,dword ptr ds:[esi+168]                                                    |
6F5BBB3D  | 33D2                     | xor edx,edx                                                                       |
6F5BBB3F  | E8 AC79FDFF              | call game.6F5934F0                                                                |
6F5BBB44  | B8 01000000              | mov eax,1                                                                         |
6F5BBB49  | 5E                       | pop esi                                                                           |
6F5BBB4A  | C2 0400                  | ret 4                                                                             |
6F5BBB4D  | 8D49 00                  | lea ecx,dword ptr ds:[ecx]                                                        |
6F5BBB50  | 2AB9 5B6F35B9            | sub bh,byte ptr ds:[ecx-46CA90A5]                                                 |
6F5BBB56  | 5B                       | pop ebx                                                                           |
6F5BBB57  | 6F                       | outsd                                                                             |
6F5BBB58  | 56                       | push esi                                                                          |
6F5BBB59  | B9 5B6F40B9              | mov ecx,B9406F5B                                                                  |
6F5BBB5E  | 5B                       | pop ebx                                                                           |
6F5BBB5F  | 6F                       | outsd                                                                             |
6F5BBB60  | 4B                       | dec ebx                                                                           |
6F5BBB61  | B9 5B6F61B9              | mov ecx,B9616F5B                                                                  |
6F5BBB66  | 5B                       | pop ebx                                                                           |
6F5BBB67  | 6F                       | outsd                                                                             |
6F5BBB68  | D1B9 5B6FB0B9            | sar dword ptr ds:[ecx-464F90A5],1                                                 |
6F5BBB6E  | 5B                       | pop ebx                                                                           |
6F5BBB6F  | 6F                       | outsd                                                                             |
6F5BBB70  | BB B95B6FC6              | mov ebx,C66F5BB9                                                                  |
6F5BBB75  | B9 5B6FDDB9              | mov ecx,B9DD6F5B                                                                  |
6F5BBB7A  | 5B                       | pop ebx                                                                           |
6F5BBB7B  | 6F                       | outsd                                                                             |
6F5BBB7C  | E9 B95B6F99              | jmp 8CB173A                                                                       |
6F5BBB81  | B9 5B6FA4B9              | mov ecx,B9A46F5B                                                                  |
6F5BBB86  | 5B                       | pop ebx                                                                           |
6F5BBB87  | 6F                       | outsd                                                                             |
6F5BBB88  | 82B9 5B6F8EB9 5B         | cmp byte ptr ds:[ecx-467190A5],5B                                                 |
6F5BBB8F  | 6F                       | outsd                                                                             |
6F5BBB90  | 6C                       | insb                                                                              |
6F5BBB91  | B9 5B6F77B9              | mov ecx,B9776F5B                                                                  |
6F5BBB96  | 5B                       | pop ebx                                                                           |
6F5BBB97  | 6F                       | outsd                                                                             |
6F5BBB98  | F5                       | cmc                                                                               |
6F5BBB99  | B9 5B6F01BA              | mov ecx,BA016F5B                                                                  |
6F5BBB9E  | 5B                       | pop ebx                                                                           |
6F5BBB9F  | 6F                       | outsd                                                                             |
6F5BBBA0  | 0D BA5B6F58              | or eax,586F5BBA                                                                   |
6F5BBBA5  | BA 5B6F10BB              | mov edx,BB106F5B                                                                  |
6F5BBBAA  | 5B                       | pop ebx                                                                           |
6F5BBBAB  | 6F                       | outsd                                                                             |
6F5BBBAC  | D3BA 5B6F7FBA            | sar dword ptr ds:[edx-458090A5],cl                                                |
6F5BBBB2  | 5B                       | pop ebx                                                                           |
6F5BBBB3  | 6F                       | outsd                                                                             |
6F5BBBB4  | 30BB 5B6FCCCC            | xor byte ptr ds:[ebx-333390A5],bh                                                 |
```
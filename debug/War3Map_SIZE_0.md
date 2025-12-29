```assembly
6F657C10  | 81EC 70040000           | sub esp,470                         |
6F657C16  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]     |
6F657C1B  | 33C4                    | xor eax,esp                         |
6F657C1D  | 898424 6C040000         | mov dword ptr ss:[esp+46C],eax      |
6F657C24  | 8B8424 7C040000         | mov eax,dword ptr ss:[esp+47C]      |
6F657C2B  | 8B9424 80040000         | mov edx,dword ptr ss:[esp+480]      |
6F657C32  | 894424 10               | mov dword ptr ss:[esp+10],eax       |
6F657C36  | 8B8424 84040000         | mov eax,dword ptr ss:[esp+484]      |
6F657C3D  | 53                      | push ebx                            |
6F657C3E  | 55                      | push ebp                            |
6F657C3F  | 8BAC24 A0040000         | mov ebp,dword ptr ss:[esp+4A0]      |
6F657C46  | 85ED                    | test ebp,ebp                        |
6F657C48  | 895424 1C               | mov dword ptr ss:[esp+1C],edx       |
6F657C4C  | 8B9424 90040000         | mov edx,dword ptr ss:[esp+490]      |
6F657C53  | 894424 20               | mov dword ptr ss:[esp+20],eax       |
6F657C57  | 8B8424 94040000         | mov eax,dword ptr ss:[esp+494]      |
6F657C5E  | 56                      | push esi                            | <--- FileInfo->NetClient->Net
6F657C5F  | 895424 28               | mov dword ptr ss:[esp+28],edx       |
6F657C63  | 8B9424 A0040000         | mov edx,dword ptr ss:[esp+4A0]      |
6F657C6A  | 894424 2C               | mov dword ptr ss:[esp+2C],eax       |
6F657C6E  | 8B8424 9C040000         | mov eax,dword ptr ss:[esp+49C]      |
6F657C75  | 57                      | push edi                            |
6F657C76  | 8BBC24 84040000         | mov edi,dword ptr ss:[esp+484]      | <--- Maps\\Download\\DotA v6.83d.w3x
6F657C7D  | 894C24 1C               | mov dword ptr ss:[esp+1C],ecx       | <--- NetProviderBNET: 内存数据在下面
6F657C81  | 894424 14               | mov dword ptr ss:[esp+14],eax       |
6F657C85  | 895424 18               | mov dword ptr ss:[esp+18],edx       |
6F657C89  | C700 00000000           | mov dword ptr ds:[eax],0            |
6F657C8F  | 74 0E                   | je game.6F657C9F                    | <--- 跳转不会执行
6F657C91  | 83BC24 AC040000 00      | cmp dword ptr ss:[esp+4AC],0        | <--- 0!=104
6F657C99  | 74 04                   | je game.6F657C9F                    | <--- 跳转不会执行
6F657C9B  | C645 00 00              | mov byte ptr ss:[ebp],0             |
6F657C9F  | 8B81 EC050000           | mov eax,dword ptr ds:[ecx+5EC]      |
6F657CA5  | 8B99 E8050000           | mov ebx,dword ptr ds:[ecx+5E8]      |
6F657CAB  | BA 04010000             | mov edx,104                         |
6F657CB0  | 8D8C24 74020000         | lea ecx,dword ptr ss:[esp+274]      |
6F657CB7  | 894424 10               | mov dword ptr ss:[esp+10],eax       |
6F657CBB  | E8 40E30600             | call game.6F6C6000                  |
6F657CC0  | 8D8C24 74020000         | lea ecx,dword ptr ss:[esp+274]      | <--- 安装目录（D:\\我的游戏\\iCCup Warcraft III\\）
6F657CC7  | 51                      | push ecx                            |
6F657CC8  | E8 99390900             | call <JMP.&Ordinal#506>             |
6F657CCD  | 8BF0                    | mov esi,eax                         | <--- eax=1F
6F657CCF  | BA 04010000             | mov edx,104                         | <--- edx=104
6F657CD4  | 2BD6                    | sub edx,esi                         |
6F657CD6  | 52                      | push edx                            | <--- edx=E5
6F657CD7  | 57                      | push edi                            | <--- Maps\\Download\\DotA v6.83d.w3x
6F657CD8  | 8D8434 7C020000         | lea eax,dword ptr ss:[esp+esi+27C]  |
6F657CDF  | 50                      | push eax                            |
6F657CE0  | E8 DF380900             | call <JMP.&Ordinal#501>             | <--- 通过 SStrPrintf (Storm.dll Ordinal #501) 拼接成完整的本地绝对路径
6F657CE5  | 8B8C24 AC040000         | mov ecx,dword ptr ss:[esp+4AC]      |
6F657CEC  | 8B5424 18               | mov edx,dword ptr ss:[esp+18]       |
6F657CF0  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F657CF4  | 51                      | push ecx                            |
6F657CF5  | 8B4C24 14               | mov ecx,dword ptr ss:[esp+14]       |
6F657CF9  | 55                      | push ebp                            |
6F657CFA  | 52                      | push edx                            |
6F657CFB  | 8B5424 2C               | mov edx,dword ptr ss:[esp+2C]       |
6F657CFF  | 50                      | push eax                            |
6F657D00  | 51                      | push ecx                            |
6F657D01  | 8B4C24 38               | mov ecx,dword ptr ss:[esp+38]       |
6F657D05  | 83EC 14                 | sub esp,14                          |
6F657D08  | 8BC4                    | mov eax,esp                         |
6F657D0A  | 8910                    | mov dword ptr ds:[eax],edx          |
6F657D0C  | 8B5424 50               | mov edx,dword ptr ss:[esp+50]       |
6F657D10  | 8948 04                 | mov dword ptr ds:[eax+4],ecx        |
6F657D13  | 8B4C24 54               | mov ecx,dword ptr ss:[esp+54]       |
6F657D17  | 8950 08                 | mov dword ptr ds:[eax+8],edx        |
6F657D1A  | 8B5424 58               | mov edx,dword ptr ss:[esp+58]       |
6F657D1E  | 8948 0C                 | mov dword ptr ds:[eax+C],ecx        |
6F657D21  | 8950 10                 | mov dword ptr ds:[eax+10],edx       |
6F657D24  | 8B9424 B0040000         | mov edx,dword ptr ss:[esp+4B0]      |
6F657D2B  | 53                      | push ebx                            |
6F657D2C  | 8BCF                    | mov ecx,edi                         | <--- 地图路径（Maps\\Download\\DotA v6.83d.w3x）
6F657D2E  | E8 FDFBFFFF             | call game.6F657930                  | <--- 关键函数
6F657D33  | 85C0                    | test eax,eax                        | <--- eax=0 -> 失败
6F657D35  | 74 0A                   | je game.6F657D41                    |
6F657D37  | B8 01000000             | mov eax,1                           |
6F657D3C  | E9 1F010000             | jmp game.6F657E60                   |
6F657D41  | 8B4C24 20               | mov ecx,dword ptr ss:[esp+20]       |
6F657D45  | 8B8424 88040000         | mov eax,dword ptr ss:[esp+488]      |
6F657D4C  | 8B5424 24               | mov edx,dword ptr ss:[esp+24]       |
6F657D50  | 894C24 40               | mov dword ptr ss:[esp+40],ecx       |
6F657D54  | 8B4C24 2C               | mov ecx,dword ptr ss:[esp+2C]       |
6F657D58  | 894424 34               | mov dword ptr ss:[esp+34],eax       |
6F657D5C  | 8B4424 28               | mov eax,dword ptr ss:[esp+28]       |
6F657D60  | 895424 44               | mov dword ptr ss:[esp+44],edx       |
6F657D64  | 8B5424 30               | mov edx,dword ptr ss:[esp+30]       |
6F657D68  | 894C24 4C               | mov dword ptr ss:[esp+4C],ecx       |
6F657D6C  | 8B8C24 AC040000         | mov ecx,dword ptr ss:[esp+4AC]      |
6F657D73  | 895C24 38               | mov dword ptr ss:[esp+38],ebx       |
6F657D77  | 894424 48               | mov dword ptr ss:[esp+48],eax       |
6F657D7B  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F657D7F  | 895424 50               | mov dword ptr ss:[esp+50],edx       |
6F657D83  | 33DB                    | xor ebx,ebx                         |
6F657D85  | 894C24 60               | mov dword ptr ss:[esp+60],ecx       |
6F657D89  | 68 04010000             | push 104                            |
6F657D8E  | 8D5424 68               | lea edx,dword ptr ss:[esp+68]       |
6F657D92  | 8BCF                    | mov ecx,edi                         |
6F657D94  | 894424 40               | mov dword ptr ss:[esp+40],eax       |
6F657D98  | 895C24 58               | mov dword ptr ss:[esp+58],ebx       |
6F657D9C  | 895C24 5C               | mov dword ptr ss:[esp+5C],ebx       |
6F657DA0  | 896C24 60               | mov dword ptr ss:[esp+60],ebp       |
6F657DA4  | 89B424 70020000         | mov dword ptr ss:[esp+270],esi      |
6F657DAB  | E8 50DC0600             | call game.6F6C5A00                  |
6F657DB0  | 83C6 01                 | add esi,1                           |
6F657DB3  | 81FE 04010000           | cmp esi,104                         |
6F657DB9  | 76 05                   | jbe game.6F657DC0                   |
6F657DBB  | BE 04010000             | mov esi,104                         |
6F657DC0  | 56                      | push esi                            |
6F657DC1  | 8D9424 78020000         | lea edx,dword ptr ss:[esp+278]      |
6F657DC8  | 52                      | push edx                            |
6F657DC9  | 8D8424 70010000         | lea eax,dword ptr ss:[esp+170]      |
6F657DD0  | 50                      | push eax                            |
6F657DD1  | E8 EE370900             | call <JMP.&Ordinal#501>             |
6F657DD6  | 8B7C24 1C               | mov edi,dword ptr ss:[esp+1C]       |
6F657DDA  | 8DAF C2020000           | lea ebp,dword ptr ds:[edi+2C2]      |
6F657DE0  | 8BCD                    | mov ecx,ebp                         |
6F657DE2  | 898424 70020000         | mov dword ptr ss:[esp+270],eax      |
6F657DE9  | 33F6                    | xor esi,esi                         |
6F657DEB  | E8 B0050800             | call game.6F6D83A0                  |
6F657DF0  | 8D87 DE030000           | lea eax,dword ptr ds:[edi+3DE]      |
6F657DF6  | 3BC3                    | cmp eax,ebx                         |
6F657DF8  | 894424 10               | mov dword ptr ss:[esp+10],eax       |
6F657DFC  | 74 41                   | je game.6F657E3F                    |
6F657DFE  | 8BFF                    | mov edi,edi                         |
6F657E00  | 3818                    | cmp byte ptr ds:[eax],bl            |
6F657E02  | 74 3B                   | je game.6F657E3F                    |
6F657E04  | 3BF3                    | cmp esi,ebx                         |
6F657E06  | 75 37                   | jne game.6F657E3F                   |
6F657E08  | 53                      | push ebx                            |
6F657E09  | 68 8810976F             | push game.6F971088                  |
6F657E0E  | 68 04010000             | push 104                            |
6F657E13  | 8D8C24 84030000         | lea ecx,dword ptr ss:[esp+384]      |
6F657E1A  | 51                      | push ecx                            |
6F657E1B  | 8D5424 20               | lea edx,dword ptr ss:[esp+20]       |
6F657E1F  | 52                      | push edx                            |
6F657E20  | E8 E1370900             | call <JMP.&Ordinal#504>             |
6F657E25  | 8D9424 78030000         | lea edx,dword ptr ss:[esp+378]      |
6F657E2C  | 8D4C24 34               | lea ecx,dword ptr ss:[esp+34]       |
6F657E30  | E8 3BFDFFFF             | call game.6F657B70                  |
6F657E35  | 8BF0                    | mov esi,eax                         |
6F657E37  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F657E3B  | 3BC3                    | cmp eax,ebx                         |
6F657E3D  | 75 C1                   | jne game.6F657E00                   |
6F657E3F  | 8BCD                    | mov ecx,ebp                         |
6F657E41  | E8 6A050800             | call game.6F6D83B0                  |
6F657E46  | 3BF3                    | cmp esi,ebx                         |
6F657E48  | 74 14                   | je game.6F657E5E                    |
6F657E4A  | 8B4424 54               | mov eax,dword ptr ss:[esp+54]       |
6F657E4E  | 8B4C24 14               | mov ecx,dword ptr ss:[esp+14]       |
6F657E52  | 8B5424 58               | mov edx,dword ptr ss:[esp+58]       |
6F657E56  | 8901                    | mov dword ptr ds:[ecx],eax          |
6F657E58  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F657E5C  | 8910                    | mov dword ptr ds:[eax],edx          |
6F657E5E  | 8BC6                    | mov eax,esi                         |
6F657E60  | 8B8C24 7C040000         | mov ecx,dword ptr ss:[esp+47C]      |
6F657E67  | 5F                      | pop edi                             |
6F657E68  | 5E                      | pop esi                             |
6F657E69  | 5D                      | pop ebp                             |
6F657E6A  | 5B                      | pop ebx                             |
6F657E6B  | 33CC                    | xor ecx,esp                         |
6F657E6D  | E8 E7911800             | call game.6F7E1059                  |
6F657E72  | 81C4 70040000           | add esp,470                         |
6F657E78  | C2 2C00                 | ret 2C                              |
```

内存示例1

```

0216006C  654E5C2E  .\Ne
02160070  6F725074  tPro
02160074  65646976  vide
02160078  454E4272  rBNE
0216007C  70632E54  T.cp
02160080  00000070  p...
02160084  00000000  ....
02160088  000407E8  è...
0216008C  6F6D0216  ..mo
02160090  6F9711F4  ô..o
02160094  6FACEC10  .ì¬o
02160098  905313EF  ï.S.
0216009C  00000002  ....
021600A0  00000001  ....
021600A4  00000001  ....
021600A8  424C495A  ZILB
021600AC  49583836  68XI
021600B0  57335850  PX3W
021600B4  57335850  PX3W
021600B8  0000001A  ....
021600BC  656E5553  SUne
021600C0  FFFFFE20   þÿÿ
021600C4  00000804  ....
021600C8  00000804  ....
021600CC  004E4843  CHN.
021600D0  6E696843  Chin
021600D4  6F5C0061  a.\o
021600D8  1A2A0230  0.*.
021600DC  6F5CD3A4  ¤Ó\o
021600E0  00000000  ....
021600E4  04F2C180  .Áò.
021600E8  04F2C180  .Áò.
021600EC  1FC7B950  P¹Ç.
021600F0  6F5CD4C9  ÉÔ\o
021600F4  172E1D04  ....
021600F8  00000000  ....
021600FC  00000010  ....
02160100  00000000  ....
02160104  1FC7B950  P¹Ç.
02160108  6F5FF511  .õ_o
0216010C  172E1D04  ....
02160110  1FC7B950  P¹Ç.
02160114  6F5FF592  .õ_o
02160118  1FC7B950  P¹Ç.
0216011C  0000000C  ....
02160120  00000000  ....
02160124  00000006  ....
02160128  00000000  ....
0216012C  6F5FF76B  k÷_o
02160130  00000008  ....
02160134  1FC7B950  P¹Ç.
02160138  6F600829  ).`o
0216013C  1A2A0230  0.*.
02160140  00000001  ....
02160144  00000001  ....
02160148  172E1354  T...
0216014C  6F4C1B3D  =.Lo
02160150  172E1354  T...
02160154  00000000  ....
02160158  00000010  ....
0216015C  00000000  ....
02160160  005E3218  .2^.
02160164  6F5FF511  .õ_o
02160168  172E1354  T...
0216016C  005E3218  .2^.
02160170  6F5FF592  .õ_o
02160174  005E3218  .2^.
02160178  0000000C  ....
0216017C  00000000  ....
02160180  00000007  ....
02160184  15025A24  $Z..
02160188  0019F7C8  È÷..
0216018C  0019F7D0  Ð÷..
02160190  00000265  e...
02160194  020B0000  ....
02160198  00000016  ....
0216019C  0019F81C  .ø..
021601A0  00000020   ...
021601A4  00000000  ....
021601A8  00000002  ....
021601AC  00000000  ....
021601B0  020B0558  X...
021601B4  020B0000  ....
021601B8  0019F81C  .ø..
021601BC  15025DE2  â]..
021601C0  00000265  e...
021601C4  020B0000  ....
021601C8  66AAA913  .©ªf
021601CC  66AAA913  .©ªf
021601D0  00000002  ....
021601D4  00000002  ....
021601D8  020A0130  0...
021601DC  00000000  ....
021601E0  6E656B61  aken
021601E4  00737365  ess.
021601E8  0019F82C  ,ø..
021601EC  00008000  ....
021601F0  15055580  .U..
021601F4  0019F834  4ø..
021601F8  00000210  ....
021601FC  02350000  ..5.
02160200  150563E0  àc..
02160204  00010000  ....
02160208  6F6294F7  ÷.bo
0216020C  0019F840  @ø..
02160210  00000000  ....
02160214  77992D8C  .-.w
02160218  6F6294F7  ÷.bo
0216021C  0019F850  Pø..
02160220  0000000C  ....
02160224  63726157  Warc
02160228  74666172  raft
0216022C  49494920   III
02160230  1FC7BA00  .ºÇ.
02160234  3E99999A  ...>
02160238  1FC7BA04  .ºÇ.
0216023C  6F605405  .T`o
02160240  7F800000  ....
02160244  63726157  Warc
02160248  74666172  raft
0216024C  49494920   III
02160250  6552203A  : Re
02160254  206E6769  ign 
02160258  4320666F  of C
0216025C  736F6168  haos
02160260  6F605700  .W`o
02160264  7F800000  ....
02160268  7F800000  ....
0216026C  BE99999A  ...¾
02160270  1FC7BA04  .ºÇ.
02160274  7F800000  ....
02160278  6F60580A  .X`o
0216027C  1FC7BA04  .ºÇ.
02160280  1FC7BA04  .ºÇ.
02160284  7F800000  ....
02160288  6F605912  .Y`o
0216028C  0019F8BC  ¼ø..
02160290  1FC7B950  P¹Ç.
02160294  00000000  ....
02160298  7F800000  ....
0216029C  00000000  ....
021602A0  00000000  ....
021602A4  1FC7B950  P¹Ç.
021602A8  0019F8F4  ôø..
021602AC  6F611E14  ..ao
021602B0  00000001  ....
021602B4  179508AC  ¬...
021602B8  020200B0  °...
021602BC  14F40374  t.ô.
021602C0  6F57DE1C  .ÞWo
021602C4  0019F8F4  ôø..
021602C8  6F57DE2B  +ÞWo
021602CC  74696E49  Init
021602D0  69746169  iati
021602D4  6320676E  ng c
021602D8  656E6E6F  onne
021602DC  6F697463  ctio
021602E0  6F74206E  n to
021602E4  74614220   Bat
021602E8  2E656C74  tle.
021602EC  2E74656E  net.
021602F0  00002E2E  ....
021602F4  15026344  Dc..
021602F8  0019F950  Pù..
021602FC  14F60404  ..ö.
02160300  00000000  ....
02160304  6F62A909  .©bo
02160308  00000000  ....
0216030C  6FA9B370  p³©o
02160310  FFFFFFFE  þÿÿÿ
02160314  400900C8  È..@
02160318  0000001B  ....
0216031C  14F61394  ..ö.
02160320  14F40374  t.ô.
02160324  14F61064  d.ö.
02160328  14F4010C  ..ô.
0216032C  00000000  ....
02160330  00000001  ....
02160334  6F53F380  .óSo
02160338  400900C8  È..@
0216033C  0000001B  ....
02160340  020200B0  °...
02160344  020C0090  ....
02160348  00000000  ....
0216034C  00000000  ....
02160350  FFFF17E0  à.ÿÿ
02160354  FFFFFFFF  ÿÿÿÿ
02160358  0000FFFF  ÿÿ..
0216035C  00000000  ....
02160360  00000000  ....
02160364  07D00000  ..Ð.
02160368  772E0200  ...w
0216036C  2E3B6D33  3m;.
02160370  00783377  w3x.
02160374  00000000  ....

```

```assembly
6F657930  | 83EC 44                 | sub esp,44                          |
6F657933  | A1 40E1AA6F             | mov eax,dword ptr ds:[6FAAE140]     |
6F657938  | 33C4                    | xor eax,esp                         |
6F65793A  | 894424 40               | mov dword ptr ss:[esp+40],eax       |
6F65793E  | 8B4424 48               | mov eax,dword ptr ss:[esp+48]       |
6F657942  | 53                      | push ebx                            |
6F657943  | 56                      | push esi                            |
6F657944  | 57                      | push edi                            |
6F657945  | 8BF9                    | mov edi,ecx                         |
6F657947  | 8B4C24 58               | mov ecx,dword ptr ss:[esp+58]       |
6F65794B  | 8B5C24 70               | mov ebx,dword ptr ss:[esp+70]       |
6F65794F  | 894C24 24               | mov dword ptr ss:[esp+24],ecx       |
6F657953  | 8B4C24 60               | mov ecx,dword ptr ss:[esp+60]       |
6F657957  | 8BF2                    | mov esi,edx                         |
6F657959  | 8B5424 5C               | mov edx,dword ptr ss:[esp+5C]       |
6F65795D  | 894C24 2C               | mov dword ptr ss:[esp+2C],ecx       |
6F657961  | 8B4C24 68               | mov ecx,dword ptr ss:[esp+68]       |
6F657965  | 895424 28               | mov dword ptr ss:[esp+28],edx       |
6F657969  | 8B5424 64               | mov edx,dword ptr ss:[esp+64]       |
6F65796D  | 894C24 34               | mov dword ptr ss:[esp+34],ecx       |
6F657971  | 8B4C24 6C               | mov ecx,dword ptr ss:[esp+6C]       |
6F657975  | 51                      | push ecx                            |
6F657976  | 895424 34               | mov dword ptr ss:[esp+34],edx       |
6F65797A  | 8B5424 78               | mov edx,dword ptr ss:[esp+78]       |
6F65797E  | 895424 20               | mov dword ptr ss:[esp+20],edx       |
6F657982  | 8B5424 7C               | mov edx,dword ptr ss:[esp+7C]       |
6F657986  | 8D4C24 3C               | lea ecx,dword ptr ss:[esp+3C]       |
6F65798A  | 51                      | push ecx                            |
6F65798B  | 50                      | push eax                            |
6F65798C  | 895424 20               | mov dword ptr ss:[esp+20],edx       |
6F657990  | 8D5424 24               | lea edx,dword ptr ss:[esp+24]       |
6F657994  | 52                      | push edx                            |
6F657995  | 8D4424 30               | lea eax,dword ptr ss:[esp+30]       |
6F657999  | 50                      | push eax                            |
6F65799A  | 8D4C24 20               | lea ecx,dword ptr ss:[esp+20]       |
6F65799E  | 51                      | push ecx                            |
6F65799F  | 8D5424 28               | lea edx,dword ptr ss:[esp+28]       |
6F6579A3  | 8BCF                    | mov ecx,edi                         | <--- Maps\\Download\\DotA v6.83d.w3x
6F6579A5  | E8 C6FDFFFF             | call game.6F657770                  | <--- 本地CRC和Sha1计算
6F6579AA  | 83F8 01                 | cmp eax,1                           | <--- eax=1
6F6579AD  | 0F85 BE000000           | jne game.6F657A71                   | <--- 跳转不会执行
6F6579B3  | 397424 18               | cmp dword ptr ss:[esp+18],esi       | <--- esi=3AEBCEF3（对比 CRC）
6F6579B7  | 0F85 B4000000           | jne game.6F657A71                   | <--- 跳转不会执行（结果: Ok）
6F6579BD  | B8 14000000             | mov eax,14                          |
6F6579C2  | 8D4C24 24               | lea ecx,dword ptr ss:[esp+24]       |
6F6579C6  | 8D5424 38               | lea edx,dword ptr ss:[esp+38]       |
6F6579CA  | 55                      | push ebp                            |
6F6579CB  | EB 03                   | jmp game.6F6579D0                   |
6F6579CD  | 8D49 00                 | lea ecx,dword ptr ds:[ecx]          |
6F6579D0  | 8B32                    | mov esi,dword ptr ds:[edx]          | <--- 读取远程服务端 SHA1 的前 4 字节
6F6579D2  | 3B31                    | cmp esi,dword ptr ds:[ecx]          | <--- 对比本地客户端 SHA1 的前 4 字节
6F6579D4  | 75 12                   | jne game.6F6579E8                   | <--- 跳转将要执行（结果: Fail）
6F6579D6  | 83E8 04                 | sub eax,4                           | 
6F6579D9  | 83C1 04                 | add ecx,4                           | 
6F6579DC  | 83C2 04                 | add edx,4                           | 
6F6579DF  | 83F8 04                 | cmp eax,4                           | 
6F6579E2  | 73 EC                   | jae game.6F6579D0                   | 
6F6579E4  | 85C0                    | test eax,eax                        | 
6F6579E6  | 74 5D                   | je game.6F657A45                    | 
6F6579E8  | 0FB632                  | movzx esi,byte ptr ds:[edx]         | <--- byte ptr ds:[edx]=[0019F734]=90
6F6579EB  | 0FB629                  | movzx ebp,byte ptr ds:[ecx]         | <--- byte ptr ds:[ecx]=[0019F720]=2E
6F6579EE  | 2BF5                    | sub esi,ebp                         | <--- esi=90, ebp=2E
6F6579F0  | 75 45                   | jne game.6F657A37                   | <--- 跳转将要执行
6F6579F2  | 83E8 01                 | sub eax,1                           |
6F6579F5  | 83C1 01                 | add ecx,1                           |
6F6579F8  | 83C2 01                 | add edx,1                           |
6F6579FB  | 85C0                    | test eax,eax                        |
6F6579FD  | 74 46                   | je game.6F657A45                    |
6F6579FF  | 0FB632                  | movzx esi,byte ptr ds:[edx]         |
6F657A02  | 0FB629                  | movzx ebp,byte ptr ds:[ecx]         |
6F657A05  | 2BF5                    | sub esi,ebp                         |
6F657A07  | 75 2E                   | jne game.6F657A37                   |
6F657A09  | 83E8 01                 | sub eax,1                           |
6F657A0C  | 83C1 01                 | add ecx,1                           |
6F657A0F  | 83C2 01                 | add edx,1                           |
6F657A12  | 85C0                    | test eax,eax                        |
6F657A14  | 74 2F                   | je game.6F657A45                    |
6F657A16  | 0FB632                  | movzx esi,byte ptr ds:[edx]         |
6F657A19  | 0FB629                  | movzx ebp,byte ptr ds:[ecx]         |
6F657A1C  | 2BF5                    | sub esi,ebp                         |
6F657A1E  | 75 17                   | jne game.6F657A37                   |
6F657A20  | 83E8 01                 | sub eax,1                           |
6F657A23  | 83C1 01                 | add ecx,1                           |
6F657A26  | 83C2 01                 | add edx,1                           |
6F657A29  | 85C0                    | test eax,eax                        |
6F657A2B  | 74 18                   | je game.6F657A45                    |
6F657A2D  | 0FB632                  | movzx esi,byte ptr ds:[edx]         |
6F657A30  | 0FB611                  | movzx edx,byte ptr ds:[ecx]         |
6F657A33  | 2BF2                    | sub esi,edx                         |
6F657A35  | 74 0E                   | je game.6F657A45                    |
6F657A37  | 85F6                    | test esi,esi                        | <--- esi=62
6F657A39  | B8 01000000             | mov eax,1                           |
6F657A3E  | 7F 07                   | jg game.6F657A47                    | <--- 跳转将要执行
6F657A40  | 83C8 FF                 | or eax,FFFFFFFF                     |
6F657A43  | EB 02                   | jmp game.6F657A47                   |
6F657A45  | 33C0                    | xor eax,eax                         |
6F657A47  | 85C0                    | test eax,eax                        | <--- eax=1
6F657A49  | 5D                      | pop ebp                             | <--- ebp=2E
6F657A4A  | 75 25                   | jne game.6F657A71                   | <--- 跳转将要执行
6F657A4C  | 8B4C24 14               | mov ecx,dword ptr ss:[esp+14]       |
6F657A50  | 8D70 01                 | lea esi,dword ptr ds:[eax+1]        |
6F657A53  | 8B4424 7C               | mov eax,dword ptr ss:[esp+7C]       |
6F657A57  | 50                      | push eax                            |
6F657A58  | 57                      | push edi                            |
6F657A59  | 51                      | push ecx                            |
6F657A5A  | E8 653B0900             | call <JMP.&Ordinal#501>             |
6F657A5F  | 8B5424 10               | mov edx,dword ptr ss:[esp+10]       |
6F657A63  | 8B4424 0C               | mov eax,dword ptr ss:[esp+C]        |
6F657A67  | 8B4C24 1C               | mov ecx,dword ptr ss:[esp+1C]       |
6F657A6B  | 8913                    | mov dword ptr ds:[ebx],edx          |
6F657A6D  | 8901                    | mov dword ptr ds:[ecx],eax          |
6F657A6F  | EB 0F                   | jmp game.6F657A80                   |
6F657A71  | 8D5424 0C               | lea edx,dword ptr ss:[esp+C]        | <--- dword ptr ss:[esp+0C]=[0019F708]=007E9836
6F657A75  | 8D4C24 10               | lea ecx,dword ptr ss:[esp+10]       | <--- dword ptr ss:[esp+10]=[0019F70C]=21A90010 "HM3W"
6F657A79  | 33F6                    | xor esi,esi                         | <--- esi=62
6F657A7B  | E8 80F3FFFF             | call game.6F656E00                  |
6F657A80  | 8B4C24 4C               | mov ecx,dword ptr ss:[esp+4C]       | <--- dword ptr ss:[esp+4C]=[0019F748]=7E47A5FD
6F657A84  | 5F                      | pop edi                             | <--- edi=180200A4 "Maps\\Download\\DotA v6.83d.w3x"
6F657A85  | 8BC6                    | mov eax,esi                         | <--- esi=0
6F657A87  | 5E                      | pop esi                             | <--- esi=0
6F657A88  | 5B                      | pop ebx                             | <--- ebx=180202D0
6F657A89  | 33CC                    | xor ecx,esp                         |
6F657A8B  | E8 C9951800             | call game.6F7E1059                  |
6F657A90  | 83C4 44                 | add esp,44                          |
6F657A93  | C2 2C00                 | ret 2C                              |
```

内存示列2

```
180202A8  00000000

180202AC  007E9836
180202B0  007E9836
180202B4  91FA5A45
180202B8  3AEBCEF3 -> CRC
180202BC  0AA52F2E
180202C0  E3FF9B37
180202C4  E74C5752
180202C8  A7B9F545
180202CC  5FA4DF1A
```
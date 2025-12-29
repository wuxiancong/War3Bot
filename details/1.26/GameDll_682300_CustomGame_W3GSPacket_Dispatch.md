# Warcraft III 网络包处理跳转表 (Packet ID -> Handler)

## 概述

- 这是基于 War3 1.26a (Game.dll) 反汇编代码整理的 网络包分发跳转表，此函数 (6F682300) 是网络层的核心分发器（Dispatcher）。它根据计算出的索引值 (ESI)，查表跳转到不同的处理分支。

**环境:**
- **客户端:** Warcraft III (1.26a)
- **模块:** `game.dll`
- **基地址:** `6F000000`

---

**网络包分发跳转表 (Game.dll + 682300) 详解**

**地址**: `6F682300`
**偏移**: `game.dll + 682300`

```assembly
6F682300  | 56                      | push esi                            |
6F682301  | 0FB67424 0C             | movzx esi,byte ptr ss:[esp+C]       |
6F682306  | 83C6 FF                 | add esi,FFFFFFFF                    |
6F682309  | 83FE 50                 | cmp esi,50                          |
6F68230C  | B8 01000000             | mov eax,1                           |
6F682311  | 0F87 D2030000           | ja game.6F6826E9                    |
6F682317  | 0FB6B6 8827686F         | movzx esi,byte ptr ds:[esi+6F682788 |
6F68231E  | FF24B5 F026686F         | jmp dword ptr ds:[esi*4+6F6826F0]   |
6F682325  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
; ===========================================================================
; ESI = 0x00 | Packet ID: 0x01
; ===========================================================================
6F682325  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=00
6F682329  | 50                      | push eax                            |
6F68232A  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68232E  | 50                      | push eax                            |
6F68232F  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682333  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682335  | 50                      | push eax                            |
6F682336  | E8 4587FFFF             | call game.6F67AA80                  | 0x01 -> W3GS_PING_FROM_HOST (主机Ping)
6F68233B  | 5E                      | pop esi                             |
6F68233C  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x01 | Packet ID: 0x04
; ===========================================================================
6F68233F  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=01
6F682343  | 50                      | push eax                            |
6F682344  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682348  | 50                      | push eax                            |
6F682349  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68234D  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F68234F  | 50                      | push eax                            |
6F682350  | E8 DBAAFFFF             | call game.6F67CE30                  | 0x04 -> W3GS_SLOTINFOJOIN (加入成功/握手)
6F682355  | 5E                      | pop esi                             |
6F682356  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x02 | Packet ID: 0x05
; ===========================================================================
6F682359  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=02
6F68235D  | 50                      | push eax                            |
6F68235E  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682362  | 50                      | push eax                            |
6F682363  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682367  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682369  | 50                      | push eax                            |
6F68236A  | E8 3114FFFF             | call game.6F6737A0                  | 0x05 -> W3GS_REJECTJOIN (拒绝加入/房间满)
6F68236F  | 5E                      | pop esi                             |
6F682370  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x03 | Packet ID: 0x06
; ===========================================================================
6F682373  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=03
6F682377  | 50                      | push eax                            |
6F682378  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68237C  | 50                      | push eax                            |
6F68237D  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682381  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682383  | 50                      | push eax                            |
6F682384  | E8 B7ACFFFF             | call game.6F67D040                  | 0x06 -> W3GS_PLAYERINFO (玩家信息更新)
6F682389  | 5E                      | pop esi                             |
6F68238A  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x04 | Packet ID: 0x07
; ===========================================================================
6F68238D  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=04
6F682391  | 50                      | push eax                            |
6F682392  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682396  | 50                      | push eax                            |
6F682397  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68239B  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F68239D  | 50                      | push eax                            |
6F68239E  | E8 8D87FFFF             | call game.6F67AB30                  | 0x07 -> W3GS_PLAYERLEAVE (玩家离开)
6F6823A3  | 5E                      | pop esi                             |
6F6823A4  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x05 | Packet ID: 0x08
; ===========================================================================
6F6823A7  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=05
6F6823AB  | 50                      | push eax                            |
6F6823AC  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6823B0  | 50                      | push eax                            |
6F6823B1  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6823B5  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6823B7  | 50                      | push eax                            |
6F6823B8  | E8 13F4FFFF             | call game.6F6817D0                  | 0x08 -> W3GS_GAMELOADED (玩家加载完毕)
6F6823BD  | 5E                      | pop esi                             |
6F6823BE  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x06 | Packet ID: 0x09
; ===========================================================================
6F6823C1  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=06
6F6823C5  | 50                      | push eax                            |
6F6823C6  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6823CA  | 50                      | push eax                            |
6F6823CB  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6823CF  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6823D1  | 50                      | push eax                            |
6F6823D2  | E8 29E2FFFF             | call game.6F680600                  | 0x09 -> W3GS_SLOTINFO (槽位信息/UI刷新)
6F6823D7  | 5E                      | pop esi                             |
6F6823D8  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x07 | Packet ID: 0x0A
; ===========================================================================
6F6823DB  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=07
6F6823DF  | 50                      | push eax                            |
6F6823E0  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6823E4  | 50                      | push eax                            |
6F6823E5  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6823E9  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6823EB  | 50                      | push eax                            |
6F6823EC  | E8 6F88FFFF             | call game.6F67AC60                  | 0x0A -> W3GS_COUNTDOWN_START (倒计时开始)
6F6823F1  | 5E                      | pop esi                             |
6F6823F2  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x08 | Packet ID: 0x0B
; ===========================================================================
6F6823F5  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=08
6F6823F9  | 50                      | push eax                            |
6F6823FA  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6823FE  | 50                      | push eax                            |
6F6823FF  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682403  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682405  | 50                      | push eax                            |
6F682406  | E8 8589FFFF             | call game.6F67AD90                  | 0x0B -> W3GS_COUNTDOWN_END (倒计时结束)
6F68240B  | 5E                      | pop esi                             |
6F68240C  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x09 | Packet ID: 0x0C
; ===========================================================================
6F68240F  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=09
6F682413  | 50                      | push eax                            |
6F682414  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682418  | 50                      | push eax                            |
6F682419  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68241D  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F68241F  | 50                      | push eax                            |
6F682420  | E8 4B8AFFFF             | call game.6F67AE70                  | 0x0C -> W3GS_INCOMING_ACTION (游戏动作)
6F682425  | 5E                      | pop esi                             |
6F682426  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x0A | Packet ID: 0x0D
; ===========================================================================
6F682429  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=0A
6F68242D  | 50                      | push eax                            |
6F68242E  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682432  | 50                      | push eax                            |
6F682433  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682437  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682439  | 50                      | push eax                            |
6F68243A  | E8 E1E1FFFF             | call game.6F680620                  | 0x0D -> W3GS_CHAT_FROM_HOST (聊天消息)
6F68243F  | 5E                      | pop esi                             |
6F682440  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x0B | Packet ID: 0x0E
; ===========================================================================
6F682443  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=0B
6F682447  | 50                      | push eax                            |
6F682448  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68244C  | 50                      | push eax                            |
6F68244D  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682451  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682453  | 50                      | push eax                            |
6F682454  | E8 57C4FFFF             | call game.6F67E8B0                  | 0x0E -> W3GS_LEAVEREQ (离开请求)
6F682459  | 5E                      | pop esi                             |
6F68245A  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x0C | Packet ID: 0x0F
; ===========================================================================
6F68245D  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=0C
6F682461  | 50                      | push eax                            |
6F682462  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682466  | 50                      | push eax                            |
6F682467  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68246B  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F68246D  | 50                      | push eax                            |
6F68246E  | E8 5DC4FFFF             | call game.6F67E8D0                  | 0x0F -> W3GS_START_DOWNLOAD (开始下载)
6F682473  | 5E                      | pop esi                             |
6F682474  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x0D | Packet ID: 0x1B
; ===========================================================================
6F682477  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=0D
6F68247B  | 50                      | push eax                            |
6F68247C  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682480  | 50                      | push eax                            |
6F682481  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682485  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682487  | 50                      | push eax                            |
6F682488  | E8 A38AFFFF             | call game.6F67AF30                  | 0x1B -> W3GS_PONG_TO_HOST (TCP Pong)
6F68248D  | 5E                      | pop esi                             |
6F68248E  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x0E | Packet ID: 0x1C
; ===========================================================================
6F682491  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       | esi=0E
6F682495  | 50                      | push eax                            |
6F682496  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F68249A  | 50                      | push eax                            |
6F68249B  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F68249F  | 50                      | push eax                            |
6F6824A0  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6824A4  | 50                      | push eax                            |
6F6824A5  | E8 96EAFFFF             | call game.6F680F40                  | 0x1C -> W3GS_INCOMING_ACTION2 (动作包2)
6F6824AA  | 5E                      | pop esi                             |
6F6824AB  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x0F | Packet ID: 0x23
; ===========================================================================
6F6824AE  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=0F
6F6824B2  | 50                      | push eax                            |
6F6824B3  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6824B7  | 50                      | push eax                            |
6F6824B8  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6824BC  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6824BE  | 50                      | push eax                            |
6F6824BF  | E8 2C8BFFFF             | call game.6F67AFF0                  | 0x23 -> W3GS_GAMELOADED_OTHERS
6F6824C4  | 5E                      | pop esi                             |
6F6824C5  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x10 | Packet ID: 0x25
; ===========================================================================
6F6824C8  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=10
6F6824CC  | 50                      | push eax                            |
6F6824CD  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6824D1  | 50                      | push eax                            |
6F6824D2  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6824D6  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6824D8  | 50                      | push eax                            |
6F6824D9  | E8 92ADFFFF             | call game.6F67D270                  | 0x25 -> W3GS_PONG_TO_OTHERS (P2P Ping回复)
6F6824DE  | 5E                      | pop esi                             |
6F6824DF  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x11 | Packet ID: 0x26
; ===========================================================================
6F6824E2  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=11
6F6824E6  | 50                      | push eax                            |
6F6824E7  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6824EB  | 50                      | push eax                            |
6F6824EC  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6824F0  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6824F2  | 50                      | push eax                            |
6F6824F3  | E8 A848FFFF             | call game.6F676DA0                  | 0x26 -> W3GS_CLIENT_INFO (客户端信息/LAN)
6F6824F8  | 5E                      | pop esi                             |
6F6824F9  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x12 | Packet ID: 0x27
; ===========================================================================
6F6824FC  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       | esi=12
6F682500  | 50                      | push eax                            |
6F682501  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F682505  | 50                      | push eax                            |
6F682506  | 8B4424 18               | mov eax,dword ptr ss:[esp+18]       |
6F68250A  | 50                      | push eax                            |
6F68250B  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68250F  | 50                      | push eax                            |
6F682510  | E8 2BE2FFFF             | call game.6F680740                  | 0x27 -> W3GS_MAPCHECK (地图检查请求/旧版)
6F682515  | 5E                      | pop esi                             |
6F682516  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x13 | Packet ID: 0x28
; ===========================================================================
6F682519  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=13
6F68251D  | 50                      | push eax                            |
6F68251E  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682522  | 50                      | push eax                            |
6F682523  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682527  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682529  | 50                      | push eax                            |
6F68252A  | E8 418EFFFF             | call game.6F67B370                  | 0x28 -> W3GS_START_LAG (开始卡顿/等待界面)
6F68252F  | 5E                      | pop esi                             |
6F682530  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x14 | Packet ID: 0x29
; ===========================================================================
6F682533  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=14
6F682537  | 50                      | push eax                            |
6F682538  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68253C  | 50                      | push eax                            |
6F68253D  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682541  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682543  | 50                      | push eax                            |
6F682544  | E8 F790FFFF             | call game.6F67B640                  | 0x29 -> W3GS_STOP_LAG (停止卡顿)
6F682549  | 5E                      | pop esi                             |
6F68254A  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x15 | Packet ID: ? (Reserved/Unknown)
; ===========================================================================
6F68254D  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=15
6F682551  | 50                      | push eax                            |
6F682552  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682556  | 50                      | push eax                            |
6F682557  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68255B  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F68255D  | 50                      | push eax                            |
6F68255E  | E8 FD90FFFF             | call game.6F67B660                  | (Unknown Packet Logic)
6F682563  | 5E                      | pop esi                             |
6F682564  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x16 | Packet ID: ? (Reserved/Unknown)
; ===========================================================================
6F682567  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=16
6F68256B  | 50                      | push eax                            |
6F68256C  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682570  | 50                      | push eax                            |
6F682571  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682575  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682577  | 50                      | push eax                            |
6F682578  | E8 9391FFFF             | call game.6F67B710                  | (Unknown Packet Logic)
6F68257D  | 5E                      | pop esi                             |
6F68257E  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x17 | Packet ID: ? (Reserved/Unknown)
; ===========================================================================
6F682581  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=17
6F682585  | 50                      | push eax                            |
6F682586  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68258A  | 50                      | push eax                            |
6F68258B  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68258F  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682591  | 50                      | push eax                            |
6F682592  | E8 D9E2FFFF             | call game.6F680870                  | (Unknown Packet Logic)
6F682597  | 5E                      | pop esi                             |
6F682598  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x18 | Packet ID: ? (Reserved/Unknown)
; ===========================================================================
6F68259B  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=18
6F68259F  | 50                      | push eax                            |
6F6825A0  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6825A4  | 50                      | push eax                            |
6F6825A5  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6825A9  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6825AB  | 50                      | push eax                            |
6F6825AC  | E8 4FE3FFFF             | call game.6F680900                  | (Unknown Packet Logic)
6F6825B1  | 5E                      | pop esi                             |
6F6825B2  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x19 | Packet ID: ? (Reserved/Unknown)
; ===========================================================================
6F6825B5  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=19
6F6825B9  | 50                      | push eax                            |
6F6825BA  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6825BE  | 50                      | push eax                            |
6F6825BF  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6825C3  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6825C5  | 50                      | push eax                            |
6F6825C6  | E8 E511FFFF             | call game.6F6737B0                  | (Unknown Packet Logic)
6F6825CB  | 5E                      | pop esi                             |
6F6825CC  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x1A | Packet ID: 0x3D
; ===========================================================================
6F6825CF  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=1A
6F6825D3  | 50                      | push eax                            |
6F6825D4  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6825D8  | 50                      | push eax                            |
6F6825D9  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6825DD  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6825DF  | 50                      | push eax                            |
6F6825E0  | E8 4B92FFFF             | call game.6F67B830                  | **跳转点入口** 实际处理逻辑在下面
6F6825E5  | 5E                      | pop esi                             |
6F6825E6  | C2 1400                 | ret 14                              |

; =========================== 0x3D 的真正入口 ===============================
6F6825E9  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=1A (Continuation)
6F6825ED  | 50                      | push eax                            |
6F6825EE  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6825F2  | 50                      | push eax                            |
6F6825F3  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6825F7  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6825F9  | 50                      | push eax                            |
6F6825FA  | E8 11C6FFFF             | call game.6F67EC10                  | 0x3D -> W3GS_MAPCHECK (地图CRC/SHA1校验) **关键**
6F6825FF  | 5E                      | pop esi                             |
6F682600  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x1B | Packet ID: 0x3F
; ===========================================================================
6F682603  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=1B
6F682607  | 50                      | push eax                            |
6F682608  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68260C  | 50                      | push eax                            |
6F68260D  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682611  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682613  | 50                      | push eax                            |
6F682614  | E8 F7C7FFFF             | call game.6F67EE10                  | 0x3F -> W3GS_START_GAME (开始游戏/进入画面)
6F682619  | 5E                      | pop esi                             |
6F68261A  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x1C | Packet ID: 0x42
; ===========================================================================
6F68261D  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=1C
6F682621  | 50                      | push eax                            |
6F682622  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682626  | 50                      | push eax                            |
6F682627  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68262B  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F68262D  | 50                      | push eax                            |
6F68262E  | E8 5DC9FFFF             | call game.6F67EF90                  | 0x42 -> W3GS_MAPSIZE (地图大小报告)
6F682633  | 5E                      | pop esi                             |
6F682634  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x1D | Packet ID: 0x44
; ===========================================================================
6F682637  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=1D
6F68263B  | 50                      | push eax                            |
6F68263C  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682640  | 50                      | push eax                            |
6F682641  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682645  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682647  | 50                      | push eax                            |
6F682648  | E8 93CAFFFF             | call game.6F67F0E0                  | 0x44 -> W3GS_MAPPART (地图下载数据块)
6F68264D  | 5E                      | pop esi                             |
6F68264E  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x1E | Packet ID: 0x45
; ===========================================================================
6F682651  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=1E
6F682655  | 50                      | push eax                            |
6F682656  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68265A  | 50                      | push eax                            |
6F68265B  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F68265F  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682661  | 50                      | push eax                            |
6F682662  | E8 5992FFFF             | call game.6F67B8C0                  | 0x45 -> W3GS_MAPPART_NOTOK (数据块重传)
6F682667  | 5E                      | pop esi                             |
6F682668  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x1F | Packet ID: 0x46
; ===========================================================================
6F68266B  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=1F
6F68266F  | 50                      | push eax                            |
6F682670  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F682674  | 50                      | push eax                            |
6F682675  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682679  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F68267B  | 50                      | push eax                            |
6F68267C  | E8 DF94FFFF             | call game.6F67BB60                  | 0x46 -> W3GS_PONG_TO_HOST (Ping回复/UDP打洞)
6F682681  | 5E                      | pop esi                             |
6F682682  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x20 | Packet ID: 0x48
; ===========================================================================
6F682685  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=20
6F682689  | 50                      | push eax                            |
6F68268A  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F68268E  | 50                      | push eax                            |
6F68268F  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F682693  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F682695  | 50                      | push eax                            |
6F682696  | E8 E594FFFF             | call game.6F67BB80                  | 0x48 -> W3GS_INCOMING_ACTION3 (动作包3)
6F68269B  | 5E                      | pop esi                             |
6F68269C  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x21 | Packet ID: 0x50
; ===========================================================================
6F68269F  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=21
6F6826A3  | 50                      | push eax                            |
6F6826A4  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6826A8  | 50                      | push eax                            |
6F6826A9  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6826AD  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6826AF  | 50                      | push eax                            |
6F6826B0  | E8 7BD2FFFF             | call game.6F67F930                  | 0x50 -> W3GS_AUTH_INFO (认证信息/Bot)
6F6826B5  | 5E                      | pop esi                             |
6F6826B6  | C2 1400                 | ret 14                              |
; ===========================================================================
; ESI = 0x22 | Packet ID: 0x51 (推测)
; ===========================================================================
6F6826B9  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=22
6F6826BD  | 50                      | push eax                            |
6F6826BE  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6826C2  | 50                      | push eax                            |
6F6826C3  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6826C7  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6826C9  | 50                      | push eax                            |
6F6826CA  | E8 91D5FFFF             | call game.6F67FC60                  | 0x51 -> W3GS_AUTH_CHECK (扩展认证/平台检测)
6F6826CF  | 5E                      | pop esi                             |
6F6826D0  | C2 1400                 | ret 14                              |

; ===========================================================================
; ESI = 0x23 | Packet ID: 0x52 (推测)
; ===========================================================================
6F6826D3  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       | esi=23
6F6826D7  | 50                      | push eax                            |
6F6826D8  | 8B4424 14               | mov eax,dword ptr ss:[esp+14]       |
6F6826DC  | 50                      | push eax                            |
6F6826DD  | 8B4424 10               | mov eax,dword ptr ss:[esp+10]       |
6F6826E1  | 8B00                    | mov eax,dword ptr ds:[eax]          |
6F6826E3  | 50                      | push eax                            |
6F6826E4  | E8 47D6FFFF             | call game.6F67FD30                  | 0x52 -> W3GS_AUTH_REFRESH (认证刷新/保活)
6F6826E9  | 5E                      | pop esi                             |
6F6826EA  | C2 1400                 | ret 14                              |
; ================================ END ======================================
6F6826ED  | 8D49 00                 | lea ecx,dword ptr ds:[ecx]          |
6F6826F0  | 25 23686F3F             | and eax,3F6F6823                    |
6F6826F5  | 2368 6F                 | and ebp,dword ptr ds:[eax+6F]       |
6F6826F8  | 59                      | pop ecx                             |
6F6826F9  | 2368 6F                 | and ebp,dword ptr ds:[eax+6F]       |
6F6826FC  | 73 23                   | jae game.6F682721                   |
6F6826FE  | 68 6F8D2368             | push 68238D6F                       |
6F682703  | 6F                      | outsd                               |
6F682704  | A7                      | cmpsd                               |
6F682705  | 2368 6F                 | and ebp,dword ptr ds:[eax+6F]       |
6F682708  | C123 68                 | shl dword ptr ds:[ebx],68           |
6F68270B  | 6F                      | outsd                               |
6F68270C  | DB                      | ???                                 |
6F68270D  | 2368 6F                 | and ebp,dword ptr ds:[eax+6F]       |
6F682710  | F5                      | cmc                                 |
6F682711  | 2368 6F                 | and ebp,dword ptr ds:[eax+6F]       |
6F682714  | 0F                      | ???                                 |
6F682715  | 24 68                   | and al,68                           |
6F682717  | 6F                      | outsd                               |
6F682718  | 292468                  | sub dword ptr ds:[eax+ebp*2],esp    |
6F68271B  | 6F                      | outsd                               |
6F68271C  | 5D                      | pop ebp                             |
6F68271D  | 24 68                   | and al,68                           |
6F68271F  | 6F                      | outsd                               |
6F682720  | 77 24                   | ja game.6F682746                    |
6F682722  | 68 6F912468             | push 6824916F                       |
6F682727  | 6F                      | outsd                               |
6F682728  | 3325 686FAE24           | xor esp,dword ptr ds:[24AE6F68]     |
6F68272E  | 68 6FC82468             | push 6824C86F                       |
6F682733  | 6F                      | outsd                               |
6F682734  | E2 24                   | loop game.6F68275A                  |
6F682736  | 68 6FFC2468             | push 6824FC6F                       |
6F68273B  | 6F                      | outsd                               |
6F68273C  | 1925 686F4D25           | sbb dword ptr ds:[254D6F68],esp     |
6F682742  | 68 6F672568             | push 6825676F                       |
6F682747  | 6F                      | outsd                               |
6F682748  | 8125 686F9B25 686FB525  | and dword ptr ds:[259B6F68],25B56F6 |
6F682752  | 68 6FCF2568             | push 6825CF6F                       |
6F682757  | 6F                      | outsd                               |
6F682758  | E9 25686F03             | jmp 72D78F82                        |
6F68275D  | 26:68 6F1D2668          | push 68261D6F                       |
6F682763  | 6F                      | outsd                               |
6F682764  | 37                      | aaa                                 |
6F682765  | 26:68 6F9F2668          | push 68269F6F                       |
6F68276B  | 6F                      | outsd                               |
6F68276C  | B9 26686FD3             | mov ecx,D36F6826                    |
6F682771  | 26:68 6F512668          | push 6826516F                       |
6F682777  | 6F                      | outsd                               |
6F682778  | 43                      | inc ebx                             |
6F682779  | 24 68                   | and al,68                           |
6F68277B  | 6F                      | outsd                               |
6F68277C  | 6B26 68                 | imul esp,dword ptr ds:[esi],68      |
6F68277F  | 6F                      | outsd                               |
6F682780  | 8526                    | test dword ptr ds:[esi],esp         |
6F682782  | 68 6FE92668             | push 6826E96F                       |
6F682787  | 6F                      | outsd                               |
6F682788  | 0001                    | add byte ptr ds:[ecx],al            |
6F68278A  | 0203                    | add al,byte ptr ds:[ebx]            |
6F68278C  | 04 05                   | add al,5                            |
6F68278E  | 06                      | push es                             |
6F68278F  | 07                      | pop es                              |
6F682790  | 0809                    | or byte ptr ds:[ecx],cl             |
6F682792  | 0A0B                    | or cl,byte ptr ds:[ebx]             |
6F682794  | 0C 0D                   | or al,D                             |
6F682796  | 0E                      | push cs                             |
6F682797  | 0F1025 11122513         | movups xmm4,xmmword ptr ds:[1325121 |
6F68279E  | 14 15                   | adc al,15                           |
6F6827A0  | 16                      | push ss                             |
6F6827A1  | 17                      | pop ss                              |
6F6827A2  | 1819                    | sbb byte ptr ds:[ecx],bl            |
6F6827A4  | 25 25252525             | and eax,25252525                    |
6F6827A9  | 25 25252525             | and eax,25252525                    |
6F6827AE  | 25 25252525             | and eax,25252525                    |
6F6827B3  | 25 25252525             | and eax,25252525                    |
6F6827B8  | 25 25252525             | and eax,25252525                    |
6F6827BD  | 25 25252525             | and eax,25252525                    |
6F6827C2  | 25 251A1B1C             | and eax,1C1B1A25                    |
6F6827C7  | 1D 25251E1F             | sbb eax,1F1E2525                    |
6F6827CC  | 2025 21222525           | and byte ptr ds:[25252221],ah       |
6F6827D2  | 25 25252525             | and eax,25252525                    |
6F6827D7  | 2324CC                  | and esp,dword ptr ss:[esp+ecx*8]    |
```


| ESI (Hex) | Packet ID | 协议名称 (Protocol) | 关键函数 (Call Target) | 功能描述 (Description) |
| :---: | :---: | :--- | :--- | :--- |
| **00** | **0x01** | `PING_FROM_HOST` | `6F67AA80` | 主机 Ping，回复 0x46 |
| **01** | **0x04** | `SLOTINFOJOIN` | `6F67CE30` | **加入成功**，解析 PID/Seed |
| **02** | **0x05** | `REJECTJOIN` | `6F6737A0` | **加入拒绝**，弹出错误框 |
| **03** | **0x06** | `PLAYERINFO` | `6F67D040` | 玩家信息更新 (IP/Port/Name) |
| **04** | **0x07** | `PLAYERLEAVE` | `6F67AB30` | 玩家离开通知 |
| **05** | **0x08** | `GAMELOADED` | `6F6817D0` | 某玩家加载完成 |
| **06** | **0x09** | `SLOTINFO` | `6F680600` | 槽位布局更新 (UI) |
| **07** | **0x0A** | `COUNTDOWN_START`| `6F67AC60` | 倒计时开始 |
| **08** | **0x0B** | `COUNTDOWN_END` | `6F67AD90` | 倒计时结束，开始加载 |
| **09** | **0x0C** | `INCOMING_ACTION`| `6F67AE70` | 游戏动作 (操作指令) |
| **0A** | **0x0D** | `CHAT_FROM_HOST` | `6F680620` | 聊天消息 |
| **0B** | **0x0E** | `LEAVEREQ` | `6F67E8B0` | 离开请求 |
| **0C** | **0x0F** | `START_DOWNLOAD` | `6F67E8D0` | 开始下载地图 |
| **0D** | **0x1B** | `PONG_TO_HOST` | `6F67AF30` | TCP Pong |
| **0E** | **0x1C** | `INCOMING_ACTION2`| `6F680F40` | 动作包格式2 |
| **0F** | **0x23** | `GAMELOADED_OTHERS`| `6F67AFF0` | 其他人加载完毕 |
| **10** | **0x25** | `PONG_TO_OTHERS` | `6F67D270` | P2P Pong |
| **11** | **0x26** | `CLIENT_INFO` | `6F676DA0` | 客户端信息 |
| **12** | **0x27** | `MAPCHECK` | `6F680740` | 地图检查 (旧版) |
| **13** | **0x28** | `START_LAG` | `6F67B370` | 卡顿开始 (Lag Screen) |
| **14** | **0x29** | `STOP_LAG` | `6F67B640` | 卡顿结束 |
| **15** | **?** | `UNKNOWN` | `6F67B660` | (未知/保留) |
| **16** | **?** | `UNKNOWN` | `6F67B710` | (未知/保留) |
| **17** | **?** | `UNKNOWN` | `6F680870` | (未知/保留) |
| **18** | **?** | `UNKNOWN` | `6F680900` | (未知/保留) |
| **19** | **?** | `UNKNOWN` | `6F6737B0` | (未知/保留) |
| **1A** | **0x3D** | `MAPCHECK` | `6F67B830` -> `6F67EC10` | **🔥 核心地图校验 (CRC/SHA1)** |
| **1B** | **0x3F** | `START_GAME` | `6F67EE10` | 开始游戏 (进入画面) |
| **1C** | **0x42** | `MAPSIZE` | `6F67EF90` | 地图大小/状态报告 |
| **1D** | **0x44** | `MAPPART` | `6F67F0E0` | 地图数据块 |
| **1E** | **0x45** | `MAPPART_ACK` | `6F67B8C0` | 地图块确认 |
| **1F** | **0x46** | `PONG_TO_HOST` | `6F67BB60` | Pong (回复 0x01) |
| **20** | **0x48** | `INCOMING_ACTION3`| `6F67BB80` | 动作包3 |
| **21** | **0x50** | `AUTH_INFO` | `6F67F930` | 认证信息 (Bot/主机) |
| **22** | **0x51** | `AUTH_CHECK` | `6F67FC60` | (推测) 扩展认证检查 |
| **23** | **0x52** | `AUTH_REFRESH` | `6F67FD30` | (推测) 扩展认证刷新 |

### 关键跳转逻辑解析

代码使用了一个**跳转表 (Jump Table)** 来实现 Switch-Case，而不是一连串的 `cmp/je`。

1.  **预处理**:
    ```assembly
    6F682301 | 0FB67424 0C | movzx esi,byte ptr ss:[esp+C] ; 读取 Packet ID
    6F682306 | 83C6 FF     | add esi,FFFFFFFF              ; ID = ID - 1 (为了把 1 映射到 0)
    ```

2.  **查表**:
    ```assembly
    6F682317 | 0FB6B6 8827B602       | movzx esi,byte ptr ds:[esi+6F682788] ; 查索引表
    6F68231E | FF24B5 F026B602       | jmp dword ptr ds:[esi*4+6F6826F0]    ; 根据索引跳转到具体地址
    ```
    *   `6F682788` 是索引数组（Index Array），把 Packet ID 映射到一个小的索引值 (0, 1, 2...)。
    *   `6F6826F0` 是函数指针数组（Jump Table），存放了上述表格中所有 `mov eax...; call...; ret` 代码块的入口地址。

### 特别标注

*   **`ESI = 0x04` (ID 0x05 - 1)**: 如果原始包 ID 是 `0x05` (Reject)，会去 `0x04` 的逻辑。但如果是指原始包 ID 就是 `0x04` (SlotInfoJoin)，那么代码里会处理为 `0x03` 的偏移，最终跳转到 `6F682373`。
*   **`ESI = 0x3C` (ID 0x3D - 1)**: 这是地图验证包。它会查表后跳转到 `6F6825E9`，那里会调用 `game.6F67EC10`（也就是 `6F657C10` 的上层封装或者别名），进而在里面进行 CRC/SHA1 的解包和校验。

希望这份表格能帮助你清晰地理解 War3 客户端是如何路由每一个网络包的！
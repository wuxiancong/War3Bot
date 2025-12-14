/**
 * @file cdkeydecoder.cpp
 * @brief CD Key 解码与哈希计算核心实现
 *
 * 作用:
 * 此文件是 bncsutil 库中处理暴雪游戏 CD Key 的核心部分。
 * 它负责解码 StarCraft (13位), WarCraft II/Diablo II (16位), 和 WarCraft III (26位) 的 CD Key。
 *
 * 核心功能:
 * 1. 解码 Key: 将用户输入的字符串 Key 还原为 Product ID, Public Value (Val1), Private Value (Val2)。
 * 2. 校验 Key: 验证 Key 的校验和 (Checksum) 是否合法（防止打字错误）。
 * 3. 计算哈希: 根据 ClientToken 和 ServerToken 计算用于战网登录的 Hash 值 (SID_AUTH_CHECK)。
 */

#include <bncsutil/mutil.h>
#include <bncsutil/cdkeydecoder.h>
#include <bncsutil/keytables.h> // W2/D2 和 W3 的置换表
#include <bncsutil/bsha1.h> // Broken SHA-1 (旧版哈希)
#include <bncsutil/sha1.h> // US Secure Hash Algorithm (W3 使用)
#include <cctype> // isdigit(), isalnum(), toupper()
#include <cstring> // memcpy(), strlen()
#include <cstdio> // sscanf()

/**
 * 具体的 CD-key 哈希结构体
 */
struct CDKEYHASH {
    uint32_t clientToken;
    uint32_t serverToken;
    uint32_t product;
    uint32_t value1;
    union {
        struct {
            uint32_t zero;
            uint32_t v;
            unsigned char reserved[2];
        } s; // 16位 Key 结构
        struct {
            char v[10];
        } l; // 26位 Key 结构 (War3)
    } value2;
};

#define W3_KEYLEN 26
#define W3_BUFLEN (W3_KEYLEN << 1)

CDKeyDecoder::CDKeyDecoder() {
    initialized = 0;
    keyOK = 0;
    hashLen = 0;
    cdkey = (char*) 0;
    w3value2 = (char*) 0;
    keyHash = (char*) 0;
    keyLen = 0;      // [修复] 初始化成员变量
    keyType = 0;     // [修复] 初始化成员变量
}

CDKeyDecoder::CDKeyDecoder(const char* cd_key) {
    // [修复] 初始化所有成员变量
    initialized = 0;
    keyOK = 0;
    hashLen = 0;
    cdkey = (char*) 0;
    w3value2 = (char*) 0;
    keyHash = (char*) 0;
    keyLen = 0;
    keyType = 0;
    product = 0;
    value1 = 0;
    value2 = 0;

    if (cd_key) {
        *this = CDKeyDecoder(cd_key, std::strlen(cd_key));
    }
}

CDKeyDecoder::CDKeyDecoder(const char* cdKey, size_t keyLength) {
    unsigned int i;

    // [修复] 初始化所有成员变量
    initialized = 0;
    product = 0;
    value1 = 0;
    value2 = 0;
    keyOK = 0;
    hashLen = 0;
    cdkey = (char*) 0;
    w3value2 = (char*) 0;
    keyHash = (char*) 0;
    keyLen = 0;
    keyType = 0;

    if (!cdKey || keyLength <= 0) return;

    // 初始健全性检查
    if (keyLength == 13) {
        // StarCraft Key (13位数字)
        for (i = 0; i < keyLength; i++) {
            if (!isdigit(cdKey[i])) return;
        }
        keyType = KEY_STARCRAFT;
#if DEBUG
        bncsutil_debug_message_a("Created CD key decoder with STAR key %s.", cdKey);
#endif
    } else {
        // D2/W2/W3 Key (字母数字混合)
        for (i = 0; i < keyLength; i++) {
            if (!isalnum(cdKey[i])) return;
        }
        switch (keyLength) {
        case 16:
            keyType = KEY_WARCRAFT2; // 也适用于 Diablo II
#if DEBUG
            bncsutil_debug_message_a("Created CD key decoder with W2/D2 key %s.", cdKey);
#endif
            break;
        case 26:
            keyType = KEY_WARCRAFT3;
#if DEBUG
            bncsutil_debug_message_a("Created CD key decoder with WAR3 key %s.", cdKey);
#endif
            break;
        default:
#if DEBUG
            bncsutil_debug_message_a("Created CD key decoder with unrecognized key %s.", cdKey);
#endif
            return;
        }
    }

    cdkey = new char[keyLength + 1];
    if (!cdkey) return; // 内存分配失败

    initialized = 1;
    keyLen = keyLength;

    // [修复] 使用 memcpy 替代 strcpy
    std::memcpy(cdkey, cdKey, keyLength);
    cdkey[keyLength] = '\0';

    switch (keyType) {
    case KEY_STARCRAFT:
        keyOK = processStarCraftKey();
#if DEBUG
        bncsutil_debug_message_a("%s: ok=%d; product=%d; public=%d; private=%d",
                                 cdkey, keyOK, getProduct(), getVal1(), getVal2());
#endif
        break;
    case KEY_WARCRAFT2:
        keyOK = processWarCraft2Key();
#if DEBUG
        bncsutil_debug_message_a("%s: ok=%d; product=%d; public=%d; private=%d",
                                 cdkey, keyOK, getProduct(), getVal1(), getVal2());
#endif
        break;
    case KEY_WARCRAFT3:
        keyOK = processWarCraft3Key();
#if DEBUG
        bncsutil_debug_message_a("%s: ok=%d; product=%d; public=%d; ",
                                 cdkey, keyOK, getProduct(), getVal1());
#endif
        break;
    default:
        return;
    }
}

CDKeyDecoder::~CDKeyDecoder() {
    if (initialized && cdkey != NULL)
        delete [] cdkey;
    if (hashLen > 0 && keyHash != NULL)
        delete [] keyHash;
    if (w3value2)
        delete [] w3value2;
}

int CDKeyDecoder::isKeyValid() {
    return (initialized && keyOK) ? 1 : 0;
}

int CDKeyDecoder::getVal2Length() {
    return (keyType == KEY_WARCRAFT3) ? 10 : 4;
}

uint32_t CDKeyDecoder::getProduct() {
    switch (keyType) {
    case KEY_STARCRAFT:
    case KEY_WARCRAFT2:
        return (uint32_t) LSB4(product);
    case KEY_WARCRAFT3:
        return (uint32_t) MSB4(product);
    default:
        return (uint32_t) -1;
    }
}

uint32_t CDKeyDecoder::getVal1() {
    switch (keyType) {
    case KEY_STARCRAFT:
    case KEY_WARCRAFT2:
        return (uint32_t) LSB4(value1);
    case KEY_WARCRAFT3:
        return (uint32_t) MSB4(value1);
    default:
        return (uint32_t) -1;
    }
}

uint32_t CDKeyDecoder::getVal2() {
    return (uint32_t) LSB4(value2);
}

int CDKeyDecoder::getLongVal2(char* out) {
    if (w3value2 != NULL && keyType == KEY_WARCRAFT3) {
        memcpy(out, w3value2, 10);
        return 10;
    } else {
        return 0;
    }
}

size_t CDKeyDecoder::calculateHash(uint32_t clientToken, uint32_t serverToken)
{
    struct CDKEYHASH kh;
    SHA1Context sha;

    if (!initialized || !keyOK) return 0;
    hashLen = 0;

    kh.clientToken = clientToken;
    kh.serverToken = serverToken;

    switch (keyType) {
    case KEY_STARCRAFT:
    case KEY_WARCRAFT2:
        kh.product = (uint32_t) LSB4(product);
        kh.value1 = (uint32_t) LSB4(value1);

        kh.value2.s.zero = 0;
        kh.value2.s.v = (uint32_t) LSB4(value2);

        keyHash = new char[20];
        if (!keyHash) return 0;

        calcHashBuf((char*) &kh, 24, keyHash);
        hashLen = 20;

#if DEBUG
        bncsutil_debug_message_a("%s: Hash calculated.", cdkey);
        bncsutil_debug_dump(keyHash, 20);
#endif
        return 20;

    case KEY_WARCRAFT3:
        kh.product = (uint32_t) MSB4(product);
        kh.value1 = (uint32_t) MSB4(value1);
        memcpy(kh.value2.l.v, w3value2, 10);

        if (SHA1Reset(&sha))
            return 0;
        if (SHA1Input(&sha, (const unsigned char*) &kh, 26))
            return 0;
        keyHash = new char[20];
        if (!keyHash) return 0;

        if (SHA1Result(&sha, (unsigned char*) keyHash)) {
            SHA1Reset(&sha);
            return 0;
        }
        SHA1Reset(&sha);
        hashLen = 20;

#if DEBUG
        bncsutil_debug_message_a("%s: Hash calculated.", cdkey);
        bncsutil_debug_dump(keyHash, 20);
#endif
        return 20;

    default:
        return 0;
    }
}

size_t CDKeyDecoder::getHash(char* outputBuffer) {
    if (hashLen == 0 || !keyHash || !outputBuffer)
        return 0;
    memcpy(outputBuffer, keyHash, hashLen);
    return hashLen;
}

int CDKeyDecoder::processStarCraftKey() {
    int accum, pos, i;
    char temp;
    int hashKey = 0x13AC9741;
    char cdkey[14];

    // [修复] 使用 memcpy 替代 strcpy，keyLen 必然为 13
    std::memcpy(cdkey, this->cdkey, 14); // 拷贝包含 null 结尾

    // Verification
    accum = 3;
    for (i = 0; i < (int) (keyLen - 1); i++) {
        accum += ((tolower(cdkey[i]) - '0') ^ (accum * 2));
    }

    if ((accum % 10) != (cdkey[12] - '0')) {
        return 0;
    }

    // Shuffling
    pos = 0x0B;
    for (i = 0xC2; i >= 7; i -= 0x11) {
        temp = cdkey[pos];
        cdkey[pos] = cdkey[i % 0x0C];
        cdkey[i % 0x0C] = temp;
        pos--;
    }

    // Final Value
    for (i = (int) (keyLen - 2); i >= 0; i--) {
        temp = toupper(cdkey[i]);
        cdkey[i] = temp;
        if (temp <= '7') {
            cdkey[i] ^= (char) (hashKey & 7);
            hashKey >>= 3;
        } else if (temp < 'A') {
            cdkey[i] ^= ((char) i & 1);
        }
    }

    if (sscanf(cdkey, "%2ld%7ld%3ld", &product, &value1, &value2) != 3) {
        return 0;
    }

    return 1;
}

int CDKeyDecoder::processWarCraft2Key() {
    unsigned long r, n, n2, v, v2, checksum;
    int i;
    unsigned char c1, c2, c;
    char cdkey[17];

    // [修复] 使用 memcpy 替代 strcpy，keyLen 必然为 16
    std::memcpy(cdkey, this->cdkey, 17); // 拷贝包含 null

    r = 1;
    checksum = 0;
    for (i = 0; i < 16; i += 2) {
        c1 = w2Map[(int) cdkey[i]];
        n = c1 * 3;
        c2 = w2Map[(int) cdkey[i + 1]];
        n = c2 + n * 8;

        if (n >= 0x100) {
            n -= 0x100;
            checksum |= r;
        }
        n2 = n >> 4;
        cdkey[i] = getHexValue(n2);
        cdkey[i + 1] = getHexValue(n);
        r <<= 1;
    }

    v = 3;
    for (i = 0; i < 16; i++) {
        c = cdkey[i];
        n = getNumValue(c);
        n2 = v * 2;
        n ^= n2;
        v += n;
    }
    v &= 0xFF;

    if (v != checksum) {
        return 0;
    }

    for (int j = 15; j >= 0; j--) {
        c = cdkey[j];
        if (j > 8) {
            n = static_cast<unsigned long>(j - 9);
        } else {
            n = static_cast<unsigned long>(0xF - (8 - j));
        }
        n &= 0xF;
        c2 = cdkey[n];
        cdkey[j] = c2;
        cdkey[n] = c;
    }

    v2 = 0x13AC9741;
    for (int j = 15; j >= 0; j--) {
        c = static_cast<unsigned char>(toupper(cdkey[j]));
        cdkey[j] = static_cast<char>(c);
        if (c <= '7') {
            v = v2;
            c2 = static_cast<unsigned char>(((char) (v & 0xFF) & 7) ^ c);
            v >>= 3;
            cdkey[j] = (char) c2;
            v2 = v;
        } else if (c < 'A') {
            cdkey[j] = static_cast<char>((((char) j) & 1) ^ c);
        }
    }

    if (sscanf(cdkey, "%2lx%6lx%8lx", &product, &value1, &value2) != 3) {
        return 0;
    }

    return 1;
}

int CDKeyDecoder::processWarCraft3Key() {
    char table[W3_BUFLEN];
    int values[4];
    int a, b;
    size_t i;
    char decode;

    b = 0x21;

    memset(table, 0, sizeof(table));
    memset(values, 0, sizeof(values));

    for (i = 0; i < keyLen; i++) {
        cdkey[i] = static_cast<char>(toupper(cdkey[i]));
        a = (b + 0x07B5) % W3_BUFLEN;
        b = (a + 0x07B5) % W3_BUFLEN;
        decode = w3KeyMap[static_cast<unsigned char>(cdkey[i])];
        table[a] = static_cast<char>(decode / 5);
        table[b] = static_cast<char>(decode % 5);
    }

    // Mult
    int mult_i = W3_BUFLEN;
    do {
        mult(4, 5, values + 3, table[mult_i - 1]);
    } while (--mult_i);

    decodeKeyTable(values);

    product = values[0] >> 0xA;
    product = SWAP4(product);
#if LITTLEENDIAN
    for (int j = 0; j < 4; j++) {
        values[j] = MSB4(values[j]);
    }
#endif

    value1 = LSB4(*(uint32_t*) (((char*) values) + 2)) & 0xFFFFFF00;

    w3value2 = new char[10];
    if (!w3value2) return 0;

#if LITTLEENDIAN
    *((uint16_t*) w3value2) = MSB2(*(uint16_t*) (((char*) values) + 6));
    *((uint32_t*) ((char*) w3value2 + 2)) = MSB4(*(uint32_t*) (((char*) values) + 8));
    *((uint32_t*) ((char*) w3value2 + 6)) = MSB4(*(uint32_t*) (((char*) values) + 12));
#else
    *((uint16_t*) w3value2) = LSB2(*(uint16_t*) (((char*) values) + 6));
    *((uint32_t*) ((char*) w3value2 + 2)) = LSB4(*(uint32_t*) (((char*) values) + 8));
    *((uint32_t*) ((char*) w3value2 + 6)) = LSB4(*(uint32_t*) (((char*) values) + 12));
#endif
    return 1;
}

inline void CDKeyDecoder::mult(int r, const int x, int* a, int dcByte) {
    while (r--) {
        int64_t edxeax = static_cast<int64_t>(*a & 0x00000000FFFFFFFFl) *
                         static_cast<int64_t>(x & 0x00000000FFFFFFFFl);
        *a-- = dcByte + static_cast<int32_t>(edxeax);
        dcByte = static_cast<int32_t>(edxeax >> 32);
    }
}

void CDKeyDecoder::decodeKeyTable(int* keyTable) {
    unsigned int eax, ebx, ecx, edx, edi, esi, ebp;
    unsigned int varC, var4, var8;
    unsigned int copy[4];
    unsigned char* scopy;
    int* ckt;
    int ckt_temp;
    var8 = 29;
    int i = 464;

    // pass 1
    do {
        int j;
        esi = (var8 & 7) << 2;
        var4 = var8 >> 3;
        varC = keyTable[3 - var4];
        varC &= (0xF << esi);
        varC = varC >> esi;

        if (i < 464) {
            for (j = 29; static_cast<unsigned int>(j) > var8; j--) {
                ecx = (j & 7) << 2;
                ebp = (keyTable[0x3 - (j >> 3)]);
                ebp &= (0xF << ecx);
                ebp = ebp >> ecx;
                // [修复] 添加括号明确优先级
                varC = w3TranslateMap[ebp ^ (w3TranslateMap[varC + i] + i)];
            }
        }

        j = static_cast<int>(--var8);
        while (j >= 0) {
            ecx = (j & 7) << 2;
            ebp = (keyTable[0x3 - (j >> 3)]);
            ebp &= (0xF << ecx);
            ebp = ebp >> ecx;
            // [修复] 添加括号明确优先级
            varC = w3TranslateMap[ebp ^ (w3TranslateMap[varC + i] + i)];
            j--;
        }

        j = 3 - var4;
        ebx = (w3TranslateMap[varC + i] & 0xF) << esi;

        // [修复] 增加括号，明确优先级
        keyTable[j] = (ebx | ((~(0xF << esi)) & keyTable[j]));
    } while ((i -= 16) >= 0);

    // pass 2
    esi = 0;

    for (i = 0; i < 4; i++) {
        copy[i] = LSB4(keyTable[i]);
    }
    scopy = (unsigned char*) copy;

    for (edi = 0; edi < 120; edi++) {
        unsigned int location = 12;
        eax = edi & 0x1F;
        ecx = esi & 0x1F;
        edx = 3 - (edi >> 5);

        location -= ((esi >> 5) << 2);
        ebp = *(int*) (scopy + location);
        ebp = LSB4(ebp);

        ebp &= (1 << ecx);
        ebp = ebp >> ecx;

        ckt = (keyTable + edx);
        ckt_temp = *ckt;
        *ckt = ebp & 1;
        *ckt = *ckt << eax;
        // [修复] 增加括号，明确优先级
        *ckt = (*ckt) | ((~(1 << eax)) & ckt_temp);
        esi += 0xB;
        if (esi >= 120)
            esi -= 120;
    }
}

inline char CDKeyDecoder::getHexValue(int v) {
    v &= 0xF;
    return static_cast<char>((v < 10) ? (v + 0x30) : (v + 0x37));
}

inline int CDKeyDecoder::getNumValue(char c) {
    c = static_cast<char>(toupper(c));
    return (isdigit(c)) ? (c - 0x30) : (c - 0x37);
}

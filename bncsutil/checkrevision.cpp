/**
* @file checkrevision.cpp
* @brief Battle.net 版本验证 (CheckRevision) 算法实现
*
* 作用:
* 此文件实现了战网协议中用于验证客户端完整性的核心逻辑 (SID_AUTH_CHECK)。
* 它是反作弊和版本控制的基础。
 */

#include <bncsutil/mutil.h>
#include <bncsutil/checkrevision.h>
#include <bncsutil/file.h>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#ifdef MOS_WINDOWS
#include <windows.h>
#else
#include <bncsutil/pe.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctime>
#endif

#ifndef HIWORD
#define HIWORD(l) ((uint16_t) ((l) >> 16))
#endif

#ifndef LOWORD
#define LOWORD(l) ((uint16_t) ((l) & 0xFFFF))
#endif

#ifdef HAVE__SNPRINTF
#define snprintf _snprintf
#endif

// BNCSutil - CheckRevision - GetNumber
#define BUCR_GETNUM(ch) (((ch) == 'S') ? 3 : ((ch) - 'A'))
// BNCSutil - CheckRevision - IsNumber
#define BUCR_ISNUM(ch) (((ch) >= '0') && ((ch) <= '9'))

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif


std::vector<long> checkrevision_seeds;
void initialize_checkrevision_seeds()
{
    static bool run = false;

    if (run)
        return;

    run = true;

    checkrevision_seeds.reserve(8);

    checkrevision_seeds.push_back(0xE7F4CB62);
    checkrevision_seeds.push_back(0xF6A14FFC);
    checkrevision_seeds.push_back(0xAA5504AF);
    checkrevision_seeds.push_back(0x871FCDC2);
    checkrevision_seeds.push_back(0x11BF6A18);
    checkrevision_seeds.push_back(0xC57292E6);
    checkrevision_seeds.push_back(0x7927D27E);
    checkrevision_seeds.push_back(0x2FEC8733);
}

MEXP(long) get_mpq_seed(int mpq_number)
{
    if (((size_t) mpq_number) >= checkrevision_seeds.size()) {
        return 0;
    }

    return checkrevision_seeds[mpq_number];
}

MEXP(long) set_mpq_seed(int mpq_number, long new_seed)
{
    long ret;

    if (((size_t) mpq_number) >= checkrevision_seeds.size()) {
        ret = 0;
        checkrevision_seeds.reserve((size_t) mpq_number);
    } else {
        ret = checkrevision_seeds[mpq_number];
    }

    checkrevision_seeds[mpq_number] = new_seed;
    return ret;
}

MEXP(int) extractMPQNumber(const char *mpqName)
{
    if (mpqName == NULL)
        return -1;

    // 1. 寻找文件名中的 ".mpq" 后缀或最后一个点
    const char *dot = std::strrchr(mpqName, '.');
    if (dot == NULL || dot == mpqName)
        return -1;

    // 2. 从点号往前倒推，找到数字的起始位置
    // 例如 "...ver12.mpq" -> dot指向点
    // p 指向 '2' -> '1' -> 'r' (停止)
    const char *p = dot - 1;
    while (p >= mpqName && std::isdigit((unsigned char)*p)) {
        p--;
    }

    // p 现在指向非数字字符（或者文件头前一位），所以数字从 p + 1 开始
    p++;

    // 如果没有找到任何数字 (p 还是等于 dot)
    if (p == dot)
        return -1;

    // 3. 转换数字
    return std::atoi(p);
}

const char *get_basename(const char *file_name)
{
    const char *base;

    for (base = (file_name + strlen(file_name)); base >= file_name; base--) {
        if (*base == '\\' || *base == '/')
            break;
    }

    return ++base;
}

MEXP(int) checkRevision(const char *formula, const char *files[], int numFiles,
              int mpqNumber, unsigned long *checksum)
{
    // [修复 1] 必须初始化数组为 0，否则 values[0] ^= seed 会使用垃圾内存导致报错
    uint64_t values[4] = {0};
    long ovd[4] = {0}, ovs1[4] = {0}, ovs2[4] = {0};
    char ops[4] = {0};

    const char *token;
    int curFormula = 0;
    file_t f;
    uint8_t *file_buffer;
    uint32_t *dwBuf;
    uint32_t *current;
    size_t seed_count;

#if DEBUG
    int i;
    bncsutil_debug_message_a("checkRevision(\"%s\", {", formula);
    for (i = 0; i < numFiles; i++) {
        bncsutil_debug_message_a("\t\"%s\",", files[i]);
    }
    bncsutil_debug_message_a("}, %d, %d, %p);", numFiles, mpqNumber, checksum);
#endif

    if (!formula || !files || numFiles == 0 || mpqNumber < 0 || !checksum) {
        return 0;
    }

    seed_count = checkrevision_seeds.size();
    if (seed_count == 0) {
        initialize_checkrevision_seeds();
        seed_count = checkrevision_seeds.size();
    }

    if (seed_count <= (size_t) mpqNumber) {
        return 0;
    }

    token = formula;
    while (token && *token) {
        if (*(token + 1) == '=') {
            int variable = BUCR_GETNUM(*token);
            if (variable < 0 || variable > 3) {
                return 0;
            }

            token += 2; // skip over equals sign
            if (BUCR_ISNUM(*token)) {
                values[variable] = ATOL64(token);
            } else {
                if (curFormula > 3) {
                    return 0;
                }
                // 增加边界检查
                int op1 = BUCR_GETNUM(*token);
                int op2 = BUCR_GETNUM(*(token + 2));
                if (op1 < 0 || op1 > 3 || op2 < 0 || op2 > 3) return 0;

                ovd[curFormula] = variable;
                ovs1[curFormula] = op1;
                ops[curFormula] = *(token + 1);
                ovs2[curFormula] = op2;
                curFormula++;
            }
        }

        for (; *token != 0; token++) {
            if (*token == ' ') {
                token++;
                break;
            }
        }
    }

    // [关键点] 这里之前如果 values 未初始化，就会报错
    values[0] ^= checkrevision_seeds[mpqNumber];

    for (int i = 0; i < numFiles; i++) {
        // [修复 2] 删除了 rounded_size，因为它未被使用
        size_t file_len, remainder, buffer_size;

        f = file_open(files[i], FILE_READ);
        if (!f) {
            return 0;
        }

        file_len = file_size(f);
        remainder = file_len % 1024;

        file_buffer = (uint8_t*) file_map(f, file_len, 0);
        if (!file_buffer) {
            file_close(f);
            return 0;
        }

        if (remainder == 0) {
            // 映射的缓冲区直接使用，无需填充
            dwBuf = (uint32_t*) file_buffer;
            buffer_size = file_len;
        } else {
            size_t extra = 1024 - remainder;
            uint8_t pad = (uint8_t) 0xFF;
            uint8_t *pad_dest;

            buffer_size = file_len + extra;
            dwBuf = (uint32_t*) malloc(buffer_size);
            if (!dwBuf) {
                file_unmap(f, file_buffer);
                file_close(f);
                return 0;
            }

            memcpy(dwBuf, file_buffer, file_len);
            file_unmap(f, file_buffer);
            file_buffer = (uint8_t*) 0; // 标记为已释放

            pad_dest = ((uint8_t*) dwBuf) + file_len;
            for (size_t j = file_len; j < buffer_size; j++) {
                *pad_dest++ = pad--;
            }
        }

        current = dwBuf;
        // [注意] buffer_size 必须保证是 4 的倍数，否则 current++ 可能越界
        // 由于 buffer_size 要么是 file_len (需为4倍数)，要么是填充后的(肯定为1024倍数)，通常没问题
        for (size_t j = 0; j < buffer_size; j += 4) {
            values[3] = LSB4(*(current++));
            for (int k = 0; k < curFormula; k++) {
                switch (ops[k]) {
                case '+':
                    values[ovd[k]] = values[ovs1[k]] + values[ovs2[k]];
                    break;
                case '-':
                    values[ovd[k]] = values[ovs1[k]] - values[ovs2[k]];
                    break;
                case '^':
                    values[ovd[k]] = values[ovs1[k]] ^ values[ovs2[k]];
                    break;
                case '*':
                    values[ovd[k]] = values[ovs1[k]] *values[ovs2[k]];
                    break;
                case '/':
                    if (values[ovs2[k]] != 0) // 防止除以零
                        values[ovd[k]] = values[ovs1[k]] / values[ovs2[k]];
                    break;
                default:
                    if (file_buffer)
                        file_unmap(f, file_buffer);
                    else if (dwBuf)
                        free(dwBuf);
                    file_close(f);
                    return 0;
                }
            }
        }

        if (file_buffer)
            file_unmap(f, file_buffer);
        else if (dwBuf && file_buffer == 0)
            free(dwBuf); // 释放填充缓冲区

        file_close(f);
    }

    *checksum = (unsigned long) LSB4(values[2]);
#if DEBUG
    bncsutil_debug_message_a("\tChecksum = %lu", *checksum);
#endif
    return 1;
}

MEXP(int) checkRevisionFlat(const char *valueString, const char *file1,
                  const char *file2, const char *file3, int mpqNumber, unsigned long* checksum)
{
    const char *files[] =
        {file1, file2, file3};
    return checkRevision(valueString, files, 3, mpqNumber, checksum);
}

MEXP(int) getExeInfo(const char *file_name, char *exe_info,
           size_t exe_info_size, uint32_t *version, int platform)
{
    const char *base = (char*) 0;
    unsigned long file_size;
    FILE *f = (FILE*) 0;
    int ret;
#ifdef MOS_WINDOWS
    HANDLE hFile;
    FILETIME ft;
    SYSTEMTIME st;
    LPBYTE buf;
    VS_FIXEDFILEINFO *ffi;
    DWORD infoSize, bytesRead;
#else
    cm_pe_t pe;
    cm_pe_resdir_t *root;
    cm_pe_version_t ffi;
    std::memset(&ffi, 0, sizeof(ffi));
    size_t i;
    struct stat st;
    struct tm *time;
#endif

    if (!file_name || !exe_info || !exe_info_size || !version)
        return 0;

    base = get_basename(file_name);

    switch (platform) {
    case BNCSUTIL_PLATFORM_X86:
#ifdef MOS_WINDOWS
        infoSize = GetFileVersionInfoSize(file_name, &bytesRead);
        if (infoSize == 0)
            return 0;
        buf = (LPBYTE) VirtualAlloc(NULL, infoSize, MEM_COMMIT,
                                    PAGE_READWRITE);
        if (buf == NULL)
            return 0;
        if (GetFileVersionInfo(file_name, 0, infoSize, buf) == FALSE)
            return 0;
        if (!VerQueryValue(buf, "\\", (LPVOID*) &ffi, (PUINT) &infoSize))
            return 0;

        if (ffi) {
            *version =
                ((HIWORD(ffi->dwProductVersionMS) & 0xFF) << 24) |
                ((LOWORD(ffi->dwProductVersionMS) & 0xFF) << 16) |
                ((HIWORD(ffi->dwProductVersionLS) & 0xFF) << 8) |
                (LOWORD(ffi->dwProductVersionLS) & 0xFF);
#if DEBUG
            bncsutil_debug_message_a("%s version = %d.%d.%d.%d (0x%08X)",
                                     base, (HIWORD(ffi->dwProductVersionMS) & 0xFF),
                                     (LOWORD(ffi->dwProductVersionMS) & 0xFF),
                                     (HIWORD(ffi->dwProductVersionLS) & 0xFF),
                                     (LOWORD(ffi->dwProductVersionLS) & 0xFF),
                                     *version);
#endif
        } else {
            *version = 0;  // 如果 ffi 为空，设置版本为 0
        }
        VirtualFree(buf, 0lu, MEM_RELEASE);
        break;  // 必须添加 break 语句
#else
        // 使用独立的作用域来封装变量定义
        {
            bool version_found = false;
            cm_pe_resdir_t *dir = NULL;

            pe = cm_pe_load(file_name);
            if (!pe)
                return 0;
            root = cm_pe_load_resources(pe);
            if (!root) {
                cm_pe_unload(pe);
                return 0;
            }

            for (i = 0; i < root->subdir_count; i++) {
                dir = (root->subdirs + i);
                if (dir->name == 16 && dir->subdirs && dir->subdirs->resources) {
                    if (cm_pe_fixed_version(pe, dir->subdirs->resources, &ffi)) {
                        version_found = true;
                        break;
                    }
                }
            }

            if (version_found) {
                *version =
                    ((HIWORD(ffi.dwProductVersionMS) & 0xFF) << 24) |
                    ((LOWORD(ffi.dwProductVersionMS) & 0xFF) << 16) |
                    ((HIWORD(ffi.dwProductVersionLS) & 0xFF) << 8) |
                    (LOWORD(ffi.dwProductVersionLS) & 0xFF);
#if DEBUG
                bncsutil_debug_message_a("%s version = %d.%d.%d.%d (0x%08X)",
                                         base, (HIWORD(ffi.dwProductVersionMS) & 0xFF),
                                         (LOWORD(ffi.dwProductVersionMS) & 0xFF),
                                         (HIWORD(ffi.dwProductVersionLS) & 0xFF),
                                         (LOWORD(ffi.dwProductVersionLS) & 0xFF),
                                         *version);
#endif
            } else {
                *version = 0;  // 如果未找到版本信息，设置版本为 0
            }
            cm_pe_unload_resources(root);
            cm_pe_unload(pe);
        }
        break;  // 必须添加 break 语句
#endif

    case BNCSUTIL_PLATFORM_MAC:
    case BNCSUTIL_PLATFORM_OSX:
    {
        f = fopen(file_name, "r");
        if (!f)
            return 0;
        if (fseek(f, -4, SEEK_END) != 0) {
            fclose(f);
            return 0;
        }
        if (fread(version, 4, 1, f) != 1) {
            fclose(f);
            return 0;
        }
#ifdef MOS_WINDOWS
        fclose(f);
#endif
    }
    break;  // 必须添加 break 语句

    default:
        // 未知平台，设置默认版本
        *version = 0;
        break;
    }

#ifdef MOS_WINDOWS
    hFile = CreateFile(file_name, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return 0;
    file_size = GetFileSize(hFile, NULL);
    if (!GetFileTime(hFile, &ft, NULL, NULL)) {
        CloseHandle(hFile);
        return 0;
    }

    if (!FileTimeToSystemTime(&ft, &st)) {
        CloseHandle(hFile);
        return 0;
    }
    CloseHandle(hFile);

    ret = snprintf(exe_info, exe_info_size,
                   "%s %02u/%02u/%02u %02u:%02u:%02u %lu", base, st.wMonth,
                   st.wDay, (st.wYear % 100), st.wHour, st.wMinute, st.wSecond,
                   file_size);

#else
    if (!f)
        f = fopen(file_name, "r");
    if (!f)
        return 0;
    if (fseek(f, 0, SEEK_END) == -1) {
        fclose(f);
        return 0;
    }
    file_size = ftell(f);
    fclose(f);

    if (stat(file_name, &st) != 0)
        return 0;

    time = gmtime(&st.st_mtime);
    if (!time)
        return 0;

    switch (platform) {
    case BNCSUTIL_PLATFORM_MAC:
    case BNCSUTIL_PLATFORM_OSX:
        if (time->tm_year >= 100) // y2k
            time->tm_year -= 100;
        break;
    }

    ret = (int) snprintf(exe_info, exe_info_size,
                         "%s %02u/%02u/%02u %02u:%02u:%02u %lu", base,
                         (time->tm_mon+1), time->tm_mday, time->tm_year,
                         time->tm_hour, time->tm_min, time->tm_sec, file_size);
#endif

#if DEBUG
    bncsutil_debug_message(exe_info);
#endif

    return ret;
}

#ifdef __cplusplus
} // extern "C"
#endif

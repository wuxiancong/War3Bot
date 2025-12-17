/**
 * @file file.cpp
 * @brief 跨平台文件操作与内存映射封装
 *
 * 作用:
 * 此文件提供了一套统一的文件 IO 接口，屏蔽了 Windows (WinAPI) 和 Linux (POSIX) 之间的差异。
 *
 * 核心功能:
 * 1. file_open/file_close: 打开和关闭文件。
 * 2. file_read/file_write: 读写文件内容。
 * 3. file_map/file_unmap: **内存映射文件 (Memory Mapped File)**。
 *    这是该模块最重要的功能。它允许将大文件（如 War3.exe, Game.dll）直接映射到内存地址空间，
 *    从而像访问数组一样访问文件内容，极大地提高了哈希计算(CheckRevision)时的读取效率。
 */

#include <bncsutil/mutil.h>
#include <bncsutil/file.h>
#include <map>
#include <new>     // for std::bad_alloc
#include <cstdlib> // malloc/free
#include <cstring> // memcpy, strlen

// 使用更标准的宏 _WIN32 来检测 Windows 平台
#if defined(_WIN32) || defined(MOS_WINDOWS)
#define BWIN 1
#include <windows.h>
typedef std::map<const void*, HANDLE> mapping_map;
#else
#define BWIN 0
#include <cstdio>
#include <cstring>
#include <sys/mman.h> // Linux 内存映射头文件
typedef std::map<const void*, size_t> mapping_map;
#endif

struct _file
{
#if BWIN
    HANDLE f;
#else
    FILE* f;
#endif
    const char* filename;
    mapping_map mappings;
};

#ifdef __cplusplus
extern "C" {
#endif

#if BWIN

// Windows 实现部分
file_t file_open(const char *filename, unsigned int mode)
{
    file_t data;
    HANDLE file;
    DWORD access;
    DWORD share_mode;
    DWORD open_mode;
    size_t filename_buf_len;

    if (mode & FILE_READ) {
        access = GENERIC_READ;
        share_mode = FILE_SHARE_READ;
        open_mode = OPEN_EXISTING;
    } else if (mode & FILE_WRITE) {
        access = GENERIC_WRITE;
        share_mode = 0;
        open_mode = CREATE_ALWAYS;
    } else {
        return ((file_t) 0);
    }

    file = CreateFileA(filename, access, share_mode, NULL, open_mode,
                       FILE_ATTRIBUTE_NORMAL, NULL);

    if (file == INVALID_HANDLE_VALUE) {
        return ((file_t) 0);
    }

    try {
        data = new _file;
    } catch (const std::bad_alloc &) {
        CloseHandle(file);
        return (file_t) 0;
    }

    filename_buf_len = strlen(filename) + 1;
    data->filename = (const char*) malloc(filename_buf_len);
    if (!data->filename) {
        CloseHandle(file);
        delete data;
        return (file_t) 0;
    }

    memcpy((void*)data->filename, filename, filename_buf_len);

    data->f = file;

    return data;
}

void file_close(file_t file)
{
    mapping_map::iterator it;

    if (!file) {
        return;
    }

    for (it = file->mappings.begin(); it != file->mappings.end(); it++) {
        UnmapViewOfFile((*it).first);
        CloseHandle((*it).second);
    }

    CloseHandle((HANDLE) file->f);
    free((void*) file->filename);
    delete file;
}

size_t file_read(file_t file, void* ptr, size_t size, size_t count)
{
    DWORD bytes_read;
    if (!ReadFile(file->f, ptr, (DWORD) (size * count), &bytes_read, NULL)) {
        return (size_t) 0;
    }
    return (size_t) bytes_read;
}

size_t file_write(file_t file, const void *ptr, size_t size, size_t count)
{
    DWORD bytes_written;
    if (!WriteFile(file->f, ptr, (DWORD) (size * count), &bytes_written, NULL))
        return (size_t) 0;
    return (size_t) bytes_written;
}

size_t file_size(file_t file)
{
    return (size_t) GetFileSize(file->f, (LPDWORD) 0);
}

void* file_map(file_t file, size_t len, off_t offset)
{
    HANDLE mapping = CreateFileMapping((HANDLE) file->f, NULL, PAGE_READONLY, 0, 0, NULL);
    void* base;

    if (!mapping) {
        return (void*) 0;
    }

    base = MapViewOfFile(mapping, FILE_MAP_READ, 0, (DWORD) offset, len);
    if (!base) {
        CloseHandle(mapping);
        return (void*) 0;
    }

    file->mappings[base] = mapping;
    return base;
}

void file_unmap(file_t file, const void *base)
{
    mapping_map::iterator item = file->mappings.find(base);
    HANDLE mapping;

    if (item == file->mappings.end()) {
        return;
    }

    mapping = (*item).second;
    UnmapViewOfFile(base);
    CloseHandle(mapping);
    file->mappings.erase(item);
}

#else

// Linux/Unix 实现部分
file_t file_open(const char* filename, unsigned int mode_flags)
{
    char mode[] = "rb";
    file_t data;
    FILE* f;
    size_t filename_buf_len;

    if (mode_flags & FILE_WRITE)
        mode[0] = 'w';

    f = fopen(filename, mode);
    if (!f) {
        return (file_t) 0;
    }

    try {
        data = new _file;
    } catch (const std::bad_alloc&) {
        fclose(f);
        return (file_t) 0;
    }

    filename_buf_len = strlen(filename) + 1;
    data->filename = (const char*) malloc(filename_buf_len);
    if (!data->filename) {
        fclose(f);
        delete data;
        return (file_t) 0;
    }

    memcpy((void*)data->filename, filename, filename_buf_len);

    data->f = f;

    return data;
}

void file_close(file_t file)
{
    mapping_map::iterator it;

    if (!file) {
        return;
    }

    for (it = file->mappings.begin(); it != file->mappings.end(); it++) {
        munmap((void*) (*it).first, (*it).second);
    }

    fclose(file->f);
    free((void*) file->filename);
    delete file;
}

size_t file_read(file_t file, void* ptr, size_t size, size_t count)
{
    return fread(ptr, size, count, file->f);
}

size_t file_write(file_t file, const void* ptr, size_t size, size_t count)
{
    return fwrite(ptr, size, count, file->f);
}

size_t file_size(file_t file)
{
    long cur_pos = ftell(file->f);
    size_t size_of_file;

    fseek(file->f, 0, SEEK_END);
    size_of_file = (size_t) ftell(file->f);
    fseek(file->f, cur_pos, SEEK_SET);

    return size_of_file;
}

void* file_map(file_t file, size_t len, off_t offset)
{
    int fd = fileno(file->f);
    void* base = mmap((void*) 0, len, PROT_READ, MAP_SHARED, fd, offset);

    if (base == MAP_FAILED) {
        return (void*) 0;
    }

    file->mappings[base] = len;
    return base;
}

void file_unmap(file_t file, const void* base)
{
    mapping_map::iterator item = file->mappings.find(base);
    size_t len;

    if (item == file->mappings.end()) {
        return;
    }

    len = (*item).second;
    munmap((void*) base, len);
    file->mappings.erase(item);
}

#endif

#ifdef __cplusplus
}
#endif

/**
* @file pe.cpp
* @brief Portable Executable 解析器
*   作用：
*   负责解析 Windows 可执行文件（如 War3.exe, Game.dll）的内部结构。
*   主要功能：
*   它是版本验证的核心工具。
*   它读取游戏文件的资源段（Resource Section），提取出具体的文件版本号（File Version），这是通过战网 CheckRevision（版本检查）步骤的必要条件。没有它，机器人无法计算出正确的 EXE 哈希值。
 */

#include <bncsutil/pe.h>
#include <bncsutil/stack.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// 前向声明
int cm_pe_load_resdir(FILE *f, uint32_t offset, cm_pe_resdir_t *dir);

MEXP(cm_pe_t) cm_pe_load(const char *filename)
{
    cm_pe_t pe;
    cm_pe_header_t *header;
    cm_pe_optional_header_t *opt_header;
    cm_pe_windows_header_t *win_header;
    long pe_offset = 0;
    //size_t i;
    size_t count;

    pe = (cm_pe_t) malloc(sizeof(struct cm_pe));
    if (!pe)
        return nullptr;

    memset(pe, 0, sizeof(struct cm_pe));

    pe->f = fopen(filename, "r");
    if (!pe->f) {
        free(pe);
        return nullptr;
    }

    if (fseek(pe->f, 0x3CL, SEEK_SET) == -1)
        goto err_trap;

    if (fread(&pe_offset, 4, 1, pe->f) != 1)
        goto err_trap;

#if BIG_ENDIAN
    pe_offset = LSB4(pe_offset);
#endif

    if (fseek(pe->f, pe_offset, SEEK_SET) == -1)
        goto err_trap;

    if (fread(&pe->header, sizeof(cm_pe_header_t), 1, pe->f) != 1)
        goto err_trap;

    header = &pe->header;
#if BIGENDIAN
    /* Without regular expressions, this would've sucked. */
    header->signature = SWAP32(header->signature);
    header->machine = SWAP16(header->machine);
    header->section_count = SWAP16(header->section_count);
    header->timestamp = SWAP32(header->timestamp);
    header->symbol_table_offset = SWAP32(header->symbol_table_offset);
    header->symbol_count = SWAP32(header->symbol_count);
    header->optional_header_size = SWAP16(header->optional_header_size);
    header->characteristics = SWAP16(header->characteristics);
#endif

    if (header->optional_header_size > 0) {
        if (fread(&pe->optional_header, PE_OPTIONAL_HEADER_MIN_SIZE, 1, pe->f)
            != 1)
        {
            goto err_trap;
        }

        opt_header = &pe->optional_header;
        win_header = &pe->windows_header;

#if BIGENDIAN
        opt_header->magic = SWAP16(opt_header->magic);
#endif

        if (opt_header->magic == IMAGE_FORMAT_PE32) {
            if (fread(&opt_header->data_base, 4, 1, pe->f) != 1)
                goto err_trap;
            if (fread(&win_header->image_base, 4, 1, pe->f) != 1)
                goto err_trap;
            // The 40 is not a typo.
            if (fread(&win_header->section_alignment, 40, 1, pe->f) != 1)
                goto err_trap;
            if (fread(&win_header->stack_reserve_size, 4, 1, pe->f) != 1)
                goto err_trap;
            if (fread(&win_header->stack_commit_size, 4, 1, pe->f) != 1)
                goto err_trap;
            if (fread(&win_header->heap_reserve_size, 4, 1, pe->f) != 1)
                goto err_trap;
            if (fread(&win_header->heap_commit_size, 4, 1, pe->f) != 1)
                goto err_trap;
            if (fread(&win_header->loader_flags, 4, 1, pe->f) != 1)
                goto err_trap;
            if (fread(&win_header->data_directory_count, 4, 1, pe->f) != 1)
                goto err_trap;
        } else if (opt_header->magic == IMAGE_FORMAT_PE32_PLUS) {
            if (fread(win_header, sizeof(cm_pe_windows_header_t), 1, pe->f)!= 1)
                goto err_trap;
        } else {
            goto err_trap;
        }

#if BIGENDIAN
        opt_header->code_section_size = SWAP32(opt_header->code_section_size);
        opt_header->initialized_data_size =
            SWAP32(opt_header->initialized_data_size);
        opt_header->uninitialized_data_size =
            SWAP32(opt_header->uninitialized_data_size);
        opt_header->entry_point = SWAP32(opt_header->entry_point);
        opt_header->code_base = SWAP32(opt_header->code_base);
        opt_header->data_base = SWAP32(opt_header->data_base);

        win_header->image_base = SWAP64(win_header->image_base);
        win_header->section_alignment = SWAP32(win_header->section_alignment);
        win_header->file_alignment = SWAP32(win_header->file_alignment);
        win_header->major_os_version = SWAP16(win_header->major_os_version);
        win_header->minor_os_version = SWAP16(win_header->minor_os_version);
        win_header->major_image_version =
            SWAP16(win_header->major_image_version);
        win_header->minor_image_version =
            SWAP16(win_header->minor_image_version);
        win_header->major_subsystem_version =
            SWAP16(win_header->major_subsystem_version);
        win_header->minor_subsystem_version =
            SWAP16(win_header->minor_subsystem_version);
        win_header->reserved = SWAP32(win_header->reserved);
        win_header->image_size = SWAP32(win_header->image_size);
        win_header->headers_size = SWAP32(win_header->headers_size);
        win_header->checksum = SWAP32(win_header->checksum);
        win_header->subsystem = SWAP16(win_header->subsystem);
        win_header->dll_characteristics =
            SWAP16(win_header->dll_characteristics);
        win_header->stack_reserve_size = SWAP64(win_header->stack_reserve_size);
        win_header->stack_commit_size = SWAP64(win_header->stack_commit_size);
        win_header->heap_reserve_size = SWAP64(win_header->heap_reserve_size);
        win_header->heap_commit_size = SWAP64(win_header->heap_commit_size);
        win_header->loader_flags = SWAP32(win_header->loader_flags);
        win_header->data_directory_count =
            SWAP32(win_header->data_directory_count);
#endif

        if (win_header->data_directory_count > 0) {
            count = win_header->data_directory_count;
            pe->data_directories = (cm_pe_data_directory_t*)
                calloc(sizeof(cm_pe_data_directory_t), count);

            if (!pe->data_directories)
                goto err_trap;

            if (fread(pe->data_directories, sizeof(cm_pe_data_directory_t),
                      count, pe->f) != count)
            {
                goto dir_err_trap;
            }

#if BIGENDIAN
            for (i = 0; i < count; i++) {
                pe->data_directories[i].rva =
                    SWAP32(pe->data_directories[i].rva);
                pe->data_directories[i].size =
                    SWAP32(pe->data_directories[i].size);
            }
#endif
        }

        count = (size_t) header->section_count;
        if (count) {
            pe->sections = (cm_pe_section_t*) calloc(sizeof(cm_pe_section_t),
                                                      count);

            if (!pe->sections)
                goto dir_err_trap;

            if (fread(pe->sections, sizeof(cm_pe_section_t), count, pe->f)
                != count)
            {
                goto sect_err_trap;
            }
        }

#if BIGENDIAN
        for (i = 0; i < count; i++) {
            pe->sections[i].virtual_size = SWAP32(pe->sections[i].virtual_size);
            pe->sections[i].virtual_address =
                SWAP32(pe->sections[i].virtual_address);
            pe->sections[i].raw_data_size =
                SWAP32(pe->sections[i].raw_data_size);
            pe->sections[i].raw_data_offset =
                SWAP32(pe->sections[i].raw_data_offset);
            pe->sections[i].relocations_offset =
                SWAP32(pe->sections[i].relocations_offset);
            pe->sections[i].line_numbers_offset =
                SWAP32(pe->sections[i].line_numbers_offset);
            pe->sections[i].relocation_count =
                SWAP16(pe->sections[i].relocation_count);
            pe->sections[i].line_number_count =
                SWAP16(pe->sections[i].line_number_count);
            pe->sections[i].characteristics =
                SWAP32(pe->sections[i].characteristics);
        }
#endif
    }

    return pe;
sect_err_trap:
    free(pe->sections);
dir_err_trap:
    free(pe->data_directories);
err_trap:
    fclose(pe->f);
    free(pe);
    return nullptr;
}

MEXP(void) cm_pe_unload(cm_pe_t pe)
{
    if (pe->data_directories)
        free(pe->data_directories);
    if (pe->sections)
        free(pe->sections);
    if (pe->f)
        fclose(pe->f);
    free(pe);
}

MEXP(cm_pe_section_t*) cm_pe_get_section(cm_pe_t pe, const char *name) {
    unsigned int i;
    cm_pe_section_t *s;
    uint32_t section_count = pe->header.section_count;

    if (!pe || !pe->sections)
        return nullptr;

    for (i = 0, s = pe->sections; i < section_count; i++, s++) {
        if (strcmp(s->name, name) == 0)
            return s;
    }

    return nullptr;
}

MEXP(cm_pe_resdir_t*) cm_pe_load_resources(cm_pe_t pe)
{
    cm_pe_section_t *sect;
    cm_pe_resdir_t *root = nullptr;
    cm_pe_resdir_t *dir;
    cm_pe_resdir_t *subdirs;
    cm_pe_res_t res;
    cm_pe_res_t *resources;
    cm_stack_t stack;
    size_t i;
    uint32_t base;

    // no need to check validity of pe pointer; cm_pe_get_section does this
    sect = cm_pe_get_section(pe, ".rsrc");
    if (!sect)
        return nullptr;

    root = (cm_pe_resdir_t*) malloc(sizeof(cm_pe_resdir_t));
    if (!root)
        return nullptr;

    base = sect->raw_data_offset;
    if (!cm_pe_load_resdir(pe->f, base, root)) {
        free(root);
        return nullptr;
    }

    stack = cm_stack_create();
    if (!stack) {
        free(root);
        return nullptr;
    }

    cm_stack_push(stack, root);

    // C++ 中 stack_pop 返回 void*，必须强转
    while ( (dir = (cm_pe_resdir_t*) cm_stack_pop(stack)) ) {
        while (dir->subdir_count + dir->resource_count <
               dir->named_entry_count + dir->id_entry_count)
        {
            if (fseek(pe->f, dir->offset, SEEK_SET) == -1) {
                cm_pe_unload_resources(root);
                cm_stack_destroy(stack);
                return nullptr;
            }

            if (fread(&res, CM_RES_REAL_SIZE, 1, pe->f) != 1) {
                cm_pe_unload_resources(root);
                cm_stack_destroy(stack);
                return nullptr;
            }

#if BIGENDIAN
            res.name = SWAP4(res.name);
            res.offset = SWAP4(res.offset);
#endif
            if (res.offset & 0x80000000) {
                // subdirectory
                i = dir->subdir_count++;
                subdirs = (cm_pe_resdir_t*) realloc(dir->subdirs,
                                                     sizeof(cm_pe_resdir_t)* dir->subdir_count);
                if (!subdirs) {
                    cm_pe_unload_resources(root);
                    cm_stack_destroy(stack);
                    return nullptr;
                }
                dir->subdirs = subdirs;

                cm_stack_push(stack, dir);
                dir->offset += CM_RES_REAL_SIZE;
                dir = (subdirs + i);

                res.offset &= 0x7FFFFFFF;
                res.file_offset = base + res.offset;

                if (!cm_pe_load_resdir(pe->f, res.file_offset, dir)) {
                    cm_pe_unload_resources(root);
                    cm_stack_destroy(stack);
                    return nullptr;
                }
                dir->name = res.name;

                cm_stack_push(stack, dir);
                break;
            }
            // real resource
            res.file_offset = base + res.offset;
            i = dir->resource_count++;
            resources = (cm_pe_res_t*) realloc(dir->resources,
                                                sizeof(cm_pe_res_t)* dir->resource_count);
            if (!resources) {
                cm_pe_unload_resources(root);
                cm_stack_destroy(stack);
                return nullptr;
            }
            dir->resources = resources;
            memcpy(dir->resources + i, &res, sizeof(cm_pe_res_t));
            dir->offset += CM_RES_REAL_SIZE;
        }
    }

    cm_stack_destroy(stack);
    return root;
}

MEXP(int) cm_pe_unload_resources(cm_pe_resdir_t *root)
{
    cm_pe_resdir_t *dir;
    cm_stack_t stack;

    stack = cm_stack_create();
    if (!stack)
        return 0;

    cm_stack_push(stack, root);

    while ( (dir = (cm_pe_resdir_t*) cm_stack_pop(stack)) ) {
        if (dir->subdir_count) {
            dir->subdir_count--;
            cm_stack_push(stack, dir);
            cm_stack_push(stack, (dir->subdirs + dir->subdir_count));
            continue;
        }

        if (dir->subdirs) {
            free(dir->subdirs);
            dir->subdirs = nullptr;
        }

        if (dir->resources) {
            free(dir->resources);
            dir->resource_count = 0;
        }
    }

    cm_stack_destroy(stack);
    free(root);
    return 1;
}

MEXP(int) cm_pe_fixed_version(cm_pe_t pe, cm_pe_res_t *res,
                    cm_pe_version_t *ver)
{
    // C++ 指针运算需要注意类型，这里 pe->sections 是数组首地址
    cm_pe_section_t *sect = (pe->sections + 3);
#if BIGENDIAN
    uint32_t check = 0xBD04EFFE;
#else
    uint32_t check = 0xFEEF04BD;
#endif
    uint32_t rva;
    uint32_t size;
    uint32_t offset;
    uint32_t align;

    if (!pe || !res || !ver)
        return 0;

    if (fseek(pe->f, res->file_offset, SEEK_SET) == -1)
        return 0;
    if (fread(&rva, 4, 1, pe->f) != 1)
        return 0;
    if (fread(&size, 4, 1, pe->f) != 1)
        return 0;
#if BIGENDIAN
    rva = SWAP4(rva);
    size = SWAP4(size);
#endif

    offset = sect->raw_data_offset + (rva - sect->virtual_address) + 0x26;
    align = 4 -(offset & 0xF % 4);
    if (align < 4)
        offset += align;
    if (fseek(pe->f, offset, SEEK_SET) == -1)
        return 0;
    if (fread(ver, sizeof(cm_pe_version_t), 1, pe->f) != 1)
        return 0;

    if (ver->dwSignature != check)
        return 0;

#if BIGENDIAN
    ver->dwSignature = SWAP32(ver->dwSignature);
    ver->dwStrucVersion = SWAP32(ver->dwStrucVersion);
    ver->dwFileVersionMS = SWAP32(ver->dwFileVersionMS);
    ver->dwFileVersionLS = SWAP32(ver->dwFileVersionLS);
    ver->dwProductVersionMS = SWAP32(ver->dwProductVersionMS);
    ver->dwProductVersionLS = SWAP32(ver->dwProductVersionLS);
    ver->dwFileFlagsMask = SWAP32(ver->dwFileFlagsMask);
    ver->dwFileFlags = SWAP32(ver->dwFileFlags);
    ver->dwFileOS = SWAP32(ver->dwFileOS);
    ver->dwFileType = SWAP32(ver->dwFileType);
    ver->dwFileSubtype = SWAP32(ver->dwFileSubtype);
    ver->dwFileDateMS = SWAP32(ver->dwFileDateMS);
    ver->dwFileDateLS = SWAP32(ver->dwFileDateLS);
#endif

    return 1;
}

int cm_pe_load_resdir(FILE *f, uint32_t offset, cm_pe_resdir_t *dir)
{
    memset(dir, 0, sizeof(cm_pe_resdir_t));

    if (fseek(f, offset, SEEK_SET) == -1)
        return 0;

    if (fread(dir, 16, 1, f) != 1)
        return 0;

#if BIGENDIAN
    dir->characteristics = SWAP32(dir->characteristics);
    dir->timestamp = SWAP32(dir->timestamp);
    dir->major_version = SWAP16(dir->major_version);
    dir->minor_version = SWAP16(dir->minor_version);
    dir->named_entry_count = SWAP16(dir->named_entry_count);
    dir->id_entry_count = SWAP16(dir->id_entry_count);
#endif

    dir->offset = (uint32_t) ftell(f);

    return 1;
}

#ifndef BNCSUTIL_CHECKREVISION_H
#define BNCSUTIL_CHECKREVISION_H
#include <bncsutil/mutil.h>
#ifdef __cplusplus


extern "C" {
#endif

/**
 * getExeInfo() 函数使用的平台常量
 */
#define BNCSUTIL_PLATFORM_X86 1        /* x86 架构 */
#define BNCSUTIL_PLATFORM_WINDOWS 1    /* Windows 平台 */
#define BNCSUTIL_PLATFORM_WIN 1        /* Windows 平台（简写） */
#define BNCSUTIL_PLATFORM_MAC 2        /* Macintosh 平台 */
#define BNCSUTIL_PLATFORM_PPC 2        /* PowerPC 架构 */
#define BNCSUTIL_PLATFORM_OSX 3        /* Mac OS X 平台 */

/**
 * 从MPQ文件名中提取版本号
 *
 * @param mpqName MPQ文件名（例如 "IX86ver#.mpq"）
 * @return 提取出的数字值，失败返回 -1
 *
 * 示例：对于 "IX86ver2.mpq"，返回 2
 */
MEXP(int) extractMPQNumber(const char *mpqName);

/**
 * 运行 CheckRevision 校验
 *
 * @param valueString 校验值字符串
 * @param files 文件路径数组
 * @param numFiles 文件数量
 * @param mpqNumber MPQ版本号
 * @param checksum 输出校验和的指针
 * @return 操作结果代码
 *
 * 注意：第一个文件必须是可执行文件
 */
MEXP(int) checkRevision(
    const char *valueString,
    const char *files[],
    int numFiles,
    int mpqNumber,
    unsigned long* checksum
    );

/**
 * CheckRevision 函数的替代形式
 *
 * @param valueString 校验值字符串
 * @param file1 第一个文件路径
 * @param file2 第二个文件路径
 * @param file3 第三个文件路径
 * @param mpqNumber MPQ版本号
 * @param checksum 输出校验和的指针
 * @return 操作结果代码
 *
 * 主要对VB程序员有用；VB似乎难以将字符串数组传递给DLL函数
 */
MEXP(int) checkRevisionFlat(
    const char *valueString,
    const char *file1,
    const char *file2,
    const char *file3,
    int mpqNumber,
    unsigned long* checksum
    );

/**
 * 从可执行文件中检索版本和日期/大小信息
 *
 * @param file_name 可执行文件路径
 * @param exe_info 输出信息字符串的缓冲区
 * @param exe_info_size 缓冲区大小
 * @param version 输出版本号的指针
 * @param platform 平台标识符
 * @return 成功返回exeInfoString的长度，失败返回0
 *
 * 注意：如果生成的字符串比缓冲区长，将返回所需的缓冲区长度，
 *       但不会将字符串复制到exeInfoString中。应用程序应检查
 *       返回值是否大于缓冲区长度，并在必要时增加缓冲区大小。
 */
MEXP(int) getExeInfo(const char *file_name,
           char *exe_info,
           size_t exe_info_size,
           uint32_t* version,
           int platform);

/**
 * 获取给定MPQ文件的种子值
 *
 * @param mpq_number MPQ版本号
 * @return 种子值，如果该MPQ在BNCSutil中未注册则返回0
 */
MEXP(long) get_mpq_seed(int mpq_number);

/**
 * 设置给定MPQ文件的种子值
 *
 * @param mpq_number MPQ版本号
 * @param new_seed 新的种子值
 * @return 原有的种子值（如果存在），如果是新添加的则返回0
 */
MEXP(long) set_mpq_seed(int mpq_number, long new_seed);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* BNCSUTIL_CHECKREVISION_H */

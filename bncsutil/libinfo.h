#ifndef BNCSUTIL_LIBINFO_H
#define BNCSUTIL_LIBINFO_H
#include <bncsutil/mutil.h>
#ifdef __cplusplus
extern "C" {
#endif

// 库版本号定义
// 版本号格式说明：对于版本 a.b.c，定义为：
// (a * 10^4) + (b * 10^2) + c
// 示例：版本 1.2.0：
#define BNCSUTIL_VERSION  10300

// 获取库版本号（数值形式）
// @return 返回库版本号的数值表示（格式如上所述）
MEXP(unsigned long) bncsutil_getVersion();

// 获取库版本号（字符串形式）
// @param outbuf 输出缓冲区（应至少9字节长度）
// @return 成功返回复制到outbuf的字节数，失败返回0
//
// 注意：版本字符串格式为"a.b.c"，例如"1.2.0"
MEXP(int) bncsutil_getVersionString(char* outbuf);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* BNCSUTIL_LIBINFO_H */

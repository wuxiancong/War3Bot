#ifndef BNCSUTIL_LIBINFO_H
#define BNCSUTIL_LIBINFO_H
#include <bncsutil/mutil.h>
#ifdef __cplusplus
extern "C" {
#endif

// Library Version
// Defined as follows, for library version a.b.c:
// (a * 10^4) + (b * 10^2) + c
// Version 1.2.0:
#define BNCSUTIL_VERSION  10300

// Get Version
MEXP(unsigned long) bncsutil_getVersion();

// Get Version as String
// Copies version into outBuf, returns number of bytes copied (or 0 on fail).
// (outBuf should be at least 9 bytes long)
MEXP(int) bncsutil_getVersionString(char* outbuf);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* BNCSUTIL_LIBINFO_H */

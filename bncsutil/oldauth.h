#ifndef BNCSUTIL_OLDAUTH_H
#define BNCSUTIL_OLDAUTH_H
#include <bncsutil/mutil.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Double-hashes the given password using the given
 * server and client tokens.
 */
MEXP(void) doubleHashPassword(const char* password, uint32_t clientToken,
    uint32_t serverToken, char* outBuffer);

/**
 * Single-hashes the password for account creation and password changes.
 */
MEXP(void) hashPassword(const char* password, char* outBuffer);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* BNCSUTIL_OLDAUTH_H */

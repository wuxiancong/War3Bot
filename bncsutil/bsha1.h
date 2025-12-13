#ifndef BSHA1_H
#define BSHA1_H
#include <bncsutil/mutil.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * calcHashBuf
 *
 * Calculates a "Broken SHA-1" hash of data.
 *
 * Paramaters:
 * data: The data to hash.
 * length: The length of data.
 * hash: Buffer, at least 20 bytes in length, to receive the hash.
 */
MEXP(void) calcHashBuf(const char* data, size_t length, char* hash);
	
/*
 * New implementation.  Broken.  No plans to fix.
 */
MEXP(void) bsha1_hash(const char* input, unsigned int length, char* result);

#ifdef __cplusplus
} // extern "C"
#endif

#endif

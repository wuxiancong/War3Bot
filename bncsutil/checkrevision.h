#ifndef BNCSUTIL_CHECKREVISION_H
#define BNCSUTIL_CHECKREVISION_H
#include <bncsutil/mutil.h>
#ifdef __cplusplus


extern "C" {
#endif
 
/**
 * Platform constants for getExeInfo().
 */
#define BNCSUTIL_PLATFORM_X86 1
#define BNCSUTIL_PLATFORM_WINDOWS 1
#define BNCSUTIL_PLATFORM_WIN 1
#define BNCSUTIL_PLATFORM_MAC 2
#define BNCSUTIL_PLATFORM_PPC 2
#define BNCSUTIL_PLATFORM_OSX 3

/**
 * Reads an MPQ filename (e.g. IX86ver#.mpq), extracts the #,
 * and returns the int value of that number.  Returns -1 on
 * failure.
 */
MEXP(int) extractMPQNumber(const char* mpqName);

/**
 * Runs CheckRevision.
 * First file must be the executable file.
 */
MEXP(int) checkRevision(
    const char* valueString,
    const char* files[],
    int numFiles,
    int mpqNumber,
    unsigned long* checksum
);

/**
 * Alternate form of CheckRevision function.
 * Really only useful for VB programmers;
 * VB seems to have trouble passing an array
 * of strings to a DLL function
 */
MEXP(int) checkRevisionFlat(
    const char* valueString,
    const char* file1,
	const char* file2,
	const char* file3,
    int mpqNumber,
    unsigned long* checksum
);

/**
 * Retrieves version and date/size information from executable file.
 * Returns 0 on failure or length of exeInfoString.
 * If the generated string is longer than the buffer, the needed buffer
 * length will be returned, but the string will not be copied into
 * exeInfoString.  Applications should check to see if the return value
 * is greater than the length of the buffer, and increase its size if
 * necessary.
 */
MEXP(int) getExeInfo(const char* file_name,
					 char* exe_info,
					 size_t exe_info_size,
					 uint32_t* version,
					 int platform);
					
/**
 * Gets the seed value for the given MPQ file.  If no seed value for the given
 * MPQ is registered with BNCSutil, returns 0.
 */
MEXP(long) get_mpq_seed(int mpq_number);

/**
 * Sets the seed value for the given MPQ file.  Returns the old seed value if
 * there was any, or 0 if this was a new addition.
 */
MEXP(long) set_mpq_seed(int mpq_number, long new_seed);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* BNCSUTIL_CHECKREVISION_H */

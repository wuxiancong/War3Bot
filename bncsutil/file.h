#ifndef _FILE_H_INCLUDED_
#define _FILE_H_INCLUDED_ 1

#include <bncsutil/mutil.h>
#include <cstdio> // FILE*

#ifdef MOS_WINDOWS
typedef long off_t;
#else
#include <sys/types.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

typedef struct _file* file_t;

#define FILE_READ	(0x01)
#define FILE_WRITE	(0x02)


file_t file_open(const char* filename, unsigned int mode);
void file_close(file_t file);
size_t file_read(file_t file, void* ptr, size_t size, size_t count);
size_t file_write(file_t file, const void* ptr, size_t size,
                  size_t count);
size_t file_size(file_t file);

void* file_map(file_t file, size_t len, off_t offset);
void file_unmap(file_t file, const void* mapping);

#ifdef __cplusplus
}
#endif

#endif /* FILE */

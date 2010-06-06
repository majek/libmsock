#ifndef _IO_H
#define _IO_H

#include <stdint.h>

DLL_LOCAL int io_open(const char *pathname, int flags, int mode, int *errno_ptr);
DLL_LOCAL int io_fsync(int fd, int *errno_ptr);
DLL_LOCAL int io_pread(int fd, char *buf, uint64_t count, uint64_t offset,
		       int *errno_ptr);

#endif // _IO_H

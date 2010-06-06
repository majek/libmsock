#define _XOPEN_SOURCE 500    // for pread
#define _FILE_OFFSET_BITS 64

#include "config.h"

#include <features.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

DLL_LOCAL int io_open(const char *pathname, int flags, int mode, int *errno_ptr)
{
	int r = open(pathname, flags, mode);
	if (r == -1) {
		*errno_ptr = errno;
	}
	return r;
}

DLL_LOCAL int io_fsync(int fd, int *errno_ptr)
{
	int r = fsync(fd);
	if (r != 0) {
		*errno_ptr = errno;
	}
	return r;
}

DLL_LOCAL int io_pread(int fd, char *buf, uint64_t count, uint64_t offset,
		       int *errno_ptr)
{
	int r =pread(fd, buf, count, offset);
	if (r == -1) {
		*errno_ptr = errno;
	}
	return r;
}

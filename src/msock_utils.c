#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

#include "msock_internal.h"



DLL_LOCAL void fatal(const char *fmt, ...)
{
	fflush(stdout);

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\nQUITTING!\n");
	va_end(ap);

	fflush(stdout);
	fflush(stderr);
	exit(-1);
}

DLL_LOCAL void pfatal(const char *fmt, ...)
{
	fflush(stdout);
	fprintf(stderr, "%s: ", strerror(errno));

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\nQUITTING!\n");
	va_end(ap);

	fflush(stdout);
	fflush(stderr);
	exit(-1);
}

DLL_LOCAL void safe_printf(const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	int r = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	write(1, buf, r);
}

DLL_LOCAL int get_max_open_files()
{
	struct rlimit rl = {0, 0};
	int r = getrlimit(RLIMIT_NOFILE, &rl);
	if (r != 0) {
		pfatal("getrlimit()");
	}
	return rl.rlim_max;
}

DLL_LOCAL unsigned long long now_msecs()
{
	struct timespec ts = {0, 0};
	int r = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (r != 0) {
		pfatal("clock_gettime(CLOCK_MONOTONIC)");
	}
	return (unsigned long long)ts.tv_sec * 1000L + \
		(unsigned long long)ts.tv_nsec / 1000000;
}

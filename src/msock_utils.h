#ifndef _MSOCK_UTILS_H
#define _MSOCK_UTILS_H

#include <stdlib.h>

#define type_malloc(type)				\
	((type*)msock_safe_malloc(sizeof(type)))

#define type_free(type, ptr)				\
	do {						\
		type *a = ptr;				\
		msock_safe_free(sizeof(type), a);	\
	} while (0)


DLL_LOCAL void fatal(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
DLL_LOCAL void pfatal(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

DLL_LOCAL void safe_printf(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
DLL_LOCAL int get_max_open_files();
DLL_LOCAL unsigned long long now_msecs();


static inline void *msock_safe_malloc(size_t size)
{
	void *ptr = calloc(1, size);
	if (unlikely(ptr == NULL)) {
		fatal("Memory allocation failed!");
	}
	return ptr;
}

static inline void msock_safe_free(size_t size, void *ptr)
{
	free(ptr);
}


#define SWAP(a, b)		\
	do {			\
		__typeof (a) c;	\
		c = (a);	\
		(a) = (b);	\
		(b) = (c);	\
	} while(0)

/* Some macros taken from Linux kernel. */
#define min(x, y) ({				\
			typeof(x) _x = (x);	\
			typeof(y) _y = (y);	\
			(void) (&_x == &_y);	\
			_x < _y ? _x : _y; })

#define max(x, y) ({				\
			typeof(x) _x = (x);	\
			typeof(y) _y = (y);	\
			(void) (&_x == &_y);	\
			_x > _y ? _x : _y; })



#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
#define BUILD_BUG_ON_ZERO(e) (sizeof(char[1 - 2 * !!(e)]) - 1)
#define __must_be_array(a) \
	BUILD_BUG_ON_ZERO(__builtin_types_compatible_p(typeof(a), typeof(&a[0])))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))


#define _NANO 1000000000LL
#define _MICRO 1000000LL
#define _MILLI 1000LL

/* Timespec subtraction in nanoseconds */
#define TIMESPEC_NSEC_SUBTRACT(a,b)				\
	(((a).tv_sec - (b).tv_sec) * _NANO + (a).tv_nsec - (b).tv_nsec)
/* Timespec subtract in milliseconds */
#define TIMESPEC_MSEC_SUBTRACT(a,b) (					\
		(((a).tv_sec - (b).tv_sec) * _MILLI)			\
		+ ((a).tv_nsec - (b).tv_nsec) / _MICRO)
/* Timespec subtract in seconds; truncate towards zero */
#define TIMESPEC_SEC_SUBTRACT(a,b) (					\
		(a).tv_sec - (b).tv_sec					\
		+ (((a).tv_nsec < (b).tv_nsec) ? -1 : 0))

#define TIMESPEC_BEFORE(a, b) (						\
		((a).tv_sec < (b).tv_sec) ||				\
		((a).tv_sec == (b).tv_sec &&				\
		 (a).tv_nsec < (b).tv_nsec))


#define TIMESPEC_ADD(a, b, nsecs) {			       \
		(a).tv_sec = (b).tv_sec + ((nsecs) / _NANO);   \
		(a).tv_nsec = (b).tv_nsec + ((nsecs) % _NANO); \
		(a).tv_sec += (a).tv_nsec / _NANO;	       \
		(a).tv_nsec %= _NANO;			       \
	}


#define fast_memcpy(_dest, _src, _size)			\
	do {						\
		void *dest = (_dest);			\
		void *src = (_src);			\
		size_t size = (_size);			\
		if (__builtin_constant_p(size)) {	\
			char *d = dest;			\
			char *s = src;			\
			int i;				\
			for (i=0; i < size; i++) {	\
				d[i] = s[i];		\
			}				\
		} else {				\
			memcpy(dest, src, size);	\
		}					\
	} while (0)

#define fast_memset(_dest, _c, _size)			\
	do {						\
		void *dest = (_dest);			\
		int c = (_c);				\
		size_t size = (_size);			\
		if (__builtin_constant_p(size)) {	\
			char *d = (void*)dest;		\
			int i;				\
			for (i=0; i < size; i++) {	\
				d[i] = c;		\
			}				\
		} else {				\
			memset(dest, c, size);		\
		}					\
	} while (0)

static inline unsigned long long _rdtsc(void)
{
     unsigned a, d;
     asm volatile("cpuid" : : "a" (0) : "bx", "cx", "dx");
     asm volatile("rdtsc" : "=a" (a), "=d" (d));

     return (((unsigned long long)a) | (((unsigned long long)d) << 32));
}

#endif // _MSOCK_UTILS_H

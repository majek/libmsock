#if __GNUC__ >= 4
    #define DLL_LOCAL __attribute__((visibility("hidden")))
    #define likely(e) __builtin_expect((e), 1)
    #define unlikely(e) __builtin_expect((e), 0)
    #define _prefetch(r) __builtin_prefetch(r)
#else
    #define DLL_LOCAL
    #define likely(e) (e)
    #define unlikely(e) (e)
    #define _prefetch(r) (r)
    #warning "Please set compiler specific macros!"
#endif

#if __GNUC__ >= 4
    #define DLL_PUBLIC __attribute__((visibility("default")))
#else
    #define DLL_PUBLIC
    #warning "Please set compiler specific macros!"
#endif

#ifdef VALGRIND
#warning "Compiling valgrind hacks. This results in slow code."
#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>
#include <valgrind/helgrind.h>
#endif

/* Must be a power of 2. Setting it to actual page size might be beneficial. */
#define PAGE_SIZE (4096)
/* To avoid fighting over cache lines, align chunks. */
#define CACHELINE_SIZE (64)


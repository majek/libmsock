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

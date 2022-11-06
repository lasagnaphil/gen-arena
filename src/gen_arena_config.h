#pragma once

// for integer typedefs
#include <stdint.h>

// For memory-related functions such as memcpy()
#include <string.h>

/* Aligned alloc/free functions. Note that the default implmeentation is platform/compiler dependent. */

#ifndef GEN_ARENA_CUSTOM_ALLOC
// Needed since MSVC is still not implementing aligned_alloc() in the C/C++ standard. Typical Windows crap.
#ifdef _WIN32

#include <corecrt_malloc.h>
inline void* gen_arena_aligned_alloc(size_t size, size_t alignment) {
    return _aligned_malloc(size, alignment);
}

inline void gen_arena_aligned_free(void* ptr) {
    _aligned_free(ptr);
}

#else
// Else, we can just use C11's aligned_alloc.
#include <stdlib.h>
inline void* gen_arena_aligned_alloc(size_t size, size_t alignment) {
    return aligned_alloc(alignment, size);
}

inline void gen_arena_aligned_free(void* ptr) {
    free(ptr);
}
#endif
#endif

/* Assert functions. The default implementation uses C's default one, but you might want to swap this out. */

#ifndef GEN_ARENA_CUSTOM_ASSERT

#include <assert.h>
#define gen_arena_assert assert
#endif

/* The clz (count-leading-zeroes) function.
 * We implement this via compiler intrinsics (so that they can be compiled to the arch-specific instructions) */

#ifndef GEN_ARENA_CUSTOM_CLZ
#if defined(_WIN32) && defined(_MSC_VER) && !defined(__clang__) // If Windows MSVC (not Clang)...
#include <intrin.h>
inline int gen_arena_clz(uint32_t value) {
    DWORD leading_zero = 0;
    if (_BitScanReverse(&leading_zero, value)) {
        return 31 - leading_zero;
    }
    else {
        return 32;
    }
}
#else

inline int gen_arena_clz(uint32_t value) { // GCC and Clang has this nice builtin function
    return __builtin_clz(value);
}

#endif
#endif

/* The logging function.
 * The default implementation prints out logs to stdout, but you probably might not want this behavior.
 * Feel free to swap this out with whatever log system you are using for your application or library. */

// Since we usually don't want to dirty stdout, we turn off logging by default.
// #define GEN_ARENA_ENABLE_LOGGING

#ifdef GEN_ARENA_ENABLE_LOGGING
#ifndef GEN_ARENA_CUSTOM_LOGGER
#include <cstdio>
template <class... Args>
void gen_arena_log(const char* fmt, Args&&... args) {
    constexpr int MAX_CHARS = 1000;
    static char buf[MAX_CHARS];
    int writelen = sprintf_s(buf, MAX_CHARS, fmt, args...);
    if (writelen >= 0) {
        printf("%s\n", buf);
    }
    else {
        printf("Error while sprintf_s in gen_arena_log. Format char: %s\n", fmt);
    }
}
#endif
#else
#define gen_arena_log(...)
#endif

/* TODO */

// #define GEN_ARENA_DONT_RETURN_REF_PTR_PAIR

/* TODO */

// #define GEN_ARENA_USE_TYPE_ID

/* Determines if we will force users to declare the constexpr type-id function gen_arena_type_id<T>().
 * If you are using type-id information, then it might be best to force users to declare this function (or else a compiler error will occur)
 * But if you are not using this feature, then the fallback implementation (which just returns zero) will work fine. */

// #define GEN_ARENA_FORCE_DECLARE_TYPE_ID_FUN


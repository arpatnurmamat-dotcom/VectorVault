/*
 * VectorVault — Platform Abstraction
 *
 * Compile-time detection of OS, CPU architecture, and endianness.
 * Included by all internal modules via core/platform.h.
 */

#ifndef VV_CORE_PLATFORM_H
#define VV_CORE_PLATFORM_H

/* ──────────────────────────────────────────────────────────────────────────
 * Operating System Detection
 * ────────────────────────────────────────────────────────────────────────── */
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #define VV_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #define VV_PLATFORM_MACOS
    #elif TARGET_OS_IPHONE
        #define VV_PLATFORM_IOS
    #endif
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <fcntl.h>
#elif defined(__linux__)
    #define VV_PLATFORM_LINUX
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <fcntl.h>
#elif defined(__ANDROID__)
    #define VV_PLATFORM_ANDROID
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #define VV_PLATFORM_BSD
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
#else
    #error "Unsupported operating system"
#endif

/* Generic POSIX flag (everything except Windows) */
#if !defined(VV_PLATFORM_WINDOWS)
    #define VV_PLATFORM_POSIX
#endif

/* ──────────────────────────────────────────────────────────────────────────
 * CPU Architecture Detection
 * ────────────────────────────────────────────────────────────────────────── */
#if defined(__x86_64__) || defined(_M_X64)
    #define VV_ARCH_X86_64
#elif defined(__i386__) || defined(_M_IX86)
    #define VV_ARCH_X86
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define VV_ARCH_ARM64
#elif defined(__arm__)
    #define VV_ARCH_ARM32
#else
    #define VV_ARCH_UNKNOWN
#endif

/* ──────────────────────────────────────────────────────────────────────────
 * Compiler Detection
 * ────────────────────────────────────────────────────────────────────────── */
#if defined(__clang__)
    #define VV_COMPILER_CLANG
#elif defined(__GNUC__)
    #define VV_COMPILER_GCC
#elif defined(_MSC_VER)
    #define VV_COMPILER_MSVC
#else
    #define VV_COMPILER_UNKNOWN
#endif

/* Likely / unlikely hints */
#if defined(VV_COMPILER_GCC) || defined(VV_COMPILER_CLANG)
    #define VV_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define VV_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define VV_LIKELY(x)   (x)
    #define VV_UNLIKELY(x) (x)
#endif

/* ──────────────────────────────────────────────────────────────────────────
 * Endianness Detection (file format uses little-endian explicit)
 * ────────────────────────────────────────────────────────────────────────── */
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define VV_LITTLE_ENDIAN_HOST 1
    #else
        #define VV_LITTLE_ENDIAN_HOST 0
    #endif
#elif defined(_WIN32)
    /* Windows is always little-endian on supported archs */
    #define VV_LITTLE_ENDIAN_HOST 1
#else
    /* Assume little-endian */
    #define VV_LITTLE_ENDIAN_HOST 1
#endif

#endif  /* VV_CORE_PLATFORM_H */

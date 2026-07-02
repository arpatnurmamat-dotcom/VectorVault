/*
 * VectorVault — Core Type Aliases
 *
 * Central definitions for types, constants, and magic numbers used across the
 * entire codebase.
 *
 * All integer types use explicit-width stdint for on-disk format stability.
 */

#ifndef VV_CORE_TYPES_H
#define VV_CORE_TYPES_H

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace vv {

/* ──────────────────────────────────────────────────────────────────────────
 * Page / Storage Constants
 * ────────────────────────────────────────────────────────────────────────── */
using PageID     = uint64_t;      /* Page identifier */
using VectorID   = uint64_t;      /* User-facing vector ID (64-bit) */
using InternalID = uint32_t;      /* HNSW internal sequential ID */

constexpr size_t   PAGE_SIZE          = 4096;          /* Fixed page size, bytes */
constexpr PageID   HEADER_PAGE_ID     = 0;             /* File header page */
constexpr PageID   FREELIST_PAGE_ID   = 1;             /* Free-list persistence page */
constexpr PageID   HNSW_PAGE_ID       = 2;             /* HNSW checkpoint header page */
constexpr PageID   BTREE_ROOT_PAGE_ID = 3;             /* B+tree root page */
constexpr PageID   FIRST_DATA_PAGE_ID = 4;             /* First page available for user data */

/* ──────────────────────────────────────────────────────────────────────────
 * File Format Magic
 * ────────────────────────────────────────────────────────────────────────── */
/* "VECVAULT" in ASCII. 8-byte signature at file offset 0. */
constexpr char FILE_MAGIC[8] = {'V', 'E', 'C', 'V', 'A', 'U', 'L', 'T'};

/* File format version */
constexpr uint32_t FILE_FORMAT_VERSION_MAJOR = 0;
constexpr uint32_t FILE_FORMAT_VERSION_MINOR = 1;

/* Byte-order check word (little-endian hosts: 0x04030201 in memory) */
constexpr uint32_t BYTE_ORDER_MARKER = 0x01020304;

/* ──────────────────────────────────────────────────────────────────────────
 * Distance Function Signature
 * ────────────────────────────────────────────────────────────────────────── */
using DistFn = float (*)(const float* a, const float* b, size_t dim);

/* ──────────────────────────────────────────────────────────────────────────
 * HNSW Algorithm Defaults
 * ────────────────────────────────────────────────────────────────────────── */
constexpr uint32_t HNSW_DEFAULT_M               = 16;
constexpr uint32_t HNSW_DEFAULT_EF_CONSTRUCTION = 200;
constexpr uint32_t HNSW_DEFAULT_EF_SEARCH       = 100;

/* ──────────────────────────────────────────────────────────────────────────
 * Buffer Pool Configuration
 * ────────────────────────────────────────────────────────────────────────── */
constexpr size_t DEFAULT_POOL_SIZE = 2048;             /* frames (2048×4KB = 8 MB) */

/* ──────────────────────────────────────────────────────────────────────────
 * B+tree / Metadata
 * ────────────────────────────────────────────────────────────────────────── */
constexpr size_t INLINE_THRESHOLD = 128;  /* Metadata <= this is stored inline */
constexpr size_t BTREE_PAGE_SIZE  = PAGE_SIZE;

/* ──────────────────────────────────────────────────────────────────────────
 * Page Frame (in-memory)
 * ────────────────────────────────────────────────────────────────────────── */
struct Frame {
    PageID page_id  = static_cast<PageID>(-1);   /* Page ID, -1 means unused */
    char   data[PAGE_SIZE]{};                     /* Page contents */
    int    pin_count = 0;                         /* Reference count; can't evict if >0 */
    bool   is_dirty  = false;                     /* Modified since last flush? */
};

/* ──────────────────────────────────────────────────────────────────────────
 * Endianness Helpers (explicit little-endian on-disk)
 * ────────────────────────────────────────────────────────────────────────── */
#if defined(__GNUC__) || defined(__clang__)
inline uint16_t ByteSwap16(uint16_t v) { return __builtin_bswap16(v); }
inline uint32_t ByteSwap32(uint32_t v) { return __builtin_bswap32(v); }
inline uint64_t ByteSwap64(uint64_t v) { return __builtin_bswap64(v); }
#elif defined(_MSC_VER)
inline uint16_t ByteSwap16(uint16_t v) { return _byteswap_ushort(v); }
inline uint32_t ByteSwap32(uint32_t v) { return _byteswap_ulong(v); }
inline uint64_t ByteSwap64(uint64_t v) { return _byteswap_uint64(v); }
#else
inline uint16_t ByteSwap16(uint16_t v) {
    return static_cast<uint16_t>((v << 8) | (v >> 8));
}
inline uint32_t ByteSwap32(uint32_t v) {
    return ((v & 0xFF000000) >> 24) | ((v & 0x00FF0000) >> 8)
         | ((v & 0x0000FF00) << 8)  | ((v & 0x000000FF) << 24);
}
inline uint64_t ByteSwap64(uint64_t v) {
    return (static_cast<uint64_t>(ByteSwap32(v & 0xFFFFFFFF)) << 32)
         | static_cast<uint64_t>(ByteSwap32(v >> 32));
}
#endif

#if !VV_LITTLE_ENDIAN_HOST
/* Big-endian host: swap on read/write to/from on-disk (little-endian) format. */
template <typename T>
T FromDiskLE(T v) {
    static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8,
                  "Unsupported byte width");
    if constexpr (sizeof(T) == 1) return v;
    else if constexpr (sizeof(T) == 2) { uint16_t u; std::memcpy(&u, &v, 2); u = ByteSwap16(u); std::memcpy(&v, &u, 2); return v; }
    else if constexpr (sizeof(T) == 4) { uint32_t u; std::memcpy(&u, &v, 4); u = ByteSwap32(u); std::memcpy(&v, &u, 4); return v; }
    else { uint64_t u; std::memcpy(&u, &v, 8); u = ByteSwap64(u); std::memcpy(&v, &u, 8); return v; }
}
template <typename T>
T ToDiskLE(T v) { return FromDiskLE(v); }   /* symmetric */
#else
/* Little-endian host: no conversion needed. */
template <typename T>
inline T FromDiskLE(T v) { return v; }
template <typename T>
inline T ToDiskLE(T v) { return v; }
#endif

} // namespace vv

#endif  /* VV_CORE_TYPES_H */

/*
 * VectorVault — Public C API
 *
 * The stable ABI entry point for all applications.
 * Thread-safe: all public functions may be called concurrently from multiple
 * threads on the same vv_t handle (internally protected by rwlock).
 *
 * Version: 0.1.0
 * License: MIT
 */

#ifndef VECTORVAULT_H
#define VECTORVAULT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──────────────────────────────────────────────────────────────────────────
 * ABI Stability Marker
 * Bump MAJOR for breaking changes, MINOR for backward-compatible additions.
 * ────────────────────────────────────────────────────────────────────────── */
#define VECTORVAULT_VERSION_MAJOR 0
#define VECTORVAULT_VERSION_MINOR 1
#define VECTORVAULT_VERSION_PATCH 0

/* ──────────────────────────────────────────────────────────────────────────
 * Opaque Handle
 * Represents an opened database instance.
 * ────────────────────────────────────────────────────────────────────────── */
typedef struct vv_db vv_t;

/* ──────────────────────────────────────────────────────────────────────────
 * Error Codes
 * All API functions return VV_OK (0) on success, or a negative error code.
 * ────────────────────────────────────────────────────────────────────────── */
typedef enum {
    VV_OK                =   0,   /* Success */
    VV_ERR_IO            =  -1,   /* I/O failure (disk full, read error) */
    VV_ERR_NOMEM         =  -2,   /* Memory allocation failed */
    VV_ERR_INVALID_ARG   =  -3,   /* Invalid argument (NULL ptr, bad size) */
    VV_ERR_NOT_FOUND     =  -4,   /* Record not found */
    VV_ERR_EXISTS        =  -5,   /* Record already exists (duplicate ID) */
    VV_ERR_CORRUPT       =  -6,   /* Database file is corrupted */
    VV_ERR_WRONG_VERSION =  -7,   /* Unsupported file format version */
    VV_ERR_LOCKED        =  -8,   /* Database is locked by another process */
    VV_ERR_FILTER_PARSE  =  -9,   /* Filter expression syntax error */
    VV_ERR_OUT_OF_RANGE  = -10,   /* Parameter out of valid range */
    VV_ERR_INTERNAL      = -99    /* Internal error (bug report required) */
} vv_error_t;

/* ──────────────────────────────────────────────────────────────────────────
 * Distance Metric
 * ────────────────────────────────────────────────────────────────────────── */
typedef enum {
    VV_DISTANCE_L2     = 0,   /* Euclidean distance (squared) */
    VV_DISTANCE_COSINE = 1,   /* 1 - dot(a,b) / (||a||*||b||) */
    VV_DISTANCE_IP     = 2    /* Inner product (negated, so smaller = more similar) */
} vv_distance_t;

/* ──────────────────────────────────────────────────────────────────────────
 * Index Type
 * ────────────────────────────────────────────────────────────────────────── */
typedef enum {
    VV_INDEX_HNSW  = 0,   /* Default: ~O(log N) query, build overhead ~O(N log N) */
    VV_INDEX_FLAT  = 1    /* Brute-force: exact KNN, O(N) query */
} vv_index_t;

/* ──────────────────────────────────────────────────────────────────────────
 * Open Flags
 * ────────────────────────────────────────────────────────────────────────── */
typedef enum {
    VV_OPEN_READONLY  = 0x00,  /* Read-only mode (no mutations allowed) */
    VV_OPEN_READWRITE = 0x01,  /* Read-write mode (default) */
    VV_OPEN_CREATE    = 0x02,  /* Create file if not exists */
    VV_OPEN_TRUNCATE  = 0x04   /* Truncate existing database (DANGEROUS) */
} vv_open_flags_t;

/* ──────────────────────────────────────────────────────────────────────────
 * Configuration (passed to vv_open())
 * ────────────────────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t      dimension;         /* REQUIRED: vector dimension */
    vv_distance_t distance_metric;   /* Default: VV_DISTANCE_L2 */
    vv_index_t    index_type;        /* Default: VV_INDEX_HNSW */

    /* HNSW-specific parameters (ignored when index_type == VV_INDEX_FLAT) */
    uint32_t      hnsw_M;                 /* Per-level max neighbors. Default: 16 */
    uint32_t      hnsw_ef_construction;   /* Build-time search width. Default: 200 */

    /* Buffer pool configuration */
    uint32_t      buffer_pool_size_mb;    /* Memory budget. Default: 8 MB */
} vv_config_t;

/* ──────────────────────────────────────────────────────────────────────────
 * Search Result
 * ────────────────────────────────────────────────────────────────────────── */
typedef struct {
    uint64_t id;          /* User-provided vector ID */
    double   distance;    /* Distance (metric-dependent) */
} vv_result_t;

/* ──────────────────────────────────────────────────────────────────────────
 * Database Lifecycle
 * ────────────────────────────────────────────────────────────────────────── */

/**
 * Open (or create) a VectorVault database file.
 *
 * @param out_db    [out] receives the open handle on success
 * @param path      Path to the .vault file (UTF-8)
 * @param flags     Bitwise OR of vv_open_flags_t
 * @param config    Database configuration (dimension REQUIRED)
 * @return VV_OK on success, or negative error code
 */
int vv_open(vv_t** out_db, const char* path, uint32_t flags, const vv_config_t* config);

/**
 * Close the database, flushing all pending writes and WAL.
 * If a HNSW checkpoint is pending, serializes the index now.
 *
 * @return VV_OK on success
 */
int vv_close(vv_t* db);

/* ──────────────────────────────────────────────────────────────────────────
 * Vector Operations
 * ────────────────────────────────────────────────────────────────────────── */

/**
 * Insert a single vector with optional JSON metadata.
 *
 * @param id         Unique user-provided vector ID (must be > 0)
 * @param vector     Pointer to `dimension` floats
 * @param metadata   Optional JSON metadata string (NULL-terminated, can be NULL)
 * @return VV_OK, VV_ERR_EXISTS if id already present
 */
int vv_insert(vv_t* db, uint64_t id, const float* vector, const char* metadata);

/**
 * KNN search: find the k nearest vectors to query.
 *
 * @param query        Pointer to `dimension` floats
 * @param k            Number of results to return
 * @param filter_expr  Optional filter expression (NULL or "" = no filter)
 * @param ef_search    HNSW search width (higher = better recall, slower).
 *                     0 uses default (hnsw_ef_construction).
 * @param out_results  [out] caller-owned array of size >= k
 * @param out_count    [out] number of results actually filled
 * @return VV_OK on success
 */
int vv_search(
    vv_t*          db,
    const float*   query,
    uint32_t       k,
    const char*    filter_expr,
    uint32_t       ef_search,
    vv_result_t*   out_results,
    uint32_t*      out_count
);

/**
 * Fetch a single vector (and optional metadata) by id.
 *
 * @param id               Vector id
 * @param out_vector       [out] buffer of size `dimension` (can be NULL)
 * @param out_metadata     [out] receives metadata string (caller must vv_free()).
 *                          NULL if you don't want it.
 * @param out_metadata_len [out] length of out_metadata (excl. NUL)
 */
int vv_get(
    vv_t*       db,
    uint64_t    id,
    float*      out_vector,
    char**      out_metadata,
    size_t*     out_metadata_len
);

/**
 * Delete a vector by id.
 * Uses lazy tombstone deletion; physical reclaim via vv_compact().
 */
int vv_delete(vv_t* db, uint64_t id);

/**
 * Check if a vector exists.
 */
int vv_exists(vv_t* db, uint64_t id, int* out_exists);

/* ──────────────────────────────────────────────────────────────────────────
 * Maintenance
 * ────────────────────────────────────────────────────────────────────────── */

/**
 * Compact the database: reclaims tombstone space, re-packs pages.
 * This is a blocking operation.
 */
int vv_compact(vv_t* db);

/**
 * Flush dirty buffer-pool pages to disk.
 * Typically called by the user in a background thread, or by vv_close().
 */
int vv_sync(vv_t* db);

/* ──────────────────────────────────────────────────────────────────────────
 * Inspection
 * ────────────────────────────────────────────────────────────────────────── */

/**
 * Return the current vector count (excluding tombstones).
 */
int vv_count(vv_t* db, uint64_t* out_count);

/**
 * Return version / dimension / index configuration.
 */
int vv_info(
    vv_t*            db,
    uint32_t*        out_version_major,
    uint32_t*        out_version_minor,
    uint32_t*        out_dimension,
    vv_distance_t*   out_distance,
    vv_index_t*      out_index_type,
    uint64_t*        out_count,
    uint64_t*        out_file_size
);

/* ──────────────────────────────────────────────────────────────────────────
 * Utility
 * ────────────────────────────────────────────────────────────────────────── */

/**
 * Free a string returned by the API (e.g. vv_get metadata).
 * Safe to call with NULL.
 */
void vv_free(char* ptr);

/**
 * Translate error code to human-readable string.
 * Lifetime: static string, do NOT free.
 */
const char* vv_error_string(int errcode);

/**
 * Return library version string (static, do not free).
 * E.g. "0.1.0"
 */
const char* vv_version_string(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* VECTORVAULT_H */

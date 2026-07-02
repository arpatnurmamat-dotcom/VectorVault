/*
 * VectorVault — Database Implementation Header
 *
 * Defines the internal structure of vv_db (opaque in C API).
 * Only included by database.cpp and internal test files that need
 * to introspect the struct.
 */

#ifndef VV_CORE_DATABASE_H
#define VV_CORE_DATABASE_H

#include <vectorvault/vectorvault.h>

#include <shared_mutex>
#include <string>

#include "core/config.h"
#include "core/types.h"

namespace vv {

/* ──────────────────────────────────────────────────────────────────────────
 * Forward declarations (to be implemented in later phases)
 * ────────────────────────────────────────────────────────────────────────── */
namespace storage { class FileIO; class PageManager; class BufferPool; class FreeList; }
namespace index   { class VectorIndex; }
namespace meta    { class BPlusTree; class FilterParser; }
namespace query   { class QueryExecutor; }

/* ──────────────────────────────────────────────────────────────────────────
 * Database (vv_db opaque backing struct)
 * ────────────────────────────────────────────────────────────────────────── */
struct DatabaseImpl {
    std::string          path;             /* .vault file path */
    RuntimeConfig        config;           /* Validated config */
    uint32_t             open_flags = 0;   /* vv_open_flags_t bits */

    /* Phase 1 modules (to be populated) */
    storage::FileIO*     file_io       = nullptr;
    storage::PageManager* page_mgr    = nullptr;
    storage::BufferPool* buffer_pool  = nullptr;
    storage::FreeList*   free_list    = nullptr;

    /* Phase 2+ modules */
    index::VectorIndex*  vector_index  = nullptr;
    meta::BPlusTree*     meta_index    = nullptr;
    query::QueryExecutor* query_exec   = nullptr;

    /* Thread-safety: rwlock at database boundary (per Oracle recommendation).
     * readers (vv_search, vv_get, vv_exists, vv_count) take shared_lock.
     * writers (vv_insert, vv_delete, vv_compact) take unique_lock. */
    mutable std::shared_mutex rw_mutex;
};

} // namespace vv

#endif  /* VV_CORE_DATABASE_H */

/*
 * VectorVault — C API Implementations
 *
 * Stub file for Phase 0. Most functions return VV_ERR_NOT_FOUND or
 * VV_ERR_INTERNAL until the corresponding phase implements them.
 *
 * Structure: the C API wraps the internal vv::DatabaseImpl via the opaque vv_db.
 */

#include "core/database.h"
#include "core/error.h"

#include <cstdlib>
#include <cstring>
#include <new>

/* ──────────────────────────────────────────────────────────────────────────
 * Lifecycle
 * ────────────────────────────────────────────────────────────────────────── */

extern "C" int vv_open(
    vv_t**            out_db,
    const char*       path,
    uint32_t          flags,
    const vv_config_t* config
) {
    if (!out_db || !path || !config) return VV_ERR_INVALID_ARG;
    *out_db = nullptr;

    /* Validate and build config */
    vv::RuntimeConfig cfg;
    if (!vv::RuntimeConfig::Build(*config, cfg)) {
        return VV_ERR_INVALID_ARG;
    }

    /* For Phase 0: create the database implementation object but don't
     * actually open the file yet (FileIO comes in Phase 1).
     */
    auto* impl = new (std::nothrow) vv::DatabaseImpl();
    if (!impl) return VV_ERR_NOMEM;

    impl->path       = path;
    impl->config     = cfg;
    impl->open_flags = flags;

    /* TODO Phase 1: open file via FileIO, read/initialize header, set up BufferPool */

    *out_db = reinterpret_cast<vv_t*>(impl);
    return VV_OK;
}

extern "C" int vv_close(vv_t* db) {
    if (!db) return VV_ERR_INVALID_ARG;

    auto* impl = reinterpret_cast<vv::DatabaseImpl*>(db);
    /* TODO Phase 1+: flush WAL, destroy modules */
    delete impl;
    return VV_OK;
}

/* ──────────────────────────────────────────────────────────────────────────
 * Stubs for phases not yet implemented
 * ────────────────────────────────────────────────────────────────────────── */

extern "C" int vv_insert(vv_t*, uint64_t, const float*, const char*) {
    return VV_ERR_INTERNAL;  /* TODO Phase 2+ */
}

extern "C" int vv_search(vv_t*, const float*, uint32_t, const char*, uint32_t,
                          vv_result_t*, uint32_t* out_count) {
    if (out_count) *out_count = 0;
    return VV_ERR_INTERNAL;
}

extern "C" int vv_get(vv_t*, uint64_t, float*, char**, size_t*) {
    return VV_ERR_NOT_FOUND;
}

extern "C" int vv_delete(vv_t*, uint64_t) {
    return VV_ERR_INTERNAL;
}

extern "C" int vv_exists(vv_t*, uint64_t, int* out_exists) {
    if (out_exists) *out_exists = 0;
    return VV_ERR_INTERNAL;
}

extern "C" int vv_compact(vv_t*) {
    return VV_ERR_INTERNAL;
}

extern "C" int vv_sync(vv_t*) {
    return VV_ERR_INTERNAL;
}

extern "C" int vv_count(vv_t*, uint64_t* out_count) {
    if (out_count) *out_count = 0;
    return VV_ERR_INTERNAL;
}

extern "C" int vv_info(vv_t* db, uint32_t* maj, uint32_t* min, uint32_t* dim,
                        vv_distance_t* dist, vv_index_t* idx, uint64_t* count, uint64_t* fsize) {
    if (!db) return VV_ERR_INVALID_ARG;
    auto* impl = reinterpret_cast<vv::DatabaseImpl*>(db);
    if (maj)  *maj  = VECTORVAULT_VERSION_MAJOR;
    if (min)  *min  = VECTORVAULT_VERSION_MINOR;
    if (dim)  *dim  = impl->config.dimension;
    if (dist) *dist = impl->config.distance_metric;
    if (idx)  *idx  = impl->config.index_type;
    if (count) *count = 0;
    if (fsize) *fsize = 0;
    return VV_OK;
}

extern "C" void vv_free(char* ptr) {
    std::free(ptr);
}

/*
 * VectorVault — Runtime Configuration Container
 */

#ifndef VV_CORE_CONFIG_H
#define VV_CORE_CONFIG_H

#include <vectorvault/vectorvault.h>
#include "core/types.h"

namespace vv {

/**
 * Validated configuration for an opened database.
 */
struct RuntimeConfig {
    uint32_t        dimension             = 0;
    vv_distance_t   distance_metric       = VV_DISTANCE_L2;
    vv_index_t      index_type            = VV_INDEX_HNSW;

    /* HNSW tunables */
    uint32_t hnsw_M                = HNSW_DEFAULT_M;
    uint32_t hnsw_ef_construction  = HNSW_DEFAULT_EF_CONSTRUCTION;
    uint32_t hnsw_ef_search        = HNSW_DEFAULT_EF_SEARCH;

    /* Buffer pool */
    size_t buffer_pool_frames = DEFAULT_POOL_SIZE;

    /** Build from public C API config, applying defaults and validation. */
    static bool Build(const vv_config_t& in, RuntimeConfig& out);
};

} // namespace vv

#endif  /* VV_CORE_CONFIG_H */

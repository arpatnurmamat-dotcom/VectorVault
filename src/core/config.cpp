/*
 * VectorVault — Configuration Building / Validation
 */

#include "core/config.h"

#include <vectorvault/vectorvault.h>

namespace vv {

bool RuntimeConfig::Build(const vv_config_t& in, RuntimeConfig& out) {
    /* Dimension is mandatory */
    if (in.dimension == 0) {
        return false;
    }

    /* Validate distance metric */
    switch (in.distance_metric) {
        case VV_DISTANCE_L2:
        case VV_DISTANCE_COSINE:
        case VV_DISTANCE_IP:
            break;
        default:
            return false;
    }

    /* Validate index type */
    switch (in.index_type) {
        case VV_INDEX_HNSW:
        case VV_INDEX_FLAT:
            break;
        default:
            return false;
    }

    out.dimension       = in.dimension;
    out.distance_metric = in.distance_metric;
    out.index_type      = in.index_type;

    /* HNSW tunables (defaults if zero) */
    out.hnsw_M = (in.hnsw_M > 0) ? in.hnsw_M : HNSW_DEFAULT_M;
    if (out.hnsw_M < 1 || out.hnsw_M > 10000) return false;

    out.hnsw_ef_construction = (in.hnsw_ef_construction > 0)
        ? in.hnsw_ef_construction
        : HNSW_DEFAULT_EF_CONSTRUCTION;
    if (out.hnsw_ef_construction < out.hnsw_M) {
        /* hnswlib enforces ef_construction >= M */
        out.hnsw_ef_construction = out.hnsw_M;
    }

    out.hnsw_ef_search = HNSW_DEFAULT_EF_SEARCH;

    /* Buffer pool */
    if (in.buffer_pool_size_mb > 0) {
        size_t bytes = static_cast<size_t>(in.buffer_pool_size_mb) * 1024 * 1024;
        out.buffer_pool_frames = bytes / PAGE_SIZE;
        if (out.buffer_pool_frames < 16) out.buffer_pool_frames = 16;
    } else {
        out.buffer_pool_frames = DEFAULT_POOL_SIZE;
    }

    return true;
}

} // namespace vv

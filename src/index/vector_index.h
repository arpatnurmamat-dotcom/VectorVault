#ifndef VV_INDEX_VECTOR_INDEX_H
#define VV_INDEX_VECTOR_INDEX_H

#include "core/types.h"
#include <cstddef>
#include <cstdint>

namespace vv::index {

class VectorIndex {
public:
    virtual ~VectorIndex() = default;
    virtual int Insert(InternalID iid, const float* vec) = 0;
    virtual int Remove(InternalID iid) = 0;
    virtual int Search(const float* query, uint32_t k, uint32_t ef,
                       InternalID* out_ids, float* out_dists, uint32_t& out_count) = 0;
    virtual size_t Count() const = 0;
};

}

#endif

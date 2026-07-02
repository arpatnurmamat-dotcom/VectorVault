#ifndef VV_INDEX_FLAT_H
#define VV_INDEX_FLAT_H

#include "index/vector_index.h"
#include "index/distance.h"
#include <vector>
#include <mutex>

namespace vv::index {

class FlatIndex : public VectorIndex {
public:
    FlatIndex(size_t dim, DistFn dfn);
    int Insert(InternalID iid, const float* vec) override;
    int Remove(InternalID iid) override;
    int Search(const float* query, uint32_t k, uint32_t ef,
               InternalID* out_ids, float* out_dists, uint32_t& out_count) override;
    size_t Count() const override;

private:
    size_t dim_;
    DistFn dist_fn_;
    struct Rec { InternalID id; std::vector<float> vec; };
    std::vector<Rec> data_;
    mutable std::mutex mu_;
};

}

#endif

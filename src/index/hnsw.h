#ifndef VV_INDEX_HNSW_H
#define VV_INDEX_HNSW_H

#include "index/vector_index.h"
#include "index/distance.h"
#include "index/visited_list.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <random>
#include <mutex>
#include <cmath>

namespace vv::index {

class HNSWCheckpoint;

class HNSWIndex : public VectorIndex {
public:
    HNSWIndex(size_t dim, DistFn dfn, uint32_t M, uint32_t ef_construction, uint32_t ef_search);
    int Insert(InternalID iid, const float* vec) override;
    int Remove(InternalID iid) override;
    int Search(const float* query, uint32_t k, uint32_t ef,
               InternalID* out_ids, float* out_dists, uint32_t& out_count) override;
    size_t Count() const override;

    friend class HNSWCheckpoint;

private:
    struct Node {
        std::vector<float> vec;
        std::vector<std::vector<InternalID>> neighbors;
        uint32_t level = 0;
        bool deleted = false;
    };

    size_t dim_;
    DistFn dist_fn_;
    uint32_t M_;
    uint32_t M_max_;
    uint32_t M_max0_;
    uint32_t ef_c_;
    uint32_t ef_s_;
    double mL_;
    size_t count_ = 0;

    std::unordered_map<InternalID, Node> nodes_;
    InternalID entry_id_ = 0;
    uint32_t max_level_ = 0;
    bool has_entry_ = false;

    mutable std::mutex mu_;
    std::mt19937 rng_{std::random_device{}()};

    uint32_t RandomLevel();
    InternalID SelectNeighbors(const float* q,
                               const std::vector<std::pair<float, InternalID>>& candidates,
                               uint32_t M,
                               std::vector<InternalID>& result);
    std::vector<std::pair<float, InternalID>> SearchLayer(
        const float* q, InternalID entry, uint32_t ef, uint32_t layer);
    InternalID GreedySearch(const float* q, InternalID entry, uint32_t layer);
    float Distance(const float* a, InternalID b) const;
};

}

#endif

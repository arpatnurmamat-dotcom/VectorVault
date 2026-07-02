#include "index/flat.h"
#include <algorithm>
#include <queue>
#include <vectorvault/vectorvault.h>

namespace vv::index {

FlatIndex::FlatIndex(size_t dim, DistFn dfn) : dim_(dim), dist_fn_(dfn) {}

int FlatIndex::Insert(InternalID iid, const float* vec) {
    std::lock_guard<std::mutex> lk(mu_);
    Rec r;
    r.id = iid;
    r.vec.assign(vec, vec + dim_);
    data_.push_back(std::move(r));
    return VV_OK;
}

int FlatIndex::Remove(InternalID iid) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto it = data_.begin(); it != data_.end(); ++it) {
        if (it->id == iid) { data_.erase(it); return VV_OK; }
    }
    return VV_ERR_NOT_FOUND;
}

int FlatIndex::Search(const float* query, uint32_t k, uint32_t ef,
                      InternalID* out_ids, float* out_dists, uint32_t& out_count) {
    (void)ef;
    std::lock_guard<std::mutex> lk(mu_);
    using P = std::pair<float, InternalID>;
    std::priority_queue<P> pq;
    for (auto& r : data_) {
        float d = dist_fn_(query, r.vec.data(), dim_);
        pq.push({d, r.id});
        if (pq.size() > k) pq.pop();
    }
    out_count = 0;
    std::vector<std::pair<float, InternalID>> buf;
    buf.reserve(k);
    while (!pq.empty()) {
        buf.push_back(pq.top());
        pq.pop();
    }
    for (auto it = buf.rbegin(); it != buf.rend(); ++it) {
        out_ids[out_count] = it->second;
        out_dists[out_count] = it->first;
        ++out_count;
    }
    return VV_OK;
}

size_t FlatIndex::Count() const {
    std::lock_guard<std::mutex> lk(mu_);
    return data_.size();
}

}

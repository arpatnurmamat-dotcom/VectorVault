#include "index/hnsw.h"
#include <vectorvault/vectorvault.h>
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <limits>

namespace vv::index {

HNSWIndex::HNSWIndex(size_t dim, DistFn dfn, uint32_t M, uint32_t ef_c, uint32_t ef_s)
    : dim_(dim), dist_fn_(dfn), M_(M), M_max_(M), M_max0_(2 * M),
      ef_c_(ef_c), ef_s_(ef_s), mL_(1.0 / std::log(M)) {}

uint32_t HNSWIndex::RandomLevel() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double r = dist(rng_);
    if (r < 1e-9) r = 1e-9;
    uint32_t level = static_cast<uint32_t>(-std::log(r) * mL_);
    return level;
}

float HNSWIndex::Distance(const float* a, InternalID b) const {
    auto it = nodes_.find(b);
    if (it == nodes_.end()) return std::numeric_limits<float>::max();
    return dist_fn_(a, it->second.vec.data(), dim_);
}

InternalID HNSWIndex::GreedySearch(const float* q, InternalID entry, uint32_t layer) {
    InternalID cur = entry;
    float cur_dist = Distance(q, cur);
    while (true) {
        auto it = nodes_.find(cur);
        if (it == nodes_.end()) break;
        const auto& nbrs = it->second.neighbors;
        if (layer >= nbrs.size()) break;
        bool improved = false;
        for (auto nb : nbrs[layer]) {
            float d = Distance(q, nb);
            auto& nb_node = nodes_[nb];
            if (nb_node.deleted) continue;
            if (d < cur_dist) {
                cur = nb;
                cur_dist = d;
                improved = true;
            }
        }
        if (!improved) break;
    }
    return cur;
}

std::vector<std::pair<float, InternalID>> HNSWIndex::SearchLayer(
    const float* q, InternalID entry, uint32_t ef, uint32_t layer) {
    VisitedList visited(nodes_.size() * 4 > 0 ? nodes_.size() * 4 : 1024);
    using P = std::pair<float, InternalID>;
    std::priority_queue<P, std::vector<P>, std::greater<P>> candidates;
    std::priority_queue<P> result;

    auto& entry_node = nodes_[entry];
    float ed = dist_fn_(q, entry_node.vec.data(), dim_);
    candidates.emplace(ed, entry);
    if (!entry_node.deleted) result.emplace(ed, entry);
    visited.Mark(entry);

    while (!candidates.empty()) {
        auto [cd, cid] = candidates.top();
        candidates.pop();

        if (!result.empty() && cd > result.top().first) break;

        auto it = nodes_.find(cid);
        if (it == nodes_.end()) continue;
        const auto& nbrs = it->second.neighbors;
        if (layer >= nbrs.size()) continue;

        for (auto nb : nbrs[layer]) {
            if (visited.Visited(nb)) continue;
            visited.Mark(nb);
            auto& nb_node = nodes_[nb];
            if (nb_node.deleted) continue;
            float nd = Distance(q, nb);
            if (result.size() < ef || nd < result.top().first) {
                candidates.emplace(nd, nb);
                result.emplace(nd, nb);
                if (result.size() > ef) result.pop();
            }
        }
    }

    std::vector<P> res;
    while (!result.empty()) {
        res.push_back(result.top());
        result.pop();
    }
    return res;
}

InternalID HNSWIndex::SelectNeighbors(const float* /*q*/,
    const std::vector<std::pair<float, InternalID>>& candidates,
    uint32_t M,
    std::vector<InternalID>& result) {
    auto sorted = candidates;
    std::sort(sorted.begin(), sorted.end());
    result.clear();
    for (size_t i = 0; i < sorted.size() && result.size() < M; ++i) {
        if (!nodes_[sorted[i].second].deleted) {
            result.push_back(sorted[i].second);
        }
    }
    return result.empty() ? 0 : result[0];
}

int HNSWIndex::Insert(InternalID iid, const float* vec) {
    std::lock_guard<std::mutex> lk(mu_);

    Node node;
    node.vec.assign(vec, vec + dim_);
    node.level = RandomLevel();
    node.deleted = false;
    node.neighbors.resize(node.level + 1);

    if (!has_entry_) {
        nodes_[iid] = std::move(node);
        entry_id_ = iid;
        max_level_ = nodes_[iid].level;
        has_entry_ = true;
        count_ = 1;
        return VV_OK;
    }

    uint32_t new_level = node.level;
    nodes_[iid] = std::move(node);
    count_ = nodes_.size();

    InternalID cur = entry_id_;
    for (uint32_t lvl = max_level_; lvl > new_level && lvl > 0; --lvl) {
        cur = GreedySearch(vec, cur, lvl);
    }

    for (uint32_t lvl = std::min(new_level, max_level_); ; --lvl) {
        auto candidates = SearchLayer(vec, cur, ef_c_, lvl);
        std::vector<InternalID> neighbors;
        SelectNeighbors(vec, candidates, M_, neighbors);

        for (auto nb : neighbors) {
            nodes_[iid].neighbors[lvl].push_back(nb);
            nodes_[nb].neighbors[lvl].push_back(iid);
            if (lvl == 0 && nodes_[nb].neighbors[lvl].size() > M_max0_) {
                auto& nb_neighbors = nodes_[nb].neighbors[lvl];
                std::vector<std::pair<float, InternalID>> nb_cands;
                for (auto n : nb_neighbors) {
                    nb_cands.emplace_back(Distance(nodes_[nb].vec.data(), n), n);
                }
                std::sort(nb_cands.begin(), nb_cands.end());
                if (nb_cands.size() > M_max0_) {
                    nb_neighbors.clear();
                    for (uint32_t i = 0; i < M_max0_; ++i) {
                        nb_neighbors.push_back(nb_cands[i].second);
                    }
                }
            } else if (lvl > 0 && nodes_[nb].neighbors[lvl].size() > M_max_) {
                auto& nb_neighbors = nodes_[nb].neighbors[lvl];
                std::vector<std::pair<float, InternalID>> nb_cands;
                for (auto n : nb_neighbors) {
                    nb_cands.emplace_back(Distance(nodes_[nb].vec.data(), n), n);
                }
                std::sort(nb_cands.begin(), nb_cands.end());
                if (nb_cands.size() > M_max_) {
                    nb_neighbors.clear();
                    for (uint32_t i = 0; i < M_max_; ++i) {
                        nb_neighbors.push_back(nb_cands[i].second);
                    }
                }
            }
        }

        if (lvl == 0) break;
    }

    if (new_level > max_level_) {
        entry_id_ = iid;
        max_level_ = new_level;
    }

    return VV_OK;
}

int HNSWIndex::Remove(InternalID iid) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = nodes_.find(iid);
    if (it == nodes_.end() || it->second.deleted) return VV_ERR_NOT_FOUND;
    it->second.deleted = true;
    --count_;
    return VV_OK;
}

int HNSWIndex::Search(const float* query, uint32_t k, uint32_t ef,
                       InternalID* out_ids, float* out_dists, uint32_t& out_count) {
    std::lock_guard<std::mutex> lk(mu_);
    out_count = 0;
    if (!has_entry_ || nodes_.empty()) return VV_OK;

    uint32_t search_ef = std::max({ef, k, ef_s_});
    InternalID cur = entry_id_;
    for (uint32_t lvl = max_level_; lvl > 0; --lvl) {
        cur = GreedySearch(query, cur, lvl);
    }

    auto candidates = SearchLayer(query, cur, search_ef, 0);
    std::sort(candidates.begin(), candidates.end());

    size_t n = std::min(static_cast<size_t>(k), candidates.size());
    for (size_t i = 0; i < n; ++i) {
        out_ids[i] = candidates[i].second;
        out_dists[i] = candidates[i].first;
        ++out_count;
    }
    return VV_OK;
}

size_t HNSWIndex::Count() const {
    std::lock_guard<std::mutex> lk(mu_);
    return count_;
}

}

#ifndef VV_INDEX_VISITED_LIST_H
#define VV_INDEX_VISITED_LIST_H

#include "core/types.h"
#include <vector>
#include <cstdint>

namespace vv::index {

class VisitedList {
public:
    explicit VisitedList(size_t capacity)
        : visits_(capacity, 0), counter_(1) {}

    void Reset() { ++counter_; if (counter_ == 0) { ++counter_; std::fill(visits_.begin(), visits_.end(), 0); } }
    bool Visited(InternalID id) const { return id < visits_.size() && visits_[id] == counter_; }
    void Mark(InternalID id) { if (id < visits_.size()) visits_[id] = counter_; }

private:
    std::vector<uint32_t> visits_;
    uint32_t counter_;
};

}

#endif

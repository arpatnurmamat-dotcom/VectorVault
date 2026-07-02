#include <gtest/gtest.h>
#include "index/hnsw.h"
#include "index/distance.h"
#include <vectorvault/vectorvault.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <vector>

namespace {

using InternalID = vv::InternalID;
constexpr float kEpsilon = 1e-3f;

static std::vector<float> RandomVector(size_t dim, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> v(dim);
    for (auto& x : v) x = dist(rng);
    return v;
}

TEST(HNSWIndex, EmptyHasZeroCount) {
    vv::index::HNSWIndex idx(4, vv::index::L2DistanceFn(), 16, 200, 100);
    EXPECT_EQ(idx.Count(), 0u);
}

TEST(HNSWIndex, InsertSingleVector) {
    vv::index::HNSWIndex idx(3, vv::index::L2DistanceFn(), 16, 200, 50);
    std::vector<float> v = {1.0f, 2.0f, 3.0f};
    EXPECT_EQ(idx.Insert(0, v.data()), VV_OK);
    EXPECT_EQ(idx.Count(), 1u);
}

TEST(HNSWIndex, InsertMultipleVectors) {
    vv::index::HNSWIndex idx(4, vv::index::L2DistanceFn(), 16, 200, 50);
    for (uint32_t i = 0; i < 10; ++i) {
        std::vector<float> v(4, static_cast<float>(i));
        EXPECT_EQ(idx.Insert(i, v.data()), VV_OK);
    }
    EXPECT_EQ(idx.Count(), 10u);
}

TEST(HNSWIndex, SearchSingleElement) {
    vv::index::HNSWIndex idx(3, vv::index::L2DistanceFn(), 16, 200, 50);
    std::vector<float> v = {1.0f, 0.0f, 0.0f};
    idx.Insert(0, v.data());

    std::vector<float> q = {0.9f, 0.0f, 0.0f};
    std::array<InternalID, 1> ids{};
    std::array<float, 1> dists{};
    uint32_t count = 0;
    EXPECT_EQ(idx.Search(q.data(), 1, 50, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ids[0], 0u);
}

TEST(HNSWIndex, SearchReturnsNearestNeighbors) {
    const size_t dim = 4;
    vv::index::HNSWIndex idx(dim, vv::index::L2DistanceFn(), 16, 200, 50);

    std::vector<std::vector<float>> vecs = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.5f, 0.5f, 0.5f, 0.5f},
    };
    for (uint32_t i = 0; i < vecs.size(); ++i) {
        idx.Insert(i, vecs[i].data());
    }

    std::vector<float> q = {1.0f, 0.0f, 0.0f, 0.0f};
    std::array<InternalID, 3> ids{};
    std::array<float, 3> dists{};
    uint32_t count = 0;
    EXPECT_EQ(idx.Search(q.data(), 3, 50, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(ids[0], 0u);
    for (size_t i = 1; i < count; ++i) {
        EXPECT_LE(dists[i - 1], dists[i]);
    }
}

TEST(HNSWIndex, SearchEmptyIndexReturnsZero) {
    vv::index::HNSWIndex idx(3, vv::index::L2DistanceFn(), 16, 200, 50);
    std::vector<float> q = {1.0f, 0.0f, 0.0f};
    std::array<InternalID, 5> ids{};
    std::array<float, 5> dists{};
    uint32_t count = 99;
    EXPECT_EQ(idx.Search(q.data(), 5, 50, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 0u);
}

TEST(HNSWIndex, SearchKLargerThanCountReturnsAll) {
    vv::index::HNSWIndex idx(3, vv::index::L2DistanceFn(), 16, 200, 50);
    std::vector<float> v = {1.0f, 2.0f, 3.0f};
    idx.Insert(0, v.data());
    idx.Insert(1, v.data());

    std::vector<float> q = {0.0f, 0.0f, 0.0f};
    std::array<InternalID, 10> ids{};
    std::array<float, 10> dists{};
    uint32_t count = 0;
    EXPECT_EQ(idx.Search(q.data(), 10, 50, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 2u);
}

TEST(HNSWIndex, HighRecallOnSmallDataset) {
    const size_t dim = 8;
    const size_t n = 50;
    vv::index::HNSWIndex idx(dim, vv::index::L2DistanceFn(), 16, 200, 50);

    std::mt19937 rng(42);
    std::vector<std::vector<float>> vecs;
    for (size_t i = 0; i < n; ++i) {
        auto v = RandomVector(dim, rng);
        vecs.push_back(v);
        idx.Insert(static_cast<InternalID>(i), v.data());
    }

    auto query = RandomVector(dim, rng);
    std::vector<std::pair<float, InternalID>> brute;
    auto fn = vv::index::L2DistanceFn();
    for (size_t i = 0; i < vecs.size(); ++i) {
        brute.emplace_back(fn(query.data(), vecs[i].data(), dim), i);
    }
    std::sort(brute.begin(), brute.end());
    std::vector<InternalID> exact_knn;
    for (size_t i = 0; i < 5; ++i) exact_knn.push_back(brute[i].second);

    std::array<InternalID, 5> ids{};
    std::array<float, 5> dists{};
    uint32_t count = 0;
    EXPECT_EQ(idx.Search(query.data(), 5, 50, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 5u);

    int hits = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (std::find(exact_knn.begin(), exact_knn.end(), ids[i]) != exact_knn.end()) {
            ++hits;
        }
    }
    EXPECT_GE(hits, 4);
}

TEST(HNSWIndex, RemoveExistingNode) {
    vv::index::HNSWIndex idx(3, vv::index::L2DistanceFn(), 16, 200, 50);
    std::vector<float> v = {1.0f, 0.0f, 0.0f};
    idx.Insert(0, v.data());
    idx.Insert(1, v.data());
    EXPECT_EQ(idx.Count(), 2u);

    EXPECT_EQ(idx.Remove(0), VV_OK);
    EXPECT_EQ(idx.Count(), 1u);

    std::vector<float> q = {1.0f, 0.0f, 0.0f};
    std::array<InternalID, 5> ids{};
    std::array<float, 5> dists{};
    uint32_t count = 0;
    EXPECT_EQ(idx.Search(q.data(), 5, 50, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ids[0], 1u);
}

TEST(HNSWIndex, RemoveNonExistentReturnsNotFound) {
    vv::index::HNSWIndex idx(3, vv::index::L2DistanceFn(), 16, 200, 50);
    EXPECT_EQ(idx.Remove(999), VV_ERR_NOT_FOUND);
}

TEST(HNSWIndex, InsertRemoveReinsert) {
    vv::index::HNSWIndex idx(3, vv::index::L2DistanceFn(), 16, 200, 50);
    std::vector<float> v = {1.0f, 2.0f, 3.0f};
    idx.Insert(0, v.data());
    idx.Remove(0);
    EXPECT_EQ(idx.Insert(0, v.data()), VV_OK);
    EXPECT_EQ(idx.Count(), 1u);
}

TEST(HNSWIndex, SearchAfterManyInserts) {
    const size_t dim = 16;
    const size_t n = 200;
    vv::index::HNSWIndex idx(dim, vv::index::L2DistanceFn(), 16, 200, 100);

    std::mt19937 rng(123);
    for (size_t i = 0; i < n; ++i) {
        auto v = RandomVector(dim, rng);
        idx.Insert(static_cast<InternalID>(i), v.data());
    }
    EXPECT_EQ(idx.Count(), n);

    auto q = RandomVector(dim, rng);
    std::array<InternalID, 10> ids{};
    std::array<float, 10> dists{};
    uint32_t count = 0;
    EXPECT_EQ(idx.Search(q.data(), 10, 50, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 10u);
    for (size_t i = 1; i < count; ++i) {
        EXPECT_LE(dists[i - 1], dists[i]);
    }
}

}

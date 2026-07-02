#include <gtest/gtest.h>
#include "index/flat.h"
#include "index/distance.h"
#include <vectorvault/vectorvault.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {

using InternalID = vv::InternalID;
constexpr float kEpsilon = 1e-4f;

static std::vector<float> MakeUnit(size_t dim, size_t hot_idx) {
    std::vector<float> v(dim, 0.0f);
    v[hot_idx] = 1.0f;
    return v;
}

TEST(FlatIndex, EmptyHasZeroCount) {
    vv::index::FlatIndex idx(4, vv::index::L2DistanceFn());
    EXPECT_EQ(idx.Count(), 0u);
}

TEST(FlatIndex, InsertIncrementsCount) {
    vv::index::FlatIndex idx(3, vv::index::L2DistanceFn());
    std::vector<float> v = {1.0f, 2.0f, 3.0f};
    EXPECT_EQ(idx.Insert(1, v.data()), VV_OK);
    EXPECT_EQ(idx.Count(), 1u);
    EXPECT_EQ(idx.Insert(2, v.data()), VV_OK);
    EXPECT_EQ(idx.Count(), 2u);
}

TEST(FlatIndex, RemoveExistingReturnsOk) {
    vv::index::FlatIndex idx(3, vv::index::L2DistanceFn());
    std::vector<float> v = {1.0f, 2.0f, 3.0f};
    idx.Insert(1, v.data());
    idx.Insert(2, v.data());
    EXPECT_EQ(idx.Remove(1), VV_OK);
    EXPECT_EQ(idx.Count(), 1u);
}

TEST(FlatIndex, RemoveNonExistentReturnsNotFound) {
    vv::index::FlatIndex idx(3, vv::index::L2DistanceFn());
    EXPECT_EQ(idx.Remove(999), VV_ERR_NOT_FOUND);
}

TEST(FlatIndex, SearchEmptyIndexReturnsZero) {
    vv::index::FlatIndex idx(3, vv::index::L2DistanceFn());
    std::vector<float> q = {1.0f, 0.0f, 0.0f};
    std::array<InternalID, 5> ids{};
    std::array<float, 5> dists{};
    uint32_t count = 99;
    int rc = idx.Search(q.data(), 5, 0, ids.data(), dists.data(), count);
    EXPECT_EQ(rc, VV_OK);
    EXPECT_EQ(count, 0u);
}

TEST(FlatIndex, SearchReturnsNearestNeighbor) {
    const size_t dim = 3;
    vv::index::FlatIndex idx(dim, vv::index::L2DistanceFn());

    std::vector<std::vector<float>> vecs = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };
    for (uint32_t i = 0; i < vecs.size(); ++i) {
        EXPECT_EQ(idx.Insert(i, vecs[i].data()), VV_OK);
    }

    std::vector<float> query = {0.9f, 0.1f, 0.0f};
    std::array<InternalID, 3> ids{};
    std::array<float, 3> dists{};
    uint32_t count = 0;
    EXPECT_EQ(idx.Search(query.data(), 3, 0, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(ids[0], 0u);
    EXPECT_LT(dists[0], dists[1]);
    EXPECT_LE(dists[1], dists[2]);
}

TEST(FlatIndex, SearchTopKSubset) {
    const size_t dim = 3;
    vv::index::FlatIndex idx(dim, vv::index::L2DistanceFn());

    for (uint32_t i = 0; i < 10; ++i) {
        std::vector<float> v = {static_cast<float>(i), 0.0f, 0.0f};
        idx.Insert(i, v.data());
    }

    std::vector<float> query = {0.0f, 0.0f, 0.0f};
    std::array<InternalID, 3> ids{};
    std::array<float, 3> dists{};
    uint32_t count = 0;
    EXPECT_EQ(idx.Search(query.data(), 3, 0, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(ids[0], 0u);
    EXPECT_NEAR(dists[0], 0.0f, kEpsilon);
    EXPECT_EQ(ids[1], 1u);
    EXPECT_NEAR(dists[1], 1.0f, kEpsilon);
    EXPECT_EQ(ids[2], 2u);
    EXPECT_NEAR(dists[2], 4.0f, kEpsilon);
}

TEST(FlatIndex, SearchKGreaterThanCountReturnsAll) {
    const size_t dim = 2;
    vv::index::FlatIndex idx(dim, vv::index::L2DistanceFn());
    std::vector<float> v = {1.0f, 1.0f};
    idx.Insert(0, v.data());

    std::vector<float> query = {0.0f, 0.0f};
    std::array<InternalID, 10> ids{};
    std::array<float, 10> dists{};
    uint32_t count = 0;
    EXPECT_EQ(idx.Search(query.data(), 10, 0, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ids[0], 0u);
}

TEST(FlatIndex, SearchAfterRemoveExcludesDeleted) {
    const size_t dim = 2;
    vv::index::FlatIndex idx(dim, vv::index::L2DistanceFn());

    std::vector<float> near = {0.1f, 0.0f};
    std::vector<float> far  = {10.0f, 0.0f};
    idx.Insert(0, near.data());
    idx.Insert(1, far.data());
    EXPECT_EQ(idx.Remove(0), VV_OK);

    std::vector<float> query = {0.0f, 0.0f};
    std::array<InternalID, 5> ids{};
    std::array<float, 5> dists{};
    uint32_t count = 0;
    EXPECT_EQ(idx.Search(query.data(), 5, 0, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ids[0], 1u);
}

TEST(FlatIndex, SearchCosineReturnsNearestByAngle) {
    const size_t dim = 3;
    vv::index::FlatIndex idx(dim, vv::index::CosineDistanceFn());

    auto near = MakeUnit(dim, 0);
    auto perp = MakeUnit(dim, 1);
    auto opposite = std::vector<float>{-1.0f, 0.0f, 0.0f};

    idx.Insert(0, near.data());
    idx.Insert(1, perp.data());
    idx.Insert(2, opposite.data());

    std::vector<float> query = {1.0f, 0.0f, 0.0f};
    std::array<InternalID, 3> ids{};
    std::array<float, 3> dists{};
    uint32_t count = 0;
    EXPECT_EQ(idx.Search(query.data(), 3, 0, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(ids[0], 0u);
    EXPECT_NEAR(dists[0], 0.0f, kEpsilon);
    EXPECT_EQ(ids[1], 1u);
    EXPECT_NEAR(dists[1], 1.0f, kEpsilon);
    EXPECT_EQ(ids[2], 2u);
    EXPECT_NEAR(dists[2], 2.0f, kEpsilon);
}

TEST(FlatIndex, HighDimensionSearch) {
    const size_t dim = 128;
    vv::index::FlatIndex idx(dim, vv::index::L2DistanceFn());

    std::vector<float> target(dim, 0.0f);
    target[0] = 1.0f;
    idx.Insert(0, target.data());

    for (uint32_t i = 1; i <= 100; ++i) {
        std::vector<float> v(dim, 0.0f);
        v[i % dim] = 1.0f;
        idx.Insert(i, v.data());
    }

    std::vector<float> query(dim, 0.0f);
    query[0] = 0.9f;
    std::array<InternalID, 5> ids{};
    std::array<float, 5> dists{};
    uint32_t count = 0;
    EXPECT_EQ(idx.Search(query.data(), 5, 0, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(ids[0], 0u);
    EXPECT_LT(dists[0], dists[1]);
}

}

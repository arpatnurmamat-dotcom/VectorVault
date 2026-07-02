#include <gtest/gtest.h>
#include "index/hnsw.h"
#include "index/hnsw_io.h"
#include "index/distance.h"
#include "storage/page_manager.h"
#include "storage/buffer_pool.h"
#include "storage/free_list.h"
#include "storage/file_io.h"
#include <vectorvault/vectorvault.h>
#include "core/types.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

using InternalID = vv::InternalID;
using PageID = vv::PageID;
using DistFn = vv::DistFn;

static std::string TempPath(const std::string& suffix) {
    return "/tmp/vv_test_hnsw_io_" + suffix + ".dat";
}

static void Remove(const std::string& path) { std::remove(path.c_str()); }

struct HNSWIOTest : ::testing::Test {
    std::string path;
    int fd = -1;
    vv::storage::BufferPool* pool = nullptr;
    vv::storage::FreeList* freelist = nullptr;
    vv::storage::PageManager* mgr = nullptr;

    void SetUp() override {
        path = TempPath("main");
        Remove(path);
        int rc = vv::storage::FileIO::Open(path, true, fd);
        ASSERT_EQ(rc, VV_OK);
        rc = vv::storage::FileIO::Resize(fd,
            vv::FIRST_DATA_PAGE_ID * vv::PAGE_SIZE);
        ASSERT_EQ(rc, VV_OK);
        pool = new vv::storage::BufferPool(256, fd);
        freelist = new vv::storage::FreeList();
        mgr = new vv::storage::PageManager(pool, freelist);
    }

    void TearDown() override {
        delete mgr;
        delete freelist;
        delete pool;
        vv::storage::FileIO::Close(fd);
        Remove(path);
    }
};

TEST_F(HNSWIOTest, SaveEmptyIndex) {
    vv::index::HNSWIndex idx(4, vv::index::L2DistanceFn(), 16, 200, 100);
    EXPECT_EQ(vv::index::HNSWCheckpoint::Save(idx, *mgr), VV_OK);
}

TEST_F(HNSWIOTest, LoadEmptyIndex) {
    vv::index::HNSWIndex idx(4, vv::index::L2DistanceFn(), 16, 200, 100);
    ASSERT_EQ(vv::index::HNSWCheckpoint::Save(idx, *mgr), VV_OK);
    ASSERT_EQ(mgr->Flush(), VV_OK);

    vv::index::HNSWIndex loaded(4, vv::index::L2DistanceFn(), 16, 200, 100);
    EXPECT_EQ(vv::index::HNSWCheckpoint::Load(loaded, *mgr, 4,
        vv::index::L2DistanceFn(), 16, 200, 100), VV_OK);
    EXPECT_EQ(loaded.Count(), 0u);
}

TEST_F(HNSWIOTest, SaveAndLoadSingleNode) {
    vv::index::HNSWIndex idx(3, vv::index::L2DistanceFn(), 16, 200, 50);
    std::vector<float> v = {1.0f, 2.0f, 3.0f};
    ASSERT_EQ(idx.Insert(0, v.data()), VV_OK);
    ASSERT_EQ(idx.Count(), 1u);

    ASSERT_EQ(vv::index::HNSWCheckpoint::Save(idx, *mgr), VV_OK);
    ASSERT_EQ(mgr->Flush(), VV_OK);

    vv::index::HNSWIndex loaded(3, vv::index::L2DistanceFn(), 16, 200, 50);
    EXPECT_EQ(vv::index::HNSWCheckpoint::Load(loaded, *mgr, 3,
        vv::index::L2DistanceFn(), 16, 200, 50), VV_OK);
    EXPECT_EQ(loaded.Count(), 1u);

    std::vector<float> q = {0.9f, 2.1f, 3.0f};
    std::array<InternalID, 1> ids{};
    std::array<float, 1> dists{};
    uint32_t count = 0;
    EXPECT_EQ(loaded.Search(q.data(), 1, 50, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ids[0], 0u);
}

TEST_F(HNSWIOTest, MultiNodeRoundTrip) {
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
        ASSERT_EQ(idx.Insert(i, vecs[i].data()), VV_OK);
    }
    ASSERT_EQ(idx.Count(), 5u);

    ASSERT_EQ(vv::index::HNSWCheckpoint::Save(idx, *mgr), VV_OK);
    ASSERT_EQ(mgr->Flush(), VV_OK);

    vv::index::HNSWIndex loaded(dim, vv::index::L2DistanceFn(), 16, 200, 50);
    EXPECT_EQ(vv::index::HNSWCheckpoint::Load(loaded, *mgr, dim,
        vv::index::L2DistanceFn(), 16, 200, 50), VV_OK);
    EXPECT_EQ(loaded.Count(), 5u);

    std::vector<float> q = {1.0f, 0.0f, 0.0f, 0.0f};
    std::array<InternalID, 3> ids{};
    std::array<float, 3> dists{};
    uint32_t count = 0;
    EXPECT_EQ(loaded.Search(q.data(), 3, 50, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(ids[0], 0u);
}

TEST_F(HNSWIOTest, GraphConnectivityPreserved) {
    const size_t dim = 4;
    vv::index::HNSWIndex idx(dim, vv::index::L2DistanceFn(), 16, 200, 50);

    for (uint32_t i = 0; i < 20; ++i) {
        std::vector<float> v(dim, static_cast<float>(i));
        ASSERT_EQ(idx.Insert(i, v.data()), VV_OK);
    }

    ASSERT_EQ(vv::index::HNSWCheckpoint::Save(idx, *mgr), VV_OK);
    ASSERT_EQ(mgr->Flush(), VV_OK);

    vv::index::HNSWIndex loaded(dim, vv::index::L2DistanceFn(), 16, 200, 50);
    ASSERT_EQ(vv::index::HNSWCheckpoint::Load(loaded, *mgr, dim,
        vv::index::L2DistanceFn(), 16, 200, 50), VV_OK);

    std::vector<float> q = {5.0f, 5.0f, 5.0f, 5.0f};
    std::array<InternalID, 5> ids{};
    std::array<float, 5> dists{};
    uint32_t count = 0;
    EXPECT_EQ(loaded.Search(q.data(), 5, 50, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 5u);

    for (size_t i = 0; i < count; ++i) {
        EXPECT_LT(ids[i], 20u);
    }
}

TEST_F(HNSWIOTest, DistanceValuesPreserved) {
    const size_t dim = 3;
    vv::index::HNSWIndex idx(dim, vv::index::L2DistanceFn(), 16, 200, 50);

    std::vector<float> v0 = {1.0f, 0.0f, 0.0f};
    std::vector<float> v1 = {0.0f, 1.0f, 0.0f};
    idx.Insert(0, v0.data());
    idx.Insert(1, v1.data());

    std::vector<float> q = {1.0f, 0.0f, 0.0f};
    std::array<InternalID, 2> orig_ids{};
    std::array<float, 2> orig_dists{};
    uint32_t orig_count = 0;
    idx.Search(q.data(), 2, 50, orig_ids.data(), orig_dists.data(), orig_count);

    ASSERT_EQ(vv::index::HNSWCheckpoint::Save(idx, *mgr), VV_OK);
    ASSERT_EQ(mgr->Flush(), VV_OK);

    vv::index::HNSWIndex loaded(dim, vv::index::L2DistanceFn(), 16, 200, 50);
    ASSERT_EQ(vv::index::HNSWCheckpoint::Load(loaded, *mgr, dim,
        vv::index::L2DistanceFn(), 16, 200, 50), VV_OK);

    std::array<InternalID, 2> loaded_ids{};
    std::array<float, 2> loaded_dists{};
    uint32_t loaded_count = 0;
    ASSERT_EQ(loaded.Search(q.data(), 2, 50, loaded_ids.data(), loaded_dists.data(), loaded_count), VV_OK);

    ASSERT_EQ(orig_count, loaded_count);
    for (uint32_t i = 0; i < orig_count; ++i) {
        EXPECT_EQ(orig_ids[i], loaded_ids[i]);
        EXPECT_NEAR(orig_dists[i], loaded_dists[i], 1e-5f);
    }
}

TEST_F(HNSWIOTest, DeletedFlagPreserved) {
    const size_t dim = 3;
    vv::index::HNSWIndex idx(dim, vv::index::L2DistanceFn(), 16, 200, 50);

    std::vector<float> v0 = {1.0f, 0.0f, 0.0f};
    std::vector<float> v1 = {0.0f, 1.0f, 0.0f};
    idx.Insert(0, v0.data());
    idx.Insert(1, v1.data());
    EXPECT_EQ(idx.Remove(0), VV_OK);
    EXPECT_EQ(idx.Count(), 1u);

    ASSERT_EQ(vv::index::HNSWCheckpoint::Save(idx, *mgr), VV_OK);
    ASSERT_EQ(mgr->Flush(), VV_OK);

    vv::index::HNSWIndex loaded(dim, vv::index::L2DistanceFn(), 16, 200, 50);
    ASSERT_EQ(vv::index::HNSWCheckpoint::Load(loaded, *mgr, dim,
        vv::index::L2DistanceFn(), 16, 200, 50), VV_OK);
    EXPECT_EQ(loaded.Count(), 1u);

    std::vector<float> q = {1.0f, 0.0f, 0.0f};
    std::array<InternalID, 5> ids{};
    std::array<float, 5> dists{};
    uint32_t count = 0;
    EXPECT_EQ(loaded.Search(q.data(), 5, 50, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ids[0], 1u);
}

TEST_F(HNSWIOTest, SaveOverwritesPreviousCheckpoint) {
    const size_t dim = 3;
    vv::index::HNSWIndex idx1(dim, vv::index::L2DistanceFn(), 16, 200, 50);
    std::vector<float> v = {1.0f, 0.0f, 0.0f};
    idx1.Insert(0, v.data());

    ASSERT_EQ(vv::index::HNSWCheckpoint::Save(idx1, *mgr), VV_OK);
    ASSERT_EQ(mgr->Flush(), VV_OK);

    vv::index::HNSWIndex idx2(dim, vv::index::L2DistanceFn(), 16, 200, 50);
    idx2.Insert(0, v.data());
    idx2.Insert(1, v.data());
    idx2.Insert(2, v.data());

    ASSERT_EQ(vv::index::HNSWCheckpoint::Save(idx2, *mgr), VV_OK);
    ASSERT_EQ(mgr->Flush(), VV_OK);

    vv::index::HNSWIndex loaded(dim, vv::index::L2DistanceFn(), 16, 200, 50);
    ASSERT_EQ(vv::index::HNSWCheckpoint::Load(loaded, *mgr, dim,
        vv::index::L2DistanceFn(), 16, 200, 50), VV_OK);
    EXPECT_EQ(loaded.Count(), 3u);
}

TEST_F(HNSWIOTest, LoadWithCosineDistance) {
    const size_t dim = 3;
    vv::index::HNSWIndex idx(dim, vv::index::CosineDistanceFn(), 16, 200, 50);
    std::vector<float> v0 = {1.0f, 0.0f, 0.0f};
    std::vector<float> v1 = {0.0f, 1.0f, 0.0f};
    idx.Insert(0, v0.data());
    idx.Insert(1, v1.data());

    ASSERT_EQ(vv::index::HNSWCheckpoint::Save(idx, *mgr), VV_OK);
    ASSERT_EQ(mgr->Flush(), VV_OK);

    vv::index::HNSWIndex loaded(dim, vv::index::CosineDistanceFn(), 16, 200, 50);
    ASSERT_EQ(vv::index::HNSWCheckpoint::Load(loaded, *mgr, dim,
        vv::index::CosineDistanceFn(), 16, 200, 50), VV_OK);
    EXPECT_EQ(loaded.Count(), 2u);

    std::vector<float> q = {1.0f, 0.0f, 0.0f};
    std::array<InternalID, 2> ids{};
    std::array<float, 2> dists{};
    uint32_t count = 0;
    EXPECT_EQ(loaded.Search(q.data(), 2, 50, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 2u);
    EXPECT_EQ(ids[0], 0u);
}

TEST_F(HNSWIOTest, LargeDimRoundTrip) {
    const size_t dim = 128;
    vv::index::HNSWIndex idx(dim, vv::index::L2DistanceFn(), 16, 200, 50);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (uint32_t i = 0; i < 10; ++i) {
        std::vector<float> v(dim);
        for (auto& x : v) x = dist(rng);
        ASSERT_EQ(idx.Insert(i, v.data()), VV_OK);
    }

    ASSERT_EQ(vv::index::HNSWCheckpoint::Save(idx, *mgr), VV_OK);
    ASSERT_EQ(mgr->Flush(), VV_OK);

    vv::index::HNSWIndex loaded(dim, vv::index::L2DistanceFn(), 16, 200, 50);
    ASSERT_EQ(vv::index::HNSWCheckpoint::Load(loaded, *mgr, dim,
        vv::index::L2DistanceFn(), 16, 200, 50), VV_OK);
    EXPECT_EQ(loaded.Count(), 10u);

    std::vector<float> q(dim);
    for (auto& x : q) x = dist(rng);
    std::array<InternalID, 5> ids{};
    std::array<float, 5> dists{};
    uint32_t count = 0;
    EXPECT_EQ(loaded.Search(q.data(), 5, 50, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 5u);
}

TEST_F(HNSWIOTest, EntryIdAndMaxLevelPreserved) {
    const size_t dim = 4;
    vv::index::HNSWIndex idx(dim, vv::index::L2DistanceFn(), 4, 200, 50);

    for (uint32_t i = 0; i < 30; ++i) {
        std::vector<float> v(dim, static_cast<float>(i));
        ASSERT_EQ(idx.Insert(i, v.data()), VV_OK);
    }
    ASSERT_EQ(idx.Count(), 30u);

    ASSERT_EQ(vv::index::HNSWCheckpoint::Save(idx, *mgr), VV_OK);
    ASSERT_EQ(mgr->Flush(), VV_OK);

    vv::index::HNSWIndex loaded(dim, vv::index::L2DistanceFn(), 4, 200, 50);
    ASSERT_EQ(vv::index::HNSWCheckpoint::Load(loaded, *mgr, dim,
        vv::index::L2DistanceFn(), 4, 200, 50), VV_OK);
    EXPECT_EQ(loaded.Count(), 30u);

    std::vector<float> q = {15.0f, 15.0f, 15.0f, 15.0f};
    std::array<InternalID, 5> ids{};
    std::array<float, 5> dists{};
    uint32_t count = 0;
    EXPECT_EQ(loaded.Search(q.data(), 5, 50, ids.data(), dists.data(), count), VV_OK);
    EXPECT_EQ(count, 5u);
    for (uint32_t i = 0; i < count; ++i) {
        EXPECT_LT(ids[i], 30u);
    }
}

}

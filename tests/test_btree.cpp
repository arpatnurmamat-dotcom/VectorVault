#include <gtest/gtest.h>
#include "meta/btree.h"
#include "storage/page_manager.h"
#include "storage/buffer_pool.h"
#include "storage/free_list.h"
#include "storage/file_io.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#include <vectorvault/vectorvault.h>

using PageID = vv::PageID;

static std::string TempPath(const std::string& suffix) {
    return "/tmp/vv_test_btree_" + suffix + ".dat";
}

static void Remove(const std::string& path) { std::remove(path.c_str()); }

struct BTreeTest : ::testing::Test {
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

TEST_F(BTreeTest, EmptyTreeFindReturnsNotFound) {
    vv::meta::BPlusTree tree(mgr);
    PageID val = 0;
    EXPECT_EQ(tree.Find(42, val), VV_ERR_NOT_FOUND);
}

TEST_F(BTreeTest, EmptyTreeContainsReturnsFalse) {
    vv::meta::BPlusTree tree(mgr);
    EXPECT_FALSE(tree.Contains(42));
}

TEST_F(BTreeTest, EmptyTreeCountIsZero) {
    vv::meta::BPlusTree tree(mgr);
    EXPECT_EQ(tree.Count(), 0u);
}

TEST_F(BTreeTest, SingleInsertAndFind) {
    vv::meta::BPlusTree tree(mgr);
    EXPECT_EQ(tree.Insert(10, 100), VV_OK);
    PageID val = 0;
    EXPECT_EQ(tree.Find(10, val), VV_OK);
    EXPECT_EQ(val, 100u);
}

TEST_F(BTreeTest, InsertUpdatesExistingKey) {
    vv::meta::BPlusTree tree(mgr);
    EXPECT_EQ(tree.Insert(10, 100), VV_OK);
    EXPECT_EQ(tree.Insert(10, 200), VV_OK);
    PageID val = 0;
    EXPECT_EQ(tree.Find(10, val), VV_OK);
    EXPECT_EQ(val, 200u);
    EXPECT_EQ(tree.Count(), 1u);
}

TEST_F(BTreeTest, BulkInsert100Keys) {
    vv::meta::BPlusTree tree(mgr);
    for (uint64_t i = 0; i < 100; ++i) {
        EXPECT_EQ(tree.Insert(i, i * 10), VV_OK);
    }
    EXPECT_EQ(tree.Count(), 100u);
    for (uint64_t i = 0; i < 100; ++i) {
        PageID val = 0;
        EXPECT_EQ(tree.Find(i, val), VV_OK);
        EXPECT_EQ(val, i * 10);
    }
}

TEST_F(BTreeTest, FindNonExistingKey) {
    vv::meta::BPlusTree tree(mgr);
    for (uint64_t i = 0; i < 50; ++i) {
        tree.Insert(i, i);
    }
    PageID val = 0;
    EXPECT_EQ(tree.Find(999, val), VV_ERR_NOT_FOUND);
    EXPECT_EQ(tree.Find(50, val), VV_ERR_NOT_FOUND);
}

TEST_F(BTreeTest, RemoveKeyAndReFind) {
    vv::meta::BPlusTree tree(mgr);
    tree.Insert(1, 10);
    tree.Insert(2, 20);
    tree.Insert(3, 30);
    EXPECT_EQ(tree.Remove(2), VV_OK);
    EXPECT_EQ(tree.Count(), 2u);
    PageID val = 0;
    EXPECT_EQ(tree.Find(2, val), VV_ERR_NOT_FOUND);
    EXPECT_EQ(tree.Find(1, val), VV_OK);
    EXPECT_EQ(val, 10u);
    EXPECT_EQ(tree.Find(3, val), VV_OK);
    EXPECT_EQ(val, 30u);
}

TEST_F(BTreeTest, RemoveNonExistingReturnsNotFound) {
    vv::meta::BPlusTree tree(mgr);
    tree.Insert(1, 10);
    EXPECT_EQ(tree.Remove(999), VV_ERR_NOT_FOUND);
}

TEST_F(BTreeTest, ContainsAfterInsertAndRemove) {
    vv::meta::BPlusTree tree(mgr);
    tree.Insert(5, 50);
    EXPECT_TRUE(tree.Contains(5));
    tree.Remove(5);
    EXPECT_FALSE(tree.Contains(5));
}

TEST_F(BTreeTest, RangeQueryBasic) {
    vv::meta::BPlusTree tree(mgr);
    for (uint64_t i = 0; i < 20; ++i) {
        tree.Insert(i, i * 100);
    }
    std::vector<std::pair<uint64_t, PageID>> results;
    EXPECT_EQ(tree.RangeQuery(5, 10, results), VV_OK);
    EXPECT_EQ(results.size(), 6u);
    for (size_t i = 0; i < results.size(); ++i) {
        EXPECT_EQ(results[i].first, 5 + i);
        EXPECT_EQ(results[i].second, (5 + i) * 100);
    }
}

TEST_F(BTreeTest, PersistenceRoundTrip) {
    PageID root_id = vv::BTREE_ROOT_PAGE_ID;
    {
        vv::meta::BPlusTree tree(mgr, root_id);
        tree.Insert(100, 1000);
        tree.Insert(200, 2000);
        tree.Insert(300, 3000);
        EXPECT_EQ(tree.Save(), VV_OK);
        EXPECT_EQ(mgr->Flush(), VV_OK);
    }
    {
        vv::meta::BPlusTree tree2(mgr, root_id);
        EXPECT_EQ(tree2.Load(), VV_OK);
        EXPECT_EQ(tree2.Count(), 3u);
        PageID val = 0;
        EXPECT_EQ(tree2.Find(100, val), VV_OK);
        EXPECT_EQ(val, 1000u);
        EXPECT_EQ(tree2.Find(200, val), VV_OK);
        EXPECT_EQ(val, 2000u);
        EXPECT_EQ(tree2.Find(300, val), VV_OK);
        EXPECT_EQ(val, 3000u);
    }
}

TEST_F(BTreeTest, Stress1000Keys) {
    vv::meta::BPlusTree tree(mgr);
    const int N = 1000;
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(tree.Insert(static_cast<uint64_t>(i), static_cast<PageID>(i * 7)), VV_OK);
    }
    EXPECT_EQ(tree.Count(), static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) {
        PageID val = 0;
        EXPECT_EQ(tree.Find(static_cast<uint64_t>(i), val), VV_OK);
        EXPECT_EQ(val, static_cast<PageID>(i * 7));
    }
    PageID val = 0;
    EXPECT_EQ(tree.Find(static_cast<uint64_t>(N), val), VV_ERR_NOT_FOUND);
}

TEST_F(BTreeTest, ReverseInsertOrder) {
    vv::meta::BPlusTree tree(mgr);
    for (int i = 99; i >= 0; --i) {
        tree.Insert(static_cast<uint64_t>(i), static_cast<PageID>(i));
    }
    EXPECT_EQ(tree.Count(), 100u);
    for (uint64_t i = 0; i < 100; ++i) {
        PageID val = 0;
        EXPECT_EQ(tree.Find(i, val), VV_OK);
        EXPECT_EQ(val, i);
    }
}

TEST_F(BTreeTest, RangeQueryEmptyRange) {
    vv::meta::BPlusTree tree(mgr);
    tree.Insert(1, 10);
    tree.Insert(5, 50);
    tree.Insert(10, 100);
    std::vector<std::pair<uint64_t, PageID>> results;
    EXPECT_EQ(tree.RangeQuery(6, 9, results), VV_OK);
    EXPECT_EQ(results.size(), 0u);
}

TEST_F(BTreeTest, PersistenceWithDelete) {
    PageID root_id = vv::BTREE_ROOT_PAGE_ID;
    {
        vv::meta::BPlusTree tree(mgr, root_id);
        tree.Insert(10, 100);
        tree.Insert(20, 200);
        tree.Insert(30, 300);
        tree.Remove(20);
        EXPECT_EQ(tree.Save(), VV_OK);
        EXPECT_EQ(mgr->Flush(), VV_OK);
    }
    {
        vv::meta::BPlusTree tree2(mgr, root_id);
        EXPECT_EQ(tree2.Load(), VV_OK);
        EXPECT_EQ(tree2.Count(), 2u);
        PageID val = 0;
        EXPECT_EQ(tree2.Find(10, val), VV_OK);
        EXPECT_EQ(val, 100u);
        EXPECT_EQ(tree2.Find(30, val), VV_OK);
        EXPECT_EQ(val, 300u);
        EXPECT_EQ(tree2.Find(20, val), VV_ERR_NOT_FOUND);
    }
}

TEST_F(BTreeTest, RangeQueryFullScan) {
    vv::meta::BPlusTree tree(mgr);
    for (uint64_t i = 0; i < 50; ++i) {
        tree.Insert(i, i * 2);
    }
    std::vector<std::pair<uint64_t, PageID>> results;
    EXPECT_EQ(tree.RangeQuery(0, 49, results), VV_OK);
    EXPECT_EQ(results.size(), 50u);
    for (uint64_t i = 0; i < 50; ++i) {
        EXPECT_EQ(results[i].first, i);
        EXPECT_EQ(results[i].second, i * 2);
    }
}

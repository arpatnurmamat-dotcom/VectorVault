#include <gtest/gtest.h>
#include "storage/buffer_pool.h"
#include "storage/file_io.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <vectorvault/vectorvault.h>

using PageID = vv::PageID;

static std::string TempPath(std::string suffix) {
    return "/tmp/vv_test_bufpool_" + suffix + ".dat";
}

static void Remove(const std::string& path) {
    std::remove(path.c_str());
}

static int CreateFileWithPages(const std::string& path, size_t page_count) {
    Remove(path);
    int fd = -1;
    int rc = vv::storage::FileIO::Open(path, true, fd);
    if (rc != VV_OK) return rc;
    char page[vv::PAGE_SIZE];
    for (size_t i = 0; i < page_count; ++i) {
        std::memset(page, static_cast<int>(i & 0xFF), vv::PAGE_SIZE);
        uint64_t page_id_val = i;
        std::memcpy(page, &page_id_val, sizeof(page_id_val));
        rc = vv::storage::FileIO::Write(fd, page, vv::PAGE_SIZE, i * vv::PAGE_SIZE);
        if (rc != VV_OK) { vv::storage::FileIO::Close(fd); Remove(path); return rc; }
    }
    vv::storage::FileIO::Sync(fd);
    return fd;
}

struct BufferPoolTest : ::testing::Test {
    std::string path;
    int fd = -1;

    void SetUp() override { path = TempPath("main"); }
    void TearDown() override {
        if (fd >= 0) vv::storage::FileIO::Close(fd);
        Remove(path);
    }
};

TEST_F(BufferPoolTest, GetPageReadsFromDisk) {
    fd = CreateFileWithPages(path, 4);
    ASSERT_GE(fd, 0);
    vv::storage::BufferPool pool(8, fd);

    vv::Frame* f = nullptr;
    EXPECT_EQ(pool.GetPage(0, f), VV_OK);
    ASSERT_NE(f, nullptr);

    PageID stored_id = 0;
    std::memcpy(&stored_id, f->data, sizeof(vv::PageID));
    EXPECT_EQ(stored_id, 0u);
}

TEST_F(BufferPoolTest, GetPageCachesSubsequentReads) {
    fd = CreateFileWithPages(path, 4);
    ASSERT_GE(fd, 0);
    vv::storage::BufferPool pool(8, fd);

    vv::Frame* f1 = nullptr;
    vv::Frame* f2 = nullptr;
    EXPECT_EQ(pool.GetPage(1, f1), VV_OK);
    EXPECT_EQ(pool.GetPage(1, f2), VV_OK);
    EXPECT_EQ(f1, f2);
    EXPECT_EQ(pool.FrameUsed(), 1u);
}

TEST_F(BufferPoolTest, NewPageAllocatesDistinctIds) {
    fd = CreateFileWithPages(path, 1);
    ASSERT_GE(fd, 0);
    vv::storage::BufferPool pool(8, fd);

    char data[vv::PAGE_SIZE] = {};
    PageID id1 = 999, id2 = 999;
    EXPECT_EQ(pool.NewPage(data, &id1), VV_OK);
    EXPECT_EQ(pool.NewPage(data, &id2), VV_OK);
    EXPECT_NE(id1, id2);
}

TEST_F(BufferPoolTest, MarkDirtyMakesPageDirty) {
    fd = CreateFileWithPages(path, 2);
    ASSERT_GE(fd, 0);
    vv::storage::BufferPool pool(8, fd);

    vv::Frame* f = nullptr;
    EXPECT_EQ(pool.GetPage(0, f), VV_OK);
    EXPECT_FALSE(f->is_dirty);
    EXPECT_EQ(pool.MarkDirty(0), VV_OK);
    EXPECT_TRUE(f->is_dirty);
}

TEST_F(BufferPoolTest, MarkDirtyOnUnknownPageFails) {
    fd = CreateFileWithPages(path, 1);
    ASSERT_GE(fd, 0);
    vv::storage::BufferPool pool(8, fd);
    EXPECT_EQ(pool.MarkDirty(999), VV_ERR_NOT_FOUND);
}

TEST_F(BufferPoolTest, FlushAllDirtyPersistsModifications) {
    fd = CreateFileWithPages(path, 2);
    ASSERT_GE(fd, 0);
    {
        vv::storage::BufferPool pool(8, fd);
        vv::Frame* f = nullptr;
        EXPECT_EQ(pool.GetPage(0, f), VV_OK);
        std::memcpy(f->data, "MODIFIED_HEADER", 16);
        EXPECT_EQ(pool.MarkDirty(0), VV_OK);
        EXPECT_EQ(pool.FlushAllDirty(), VV_OK);
    }

    char buf[vv::PAGE_SIZE] = {};
    size_t n = 0;
    EXPECT_EQ(vv::storage::FileIO::Read(fd, buf, vv::PAGE_SIZE, 0, n), VV_OK);
    EXPECT_EQ(std::memcmp(buf, "MODIFIED_HEADER", 16), 0);
}

TEST_F(BufferPoolTest, LruEvictsColdPageOnCapacityExceeded) {
    fd = CreateFileWithPages(path, 8);
    ASSERT_GE(fd, 0);
    vv::storage::BufferPool pool(4, fd);

    vv::Frame* f[4] = {};
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(pool.GetPage(static_cast<PageID>(i), f[i]), VV_OK);
    }
    EXPECT_EQ(pool.FrameUsed(), 4u);

    vv::Frame* f4 = nullptr;
    EXPECT_EQ(pool.GetPage(4, f4), VV_OK);
    EXPECT_EQ(pool.FrameUsed(), 4u);

    vv::Frame* f0_again = nullptr;
    EXPECT_EQ(pool.GetPage(0, f0_again), VV_OK);
    EXPECT_NE(f0_again, f[0]);
}

TEST_F(BufferPoolTest, PinnedPageBlocksEviction) {
    fd = CreateFileWithPages(path, 8);
    ASSERT_GE(fd, 0);
    vv::storage::BufferPool pool(2, fd);

    vv::Frame* f0 = nullptr;
    EXPECT_EQ(pool.GetPage(0, f0), VV_OK);
    f0->pin_count = 1;

    vv::Frame* f1 = nullptr;
    EXPECT_EQ(pool.GetPage(1, f1), VV_OK);
    f1->pin_count = 1;

    vv::Frame* f2 = nullptr;
    EXPECT_EQ(pool.GetPage(2, f2), VV_ERR_NOMEM);
    f0->pin_count = 0;
    f1->pin_count = 0;
}

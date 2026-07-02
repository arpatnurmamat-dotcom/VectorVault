#include <gtest/gtest.h>
#include "storage/page_manager.h"
#include "storage/buffer_pool.h"
#include "storage/free_list.h"
#include "storage/file_io.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vectorvault/vectorvault.h>

using PageID = vv::PageID;

static std::string TempPath(const std::string& suffix) {
    return "/tmp/vv_test_page_manager_" + suffix + ".dat";
}

static void Remove(const std::string& path) { std::remove(path.c_str()); }

struct PageManagerTest : ::testing::Test {
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
        pool = new vv::storage::BufferPool(16, fd);
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

TEST_F(PageManagerTest, AllocPageCreatesNewWhenFreeListEmpty) {
    PageID id = 999;
    EXPECT_EQ(mgr->AllocPage(nullptr, &id), VV_OK);
    EXPECT_NE(id, 999u);

    PageID id2 = 999;
    EXPECT_EQ(mgr->AllocPage(nullptr, &id2), VV_OK);
    EXPECT_NE(id, id2);
}

TEST_F(PageManagerTest, AllocPageWithDataCopiesToFrame) {
    char data[vv::PAGE_SIZE];
    std::memset(data, 0xAB, vv::PAGE_SIZE);
    PageID id = 0;
    EXPECT_EQ(mgr->AllocPage(data, &id), VV_OK);

    vv::Frame* f = nullptr;
    EXPECT_EQ(mgr->GetPage(id, f), VV_OK);
    for (size_t i = 0; i < 16; ++i) {
        EXPECT_EQ(static_cast<unsigned char>(f->data[i]), 0xAB);
    }
}

TEST_F(PageManagerTest, GetPageReadsAllocatedPage) {
    char data[vv::PAGE_SIZE] = {};
    std::memcpy(data, "PAGE_DATA", 10);
    PageID id = 0;
    EXPECT_EQ(mgr->AllocPage(data, &id), VV_OK);
    EXPECT_EQ(mgr->Flush(), VV_OK);

    vv::Frame* f = nullptr;
    EXPECT_EQ(mgr->GetPage(id, f), VV_OK);
    EXPECT_EQ(std::memcmp(f->data, "PAGE_DATA", 10), 0);
}

TEST_F(PageManagerTest, MarkDirtyAndFlushPersists) {
    PageID id = 0;
    EXPECT_EQ(mgr->AllocPage(nullptr, &id), VV_OK);

    vv::Frame* f = nullptr;
    EXPECT_EQ(mgr->GetPage(id, f), VV_OK);
    std::memcpy(f->data, "DIRTY_WRITE", 12);
    EXPECT_EQ(mgr->MarkDirty(id), VV_OK);
    EXPECT_EQ(mgr->Flush(), VV_OK);

    char buf[vv::PAGE_SIZE] = {};
    size_t n = 0;
    EXPECT_EQ(vv::storage::FileIO::Read(fd, buf, vv::PAGE_SIZE, id * vv::PAGE_SIZE, n), VV_OK);
    EXPECT_EQ(std::memcmp(buf, "DIRTY_WRITE", 12), 0);
}

TEST_F(PageManagerTest, FreePageAddsToFreeList) {
    PageID id1 = 0, id2 = 0;
    EXPECT_EQ(mgr->AllocPage(nullptr, &id1), VV_OK);
    EXPECT_EQ(mgr->AllocPage(nullptr, &id2), VV_OK);
    EXPECT_EQ(freelist->FreeCount(), 0u);

    EXPECT_EQ(mgr->FreePage(id1), VV_OK);
    EXPECT_EQ(freelist->FreeCount(), 1u);
}

TEST_F(PageManagerTest, AllocPageRecyclesFromFreeList) {
    PageID id1 = 0, id2 = 0;
    EXPECT_EQ(mgr->AllocPage(nullptr, &id1), VV_OK);
    EXPECT_EQ(mgr->AllocPage(nullptr, &id2), VV_OK);

    EXPECT_EQ(mgr->FreePage(id1), VV_OK);
    EXPECT_EQ(freelist->FreeCount(), 1u);

    PageID recycled = 999;
    EXPECT_EQ(mgr->AllocPage(nullptr, &recycled), VV_OK);
    EXPECT_EQ(recycled, id1);
    EXPECT_EQ(freelist->FreeCount(), 0u);
}

TEST_F(PageManagerTest, FreeThenRecycleWithDataOverwrite) {
    char old_data[vv::PAGE_SIZE];
    std::memset(old_data, 0x11, vv::PAGE_SIZE);
    PageID id = 0;
    EXPECT_EQ(mgr->AllocPage(old_data, &id), VV_OK);
    EXPECT_EQ(mgr->Flush(), VV_OK);

    EXPECT_EQ(mgr->FreePage(id), VV_OK);

    char new_data[vv::PAGE_SIZE];
    std::memset(new_data, 0x22, vv::PAGE_SIZE);
    PageID recycled = 999;
    EXPECT_EQ(mgr->AllocPage(new_data, &recycled), VV_OK);
    EXPECT_EQ(recycled, id);
    EXPECT_EQ(mgr->Flush(), VV_OK);

    char verify[vv::PAGE_SIZE] = {};
    size_t n = 0;
    EXPECT_EQ(vv::storage::FileIO::Read(fd, verify, vv::PAGE_SIZE, id * vv::PAGE_SIZE, n), VV_OK);
    EXPECT_EQ(static_cast<unsigned char>(verify[0]), 0x22);
}

TEST_F(PageManagerTest, FlushWhenNothingDirtySucceeds) {
    EXPECT_EQ(mgr->Flush(), VV_OK);
}

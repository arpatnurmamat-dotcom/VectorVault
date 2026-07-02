#include <gtest/gtest.h>
#include "storage/free_list.h"

#include <cstring>
#include <vector>
#include <vectorvault/vectorvault.h>

using PageID = vv::PageID;

TEST(FreeList, AllocateFromEmptyReturnsNotFound) {
    vv::storage::FreeList fl;
    PageID id = 0;
    EXPECT_EQ(fl.Allocate(id), VV_ERR_NOT_FOUND);
    EXPECT_EQ(fl.FreeCount(), 0u);
}

TEST(FreeList, FreeAndAllocateLifo) {
    vv::storage::FreeList fl;
    EXPECT_EQ(fl.Free(10), VV_OK);
    EXPECT_EQ(fl.Free(20), VV_OK);
    EXPECT_EQ(fl.Free(30), VV_OK);
    EXPECT_EQ(fl.FreeCount(), 3u);

    PageID id = 0;
    EXPECT_EQ(fl.Allocate(id), VV_OK);
    EXPECT_EQ(id, 30u);
    EXPECT_EQ(fl.Allocate(id), VV_OK);
    EXPECT_EQ(id, 20u);
    EXPECT_EQ(fl.Allocate(id), VV_OK);
    EXPECT_EQ(id, 10u);
    EXPECT_EQ(fl.FreeCount(), 0u);
}

TEST(FreeList, FreeCountTracksOperations) {
    vv::storage::FreeList fl;
    EXPECT_EQ(fl.FreeCount(), 0u);
    fl.Free(1);
    EXPECT_EQ(fl.FreeCount(), 1u);
    fl.Free(2);
    EXPECT_EQ(fl.FreeCount(), 2u);
    PageID id;
    fl.Allocate(id);
    EXPECT_EQ(fl.FreeCount(), 1u);
}

TEST(FreeList, SerializeDeserializeRoundtrip) {
    vv::storage::FreeList fl;
    fl.Free(5);
    fl.Free(15);
    fl.Free(255);
    fl.Free(1024);

    char buf[vv::PAGE_SIZE] = {};
    size_t bytes_written = 0;
    fl.SerializeToPage(buf, bytes_written);
    EXPECT_GT(bytes_written, 0u);

    vv::storage::FreeList fl2;
    EXPECT_EQ(fl2.DeserializeFromPage(buf, vv::PAGE_SIZE), VV_OK);
    EXPECT_EQ(fl2.FreeCount(), 4u);

    PageID id;
    EXPECT_EQ(fl2.Allocate(id), VV_OK); EXPECT_EQ(id, 1024u);
    EXPECT_EQ(fl2.Allocate(id), VV_OK); EXPECT_EQ(id, 255u);
    EXPECT_EQ(fl2.Allocate(id), VV_OK); EXPECT_EQ(id, 15u);
    EXPECT_EQ(fl2.Allocate(id), VV_OK); EXPECT_EQ(id, 5u);
    EXPECT_EQ(fl2.Allocate(id), VV_ERR_NOT_FOUND);
}

TEST(FreeList, SerializeEmptyList) {
    vv::storage::FreeList fl;
    char buf[vv::PAGE_SIZE] = {};
    size_t bytes_written = 0;
    fl.SerializeToPage(buf, bytes_written);

    vv::storage::FreeList fl2;
    fl2.Free(99);
    EXPECT_EQ(fl2.DeserializeFromPage(buf, vv::PAGE_SIZE), VV_OK);
    EXPECT_EQ(fl2.FreeCount(), 0u);
}

TEST(FreeList, DeserializeFromTooSmallPage) {
    vv::storage::FreeList fl;
    fl.Free(42);
    EXPECT_EQ(fl.DeserializeFromPage("", 0), VV_OK);
    EXPECT_EQ(fl.FreeCount(), 0u);
}

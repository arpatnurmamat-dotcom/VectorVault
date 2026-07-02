#include <gtest/gtest.h>
#include "storage/file_io.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <vectorvault/vectorvault.h>

static std::string TempPath(std::string suffix) {
    return "/tmp/vv_test_fileio_" + suffix + ".dat";
}

static void Remove(const std::string& path) {
    std::remove(path.c_str());
}

TEST(FileIO, OpenCreatesNewFile) {
    std::string p = TempPath("open_create");
    Remove(p);
    int fd = -1;
    ASSERT_EQ(vv::storage::FileIO::Open(p, true, fd), VV_OK);
    ASSERT_GE(fd, 0);
    EXPECT_EQ(vv::storage::FileIO::Close(fd), VV_OK);
    Remove(p);
}

TEST(FileIO, OpenExistingWithoutCreateSucceeds) {
    std::string p = TempPath("open_existing");
    Remove(p);
    int fd = -1;
    ASSERT_EQ(vv::storage::FileIO::Open(p, true, fd), VV_OK);
    ASSERT_EQ(vv::storage::FileIO::Close(fd), VV_OK);
    int fd2 = -1;
    EXPECT_EQ(vv::storage::FileIO::Open(p, false, fd2), VV_OK);
    EXPECT_GE(fd2, 0);
    EXPECT_EQ(vv::storage::FileIO::Close(fd2), VV_OK);
    Remove(p);
}

TEST(FileIO, OpenNonExistentWithoutCreateFails) {
    std::string p = TempPath("open_no_such");
    Remove(p);
    int fd = -1;
    EXPECT_EQ(vv::storage::FileIO::Open(p, false, fd), VV_ERR_IO);
}

TEST(FileIO, WriteReadRoundtrip) {
    std::string p = TempPath("rw_roundtrip");
    Remove(p);
    int fd = -1;
    ASSERT_EQ(vv::storage::FileIO::Open(p, true, fd), VV_OK);

    std::vector<char> buf(4096);
    for (size_t i = 0; i < buf.size(); ++i) buf[i] = static_cast<char>(i % 251);

    EXPECT_EQ(vv::storage::FileIO::Write(fd, buf.data(), buf.size(), 0), VV_OK);

    std::vector<char> out(4096, 0);
    size_t bytes_read = 0;
    EXPECT_EQ(vv::storage::FileIO::Read(fd, out.data(), out.size(), 0, bytes_read), VV_OK);
    EXPECT_EQ(bytes_read, buf.size());
    EXPECT_EQ(std::memcmp(buf.data(), out.data(), buf.size()), 0);

    EXPECT_EQ(vv::storage::FileIO::Close(fd), VV_OK);
    Remove(p);
}

TEST(FileIO, ReadPastEndReturnsZeroBytes) {
    std::string p = TempPath("read_past_end");
    Remove(p);
    int fd = -1;
    ASSERT_EQ(vv::storage::FileIO::Open(p, true, fd), VV_OK);

    char data[8] = {'1','2','3','4','5','6','7','8'};
    EXPECT_EQ(vv::storage::FileIO::Write(fd, data, 8, 0), VV_OK);

    char out[16] = {};
    size_t bytes_read = 99;
    EXPECT_EQ(vv::storage::FileIO::Read(fd, out, 16, 8, bytes_read), VV_OK);
    EXPECT_EQ(bytes_read, 0u);

    EXPECT_EQ(vv::storage::FileIO::Close(fd), VV_OK);
    Remove(p);
}

TEST(FileIO, GetFileSizeTracksWrites) {
    std::string p = TempPath("file_size");
    Remove(p);
    int fd = -1;
    ASSERT_EQ(vv::storage::FileIO::Open(p, true, fd), VV_OK);

    size_t sz = 999;
    EXPECT_EQ(vv::storage::FileIO::GetFileSize(fd, sz), VV_OK);
    EXPECT_EQ(sz, 0u);

    char data[1024] = {};
    EXPECT_EQ(vv::storage::FileIO::Write(fd, data, 1024, 0), VV_OK);
    EXPECT_EQ(vv::storage::FileIO::GetFileSize(fd, sz), VV_OK);
    EXPECT_EQ(sz, 1024u);

    EXPECT_EQ(vv::storage::FileIO::Write(fd, data, 1024, 2048), VV_OK);
    EXPECT_EQ(vv::storage::FileIO::GetFileSize(fd, sz), VV_OK);
    EXPECT_EQ(sz, 3072u);

    EXPECT_EQ(vv::storage::FileIO::Close(fd), VV_OK);
    Remove(p);
}

TEST(FileIO, ResizeShrinksAndGrows) {
    std::string p = TempPath("resize");
    Remove(p);
    int fd = -1;
    ASSERT_EQ(vv::storage::FileIO::Open(p, true, fd), VV_OK);

    char data[8192] = {};
    EXPECT_EQ(vv::storage::FileIO::Write(fd, data, 8192, 0), VV_OK);

    EXPECT_EQ(vv::storage::FileIO::Resize(fd, 4096), VV_OK);
    size_t sz = 0;
    EXPECT_EQ(vv::storage::FileIO::GetFileSize(fd, sz), VV_OK);
    EXPECT_EQ(sz, 4096u);

    EXPECT_EQ(vv::storage::FileIO::Resize(fd, 16384), VV_OK);
    EXPECT_EQ(vv::storage::FileIO::GetFileSize(fd, sz), VV_OK);
    EXPECT_EQ(sz, 16384u);

    EXPECT_EQ(vv::storage::FileIO::Close(fd), VV_OK);
    Remove(p);
}

TEST(FileIO, SyncSucceeds) {
    std::string p = TempPath("sync");
    Remove(p);
    int fd = -1;
    ASSERT_EQ(vv::storage::FileIO::Open(p, true, fd), VV_OK);
    char data[64] = {'X'};
    EXPECT_EQ(vv::storage::FileIO::Write(fd, data, 64, 0), VV_OK);
    EXPECT_EQ(vv::storage::FileIO::Sync(fd), VV_OK);
    EXPECT_EQ(vv::storage::FileIO::Close(fd), VV_OK);
    Remove(p);
}

TEST(FileIO, MultipleWritesAtDifferentOffsets) {
    std::string p = TempPath("multi_offset");
    Remove(p);
    int fd = -1;
    ASSERT_EQ(vv::storage::FileIO::Open(p, true, fd), VV_OK);

    char a[8] = {'A','A','A','A','A','A','A','A'};
    char b[8] = {'B','B','B','B','B','B','B','B'};
    EXPECT_EQ(vv::storage::FileIO::Write(fd, a, 8, 0), VV_OK);
    EXPECT_EQ(vv::storage::FileIO::Write(fd, b, 8, 16), VV_OK);

    char out[8] = {};
    size_t n = 0;
    EXPECT_EQ(vv::storage::FileIO::Read(fd, out, 8, 16, n), VV_OK);
    EXPECT_EQ(n, 8u);
    EXPECT_EQ(std::memcmp(out, b, 8), 0);

    EXPECT_EQ(vv::storage::FileIO::Read(fd, out, 8, 0, n), VV_OK);
    EXPECT_EQ(std::memcmp(out, a, 8), 0);

    EXPECT_EQ(vv::storage::FileIO::Close(fd), VV_OK);
    Remove(p);
}

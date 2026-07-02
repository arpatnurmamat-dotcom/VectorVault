// Sanity test: verify the stub library links, the public API compiles,
// and vv_open/vv_close work without a real backend yet.

#include <gtest/gtest.h>
#include <vectorvault/vectorvault.h>
#include <vectorvault/vectorvault.hpp>

TEST(Sanity, VersionString) {
    EXPECT_STREQ(vv_version_string(), "0.1.0");
}

TEST(Sanity, ErrorString) {
    EXPECT_STREQ(vv_error_string(VV_OK),                "VV_OK");
    EXPECT_STREQ(vv_error_string(VV_ERR_IO),            "VV_ERR_IO");
    EXPECT_STREQ(vv_error_string(VV_ERR_NOMEM),         "VV_ERR_NOMEM");
    EXPECT_STREQ(vv_error_string(VV_ERR_INVALID_ARG),   "VV_ERR_INVALID_ARG");
    EXPECT_STREQ(vv_error_string(VV_ERR_NOT_FOUND),     "VV_ERR_NOT_FOUND");
    EXPECT_STREQ(vv_error_string(VV_ERR_EXISTS),        "VV_ERR_EXISTS");
    EXPECT_STREQ(vv_error_string(VV_ERR_INTERNAL),      "VV_ERR_INTERNAL");
    EXPECT_STREQ(vv_error_string(-999),       "VV_ERR_UNKNOWN");
}

TEST(Sanity, RejectZeroDimension) {
    vv_t* db = nullptr;
    vv_config_t cfg{};
    cfg.dimension = 0;
    int rc = vv_open(&db, "does_not_matter.vault", VV_OPEN_READWRITE, &cfg);
    EXPECT_EQ(rc, VV_ERR_INVALID_ARG);
    EXPECT_EQ(db, nullptr);
}

TEST(Sanity, RejectNullPtrs) {
    vv_t* db = nullptr;
    vv_config_t cfg{};
    cfg.dimension = 128;

    EXPECT_EQ(vv_open(nullptr,  "x.vault", VV_OPEN_READWRITE, &cfg), VV_ERR_INVALID_ARG);
    EXPECT_EQ(vv_open(&db,      nullptr,   VV_OPEN_READWRITE, &cfg), VV_ERR_INVALID_ARG);
    EXPECT_EQ(vv_open(&db,      "x.vault", VV_OPEN_READWRITE, nullptr), VV_ERR_INVALID_ARG);
}

TEST(Sanity, OpenCloseRoundtrip) {
    vv_t* db = nullptr;
    vv_config_t cfg{};
    cfg.dimension = 768;
    cfg.distance_metric = VV_DISTANCE_L2;
    cfg.index_type = VV_INDEX_HNSW;

    ASSERT_EQ(vv_open(&db, "/tmp/vv_sanity_open_close.vault", VV_OPEN_READWRITE, &cfg),
              VV_OK);
    ASSERT_NE(db, nullptr);
    EXPECT_EQ(vv_close(db), VV_OK);
}

TEST(Sanity, InfoApi) {
    vv_t* db = nullptr;
    vv_config_t cfg{};
    cfg.dimension = 384;
    ASSERT_EQ(vv_open(&db, "/tmp/vv_sanity_info.vault", VV_OPEN_READWRITE, &cfg), VV_OK);

    uint32_t maj = 0, min = 0, dim = 0;
    vv_distance_t dist{};
    vv_index_t idx{};
    uint64_t count = 999, fsize = 999;
    EXPECT_EQ(vv_info(db, &maj, &min, &dim, &dist, &idx, &count, &fsize), VV_OK);
    EXPECT_EQ(maj, VECTORVAULT_VERSION_MAJOR);
    EXPECT_EQ(min, VECTORVAULT_VERSION_MINOR);
    EXPECT_EQ(dim, 384u);
    EXPECT_EQ(dist, VV_DISTANCE_L2);
    EXPECT_EQ(idx, VV_INDEX_HNSW);
    EXPECT_EQ(count, 0u);

    EXPECT_EQ(vv_close(db), VV_OK);
}

TEST(Sanity, StubsReturnInternalOrNotFound) {
    vv_t* db = nullptr;
    vv_config_t cfg{};
    cfg.dimension = 128;
    ASSERT_EQ(vv_open(&db, "/tmp/vv_sanity_stubs.vault", VV_OPEN_READWRITE, &cfg), VV_OK);

    float v[128]{};
    EXPECT_EQ(vv_insert(db, 1, v, nullptr), VV_ERR_INTERNAL);

    vv_result_t r;
    uint32_t n = 0;
    EXPECT_EQ(vv_search(db, v, 1, nullptr, 0, &r, &n), VV_ERR_INTERNAL);
    EXPECT_EQ(n, 0u);

    EXPECT_EQ(vv_get(db, 1, v, nullptr, nullptr), VV_ERR_NOT_FOUND);
    EXPECT_EQ(vv_delete(db, 1), VV_ERR_INTERNAL);

    int exists = 99;
    EXPECT_EQ(vv_exists(db, 1, &exists), VV_ERR_INTERNAL);
    EXPECT_EQ(exists, 0);

    uint64_t cnt = 7;
    EXPECT_EQ(vv_count(db, &cnt), VV_ERR_INTERNAL);
    EXPECT_EQ(cnt, 0u);

    EXPECT_EQ(vv_close(db), VV_OK);
}

TEST(Sanity, CppWrapperCompilesAndThrows) {
    using namespace vv;
    EXPECT_THROW({
        Config cfg;
        cfg.SetDimension(0);
        auto db = Database::Open("/tmp/vv_sanity_cpp.vault", OPEN_READWRITE, cfg);
    }, Error);

    Config cfg;
    cfg.SetDimension(128);
    Database db = Database::Open("/tmp/vv_sanity_cpp.vault", OPEN_READWRITE, cfg);
    EXPECT_TRUE(db.IsOpen());
    EXPECT_THROW({ db.Insert(1, std::vector<float>(128)); }, Error);  // stub path
    db.Close();
    EXPECT_FALSE(db.IsOpen());
}

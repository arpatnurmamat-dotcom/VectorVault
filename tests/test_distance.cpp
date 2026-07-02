#include <gtest/gtest.h>
#include "index/distance.h"
#include "core/types.h"

#include <array>
#include <cmath>
#include <vector>

namespace {

constexpr float kEpsilon = 1e-4f;

TEST(Distance, DetectImplReturnsValidEnum) {
    auto impl = vv::index::DetectImpl();
    EXPECT_TRUE(impl == vv::index::Impl::kScalar ||
                impl == vv::index::Impl::kSSE ||
                impl == vv::index::Impl::kAVX2 ||
                impl == vv::index::Impl::kNEON);
}

TEST(Distance, L2ZeroVectorDistanceIsZero) {
    auto fn = vv::index::L2DistanceFn();
    ASSERT_NE(fn, nullptr);
    std::array<float, 8> z{};
    EXPECT_FLOAT_EQ(fn(z.data(), z.data(), 8), 0.0f);
}

TEST(Distance, L2SymmetricDistance) {
    auto fn = vv::index::L2DistanceFn();
    std::array<float, 4> a = {1.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 4> b = {0.0f, 1.0f, 0.0f, 0.0f};
    float d_ab = fn(a.data(), b.data(), 4);
    float d_ba = fn(b.data(), a.data(), 4);
    EXPECT_FLOAT_EQ(d_ab, d_ba);
}

TEST(Distance, L2KnownValue4d) {
    auto fn = vv::index::L2DistanceFn();
    std::array<float, 4> a = {1.0f, 2.0f, 3.0f, 4.0f};
    std::array<float, 4> b = {1.0f, 2.0f, 3.0f, 5.0f};
    EXPECT_NEAR(fn(a.data(), b.data(), 4), 1.0f, kEpsilon);
}

TEST(Distance, L2UnitVectors) {
    auto fn = vv::index::L2DistanceFn();
    std::array<float, 3> a = {1.0f, 0.0f, 0.0f};
    std::array<float, 3> b = {0.0f, 1.0f, 0.0f};
    EXPECT_NEAR(fn(a.data(), b.data(), 3), 2.0f, kEpsilon);
}

TEST(Distance, L2LargeDimension) {
    auto fn = vv::index::L2DistanceFn();
    std::vector<float> a(768, 0.1f);
    std::vector<float> b(768, 0.0f);
    float d = fn(a.data(), b.data(), 768);
    EXPECT_NEAR(d, 768.0f * 0.01f, kEpsilon);
}

TEST(Distance, L2DimOne) {
    auto fn = vv::index::L2DistanceFn();
    float a = 3.0f, b = 7.0f;
    EXPECT_NEAR(fn(&a, &b, 1), 16.0f, kEpsilon);
}

TEST(Distance, CosineZeroVectorReturnsOne) {
    auto fn = vv::index::CosineDistanceFn();
    std::array<float, 4> z{};
    std::array<float, 4> v = {1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_NEAR(fn(z.data(), v.data(), 4), 1.0f, kEpsilon);
}

TEST(Distance, CosineIdenticalVectorsZero) {
    auto fn = vv::index::CosineDistanceFn();
    std::array<float, 4> a = {1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_NEAR(fn(a.data(), a.data(), 4), 0.0f, kEpsilon);
}

TEST(Distance, CosineOrthogonalVectorsOne) {
    auto fn = vv::index::CosineDistanceFn();
    std::array<float, 3> a = {1.0f, 0.0f, 0.0f};
    std::array<float, 3> b = {0.0f, 1.0f, 0.0f};
    EXPECT_NEAR(fn(a.data(), b.data(), 3), 1.0f, kEpsilon);
}

TEST(Distance, CosineOppositeVectorsTwo) {
    auto fn = vv::index::CosineDistanceFn();
    std::array<float, 3> a = {1.0f, 0.0f, 0.0f};
    std::array<float, 3> b = {-1.0f, 0.0f, 0.0f};
    EXPECT_NEAR(fn(a.data(), b.data(), 3), 2.0f, kEpsilon);
}

TEST(Distance, CosineSymmetric) {
    auto fn = vv::index::CosineDistanceFn();
    std::array<float, 3> a = {1.0f, 2.0f, 3.0f};
    std::array<float, 3> b = {4.0f, 5.0f, 6.0f};
    float d_ab = fn(a.data(), b.data(), 3);
    float d_ba = fn(b.data(), a.data(), 3);
    EXPECT_FLOAT_EQ(d_ab, d_ba);
}

TEST(Distance, CosineScaleInvariant) {
    auto fn = vv::index::CosineDistanceFn();
    std::array<float, 3> a = {1.0f, 2.0f, 3.0f};
    std::array<float, 3> b = {2.0f, 4.0f, 6.0f};
    EXPECT_NEAR(fn(a.data(), b.data(), 3), 0.0f, kEpsilon);
}

TEST(Distance, IPDistanceNegatesDot) {
    auto fn = vv::index::InnerProductDistanceFn();
    std::array<float, 3> a = {1.0f, 2.0f, 3.0f};
    std::array<float, 3> b = {4.0f, 5.0f, 6.0f};
    float expected = -(1.0f * 4.0f + 2.0f * 5.0f + 3.0f * 6.0f);
    EXPECT_NEAR(fn(a.data(), b.data(), 3), expected, kEpsilon);
}

TEST(Distance, IPDistanceZeroVectors) {
    auto fn = vv::index::InnerProductDistanceFn();
    std::array<float, 4> z{};
    std::array<float, 4> v = {1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_NEAR(fn(z.data(), v.data(), 4), 0.0f, kEpsilon);
}

TEST(Distance, IPDistanceLargerMeansMoreSimilar) {
    auto fn = vv::index::InnerProductDistanceFn();

    std::array<float, 3> query = {1.0f, 0.0f, 0.0f};
    std::array<float, 3> aligned = {1.0f, 0.0f, 0.0f};
    std::array<float, 3> orthogonal = {0.0f, 1.0f, 0.0f};

    float d_aligned = fn(query.data(), aligned.data(), 3);
    float d_orthogonal = fn(query.data(), orthogonal.data(), 3);

    EXPECT_LT(d_aligned, d_orthogonal);
}

}

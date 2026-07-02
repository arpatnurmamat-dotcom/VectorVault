#include "index/distance.h"
#include <cmath>

namespace vv::index {

static float L2Scalar(const float* a, const float* b, size_t dim) {
    float sum = 0.0f;
    for (size_t i = 0; i < dim; ++i) {
        float d = a[i] - b[i];
        sum += d * d;
    }
    return sum;
}

static float CosineScalar(const float* a, const float* b, size_t dim) {
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (size_t i = 0; i < dim; ++i) {
        dot += a[i] * b[i];
        na  += a[i] * a[i];
        nb  += b[i] * b[i];
    }
    if (na == 0.0f || nb == 0.0f) return 1.0f;
    return 1.0f - dot / (std::sqrt(na) * std::sqrt(nb));
}

static float IPScalar(const float* a, const float* b, size_t dim) {
    float dot = 0.0f;
    for (size_t i = 0; i < dim; ++i) dot += a[i] * b[i];
    return -dot;
}

Impl DetectImpl() {
#if defined(VV_ARCH_X86_64)
    return Impl::kScalar;
#elif defined(VV_ARCH_ARM64)
    return Impl::kNEON;
#else
    return Impl::kScalar;
#endif
}

DistFn L2DistanceFn()           { return &L2Scalar; }
DistFn CosineDistanceFn()       { return &CosineScalar; }
DistFn InnerProductDistanceFn() { return &IPScalar; }

}

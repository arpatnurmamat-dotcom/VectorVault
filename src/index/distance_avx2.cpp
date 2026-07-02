#include "index/distance.h"

namespace vv::index {

#if defined(VV_ARCH_X86_64) && defined(__AVX2__) && defined(__FMA__)
#include <immintrin.h>

float L2AVX2(const float* a, const float* b, size_t dim) {
    __m256 sum = _mm256_setzero_ps();
    size_t i = 0;
    for (; i + 8 <= dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 d  = _mm256_sub_ps(va, vb);
        sum = _mm256_fmadd_ps(d, d, sum);
    }
    alignas(32) float tmp[8];
    _mm256_store_ps(tmp, sum);
    float s = 0.0f;
    for (int j = 0; j < 8; ++j) s += tmp[j];
    for (; i < dim; ++i) { float d = a[i] - b[i]; s += d * d; }
    return s;
}
#endif

}

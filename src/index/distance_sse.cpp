#include "index/distance.h"

namespace vv::index {

#if defined(VV_ARCH_X86_64) && defined(__SSE4_1__)
#include <smmintrin.h>

float L2SSE(const float* a, const float* b, size_t dim) {
    __m128 sum = _mm_setzero_ps();
    size_t i = 0;
    for (; i + 4 <= dim; i += 4) {
        __m128 va = _mm_loadu_ps(a + i);
        __m128 vb = _mm_loadu_ps(b + i);
        __m128 d  = _mm_sub_ps(va, vb);
        sum = _mm_add_ps(sum, _mm_mul_ps(d, d));
    }
    alignas(16) float tmp[4];
    _mm_store_ps(tmp, sum);
    float s = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    for (; i < dim; ++i) { float d = a[i] - b[i]; s += d * d; }
    return s;
}
#endif

}

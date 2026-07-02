#include "index/distance.h"

namespace vv::index {

#if defined(VV_ARCH_ARM64)
#include <arm_neon.h>

float L2NEON(const float* a, const float* b, size_t dim) {
    float32x4_t sum = vdupq_n_f32(0.0f);
    size_t i = 0;
    for (; i + 4 <= dim; i += 4) {
        float32x4_t va = vld1q_f32(a + i);
        float32x4_t vb = vld1q_f32(b + i);
        float32x4_t d  = vsubq_f32(va, vb);
        sum = vmlaq_f32(sum, d, d);
    }
    float tmp[4];
    vst1q_f32(tmp, sum);
    float s = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    for (; i < dim; ++i) { float d = a[i] - b[i]; s += d * d; }
    return s;
}
#endif

}

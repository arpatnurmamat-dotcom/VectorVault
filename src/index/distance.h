#ifndef VV_INDEX_DISTANCE_H
#define VV_INDEX_DISTANCE_H

#include "core/types.h"

namespace vv::index {

enum class Impl { kScalar, kSSE, kAVX2, kNEON };

Impl DetectImpl();
DistFn L2DistanceFn();
DistFn CosineDistanceFn();
DistFn InnerProductDistanceFn();

}

#endif

#include "query/executor.h"

namespace vv::query {

int QueryExecutor::Execute(QueryContext& ctx) {
    if (ctx.out_count) *ctx.out_count = 0;
    return VV_ERR_INTERNAL;
}

}

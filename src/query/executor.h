#ifndef VV_QUERY_EXECUTOR_H
#define VV_QUERY_EXECUTOR_H

#include "core/types.h"
#include <vectorvault/vectorvault.h>
#include <cstdint>

namespace vv::query {

struct QueryContext {
    const float*    query_vec    = nullptr;
    uint32_t        k            = 0;
    uint32_t        ef_search    = 0;
    const char*     filter_expr  = nullptr;
    vv_result_t*    out_results  = nullptr;
    uint32_t*       out_count    = nullptr;
};

class QueryExecutor {
public:
    int Execute(QueryContext& ctx);
};

}

#endif

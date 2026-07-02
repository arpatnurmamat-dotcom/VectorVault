#ifndef VV_META_FILTER_H
#define VV_META_FILTER_H

#include "core/types.h"
#include <string>
#include <vector>

namespace vv::meta {

class FilterParser {
public:
    struct Predicate { std::string field; std::string op; std::string value; };
    int Parse(const std::string& expr, std::vector<Predicate>& out_preds);
    bool Evaluate(const std::vector<Predicate>& preds, const char* metadata_json);
};

}

#endif

#include "meta/filter.h"
#include <vectorvault/vectorvault.h>

namespace vv::meta {

int FilterParser::Parse(const std::string& /*expr*/, std::vector<Predicate>& out_preds) {
    out_preds.clear();
    return VV_ERR_FILTER_PARSE;
}

bool FilterParser::Evaluate(const std::vector<Predicate>& /*preds*/, const char* /*metadata_json*/) {
    return true;
}

}

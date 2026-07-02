/*
 * VectorVault — Error Handling Implementation
 */

#include "core/error.h"

namespace vv {

const char* ErrorToString(int errcode) {
    switch (errcode) {
        case VV_OK:                return "VV_OK";
        case VV_ERR_IO:            return "VV_ERR_IO";
        case VV_ERR_NOMEM:         return "VV_ERR_NOMEM";
        case VV_ERR_INVALID_ARG:   return "VV_ERR_INVALID_ARG";
        case VV_ERR_NOT_FOUND:     return "VV_ERR_NOT_FOUND";
        case VV_ERR_EXISTS:        return "VV_ERR_EXISTS";
        case VV_ERR_CORRUPT:       return "VV_ERR_CORRUPT";
        case VV_ERR_WRONG_VERSION: return "VV_ERR_WRONG_VERSION";
        case VV_ERR_LOCKED:        return "VV_ERR_LOCKED";
        case VV_ERR_FILTER_PARSE:  return "VV_ERR_FILTER_PARSE";
        case VV_ERR_OUT_OF_RANGE:  return "VV_ERR_OUT_OF_RANGE";
        case VV_ERR_INTERNAL:      return "VV_ERR_INTERNAL";
        default:                   return "VV_ERR_UNKNOWN";
    }
}

} // namespace vv

/* Public C API: vv_error_string (declared in vectorvault.h) */
extern "C" const char* vv_error_string(int errcode) {
    return vv::ErrorToString(errcode);
}

/* Public C API: vv_version_string */
extern "C" const char* vv_version_string(void) {
    return "0.1.0";
}

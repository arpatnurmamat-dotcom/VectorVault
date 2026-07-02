/*
 * VectorVault — Error Handling Utilities
 */

#ifndef VV_CORE_ERROR_H
#define VV_CORE_ERROR_H

#include <vectorvault/vectorvault.h>

namespace vv {

/**
 * Convert a vv_error_t to a static string.
 * Unknown codes return "VV_ERR_UNKNOWN".
 */
const char* ErrorToString(int errcode);

/**
 * Macro: return early on error.
 *
 * Usage:
 *   VV_RETURN_IF_ERROR(some_function_returning_int());
 *   // only reached if rc == VV_OK
 */
#define VV_RETURN_IF_ERROR(expr) \
    do { \
        int _vv_rc = (expr); \
        if (_vv_rc != VV_OK) return _vv_rc; \
    } while (0)

} // namespace vv

#endif  /* VV_CORE_ERROR_H */

# ─────────────────────────────────────────────────────────────────────────────
# VectorVault Tests
# ─────────────────────────────────────────────────────────────────────────────
# Uses CTest + GoogleTest (fetched via FetchContent for zero-depend policy)
# ─────────────────────────────────────────────────────────────────────────────

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
# Prevent GTest from overriding parent build flags
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Helper macro: add a unit test file and register it with CTest.
# Usage: vv_add_test(test_file_name [extra_sources...])
function(vv_add_test TEST_NAME)
    set(TEST_SOURCES ${TEST_NAME}.cpp ${ARGN})
    add_executable(${TEST_NAME} ${TEST_SOURCES})
    target_link_libraries(${TEST_NAME}
        PRIVATE
            vectorvault
            gtest_main
    )
    target_include_directories(${TEST_NAME}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_target_properties(${TEST_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endfunction()

# Phase 0: Sanity / Baseline
vv_add_test(test_sanity)

# ─────────────────────────────────────────────────────────────────────────────
# Phase 1: Storage Foundation
# ─────────────────────────────────────────────────────────────────────────────
vv_add_test(test_file_io)
vv_add_test(test_free_list)
vv_add_test(test_buffer_pool)
vv_add_test(test_page_manager)

# Phase 2: Vector Storage + Flat Index
vv_add_test(test_distance)
vv_add_test(test_flat)

# Phase 3: HNSW Index
vv_add_test(test_hnsw)
vv_add_test(test_hnsw_io)

# Phase 4: Meta Index (B+tree)
vv_add_test(test_btree)

# Phase 5: API + WAL
# vv_add_test(test_api)
# vv_add_test(test_wal)
# vv_add_test(test_crash_recovery)

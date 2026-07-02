cmake_minimum_required(VERSION 3.16)
project(vectorvault VERSION 0.1.0 LANGUAGES C CXX)

# ─────────────────────────────────────────────────────────────────────────────
# Language Standards
# ─────────────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ─────────────────────────────────────────────────────────────────────────────
# Build Options
# ─────────────────────────────────────────────────────────────────────────────
option(VECTORVAULT_BUILD_TESTS    "Build unit tests"         ON)
option(VECTORVAULT_BUILD_BENCH    "Build benchmark suite"   OFF)
option(VECTORVAULT_BUILD_CLI      "Build vv CLI tool"       ON)
option(VECTORVAULT_BUILD_EXAMPLES "Build examples"          OFF)

# ─────────────────────────────────────────────────────────────────────────────
# SIMD Compiler Flag Detection (per-file flags used later)
# ─────────────────────────────────────────────────────────────────────────────
include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|x86")
    check_cxx_compiler_flag("-mavx2"          VV_COMPILER_HAS_AVX2)
    check_cxx_compiler_flag("-mavx2 -mfma"    VV_COMPILER_HAS_AVX2_FMA)
    check_cxx_compiler_flag("-msse4.1"        VV_COMPILER_HAS_SSE41)
    check_cxx_compiler_flag("-mfma"           VV_COMPILER_HAS_FMA)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Core Library
# ─────────────────────────────────────────────────────────────────────────────
set(VV_SOURCES
    src/core/database.cpp
    src/core/config.cpp
    src/core/error.cpp
    src/storage/page_manager.cpp
    src/storage/buffer_pool.cpp
    src/storage/free_list.cpp
    src/storage/file_io.cpp
    src/index/hnsw.cpp
    src/index/hnsw_io.cpp
    src/index/flat.cpp
    src/index/distance.cpp
    src/meta/btree.cpp
    src/meta/filter.cpp
    src/query/executor.cpp
)

# Add platform-specific SIMD distance implementations
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|x86")
    list(APPEND VV_SOURCES src/index/distance_sse.cpp)
    if(VV_COMPILER_HAS_SSE41)
        set_source_files_properties(src/index/distance_sse.cpp
            PROPERTIES COMPILE_OPTIONS "$<$<COMPILE_LANGUAGE:CXX>:-msse4.1>")
    endif()
    if(VV_COMPILER_HAS_AVX2_FMA)
        list(APPEND VV_SOURCES src/index/distance_avx2.cpp)
        set_source_files_properties(src/index/distance_avx2.cpp
            PROPERTIES COMPILE_OPTIONS "$<$<COMPILE_LANGUAGE:CXX>:-mavx2$<SEMICOLON>-mfma>")
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64|armv8")
    list(APPEND VV_SOURCES src/index/distance_neon.cpp)
endif()

add_library(vectorvault STATIC ${VV_SOURCES})

target_include_directories(vectorvault
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(vectorvault PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion
        -Wsign-conversion -Wnull-dereference
    >
    $<$<CXX_COMPILER_ID:GNU>:
        -Wduplicated-cond
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4 /permissive- /w14242 /w14254 /w14263 /w14265 /w14287
        /w14289 /w14296 /w14302 /w14311 /w14545
    >
    $<$<CONFIG:Debug>:-O0 -g>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
    $<$<CONFIG:RelWithDebInfo>:-O3 -g -DNDEBUG>
)

# Thread support (std::shared_mutex used in database)
find_package(Threads REQUIRED)
target_link_libraries(vectorvault PUBLIC Threads::Threads)

# Platform-specific link libs
if(WIN32)
    # Windows file handle APIs
    # target_link_libraries(vectorvault PRIVATE ws2_32 kernel32)
elseif(APPLE)
    # No extra libs needed on macOS
elseif(UNIX)
    # No extra libs needed on Linux
endif()

# ─────────────────────────────────────────────────────────────────────────────
# CLI Tool
# ─────────────────────────────────────────────────────────────────────────────
if(VECTORVAULT_BUILD_CLI)
    add_executable(vv tools/vv_cli.cpp)
    target_link_libraries(vv PRIVATE vectorvault)
    set_target_properties(vv PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Tests
# ─────────────────────────────────────────────────────────────────────────────
if(VECTORVAULT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Benchmarks
# ─────────────────────────────────────────────────────────────────────────────
if(VECTORVAULT_BUILD_BENCH)
    add_subdirectory(tests/bench)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Examples
# ─────────────────────────────────────────────────────────────────────────────
if(VECTORVAULT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Install
# ─────────────────────────────────────────────────────────────────────────────
install(TARGETS vectorvault
    EXPORT vectorvaultTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
install(
    DIRECTORY include/vectorvault
    DESTINATION include
)

# Generate install package config for cmake find_package()
install(EXPORT vectorvaultTargets
    FILE vectorvaultTargets.cmake
    NAMESPACE vectorvault::
    DESTINATION lib/cmake/vectorvault
)

/*
 * VectorVault — C++ Wrapper
 *
 * A thin, RAII-friendly wrapper over the C API.
 * Uses std::optional, std::vector, std::string_view, std::unique_ptr.
 *
 * This wrapper is HEADER-ONLY for simplicity; it only includes the public C API.
 *
 * Version: 0.1.0
 * License: MIT
 */

#ifndef VECTORVAULT_HPP
#define VECTORVAULT_HPP

#include "vectorvault.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vv {

/* ──────────────────────────────────────────────────────────────────────────
 * Exception Type
 * ────────────────────────────────────────────────────────────────────────── */
class Error : public std::runtime_error {
public:
    explicit Error(int code, const char* msg)
        : std::runtime_error(msg)
        , code_(code)
    {}

    int code() const noexcept { return code_; }

private:
    int code_;
};

namespace detail {
    [[noreturn]] inline void ThrowOnError(int rc) {
        throw Error(rc, vv_error_string(rc));
    }
} // namespace detail

/* ──────────────────────────────────────────────────────────────────────────
 * Configuration Builder
 * ────────────────────────────────────────────────────────────────────────── */
struct Config {
    uint32_t        dimension             = 0;
    vv_distance_t   distance_metric       = VV_DISTANCE_L2;
    vv_index_t      index_type            = VV_INDEX_HNSW;
    uint32_t        hnsw_M                = 16;
    uint32_t        hnsw_ef_construction  = 200;
    uint32_t        buffer_pool_size_mb   = 8;

    Config& SetDimension(uint32_t d)       { dimension = d; return *this; }
    Config& SetDistance(vv_distance_t m)   { distance_metric = m; return *this; }
    Config& SetIndex(vv_index_t t)         { index_type = t; return *this; }
    Config& SetHnswM(uint32_t m)           { hnsw_M = m; return *this; }
    Config& SetHnswEfConstruction(uint32_t ef) { hnsw_ef_construction = ef; return *this; }
    Config& SetBufferPoolSizeMB(uint32_t mb)   { buffer_pool_size_mb = mb; return *this; }

    vv_config_t ToC() const noexcept {
        vv_config_t c{};
        c.dimension             = dimension;
        c.distance_metric       = distance_metric;
        c.index_type            = index_type;
        c.hnsw_M                = hnsw_M;
        c.hnsw_ef_construction  = hnsw_ef_construction;
        c.buffer_pool_size_mb   = buffer_pool_size_mb;
        return c;
    }
};

/* ──────────────────────────────────────────────────────────────────────────
 * Search Result
 * ────────────────────────────────────────────────────────────────────────── */
struct Result {
    uint64_t id;
    double   distance;
};

/* ──────────────────────────────────────────────────────────────────────────
 * Database Handle (RAII)
 * ────────────────────────────────────────────────────────────────────────── */
class Database {
public:
    Database() noexcept = default;

    ~Database() noexcept { Close(); }

    /* Move only */
    Database(Database&& other) noexcept : db_(other.db_) { other.db_ = nullptr; }
    Database& operator=(Database&& other) noexcept {
        if (this != &other) {
            Close();
            db_ = other.db_;
            other.db_ = nullptr;
        }
        return *this;
    }
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /** Open (or create) a database. Throws vv::Error on failure. */
    static Database Open(std::string_view path, uint32_t flags, const Config& cfg) {
        vv_t* raw = nullptr;
        std::string path_buf(path);
        vv_config_t c = cfg.ToC();
        int rc = vv_open(&raw, path_buf.c_str(), flags, &c);
        if (rc != VV_OK) detail::ThrowOnError(rc);
        return Database(raw);
    }

    /** Close the database. Throws on error. */
    void Close() {
        if (db_) {
            int rc = vv_close(db_);
            db_ = nullptr;
            if (rc != VV_OK) detail::ThrowOnError(rc);
        }
    }

    vv_t* raw() const noexcept { return db_; }
    bool IsOpen() const noexcept { return db_ != nullptr; }

    /* ───────────────────────────────────────────────────────────────
     * Mutations
     * ─────────────────────────────────────────────────────────────── */

    void Insert(uint64_t id, const std::vector<float>& vector, std::string_view metadata = {}) {
        const char* meta_ptr = nullptr;
        std::string meta_buf;
        if (!metadata.empty()) {
            meta_buf = std::string(metadata);
            meta_ptr = meta_buf.c_str();
        }
        int rc = vv_insert(db_, id, vector.data(), meta_ptr);
        if (rc != VV_OK) detail::ThrowOnError(rc);
    }

    void Delete(uint64_t id) {
        int rc = vv_delete(db_, id);
        if (rc != VV_OK && rc != VV_ERR_NOT_FOUND) detail::ThrowOnError(rc);
    }

    /* ───────────────────────────────────────────────────────────────
     * Queries
     * ─────────────────────────────────────────────────────────────── */

    std::vector<Result> Search(
        const std::vector<float>& query,
        uint32_t k,
        uint32_t ef_search = 0,
        std::string_view filter = {}
    ) {
        std::vector<vv_result_t> raw(k);
        uint32_t filled = 0;
        std::string filter_buf;
        const char* filter_ptr = nullptr;
        if (!filter.empty()) {
            filter_buf = std::string(filter);
            filter_ptr = filter_buf.c_str();
        }
        int rc = vv_search(db_, query.data(), k, filter_ptr, ef_search, raw.data(), &filled);
        if (rc != VV_OK) detail::ThrowOnError(rc);
        std::vector<Result> results;
        results.reserve(filled);
        for (uint32_t i = 0; i < filled; ++i) {
            results.push_back(Result{raw[i].id, raw[i].distance});
        }
        return results;
    }

    /** Get a single vector (+metadata) by id. Returns nullopt if not found. */
    std::optional<std::pair<std::vector<float>, std::string>>
    Get(uint64_t id, uint32_t dim, bool with_metadata = true) {
        std::vector<float> vec(dim);
        char* meta = nullptr;
        size_t meta_len = 0;
        int rc = vv_get(
            db_, id,
            vec.data(),
            with_metadata ? &meta : nullptr,
            with_metadata ? &meta_len : nullptr
        );
        if (rc == VV_ERR_NOT_FOUND) return std::nullopt;
        if (rc != VV_OK) detail::ThrowOnError(rc);

        std::string meta_str;
        if (meta) {
            meta_str.assign(meta, meta_len);
            vv_free(meta);
        }
        return std::make_pair(std::move(vec), std::move(meta_str));
    }

    bool Exists(uint64_t id) {
        int exists = 0;
        int rc = vv_exists(db_, id, &exists);
        if (rc != VV_OK) detail::ThrowOnError(rc);
        return exists != 0;
    }

    uint64_t Count() {
        uint64_t c = 0;
        int rc = vv_count(db_, &c);
        if (rc != VV_OK) detail::ThrowOnError(rc);
        return c;
    }

    /* ───────────────────────────────────────────────────────────────
     * Maintenance
     * ─────────────────────────────────────────────────────────────── */
    void Compact() {
        int rc = vv_compact(db_);
        if (rc != VV_OK) detail::ThrowOnError(rc);
    }

    void Sync() {
        int rc = vv_sync(db_);
        if (rc != VV_OK) detail::ThrowOnError(rc);
    }

private:
    explicit Database(vv_t* db) noexcept : db_(db) {}
    vv_t* db_ = nullptr;
};

/* ──────────────────────────────────────────────────────────────────────────
 * Library Version Info
 * ────────────────────────────────────────────────────────────────────────── */
inline std::string_view Version() noexcept {
    return vv_version_string();
}

/* ──────────────────────────────────────────────────────────────────────────
 * Convenience Constants (mirror the C enum)
 * ────────────────────────────────────────────────────────────────────────── */
constexpr uint32_t OPEN_READONLY  = VV_OPEN_READONLY;
constexpr uint32_t OPEN_READWRITE = VV_OPEN_READWRITE;
constexpr uint32_t OPEN_CREATE    = VV_OPEN_CREATE;
constexpr uint32_t OPEN_TRUNCATE  = VV_OPEN_TRUNCATE;

constexpr vv_distance_t DISTANCE_L2     = VV_DISTANCE_L2;
constexpr vv_distance_t DISTANCE_COSINE = VV_DISTANCE_COSINE;
constexpr vv_distance_t DISTANCE_IP     = VV_DISTANCE_IP;

constexpr vv_index_t INDEX_HNSW = VV_INDEX_HNSW;
constexpr vv_index_t INDEX_FLAT = VV_INDEX_FLAT;

} // namespace vv

#endif  /* VECTORVAULT_HPP */

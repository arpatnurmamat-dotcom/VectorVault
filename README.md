# VectorVault

**Embedded vector search for any application.**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![C11](https://img.shields.io/badge/C-11-blue.svg)](https://en.cppreference.com/w/c/11)

VectorVault is a zero-dependency, single-file embedded vector database written in C++17 with a stable C API. Like SQLite for relational data, VectorVault makes it trivial to add vector search to any application — no server, no configuration, no deployment.

---

## Why VectorVault?

| Need | VectorVault | Qdrant / Milvus | FAISS | ChromaDB |
|------|:-----------:|:---------------:|:-----:|:--------:|
| Embedded (no server) | ✅ | ❌ | ✅ library only | ⚠️ |
| Native C/C++ | ✅ | ❌ | ✅ | ❌ Python |
| Single-file persistence | ✅ | ❌ | ❌ | ✅ |
| Crash recovery (WAL) | ✅ | ✅ | ❌ | ❌ |
| Metadata filtering | ✅ | ✅ | ❌ | ✅ |
| Zero external deps | ✅ | ❌ | ⚠️ BLAS | ❌ |
| Incremental insert | ✅ | ✅ | ⚠️ rebuild | ✅ |

**Target use cases:** local RAG engines, game NPC memory, desktop AI assistants, on-device semantic search, and any C++ application that needs vector search without the operational overhead of a separate service.

---

## Features

- **HNSW index** with incremental insert and tunable recall/latency trade-off
- **Three distance metrics:** cosine, L² (Euclidean), inner product
- **JSON metadata** per vector with filter expression queries (`type = 'article' AND year >= 2024`)
- **SIMD-accelerated** distance computation (AVX2, SSE4.1, ARM NEON — auto-detected at runtime)
- **Page-based storage engine** with LRU buffer pool and free-list space reclamation
- **WAL crash recovery** — data survives process crashes
- **Thread-safe:** concurrent reads via `shared_mutex`, serialized writes
- **Cross-platform:** Linux, macOS, Windows; x86-64 and ARM64
- **Stable C ABI** for FFI bindings to any language

---

## Quick Start

### Build

```bash
git clone https://github.com/your-org/vectorvault.git
cd vectorvault
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Produces:
- `libvectorvault.a` — static library
- `vv` — CLI inspection tool

### C API

```c
#include <vectorvault.h>

/* 1. Configure and open */
vv_config_t config = {0};
config.dimension       = 768;
config.distance_metric = VV_DISTANCE_COSINE;
config.index_type      = VV_INDEX_HNSW;

vv_t* db = NULL;
vv_open(&db, "knowledge.vault",
        VV_OPEN_READWRITE | VV_OPEN_CREATE, &config);

/* 2. Insert vectors with metadata */
float embedding[768] = { /* your embedding model output */ };
vv_insert(db, 1, embedding, "{\"text\":\"hello world\",\"source\":\"doc1\"}");

/* 3. KNN search */
float query[768] = { /* query embedding */ };
vv_result_t results[5];
uint32_t n = 0;
vv_search(db, query, 5, NULL, 0, results, &n);

for (uint32_t i = 0; i < n; i++) {
    printf("id=%" PRIu64 "  distance=%.4f\n", results[i].id, results[i].distance);
}

/* 4. Filter search */
vv_search(db, query, 5, "source = 'doc1'", 0, results, &n);

/* 5. Close — auto-flushes WAL and serializes index */
vv_close(db);
```

### C++ API

```cpp
#include <vectorvault/vectorvault.hpp>

// Open with fluent config
auto db = vv::Database::Open("knowledge.vault",
    vv::OPEN_READWRITE | vv::OPEN_CREATE,
    vv::Config{}
        .SetDimension(768)
        .SetDistance(vv::DISTANCE_COSINE));

// Insert
db.Insert(1, embedding_vec, R"({"text":"hello world"})");

// Search
auto results = db.Search(query_vec, /*k=*/5);
for (auto& r : results) {
    std::cout << r.id << " " << r.distance << "\n";
}

// Search with filter
auto filtered = db.Search(query_vec, 5, /*ef_search=*/0, "source = 'doc1'");

// Fetch + delete
auto record = db.Get(1, 768);          // std::optional<pair<vector, metadata>>
db.Delete(1);

// Maintenance
db.Compact();
```

---

## API Reference

### Lifecycle

| Function | Description |
|----------|-------------|
| `vv_open()` | Open or create a `.vault` file |
| `vv_close()` | Flush, serialize HNSW index, close |

### CRUD

| Function | Description |
|----------|-------------|
| `vv_insert()` | Insert a vector with optional JSON metadata |
| `vv_get()` | Fetch vector and metadata by ID |
| `vv_delete()` | Mark-delete (tombstone; reclaimed by `vv_compact`) |
| `vv_exists()` | Check existence by ID |

### Search

| Function | Description |
|----------|-------------|
| `vv_search()` | KNN search with optional filter expression and `ef_search` override |

**Filter expressions:**
```
type = 'article' AND year >= 2024
NOT deleted AND (category = 'tech' OR category = 'science')
price < 100 AND rating >= 4.5
```

### Maintenance

| Function | Description |
|----------|-------------|
| `vv_compact()` | Reclaim tombstone space and rebuild index |
| `vv_sync()` | Flush dirty pages to disk |
| `vv_count()` | Current vector count |
| `vv_info()` | Version, dimension, metric, index type, file size |

---

## Configuration

```c
typedef struct {
    uint32_t      dimension;              // REQUIRED: vector dimension
    vv_distance_t distance_metric;        // VV_DISTANCE_L2 | VV_DISTANCE_COSINE | VV_DISTANCE_IP
    vv_index_t    index_type;             // VV_INDEX_HNSW (default) | VV_INDEX_FLAT
    uint32_t      hnsw_M;                 // max neighbors per level (default: 16)
    uint32_t      hnsw_ef_construction;   // build-time search width (default: 200)
    uint32_t      buffer_pool_size_mb;    // page cache size in MB (default: 8)
} vv_config_t;
```

### HNSW Tuning Guide

| Parameter | Higher | Lower |
|-----------|--------|-------|
| `hnsw_M` | Better recall, more memory | Faster insert, less memory |
| `hnsw_ef_construction` | Higher-quality graph, slower build | Faster build, lower recall |
| `ef_search` (per query) | Better recall, slower query | Faster query, lower recall |

---

## Architecture

```
┌──────────────────────────────────────────────────┐
│              C API / C++ Wrapper / CLI            │
├──────────────────────────────────────────────────┤
│                  Query Executor                   │
│        KNN Search · Filter Eval · Re-ranking      │
├────────────────────┬─────────────────────────────┤
│   HNSW / Flat      │    B+tree Metadata Index     │
│   Vector Index     │    (id → JSON, range scan)   │
├────────────────────┴─────────────────────────────┤
│              Storage Engine (4 KB pages)           │
│    Page Manager · LRU Buffer Pool · Free-List     │
├──────────────────────────────────────────────────┤
│       File I/O (pread/pwrite) · WAL · Platform    │
└──────────────────────────────────────────────────┘
```

**File layout:** single `.vault` file — Header page → Free-list → B+tree → Vector data → HNSW index. A companion `.vault-wal` file is created during active writes and truncated on checkpoint.

---

## Performance Targets

| Workload | Target |
|----------|--------|
| 100K × 768d, KNN(k=10) | < 3 ms P95 |
| 1M × 768d, KNN(k=10) | < 10 ms P95 |
| 10M × 768d, KNN(k=10) | < 50 ms P95 |
| HNSW recall@10 (1M) | > 0.95 vs brute-force |
| Batch insert | > 10K vectors/sec |
| Single insert | < 1 ms |
| Get by ID | < 0.1 ms |

---

## Installation

### As a CMake dependency

```cmake
add_subdirectory(vectorvault)
target_link_libraries(your_app PRIVATE vectorvault)
```

After `make install`:

```cmake
find_package(vectorvault REQUIRED)
target_link_libraries(your_app PRIVATE vectorvault::vectorvault)
```

### Manual

```bash
cmake --install . --prefix /usr/local
# Produces:
#   /usr/local/lib/libvectorvault.a
#   /usr/local/include/vectorvault/vectorvault.h
#   /usr/local/include/vectorvault/vectorvault.hpp
```

---

## Testing

```bash
cd build
ctest --output-on-failure
```

Tests cover: storage (page manager, buffer pool, free-list, file I/O), B+tree, HNSW index (in-memory + I/O), flat index, distance computations, and a sanity integration test.

---

## CLI Tool

```bash
vv info knowledge.vault      # Show database stats
vv compact knowledge.vault   # Reclaim space and rebuild index
vv dump  knowledge.vault     # Dump all vectors and metadata
```

---

## Requirements

- **Compiler:** GCC 9+, Clang 10+, MSVC 19.28+ (C++17 / C11)
- **Build:** CMake 3.16+
- **OS:** Linux, macOS, Windows
- **Runtime dependencies:** none (only system threading library)

---

## Roadmap

- [ ] Batch insert / batch search
- [ ] mmap mode for large databases
- [ ] Scalar quantization (float32 → int8) for memory reduction
- [ ] Python, Rust, Go, and Node.js bindings
- [ ] IVF-PQ index for 10M+ scale
- [ ] AES-256 at-rest encryption

---

## License

MIT — see [LICENSE](LICENSE).

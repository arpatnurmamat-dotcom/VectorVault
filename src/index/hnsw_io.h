#ifndef VV_INDEX_HNSW_IO_H
#define VV_INDEX_HNSW_IO_H

#include "index/hnsw.h"
#include "core/types.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace vv::storage { class PageManager; }

namespace vv::index {

/** Serialize/deserialize an HNSW graph to/from page storage.
 *
 *  On-disk layout
 *  ─────────────
 *  Header page (HNSW_PAGE_ID):
 *    [uint32_t magic] [uint64_t node_count] [uint32_t entry_id]
 *    [uint32_t max_level] [uint32_t has_entry] [uint64_t chain_head]
 *    [uint64_t chain_bytes]
 *
 *  Node pages (linked list via overflow PageID at end of each page):
 *    Per-node records packed sequentially.  Each record:
 *      [uint32_t internal_id] [uint32_t level] [uint8_t deleted]
 *      [float vec[dim]]
 *      [uint32_t num_levels] [uint32_t num_nbrs_0] [InternalID nbr[]] ...
 */
class HNSWCheckpoint {
public:
    /** Write the current in-memory graph state to pages.
     *  The caller must ensure no concurrent modifications during Save. */
    static int Save(HNSWIndex& idx, storage::PageManager& pm);

    /** Reconstruct the in-memory graph from previously saved pages.
     *  Clears any existing graph state before loading. */
    static int Load(HNSWIndex& idx, storage::PageManager& pm,
                    size_t dim, DistFn dfn,
                    uint32_t M, uint32_t ef_c, uint32_t ef_s);

private:
    static int WriteChain(storage::PageManager& pm,
                          const std::vector<char>& data,
                          PageID& head_out);

    static int ReadChain(storage::PageManager& pm,
                         PageID head, size_t total_bytes,
                         std::vector<char>& out);
};

}

#endif

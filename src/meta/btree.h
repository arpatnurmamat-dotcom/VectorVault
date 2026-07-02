#ifndef VV_META_BTREE_H
#define VV_META_BTREE_H

#include "core/types.h"
#include "storage/page_manager.h"
#include <cstdint>
#include <cstddef>
#include <utility>
#include <vector>

namespace vv::meta {

class BPlusTree {
public:
    /**
     * Construct a B+tree backed by the given page manager.
     * @param mgr  PageManager for disk I/O
     * @param root_page_id  Page ID reserved for the root node (default: BTREE_ROOT_PAGE_ID)
     * @param order  Maximum number of keys per node (default: 127)
     */
    explicit BPlusTree(storage::PageManager* mgr,
                       PageID root_page_id = BTREE_ROOT_PAGE_ID,
                       uint16_t order = 127);

    ~BPlusTree();

    BPlusTree(const BPlusTree&) = delete;
    BPlusTree& operator=(const BPlusTree&) = delete;

    /**
     * Insert a key-value pair. If key already exists, the value is updated.
     * @return VV_OK on success, VV_ERR_NOMEM if page allocation fails
     */
    int Insert(uint64_t key, PageID value_page);

    /**
     * Find the value associated with a key.
     * @param key           Key to look up
     * @param value_page_out [out] Receives the value if found
     * @return VV_OK if found, VV_ERR_NOT_FOUND if key is absent
     */
    int Find(uint64_t key, PageID& value_page_out) const;

    /**
     * Remove a key from the tree.
     * @return VV_OK on success, VV_ERR_NOT_FOUND if key is absent
     */
    int Remove(uint64_t key);

    /**
     * Check whether a key exists in the tree.
     */
    bool Contains(uint64_t key) const;

    /**
     * Return the number of key-value pairs in the tree.
     */
    size_t Count() const;

    /**
     * Query all key-value pairs where key_start <= key <= key_end.
     * Results are returned in ascending key order.
     * @return VV_OK on success
     */
    int RangeQuery(uint64_t key_start, uint64_t key_end,
                   std::vector<std::pair<uint64_t, PageID>>& out) const;

    /**
     * Persist tree metadata (root page, count) so it can be loaded later.
     * @return VV_OK on success
     */
    int Save();

    /**
     * Load tree metadata from the root page. The tree must have been Save()d previously.
     * @return VV_OK on success
     */
    int Load();

private:
    static constexpr uint8_t NODE_LEAF    = 1;
    static constexpr uint8_t NODE_INTERNAL = 2;

    struct NodeHeader {
        uint8_t  type;       ///< NODE_LEAF or NODE_INTERNAL
        uint16_t num_keys;   ///< Number of keys in this node
    };

    struct SearchResult {
        int  index;
        bool found;
    };

    storage::PageManager* mgr_;
    PageID    root_page_id_;
    uint16_t  order_;
    size_t    count_;

    SearchResult BinarySearch(const uint64_t* keys, uint16_t num_keys, uint64_t key) const;

    int ReadNode(PageID page_id, NodeHeader& hdr,
                 uint64_t* keys, uint64_t* vals, uint64_t& next_or_rightmost) const;

    int WriteLeafNode(PageID page_id, uint16_t num_keys,
                      const uint64_t* keys, const uint64_t* vals, PageID next_leaf);

    int WriteInternalNode(PageID page_id, uint16_t num_keys,
                          const uint64_t* keys, const uint64_t* children, PageID rightmost);

    int AllocAndWriteLeaf(uint16_t num_keys, const uint64_t* keys,
                          const uint64_t* vals, PageID next_leaf, PageID& out_id);

    int AllocAndWriteInternal(uint16_t num_keys, const uint64_t* keys,
                              const uint64_t* children, PageID rightmost, PageID& out_id);

    int InsertInternal(PageID page_id, uint64_t key, PageID left_child, PageID right_child);

    int RemoveFromLeaf(PageID page_id, uint64_t key);

    int RemoveFromInternal(PageID page_id, uint64_t key);

    int CountSubtree(PageID page_id) const;
};

} // namespace vv::meta

#endif

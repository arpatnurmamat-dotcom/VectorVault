#include "meta/btree.h"
#include <vectorvault/vectorvault.h>
#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

namespace vv::meta {

BPlusTree::BPlusTree(storage::PageManager* mgr, PageID root_page_id, uint16_t order)
    : mgr_(mgr), root_page_id_(root_page_id), order_(order), count_(0) {}

BPlusTree::~BPlusTree() = default;

BPlusTree::SearchResult BPlusTree::BinarySearch(const uint64_t* keys, uint16_t num_keys, uint64_t key) const {
    SearchResult sr{0, false};
    uint16_t lo = 0, hi = num_keys;
    while (lo < hi) {
        uint16_t mid = static_cast<uint16_t>((lo + hi) / 2);
        if (keys[mid] == key) {
            sr.index = static_cast<int>(mid);
            sr.found = true;
            return sr;
        }
        if (keys[mid] < key) {
            lo = static_cast<uint16_t>(mid + 1);
        } else {
            hi = mid;
        }
    }
    sr.index = static_cast<int>(lo);
    sr.found = false;
    return sr;
}

int BPlusTree::ReadNode(PageID page_id, NodeHeader& hdr,
                         uint64_t* keys, uint64_t* vals, uint64_t& next_or_rightmost) const {
    Frame* f = nullptr;
    int rc = mgr_->GetPage(page_id, f);
    if (rc != VV_OK) return rc;
    const char* p = f->data;
    hdr.type = static_cast<uint8_t>(p[0]);
    std::memcpy(&hdr.num_keys, p + 1, 2);
    size_t off = 8;
    if (hdr.num_keys > 0) {
        std::memcpy(keys, p + off, static_cast<size_t>(hdr.num_keys) * 8);
        off += static_cast<size_t>(hdr.num_keys) * 8;
        std::memcpy(vals, p + off, static_cast<size_t>(hdr.num_keys) * 8);
        off += static_cast<size_t>(hdr.num_keys) * 8;
    }
    std::memcpy(&next_or_rightmost, p + off, 8);
    return VV_OK;
}

int BPlusTree::WriteLeafNode(PageID page_id, uint16_t num_keys,
                              const uint64_t* keys, const uint64_t* vals, PageID next_leaf) {
    Frame* f = nullptr;
    int rc = mgr_->GetPage(page_id, f);
    if (rc != VV_OK) return rc;
    char* p = f->data;
    p[0] = static_cast<char>(NODE_LEAF);
    std::memset(p + 1, 0, 7);
    std::memcpy(p + 1, &num_keys, 2);
    size_t off = 8;
    if (num_keys > 0) {
        std::memcpy(p + off, keys, static_cast<size_t>(num_keys) * 8);
        off += static_cast<size_t>(num_keys) * 8;
        std::memcpy(p + off, vals, static_cast<size_t>(num_keys) * 8);
        off += static_cast<size_t>(num_keys) * 8;
    }
    std::memcpy(p + off, &next_leaf, 8);
    return mgr_->MarkDirty(page_id);
}

int BPlusTree::WriteInternalNode(PageID page_id, uint16_t num_keys,
                                  const uint64_t* keys, const uint64_t* children, PageID rightmost) {
    Frame* f = nullptr;
    int rc = mgr_->GetPage(page_id, f);
    if (rc != VV_OK) return rc;
    char* p = f->data;
    p[0] = static_cast<char>(NODE_INTERNAL);
    std::memset(p + 1, 0, 7);
    std::memcpy(p + 1, &num_keys, 2);
    size_t off = 8;
    if (num_keys > 0) {
        std::memcpy(p + off, keys, static_cast<size_t>(num_keys) * 8);
        off += static_cast<size_t>(num_keys) * 8;
        std::memcpy(p + off, children, static_cast<size_t>(num_keys) * 8);
        off += static_cast<size_t>(num_keys) * 8;
    }
    std::memcpy(p + off, &rightmost, 8);
    return mgr_->MarkDirty(page_id);
}

int BPlusTree::AllocAndWriteLeaf(uint16_t num_keys, const uint64_t* keys,
                                  const uint64_t* vals, PageID next_leaf, PageID& out_id) {
    char buf[PAGE_SIZE] = {};
    buf[0] = static_cast<char>(NODE_LEAF);
    std::memcpy(buf + 1, &num_keys, 2);
    size_t off = 8;
    if (num_keys > 0) {
        std::memcpy(buf + off, keys, static_cast<size_t>(num_keys) * 8);
        off += static_cast<size_t>(num_keys) * 8;
        std::memcpy(buf + off, vals, static_cast<size_t>(num_keys) * 8);
        off += static_cast<size_t>(num_keys) * 8;
    }
    std::memcpy(buf + off, &next_leaf, 8);
    return mgr_->AllocPage(buf, &out_id);
}

int BPlusTree::AllocAndWriteInternal(uint16_t num_keys, const uint64_t* keys,
                                      const uint64_t* children, PageID rightmost, PageID& out_id) {
    char buf[PAGE_SIZE] = {};
    buf[0] = static_cast<char>(NODE_INTERNAL);
    std::memcpy(buf + 1, &num_keys, 2);
    size_t off = 8;
    if (num_keys > 0) {
        std::memcpy(buf + off, keys, static_cast<size_t>(num_keys) * 8);
        off += static_cast<size_t>(num_keys) * 8;
        std::memcpy(buf + off, children, static_cast<size_t>(num_keys) * 8);
        off += static_cast<size_t>(num_keys) * 8;
    }
    std::memcpy(buf + off, &rightmost, 8);
    return mgr_->AllocPage(buf, &out_id);
}

int BPlusTree::Insert(uint64_t key, PageID value_page) {
    Frame* root_frame = nullptr;
    int rc = mgr_->GetPage(root_page_id_, root_frame);
    if (rc != VV_OK) {
        uint64_t k = key, v = value_page;
        PageID next_leaf = static_cast<PageID>(-1);
        rc = WriteLeafNode(root_page_id_, 1, &k, &v, next_leaf);
        if (rc != VV_OK) return rc;
        ++count_;
        return VV_OK;
    }

    {
        Frame* rf = nullptr;
        if (mgr_->GetPage(root_page_id_, rf) == VV_OK) {
            uint8_t rtype = static_cast<uint8_t>(rf->data[0]);
            if (rtype != NODE_LEAF && rtype != NODE_INTERNAL) {
                PageID next_leaf = static_cast<PageID>(-1);
                rc = WriteLeafNode(root_page_id_, 1, &key, &value_page, next_leaf);
                if (rc != VV_OK) return rc;
                ++count_;
                return VV_OK;
            }
        }
    }

    struct PathEntry {
        PageID page_id;
        int    child_index;
    };
    std::vector<PathEntry> path;
    PageID current = root_page_id_;

    while (true) {
        NodeHeader hdr;
        uint64_t keys[256], vals[256];
        uint64_t next_or_rm;
        rc = ReadNode(current, hdr, keys, vals, next_or_rm);
        if (rc != VV_OK) return rc;
        if (hdr.type == NODE_LEAF) break;

        auto sr = BinarySearch(keys, hdr.num_keys, key);
        int ci = sr.found ? sr.index + 1 : sr.index;
        PageID child = (ci >= static_cast<int>(hdr.num_keys))
                             ? static_cast<PageID>(next_or_rm)
                             : static_cast<PageID>(vals[ci]);
        path.push_back({current, ci});
        current = child;
    }

    NodeHeader leaf_hdr;
    uint64_t leaf_keys[257], leaf_vals[257];
    uint64_t leaf_next;
    rc = ReadNode(current, leaf_hdr, leaf_keys, leaf_vals, leaf_next);
    if (rc != VV_OK) return rc;

    auto sr = BinarySearch(leaf_keys, leaf_hdr.num_keys, key);
    if (sr.found) {
        leaf_vals[sr.index] = value_page;
        return WriteLeafNode(current, leaf_hdr.num_keys, leaf_keys, leaf_vals,
                             static_cast<PageID>(leaf_next));
    }

    uint16_t n = leaf_hdr.num_keys;
    for (int i = static_cast<int>(n); i > sr.index; --i) {
        leaf_keys[i] = leaf_keys[i - 1];
        leaf_vals[i] = leaf_vals[i - 1];
    }
    leaf_keys[sr.index] = key;
    leaf_vals[sr.index] = value_page;
    ++n;

    if (n <= 2 * order_) {
        rc = WriteLeafNode(current, n, leaf_keys, leaf_vals, static_cast<PageID>(leaf_next));
        if (rc != VV_OK) return rc;
        ++count_;
        return VV_OK;
    }

    int mid = n / 2;
    PageID new_leaf_id = 0;
    rc = AllocAndWriteLeaf(static_cast<uint16_t>(n - mid),
                           leaf_keys + mid, leaf_vals + mid,
                           static_cast<PageID>(leaf_next), new_leaf_id);
    if (rc != VV_OK) return rc;

    rc = WriteLeafNode(current, static_cast<uint16_t>(mid),
                       leaf_keys, leaf_vals, new_leaf_id);
    if (rc != VV_OK) return rc;

    uint64_t promote_key = leaf_keys[mid];
    PageID right_page = new_leaf_id;
    PageID left_page = current;

    for (int level = static_cast<int>(path.size()) - 1; level >= 0; --level) {
        PageID parent_id = path[static_cast<size_t>(level)].page_id;
        int child_idx = path[static_cast<size_t>(level)].child_index;

        NodeHeader phdr;
        uint64_t pkeys[257], pvals[258];
        uint64_t pnext;
        rc = ReadNode(parent_id, phdr, pkeys, pvals, pnext);
        if (rc != VV_OK) return rc;

        uint64_t children[258];
        for (int i = 0; i < static_cast<int>(phdr.num_keys); ++i) {
            children[i] = pvals[i];
        }
        children[static_cast<int>(phdr.num_keys)] = pnext;

        for (int i = static_cast<int>(phdr.num_keys); i > child_idx; --i) {
            pkeys[i] = pkeys[i - 1];
        }
        pkeys[child_idx] = promote_key;

        for (int i = static_cast<int>(phdr.num_keys) + 1; i > child_idx + 1; --i) {
            children[i] = children[i - 1];
        }
        children[child_idx] = left_page;
        children[child_idx + 1] = right_page;

        uint16_t new_nk = static_cast<uint16_t>(phdr.num_keys + 1);

        if (new_nk <= 2 * order_) {
            uint64_t final_vals[257];
            for (int i = 0; i < static_cast<int>(new_nk); ++i) {
                final_vals[i] = children[i];
            }
            PageID final_rm = static_cast<PageID>(children[new_nk]);
            rc = WriteInternalNode(parent_id, new_nk, pkeys, final_vals, final_rm);
            if (rc != VV_OK) return rc;
            ++count_;
            return VV_OK;
        }

        int smid = new_nk / 2;
        uint64_t up_key = pkeys[smid];

        PageID new_int_id = 0;
        rc = AllocAndWriteInternal(static_cast<uint16_t>(new_nk - smid - 1),
                                   pkeys + smid + 1,
                                   children + smid + 1,
                                   static_cast<PageID>(children[new_nk]),
                                   new_int_id);
        if (rc != VV_OK) return rc;

        rc = WriteInternalNode(parent_id, static_cast<uint16_t>(smid),
                               pkeys, children,
                               static_cast<PageID>(children[smid]));
        if (rc != VV_OK) return rc;

        promote_key = up_key;
        left_page = parent_id;
        right_page = new_int_id;

        if (level == 0) {
            PageID new_root_id = 0;
            uint64_t rk = promote_key;
            uint64_t rc_arr[1] = {static_cast<uint64_t>(left_page)};
            rc = AllocAndWriteInternal(1, &rk, rc_arr, right_page, new_root_id);
            if (rc != VV_OK) return rc;
            root_page_id_ = new_root_id;
            ++count_;
            return VV_OK;
        }
    }

    if (path.empty()) {
        PageID new_root_id = 0;
        uint64_t rk = promote_key;
        uint64_t rc_arr[1] = {static_cast<uint64_t>(left_page)};
        rc = AllocAndWriteInternal(1, &rk, rc_arr, right_page, new_root_id);
        if (rc != VV_OK) return rc;
        root_page_id_ = new_root_id;
        ++count_;
        return VV_OK;
    }

    ++count_;
    return VV_OK;
}

int BPlusTree::Find(uint64_t key, PageID& value_page_out) const {
    Frame* f = nullptr;
    int rc = mgr_->GetPage(root_page_id_, f);
    if (rc != VV_OK) return VV_ERR_NOT_FOUND;

    PageID current = root_page_id_;
    while (true) {
        NodeHeader hdr;
        uint64_t keys[256], vals[256];
        uint64_t next_or_rm;
        rc = ReadNode(current, hdr, keys, vals, next_or_rm);
        if (rc != VV_OK) return rc;
        if (hdr.type != NODE_LEAF && hdr.type != NODE_INTERNAL) return VV_ERR_NOT_FOUND;

        auto sr = BinarySearch(keys, hdr.num_keys, key);
        if (hdr.type == NODE_LEAF) {
            if (sr.found) {
                value_page_out = static_cast<PageID>(vals[sr.index]);
                return VV_OK;
            }
            return VV_ERR_NOT_FOUND;
        }

        int ci = sr.found ? sr.index + 1 : sr.index;
        current = (ci >= static_cast<int>(hdr.num_keys))
                      ? static_cast<PageID>(next_or_rm)
                      : static_cast<PageID>(vals[ci]);
    }
}

int BPlusTree::Remove(uint64_t key) {
    Frame* f = nullptr;
    int rc = mgr_->GetPage(root_page_id_, f);
    if (rc != VV_OK) return VV_ERR_NOT_FOUND;

    std::vector<PageID> path;
    PageID current = root_page_id_;

    while (true) {
        NodeHeader hdr;
        uint64_t keys[256], vals[256];
        uint64_t next_or_rm;
        rc = ReadNode(current, hdr, keys, vals, next_or_rm);
        if (rc != VV_OK) return rc;
        if (hdr.type != NODE_LEAF && hdr.type != NODE_INTERNAL) return VV_ERR_NOT_FOUND;
        if (hdr.type == NODE_LEAF) break;

        path.push_back(current);
        auto sr = BinarySearch(keys, hdr.num_keys, key);
        int ci = sr.found ? sr.index + 1 : sr.index;
        current = (ci >= static_cast<int>(hdr.num_keys))
                      ? static_cast<PageID>(next_or_rm)
                      : static_cast<PageID>(vals[ci]);
    }

    NodeHeader leaf_hdr;
    uint64_t leaf_keys[256], leaf_vals[256];
    uint64_t leaf_next;
    rc = ReadNode(current, leaf_hdr, leaf_keys, leaf_vals, leaf_next);
    if (rc != VV_OK) return rc;

    auto sr = BinarySearch(leaf_keys, leaf_hdr.num_keys, key);
    if (!sr.found) return VV_ERR_NOT_FOUND;

    for (int i = sr.index; i < static_cast<int>(leaf_hdr.num_keys) - 1; ++i) {
        leaf_keys[i] = leaf_keys[i + 1];
        leaf_vals[i] = leaf_vals[i + 1];
    }
    --leaf_hdr.num_keys;
    --count_;

    if (leaf_hdr.num_keys == 0 && !path.empty()) {
        PageID parent_id = path.back();
        path.pop_back();

        NodeHeader phdr;
        uint64_t pkeys[256], pvals[256];
        uint64_t pnext;
        rc = ReadNode(parent_id, phdr, pkeys, pvals, pnext);
        if (rc != VV_OK) return rc;

        auto psr = BinarySearch(pkeys, phdr.num_keys, key);
        int ci = psr.index;
        if (ci >= static_cast<int>(phdr.num_keys)) {
            ci = static_cast<int>(phdr.num_keys) - 1;
        }

        uint64_t children[257];
        for (int i = 0; i < static_cast<int>(phdr.num_keys); ++i) {
            children[i] = pvals[i];
        }
        children[static_cast<int>(phdr.num_keys)] = pnext;

        for (int i = ci; i < static_cast<int>(phdr.num_keys) - 1; ++i) {
            pkeys[i] = pkeys[i + 1];
        }
        for (int i = ci; i < static_cast<int>(phdr.num_keys); ++i) {
            children[i] = children[i + 1];
        }
        --phdr.num_keys;

        if (phdr.num_keys == 0 && path.empty()) {
            PageID new_root = static_cast<PageID>(children[0]);
            root_page_id_ = new_root;
            return VV_OK;
        }

        PageID new_rm = static_cast<PageID>(children[phdr.num_keys]);
        rc = WriteInternalNode(parent_id, phdr.num_keys, pkeys, pvals, new_rm);
        return rc;
    }

    rc = WriteLeafNode(current, leaf_hdr.num_keys, leaf_keys, leaf_vals,
                       static_cast<PageID>(leaf_next));
    return rc;
}

bool BPlusTree::Contains(uint64_t key) const {
    PageID val = 0;
    return Find(key, val) == VV_OK;
}

size_t BPlusTree::Count() const {
    return count_;
}

int BPlusTree::RangeQuery(uint64_t key_start, uint64_t key_end,
                           std::vector<std::pair<uint64_t, PageID>>& out) const {
    out.clear();
    Frame* f = nullptr;
    int rc = mgr_->GetPage(root_page_id_, f);
    if (rc != VV_OK) return VV_OK;

    PageID current = root_page_id_;
    while (true) {
        NodeHeader hdr;
        uint64_t keys[256], vals[256];
        uint64_t next_or_rm;
        rc = ReadNode(current, hdr, keys, vals, next_or_rm);
        if (rc != VV_OK) return rc;
        if (hdr.type != NODE_LEAF && hdr.type != NODE_INTERNAL) return VV_OK;
        if (hdr.type == NODE_LEAF) break;

        auto sr = BinarySearch(keys, hdr.num_keys, key_start);
        int ci = sr.found ? sr.index + 1 : sr.index;
        current = (ci >= static_cast<int>(hdr.num_keys))
                      ? static_cast<PageID>(next_or_rm)
                      : static_cast<PageID>(vals[ci]);
    }

    PageID leaf = current;
    while (leaf != static_cast<PageID>(-1)) {
        NodeHeader hdr;
        uint64_t keys[256], vals[256];
        uint64_t next_or_rm;
        rc = ReadNode(leaf, hdr, keys, vals, next_or_rm);
        if (rc != VV_OK) return rc;

        bool past_end = false;
        for (uint16_t i = 0; i < hdr.num_keys; ++i) {
            if (keys[i] > key_end) { past_end = true; break; }
            if (keys[i] >= key_start) {
                out.emplace_back(keys[i], static_cast<PageID>(vals[i]));
            }
        }
        if (past_end) break;
        leaf = static_cast<PageID>(next_or_rm);
    }
    return VV_OK;
}

int BPlusTree::Save() {
    Frame* f = nullptr;
    int rc = mgr_->GetPage(root_page_id_, f);
    if (rc != VV_OK) return rc;
    uint64_t saved_root = root_page_id_;
    std::memcpy(f->data + PAGE_SIZE - 32, &saved_root, 8);
    std::memcpy(f->data + PAGE_SIZE - 24, &count_, 8);
    return mgr_->MarkDirty(root_page_id_);
}

int BPlusTree::Load() {
    Frame* f = nullptr;
    int rc = mgr_->GetPage(root_page_id_, f);
    if (rc != VV_OK) return rc;

    uint64_t loaded_root = 0;
    std::memcpy(&loaded_root, f->data + PAGE_SIZE - 32, 8);
    size_t loaded_count = 0;
    std::memcpy(&loaded_count, f->data + PAGE_SIZE - 24, 8);

    if (loaded_root == 0 || loaded_root == static_cast<PageID>(-1)) {
        return VV_ERR_NOT_FOUND;
    }
    root_page_id_ = static_cast<PageID>(loaded_root);
    count_ = loaded_count;
    return VV_OK;
}

int BPlusTree::CountSubtree(PageID) const {
    return VV_OK;
}

} // namespace vv::meta

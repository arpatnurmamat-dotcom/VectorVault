#include "index/hnsw_io.h"
#include "storage/page_manager.h"
#include <vectorvault/vectorvault.h>

#include <cmath>
#include <cstring>
#include <vector>

using PageID = vv::PageID;

namespace vv::index {

namespace {

constexpr uint32_t HNSW_CKPT_MAGIC = 0x484E5753;
constexpr size_t USABLE = PAGE_SIZE - sizeof(PageID);

static void W32(char* p, uint32_t v) { std::memcpy(p, &v, 4); }
static void W64(char* p, uint64_t v) { std::memcpy(p, &v, 8); }
static uint32_t R32(const char* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
static uint64_t R64(const char* p) { uint64_t v; std::memcpy(&v, p, 8); return v; }

}

int HNSWCheckpoint::WriteChain(storage::PageManager& pm,
                                const std::vector<char>& data,
                                PageID& head_out) {
    if (data.empty()) { head_out = 0; return VV_OK; }

    PageID prev = 0;
    bool has_prev = false;
    size_t off = 0;

    while (off < data.size()) {
        char buf[PAGE_SIZE]{};
        size_t n = std::min(USABLE, data.size() - off);
        std::memcpy(buf, data.data() + off, n);
        off += n;

        PageID pid = 0;
        int rc = pm.AllocPage(buf, &pid);
        if (rc != VV_OK) return rc;
        pm.MarkDirty(pid);

        if (!has_prev) { head_out = pid; has_prev = true; }
        else {
            Frame* pf = nullptr;
            rc = pm.GetPage(prev, pf);
            if (rc != VV_OK) return rc;
            std::memcpy(pf->data + USABLE, &pid, sizeof(PageID));
            pm.MarkDirty(prev);
        }
        prev = pid;
    }
    return VV_OK;
}

int HNSWCheckpoint::ReadChain(storage::PageManager& pm,
                               PageID head, size_t total_bytes,
                               std::vector<char>& out) {
    out.clear();
    out.reserve(total_bytes);
    PageID cur = head;
    size_t remaining = total_bytes;

    while (remaining > 0) {
        Frame* f = nullptr;
        int rc = pm.GetPage(cur, f);
        if (rc != VV_OK) return rc;
        size_t n = std::min(USABLE, remaining);
        out.insert(out.end(), f->data, f->data + n);
        remaining -= n;
        if (remaining > 0) {
            std::memcpy(&cur, f->data + USABLE, sizeof(PageID));
        }
    }
    return VV_OK;
}

int HNSWCheckpoint::Save(HNSWIndex& idx, storage::PageManager& pm) {
    std::lock_guard<std::mutex> lk(idx.mu_);

    char hdr[PAGE_SIZE]{};
    W32(hdr + 0, HNSW_CKPT_MAGIC);
    W64(hdr + 4, idx.nodes_.size());
    W32(hdr + 12, idx.entry_id_);
    W32(hdr + 16, idx.max_level_);
    W32(hdr + 20, idx.has_entry_ ? 1 : 0);

    std::vector<char> buf;
    for (auto& [id, nd] : idx.nodes_) {
        size_t base = buf.size();
        uint32_t nl = static_cast<uint32_t>(nd.neighbors.size());
        size_t sz = sizeof(uint32_t) + sizeof(uint32_t) + 1
                  + idx.dim_ * sizeof(float) + sizeof(uint32_t);
        for (uint32_t l = 0; l < nl; ++l) {
            sz += sizeof(uint32_t) + nd.neighbors[l].size() * sizeof(InternalID);
        }
        buf.resize(base + sz);
        char* p = buf.data() + base;
        std::memcpy(p, &id, sizeof(InternalID)); p += sizeof(InternalID);
        W32(p, nd.level); p += sizeof(uint32_t);
        *p++ = nd.deleted ? 1 : 0;
        std::memcpy(p, nd.vec.data(), idx.dim_ * sizeof(float));
        p += idx.dim_ * sizeof(float);
        W32(p, nl); p += sizeof(uint32_t);
        for (uint32_t l = 0; l < nl; ++l) {
            uint32_t nn = static_cast<uint32_t>(nd.neighbors[l].size());
            W32(p, nn); p += sizeof(uint32_t);
            if (nn) {
                std::memcpy(p, nd.neighbors[l].data(), nn * sizeof(InternalID));
                p += nn * sizeof(InternalID);
            }
        }
    }

    PageID chain = 0;
    if (!buf.empty()) {
        int rc = WriteChain(pm, buf, chain);
        if (rc != VV_OK) return rc;
    }
    W64(hdr + 28, chain);
    W64(hdr + 36, buf.size());

    Frame* f = nullptr;
    int rc = pm.GetPage(HNSW_PAGE_ID, f);
    if (rc != VV_OK) return rc;
    std::memcpy(f->data, hdr, PAGE_SIZE);
    pm.MarkDirty(HNSW_PAGE_ID);
    return VV_OK;
}

int HNSWCheckpoint::Load(HNSWIndex& idx, storage::PageManager& pm,
                          size_t dim, DistFn dfn,
                          uint32_t M, uint32_t ef_c, uint32_t ef_s) {
    Frame* f = nullptr;
    int rc = pm.GetPage(HNSW_PAGE_ID, f);
    if (rc != VV_OK) return rc;
    if (R32(f->data) != HNSW_CKPT_MAGIC) return VV_ERR_CORRUPT;

    size_t node_count = static_cast<size_t>(R64(f->data + 4));
    idx.entry_id_ = R32(f->data + 12);
    idx.max_level_ = R32(f->data + 16);
    idx.has_entry_ = R32(f->data + 20) != 0;
    PageID chain = R64(f->data + 28);
    size_t chain_sz = static_cast<size_t>(R64(f->data + 36));

    idx.dim_ = dim;
    idx.dist_fn_ = dfn;
    idx.M_ = M;
    idx.M_max_ = M;
    idx.M_max0_ = 2 * M;
    idx.ef_c_ = ef_c;
    idx.ef_s_ = ef_s;
    idx.mL_ = 1.0 / std::log(static_cast<double>(M));
    idx.nodes_.clear();

    if (chain_sz == 0) {
        idx.count_ = 0;
        return VV_OK;
    }

    std::vector<char> buf;
    rc = ReadChain(pm, chain, chain_sz, buf);
    if (rc != VV_OK) return rc;

    size_t pos = 0;
    for (size_t i = 0; i < node_count; ++i) {
        if (pos + sizeof(uint32_t) + sizeof(uint32_t) + 1
            + dim * sizeof(float) + sizeof(uint32_t) > buf.size()) break;

        InternalID id;
        std::memcpy(&id, buf.data() + pos, sizeof(InternalID));
        pos += sizeof(InternalID);
        uint32_t lvl = R32(buf.data() + pos); pos += sizeof(uint32_t);
        bool del = buf[pos++] != 0;

        HNSWIndex::Node nd;
        nd.level = lvl;
        nd.deleted = del;
        nd.vec.resize(dim);
        std::memcpy(nd.vec.data(), buf.data() + pos, dim * sizeof(float));
        pos += dim * sizeof(float);

        uint32_t nl = R32(buf.data() + pos); pos += sizeof(uint32_t);
        nd.neighbors.resize(nl);
        for (uint32_t l = 0; l < nl; ++l) {
            uint32_t nn = R32(buf.data() + pos); pos += sizeof(uint32_t);
            nd.neighbors[l].resize(nn);
            if (nn) {
                std::memcpy(nd.neighbors[l].data(), buf.data() + pos,
                            nn * sizeof(InternalID));
                pos += nn * sizeof(InternalID);
            }
        }
        idx.nodes_[id] = std::move(nd);
    }

    idx.count_ = idx.nodes_.size();
    for (auto& [_, n] : idx.nodes_) {
        if (n.deleted) --idx.count_;
    }
    return VV_OK;
}

}

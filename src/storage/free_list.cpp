#include "storage/free_list.h"
#include <cstring>
#include <vectorvault/vectorvault.h>

namespace vv::storage {

int FreeList::Allocate(PageID& id_out) {
    std::lock_guard<std::mutex> lk(mu_);
    if (free_pages_.empty()) return VV_ERR_NOT_FOUND;
    id_out = free_pages_.back();
    free_pages_.pop_back();
    return VV_OK;
}

int FreeList::Free(PageID id) {
    std::lock_guard<std::mutex> lk(mu_);
    free_pages_.push_back(id);
    return VV_OK;
}

void FreeList::SerializeToPage(char* page_buf, size_t& bytes_written) const {
    std::lock_guard<std::mutex> lk(mu_);
    size_t count = free_pages_.size();
    size_t byte_size = sizeof(uint32_t) + count * sizeof(PageID);
    if (byte_size > PAGE_SIZE) byte_size = PAGE_SIZE;
    std::memset(page_buf, 0, PAGE_SIZE);
    uint32_t n = static_cast<uint32_t>(count);
    std::memcpy(page_buf, &n, sizeof(uint32_t));
    for (size_t i = 0; i < count && (sizeof(uint32_t) + (i+1)*sizeof(PageID)) <= PAGE_SIZE; ++i) {
        std::memcpy(page_buf + sizeof(uint32_t) + i*sizeof(PageID),
                    &free_pages_[i], sizeof(PageID));
    }
    bytes_written = byte_size;
}

int FreeList::DeserializeFromPage(const char* page_buf, size_t page_size) {
    std::lock_guard<std::mutex> lk(mu_);
    free_pages_.clear();
    if (page_size < sizeof(uint32_t)) return VV_OK;
    uint32_t n = 0;
    std::memcpy(&n, page_buf, sizeof(uint32_t));
    size_t offset = sizeof(uint32_t);
    for (uint32_t i = 0; i < n && offset + sizeof(PageID) <= page_size; ++i) {
        PageID id;
        std::memcpy(&id, page_buf + offset, sizeof(PageID));
        free_pages_.push_back(id);
        offset += sizeof(PageID);
    }
    return VV_OK;
}

size_t FreeList::FreeCount() const {
    std::lock_guard<std::mutex> lk(mu_);
    return free_pages_.size();
}

}

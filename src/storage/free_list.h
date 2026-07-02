#ifndef VV_STORAGE_FREE_LIST_H
#define VV_STORAGE_FREE_LIST_H

#include "core/types.h"
#include <vector>
#include <mutex>

namespace vv::storage {

class FreeList {
public:
    int Allocate(PageID& id_out);
    int Free(PageID id);
    void SerializeToPage(char* page_buf, size_t& bytes_written) const;
    int DeserializeFromPage(const char* page_buf, size_t page_size);
    size_t FreeCount() const;

private:
    std::vector<PageID> free_pages_;
    mutable std::mutex mu_;
};

}

#endif

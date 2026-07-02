#include "storage/page_manager.h"
#include <cstring>

namespace vv::storage {

PageManager::PageManager(BufferPool* pool, FreeList* freelist)
    : pool_(pool), freelist_(freelist) {}

int PageManager::AllocPage(char* data_out, PageID* id_out) {
    PageID recycled_id;
    if (freelist_->Allocate(recycled_id) == VV_OK) {
        if (id_out) *id_out = recycled_id;
        if (data_out) {
            Frame* f = nullptr;
            int rc = pool_->GetPage(recycled_id, f);
            if (rc != VV_OK) return rc;
            std::memcpy(f->data, data_out, PAGE_SIZE);
            return pool_->MarkDirty(recycled_id);
        }
        return VV_OK;
    }
    return pool_->NewPage(data_out, id_out);
}

int PageManager::GetPage(PageID id, Frame*& frame_out) {
    return pool_->GetPage(id, frame_out);
}

int PageManager::MarkDirty(PageID id) {
    return pool_->MarkDirty(id);
}

int PageManager::FreePage(PageID id) {
    return freelist_->Free(id);
}

int PageManager::Flush() {
    return pool_->FlushAllDirty();
}

}

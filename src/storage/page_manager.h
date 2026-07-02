#ifndef VV_STORAGE_PAGE_MANAGER_H
#define VV_STORAGE_PAGE_MANAGER_H

#include <vectorvault/vectorvault.h>
#include "core/types.h"
#include "storage/buffer_pool.h"
#include "storage/free_list.h"
#include "storage/file_io.h"

namespace vv::storage {

class PageManager {
public:
    PageManager(BufferPool* pool, FreeList* freelist);
    int AllocPage(char* data_out, PageID* id_out);
    int GetPage(PageID id, Frame*& frame_out);
    int MarkDirty(PageID id);
    int FreePage(PageID id);
    int Flush();

private:
    BufferPool* pool_;
    FreeList*   freelist_;
};

}

#endif

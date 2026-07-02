#ifndef VV_STORAGE_BUFFER_POOL_H
#define VV_STORAGE_BUFFER_POOL_H

#include <vectorvault/vectorvault.h>
#include "core/types.h"
#include "storage/file_io.h"
#include <unordered_map>
#include <list>
#include <mutex>

namespace vv::storage {

class BufferPool {
public:
    BufferPool(size_t capacity, int fd);
    ~BufferPool();

    int GetPage(PageID id, Frame*& frame_out);
    int NewPage(char* data_out, PageID* id_out);
    int MarkDirty(PageID id);
    int FlushAllDirty();
    size_t FrameUsed() const { return page_table_.size(); }

private:
    int file_fd_;
    size_t capacity_;
    std::unordered_map<PageID, Frame*> page_table_;
    std::list<PageID> lru_;
    std::unordered_map<PageID, std::list<PageID>::iterator> lru_pos_;
    Frame* frames_ = nullptr;
    PageID next_page_id_ = 0;
    std::mutex mu_;

    int Evict();
    Frame* AllocFrame();
    void TouchLRU(PageID id);
};

}

#endif

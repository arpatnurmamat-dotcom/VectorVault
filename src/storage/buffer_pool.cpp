#include "storage/buffer_pool.h"
#include <algorithm>
#include <cstring>

namespace vv::storage {

BufferPool::BufferPool(size_t capacity, int fd) : file_fd_(fd), capacity_(capacity) {
    frames_ = new Frame[capacity];
    size_t file_size = 0;
    FileIO::GetFileSize(file_fd_, file_size);
    PageID calculated_id = static_cast<PageID>((file_size + PAGE_SIZE - 1) / PAGE_SIZE);
    next_page_id_ = std::max(calculated_id, FIRST_DATA_PAGE_ID);
}

BufferPool::~BufferPool() {
    FlushAllDirty();
    delete[] frames_;
}

int BufferPool::GetPage(PageID id, Frame*& frame_out) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = page_table_.find(id);
    if (it != page_table_.end()) {
        TouchLRU(id);
        frame_out = it->second;
        return VV_OK;
    }
    if (page_table_.size() >= capacity_) {
        int rc = Evict();
        if (rc != VV_OK) return rc;
    }
    Frame* f = AllocFrame();
    if (!f) return VV_ERR_NOMEM;
    f->page_id = id;
    size_t bytes_read = 0;
    int rc = FileIO::Read(file_fd_, f->data, PAGE_SIZE, id * PAGE_SIZE, bytes_read);
    if (rc != VV_OK) return rc;
    if (bytes_read == 0) std::memset(f->data, 0, PAGE_SIZE);
    f->is_dirty = false;
    f->pin_count = 0;
    page_table_[id] = f;
    lru_.push_back(id);
    lru_pos_[id] = std::prev(lru_.end());
    frame_out = f;
    return VV_OK;
}

int BufferPool::NewPage(char* data_out, PageID* id_out) {
    std::lock_guard<std::mutex> lk(mu_);
    if (page_table_.size() >= capacity_) {
        int rc = Evict();
        if (rc != VV_OK) return rc;
    }
    Frame* f = AllocFrame();
    if (!f) return VV_ERR_NOMEM;
    PageID new_id = next_page_id_++;
    f->page_id = new_id;
    if (data_out) std::memcpy(f->data, data_out, PAGE_SIZE);
    else std::memset(f->data, 0, PAGE_SIZE);
    f->is_dirty = true;
    f->pin_count = 0;
    page_table_[new_id] = f;
    lru_.push_back(new_id);
    lru_pos_[new_id] = std::prev(lru_.end());
    if (id_out) *id_out = new_id;
    return VV_OK;
}

int BufferPool::MarkDirty(PageID id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = page_table_.find(id);
    if (it == page_table_.end()) return VV_ERR_NOT_FOUND;
    it->second->is_dirty = true;
    return VV_OK;
}

int BufferPool::FlushAllDirty() {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& [id, f] : page_table_) {
        if (f && f->is_dirty) {
            int rc = FileIO::Write(file_fd_, f->data, PAGE_SIZE, id * PAGE_SIZE);
            if (rc != VV_OK) return rc;
            f->is_dirty = false;
        }
    }
    return FileIO::Sync(file_fd_);
}

int BufferPool::Evict() {
    for (auto it = lru_.begin(); it != lru_.end(); ++it) {
        PageID victim = *it;
        Frame* f = page_table_[victim];
        if (f->pin_count == 0) {
            if (f->is_dirty) {
                int rc = FileIO::Write(file_fd_, f->data, PAGE_SIZE, victim * PAGE_SIZE);
                if (rc != VV_OK) return rc;
            }
            page_table_.erase(victim);
            lru_pos_.erase(victim);
            lru_.erase(it);
            f->page_id = static_cast<PageID>(-1);
            f->is_dirty = false;
            return VV_OK;
        }
    }
    return VV_ERR_NOMEM;
}

Frame* BufferPool::AllocFrame() {
    for (size_t i = 0; i < capacity_; ++i) {
        if (frames_[i].page_id == static_cast<PageID>(-1)) return &frames_[i];
    }
    return nullptr;
}

void BufferPool::TouchLRU(PageID id) {
    auto it = lru_pos_.find(id);
    if (it != lru_pos_.end()) {
        lru_.erase(it->second);
        lru_.push_back(id);
        lru_pos_[id] = std::prev(lru_.end());
    }
}

}

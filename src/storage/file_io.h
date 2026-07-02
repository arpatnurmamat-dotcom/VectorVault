#ifndef VV_STORAGE_FILE_IO_H
#define VV_STORAGE_FILE_IO_H

#include <vectorvault/vectorvault.h>
#include "core/types.h"
#include <string>
#include <cstddef>
#include <cstdint>

namespace vv::storage {

class FileIO {
public:
    static int Open(const std::string& path, bool create, int& fd_out);
    static int Close(int fd);
    static int Read(int fd, char* buf, size_t nbytes, size_t offset, size_t& bytes_read);
    static int Write(int fd, const char* buf, size_t nbytes, size_t offset);
    static int Sync(int fd);
    static int GetFileSize(int fd, size_t& size_out);
    static int Resize(int fd, size_t new_size);
};

}

#endif

#include "storage/file_io.h"
#include "core/platform.h"

#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

namespace vv::storage {

int FileIO::Open(const std::string& path, bool create, int& fd_out) {
    int flags = O_RDWR;
    if (create) flags |= O_CREAT;
    int fd = ::open(path.c_str(), flags, 0644);
    if (fd < 0) return VV_ERR_IO;
    fd_out = fd;
    return VV_OK;
}

int FileIO::Close(int fd) {
    if (::close(fd) != 0) return VV_ERR_IO;
    return VV_OK;
}

int FileIO::Read(int fd, char* buf, size_t nbytes, size_t offset, size_t& bytes_read) {
    ssize_t rc = ::pread(fd, buf, nbytes, static_cast<off_t>(offset));
    if (rc < 0) return VV_ERR_IO;
    bytes_read = static_cast<size_t>(rc);
    return VV_OK;
}

int FileIO::Write(int fd, const char* buf, size_t nbytes, size_t offset) {
    size_t written = 0;
    while (written < nbytes) {
        ssize_t rc = ::pwrite(fd, buf + written, nbytes - written,
                              static_cast<off_t>(offset + written));
        if (rc <= 0) return VV_ERR_IO;
        written += static_cast<size_t>(rc);
    }
    return VV_OK;
}

int FileIO::Sync(int fd) {
    if (::fsync(fd) != 0) return VV_ERR_IO;
    return VV_OK;
}

int FileIO::GetFileSize(int fd, size_t& size_out) {
    struct stat st{};
    if (::fstat(fd, &st) != 0) return VV_ERR_IO;
    size_out = static_cast<size_t>(st.st_size);
    return VV_OK;
}

int FileIO::Resize(int fd, size_t new_size) {
    if (::ftruncate(fd, static_cast<off_t>(new_size)) != 0) return VV_ERR_IO;
    return VV_OK;
}

}

#include "instance_lock.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <system_error>
#include <unistd.h>

#include "lucent/log.h"

namespace opensu::fileio {

struct InstanceLock::File {
    int fd{-1};
};

InstanceLock::InstanceLock(const std::filesystem::path& file) : file_{std::make_unique<File>()} {
    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    file_->fd = ::open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (file_->fd < 0) {
        lucent::warn("host", "cannot open {}: {}", file.string(), std::strerror(errno));
        return;
    }
    if (::flock(file_->fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(file_->fd);
        file_->fd = -1;
        return;
    }
    held_ = true;
}

InstanceLock::~InstanceLock() {
    if (file_->fd >= 0) {
        ::close(file_->fd);
    }
}

} // namespace opensu::fileio

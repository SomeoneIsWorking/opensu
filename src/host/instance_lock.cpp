#include "instance_lock.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <system_error>
#include <unistd.h>

#include "lucent/log.h"

namespace opensu::host {

InstanceLock::InstanceLock(const std::filesystem::path& file) {
    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    fd_ = ::open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd_ < 0) {
        lucent::warn("host", "cannot open {}: {}", file.string(), std::strerror(errno));
        return;
    }
    if (::flock(fd_, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd_);
        fd_ = -1;
        return;
    }
    held_ = true;
}

InstanceLock::~InstanceLock() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

} // namespace opensu::host

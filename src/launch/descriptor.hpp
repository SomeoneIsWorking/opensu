// descriptor — a file descriptor closed when it goes out of scope.
#pragma once

#include <unistd.h>

namespace opensu::launch {

class Descriptor {
  public:
    explicit Descriptor(int fd) noexcept : fd_{fd} {
    }
    ~Descriptor() {
        close();
    }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    Descriptor(Descriptor&&) = delete;
    Descriptor& operator=(Descriptor&&) = delete;

    [[nodiscard]] int get() const noexcept {
        return fd_;
    }
    void close() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

  private:
    int fd_;
};

} // namespace opensu::launch

// instance_lock — one openSU at a time, so a second launch does not start a second shell and a
// second Steam client.
#pragma once

#include <filesystem>

namespace opensu::host {

class InstanceLock {
  public:
    explicit InstanceLock(std::filesystem::path file);
    ~InstanceLock();
    InstanceLock(const InstanceLock&) = delete;
    InstanceLock& operator=(const InstanceLock&) = delete;
    InstanceLock(InstanceLock&&) = delete;
    InstanceLock& operator=(InstanceLock&&) = delete;

    /// Whether this process took the lock; false when another holds it.
    [[nodiscard]] bool held() const noexcept {
        return held_;
    }

  private:
    int fd_{-1};
    bool held_{false};
};

} // namespace opensu::host

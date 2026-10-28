// instance_lock — one openSU at a time, so a second launch does not start a second shell and a
// second Steam client. One backend per OS, picked in CMakeLists.txt: `flock` on POSIX
// (`instance_lock_posix.cpp`), an unshared open on Windows (`instance_lock_win32.cpp`).
#pragma once

#include <filesystem>
#include <memory>

namespace opensu::fileio {

class InstanceLock {
  public:
    explicit InstanceLock(const std::filesystem::path& file);
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
    /// What the OS's backend keeps open while the lock is held.
    struct File;

    std::unique_ptr<File> file_;
    bool held_{false};
};

} // namespace opensu::fileio

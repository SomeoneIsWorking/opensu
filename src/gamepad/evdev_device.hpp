// evdev_device — one /dev/input/event node opened for reading, which iideck can grab so nothing
// else receives its events.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "pad_translator.hpp"

namespace iideck::gamepad {

class EvdevDevice {
  public:
    /// Opens the node non-blocking; throws std::system_error when it cannot.
    explicit EvdevDevice(const std::filesystem::path& node);
    EvdevDevice(EvdevDevice&& other) noexcept;
    EvdevDevice& operator=(EvdevDevice&& other) noexcept;
    EvdevDevice(const EvdevDevice&) = delete;
    EvdevDevice& operator=(const EvdevDevice&) = delete;
    /// Closing the node also releases a grab.
    ~EvdevDevice();

    /// Every gamepad node this user can open, except iideck's own virtual pads.
    [[nodiscard]] static std::vector<EvdevDevice> openGamepads(const std::filesystem::path& dir);

    [[nodiscard]] int fd() const {
        return fd_;
    }
    [[nodiscard]] const std::filesystem::path& node() const {
        return node_;
    }
    [[nodiscard]] const std::string& name() const {
        return name_;
    }
    [[nodiscard]] const std::string& phys() const {
        return phys_;
    }
    [[nodiscard]] const PadCapabilities& capabilities() const {
        return capabilities_;
    }

    /// Makes iideck the device's only reader. False when another process holds it.
    [[nodiscard]] bool grab();

    /// The device's present keys and axes, as events.
    [[nodiscard]] std::vector<PadEvent> state() const;

    /// Appends every event waiting. False once the device is gone.
    bool read(std::vector<PadEvent>& out);

  private:
    int fd_{-1};
    std::filesystem::path node_;
    std::string name_;
    std::string phys_;
    PadCapabilities capabilities_;
};

} // namespace iideck::gamepad

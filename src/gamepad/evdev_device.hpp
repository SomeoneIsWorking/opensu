// evdev_device — one /dev/input/event node opened for reading, which iideck can grab so nothing
// else receives its events.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
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

    /// The node opened, if it is a pad this user can read.
    [[nodiscard]] static std::optional<EvdevDevice> openPad(const std::filesystem::path& node);

    /// A controller iideck reads: a gamepad, and not a virtual pad iideck or Steam made from one.
    [[nodiscard]] bool isPad() const;

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
    /// Lets every reader see the device again.
    void ungrab();

    /// The device's present keys and axes, as events.
    [[nodiscard]] std::vector<PadEvent> state() const;

    /// Appends every event waiting. False once the device is gone.
    bool read(std::vector<PadEvent>& out);

  private:
    int fd_{-1};
    std::filesystem::path node_;
    std::string name_;
    std::string phys_;
    std::uint16_t vendor_{0};
    std::uint16_t product_{0};
    PadCapabilities capabilities_;
};

} // namespace iideck::gamepad

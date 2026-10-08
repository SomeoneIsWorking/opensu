#include "evdev_device.hpp"

#include <array>
#include <cerrno>
#include <climits>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "lucent/log.h"
#include "virtual_pad.hpp"

namespace iideck::gamepad {
namespace {

constexpr std::size_t bitsPerLong = sizeof(unsigned long) * CHAR_BIT;

template <std::size_t Bits>
using BitSet = std::array<unsigned long, (Bits + bitsPerLong - 1) / bitsPerLong>;

template <std::size_t Bits> bool test(const BitSet<Bits>& bits, std::size_t bit) {
    return ((bits[bit / bitsPerLong] >> (bit % bitsPerLong)) & 1UL) != 0;
}

std::string readString(int fd, unsigned long request) {
    std::array<char, 256> buffer{};
    if (ioctl(fd, request, buffer.data()) < 0) {
        return {};
    }
    return std::string{buffer.data()};
}

} // namespace

EvdevDevice::EvdevDevice(const std::filesystem::path& node) : node_{node} {
    fd_ = ::open(node.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) {
        throw std::system_error{errno, std::generic_category(), "open " + node.string()};
    }
    name_ = readString(fd_, EVIOCGNAME(256));
    phys_ = readString(fd_, EVIOCGPHYS(256));
    input_id id{};
    if (ioctl(fd_, EVIOCGID, &id) == 0) {
        vendor_ = id.vendor;
        product_ = id.product;
    }

    BitSet<KEY_CNT> keys{};
    BitSet<ABS_CNT> axes{};
    ioctl(fd_, EVIOCGBIT(EV_KEY, sizeof(keys)), keys.data());
    ioctl(fd_, EVIOCGBIT(EV_ABS, sizeof(axes)), axes.data());
    for (std::size_t code = 0; code < KEY_CNT; ++code) {
        if (test<KEY_CNT>(keys, code)) {
            capabilities_.keys.insert(static_cast<std::uint16_t>(code));
        }
    }
    for (std::size_t code = 0; code < ABS_CNT; ++code) {
        input_absinfo info{};
        if (test<ABS_CNT>(axes, code) && ioctl(fd_, EVIOCGABS(code), &info) == 0) {
            capabilities_.axes[static_cast<std::uint16_t>(code)] = {info.minimum, info.maximum};
        }
    }
}

EvdevDevice::EvdevDevice(EvdevDevice&& other) noexcept
    : fd_{std::exchange(other.fd_, -1)}, node_{std::move(other.node_)},
      name_{std::move(other.name_)}, phys_{std::move(other.phys_)}, vendor_{other.vendor_},
      product_{other.product_}, capabilities_{std::move(other.capabilities_)} {
}

EvdevDevice& EvdevDevice::operator=(EvdevDevice&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = std::exchange(other.fd_, -1);
        node_ = std::move(other.node_);
        name_ = std::move(other.name_);
        phys_ = std::move(other.phys_);
        vendor_ = other.vendor_;
        product_ = other.product_;
        capabilities_ = std::move(other.capabilities_);
    }
    return *this;
}

EvdevDevice::~EvdevDevice() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

bool EvdevDevice::isPad() const {
    // Steam Input's virtual gamepad, which Steam feeds from a pad iideck already reads.
    constexpr std::uint16_t valve = 0x28de;
    constexpr std::uint16_t steamVirtualGamepad = 0x11ff;
    return capabilities_.isGamepad() && phys_ != VirtualPad::phys &&
           !(vendor_ == valve && product_ == steamVirtualGamepad);
}

std::optional<EvdevDevice> EvdevDevice::openPad(const std::filesystem::path& node) {
    try {
        EvdevDevice device{node};
        if (device.isPad()) {
            return device;
        }
    } catch (const std::system_error& failure) {
        // Keyboards and the like are not ours to read; anything else is worth knowing about.
        if (failure.code() != std::errc::permission_denied) {
            lucent::warn("gamepad", "{}", failure.what());
        }
    }
    return std::nullopt;
}

void EvdevDevice::ungrab() {
    ioctl(fd_, EVIOCGRAB, 0);
}

bool EvdevDevice::grab() {
    if (ioctl(fd_, EVIOCGRAB, 1) < 0) {
        lucent::warn("gamepad", "cannot grab {} ({}): {}", name_, node_.string(),
                     std::generic_category().message(errno));
        return false;
    }
    return true;
}

std::vector<PadEvent> EvdevDevice::state() const {
    std::vector<PadEvent> events;
    BitSet<KEY_CNT> held{};
    if (ioctl(fd_, EVIOCGKEY(sizeof(held)), held.data()) >= 0) {
        for (const auto key : capabilities_.keys) {
            events.push_back(PadEvent{EV_KEY, key, test<KEY_CNT>(held, key) ? 1 : 0});
        }
    }
    for (const auto& [axis, range] : capabilities_.axes) {
        input_absinfo info{};
        if (ioctl(fd_, EVIOCGABS(axis), &info) == 0) {
            events.push_back(PadEvent{EV_ABS, axis, info.value});
        }
    }
    events.push_back(PadEvent{EV_SYN, SYN_REPORT, 0});
    return events;
}

bool EvdevDevice::read(std::vector<PadEvent>& out) {
    std::array<input_event, 64> buffer{};
    for (;;) {
        const ssize_t got = ::read(fd_, buffer.data(), sizeof(buffer));
        if (got < 0) {
            return errno == EAGAIN || errno == EINTR;
        }
        if (got == 0) {
            return false;
        }
        const auto count = static_cast<std::size_t>(got) / sizeof(input_event);
        for (std::size_t i = 0; i < count; ++i) {
            out.push_back(PadEvent{buffer[i].type, buffer[i].code, buffer[i].value});
        }
    }
}

} // namespace iideck::gamepad

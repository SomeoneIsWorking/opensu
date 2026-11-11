#include "wii_slot.hpp"

#include <algorithm>
#include <cerrno>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/input-event-codes.h>

namespace opensu::gamepad {
namespace {

// Output reports.
constexpr std::uint8_t setLights = 0x11;
constexpr std::uint8_t setReportMode = 0x12;
constexpr std::uint8_t requestStatus = 0x15;
constexpr std::uint8_t writeMemory = 0x16;
constexpr std::uint8_t readMemory = 0x17;
// Input reports.
constexpr std::uint8_t status = 0x20;
constexpr std::uint8_t memoryRead = 0x21;
constexpr std::uint8_t acknowledge = 0x22;
constexpr std::uint8_t buttonsOnly = 0x30;
constexpr std::uint8_t buttonsAndExtension = 0x32;

// The extension's registers, through the remote's register space (flag 0x04).
constexpr std::uint8_t registers = 0x04;
constexpr std::uint32_t unlock = 0xa400f0;
constexpr std::uint32_t unencrypt = 0xa400fb;
constexpr std::uint32_t extensionId = 0xa400fa;
constexpr std::uint8_t extensionIdSize = 6;
// The status's flag for an extension plugged in.
constexpr std::uint8_t extensionPlugged = 0x02;
// Player 1's light; each next slot's is the next bit.
constexpr std::uint8_t firstLight = 0x10;
constexpr std::size_t extensionBytes = 6;

/// A remote's core button bits, byte by byte, as hid-wiimote names them.
struct CoreKey {
    std::size_t byte;
    std::uint8_t bit;
    std::uint16_t code;
};

constexpr std::array coreKeys{
    CoreKey{0, 0x01, KEY_LEFT},     CoreKey{0, 0x02, KEY_RIGHT}, CoreKey{0, 0x04, KEY_DOWN},
    CoreKey{0, 0x08, KEY_UP},       CoreKey{0, 0x10, KEY_NEXT},  CoreKey{1, 0x01, BTN_2},
    CoreKey{1, 0x02, BTN_1},        CoreKey{1, 0x04, BTN_B},     CoreKey{1, 0x08, BTN_A},
    CoreKey{1, 0x10, KEY_PREVIOUS}, CoreKey{1, 0x80, BTN_MODE},
};

// A Classic Controller's buttons in its last two bytes, 0 while pressed (hid-wiimote's
// `wiimod_classic_in_ext`, without Motion Plus).
constexpr std::array classicKeys{
    CoreKey{4, 0x80, KEY_RIGHT},    CoreKey{4, 0x40, KEY_DOWN}, CoreKey{4, 0x20, BTN_TL},
    CoreKey{4, 0x10, KEY_PREVIOUS}, CoreKey{4, 0x08, BTN_MODE}, CoreKey{4, 0x04, KEY_NEXT},
    CoreKey{4, 0x02, BTN_TR},       CoreKey{5, 0x80, BTN_TL2},  CoreKey{5, 0x40, BTN_B},
    CoreKey{5, 0x20, BTN_Y},        CoreKey{5, 0x10, BTN_A},    CoreKey{5, 0x08, BTN_X},
    CoreKey{5, 0x04, BTN_TR2},      CoreKey{5, 0x02, KEY_LEFT}, CoreKey{5, 0x01, KEY_UP},
};

constexpr std::array nunchukKeys{
    CoreKey{5, 0x01, BTN_Z},
    CoreKey{5, 0x02, BTN_C},
};

// hid-wiimote's ranges.
constexpr AxisRange nunchukStick{-120, 120};
constexpr AxisRange classicAxis{-30, 30};
constexpr std::uint8_t stickMiddle = 0x80;
constexpr std::int32_t classicMiddle = 0x20;

/// The size of each input report read, by its id; 0 for one not read.
std::size_t sizeOf(std::uint8_t id) {
    switch (id) {
    case status:
        return 7;
    case memoryRead:
        return 22;
    case acknowledge:
        return 5;
    case buttonsOnly:
        return 3;
    case buttonsAndExtension:
        return 11;
    default:
        return 0;
    }
}

int openSlot(const std::filesystem::path& node) {
    const int fd = ::open(node.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC | O_NOCTTY);
    if (fd < 0) {
        throw std::system_error{errno, std::generic_category(), "open " + node.string()};
    }
    struct stat info{};
    if (::fstat(fd, &info) != 0 || !S_ISCHR(info.st_mode)) {
        ::close(fd);
        throw std::system_error{ENODEV, std::generic_category(),
                                node.string() + " is no hidraw node"};
    }
    return fd;
}

} // namespace

WiiSlot::WiiSlot(const std::filesystem::path& node, int index)
    : WiiSlot{openSlot(node), node, index} {
}

WiiSlot::WiiSlot(int fd, std::filesystem::path node, int index)
    : fd_{fd}, node_{std::move(node)}, index_{index} {
}

WiiSlot::WiiSlot(WiiSlot&& other) noexcept
    : fd_{std::exchange(other.fd_, -1)}, node_{std::move(other.node_)},
      name_{std::move(other.name_)}, index_{other.index_}, remote_{other.remote_},
      plugged_{other.plugged_}, setup_{other.setup_}, extension_{other.extension_},
      now_{std::move(other.now_)} {
}

WiiSlot& WiiSlot::operator=(WiiSlot&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = std::exchange(other.fd_, -1);
        node_ = std::move(other.node_);
        name_ = std::move(other.name_);
        index_ = other.index_;
        remote_ = other.remote_;
        plugged_ = other.plugged_;
        setup_ = other.setup_;
        extension_ = other.extension_;
        now_ = std::move(other.now_);
    }
    return *this;
}

WiiSlot::~WiiSlot() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

PadCapabilities WiiSlot::capabilitiesOf(WiiRemotePart extension) {
    PadCapabilities caps;
    if (extension == WiiRemotePart::Nunchuk) {
        for (const CoreKey& key : nunchukKeys) {
            caps.keys.insert(key.code);
        }
        caps.axes[ABS_HAT0X] = nunchukStick;
        caps.axes[ABS_HAT0Y] = nunchukStick;
    } else if (extension == WiiRemotePart::Classic) {
        for (const CoreKey& key : classicKeys) {
            caps.keys.insert(key.code);
        }
        for (std::uint16_t code = ABS_HAT1X; code <= ABS_HAT3Y; ++code) {
            caps.axes[code] = classicAxis;
        }
    }
    return caps;
}

bool WiiSlot::ask() {
    const std::array<std::uint8_t, 2> request{requestStatus, 0x00};
    if (send(request)) {
        return true;
    }
    remote_ = false;
    plugged_ = false;
    setup_ = Setup::None;
    extension_.reset();
    now_.clear();
    return false;
}

bool WiiSlot::read(std::vector<WiiPartEvent>& out) {
    std::array<std::uint8_t, 64> buffer{};
    while (true) {
        const ssize_t got = ::read(fd_, buffer.data(), buffer.size());
        if (got < 0) {
            return errno == EAGAIN || errno == EINTR;
        }
        if (got == 0) {
            return false;
        }
        takeReport(std::span{buffer.data(), static_cast<std::size_t>(got)}, out);
    }
}

void WiiSlot::takeReport(std::span<const std::uint8_t> report, std::vector<WiiPartEvent>& out) {
    const std::size_t size = sizeOf(report.front());
    if (size == 0 || report.size() < size) {
        return;
    }
    // Every report read carries the core buttons.
    takeButtons(report[1], report[2], out);
    switch (report.front()) {
    case status:
        takeStatus(report);
        break;
    case acknowledge:
        takeAck(report);
        break;
    case memoryRead:
        takeName(report);
        break;
    case buttonsAndExtension:
        if (extension_) {
            takeExtension(report.subspan(3, extensionBytes), out);
        }
        break;
    default:
        break;
    }
}

void WiiSlot::takeStatus(std::span<const std::uint8_t> report) {
    if (!remote_) {
        remote_ = true;
        const std::array<std::uint8_t, 2> lights{
            setLights, static_cast<std::uint8_t>(firstLight << std::clamp(index_, 0, 3))};
        static_cast<void>(send(lights));
    }
    plugged_ = (report[3] & extensionPlugged) != 0;
    if (!plugged_) {
        setup_ = Setup::None;
        if (extension_) {
            const WiiRemotePart gone = *extension_;
            extension_.reset();
            std::erase_if(now_, [gone](const WiiPartEvent& held) {
                return held.part == gone;
            });
        }
    } else if (!extension_) {
        // Started again at each status until it is named, so a lost answer stalls nothing.
        setup_ = Setup::Unlocking;
        writeRegister(unlock, 0x55);
        return;
    }
    // A status ends the report mode; it is asked again.
    if (setup_ == Setup::None) {
        requestReports();
    }
}

void WiiSlot::takeAck(std::span<const std::uint8_t> report) {
    if (report[3] != writeMemory) {
        return;
    }
    if (report[4] != 0) {
        // The extension refused its setup: it is read as none until it is plugged in again.
        setup_ = Setup::None;
        requestReports();
        return;
    }
    if (setup_ == Setup::Unlocking) {
        setup_ = Setup::Unencrypting;
        writeRegister(unencrypt, 0x00);
    } else if (setup_ == Setup::Unencrypting) {
        setup_ = Setup::Naming;
        const std::array<std::uint8_t, 7> request{readMemory,
                                                  registers,
                                                  static_cast<std::uint8_t>(extensionId >> 16),
                                                  static_cast<std::uint8_t>(extensionId >> 8),
                                                  static_cast<std::uint8_t>(extensionId),
                                                  0x00,
                                                  extensionIdSize};
        static_cast<void>(send(request));
    }
}

void WiiSlot::takeName(std::span<const std::uint8_t> report) {
    if (setup_ != Setup::Naming) {
        return;
    }
    setup_ = Setup::None;
    const bool failed = (report[3] & 0x0f) != 0;
    // The id's last four bytes: a4 20 00 00 for a Nunchuk, a4 20 01 01 for a Classic Controller
    // (the Pro's too).
    const std::span<const std::uint8_t> id = report.subspan(6 + 2, 4);
    if (!failed && id[0] == 0xa4 && id[1] == 0x20 && id[2] == 0x00 && id[3] == 0x00) {
        extension_ = WiiRemotePart::Nunchuk;
    } else if (!failed && id[0] == 0xa4 && id[1] == 0x20 && id[2] == 0x01 && id[3] == 0x01) {
        extension_ = WiiRemotePart::Classic;
    }
    requestReports();
}

void WiiSlot::takeButtons(std::uint8_t first, std::uint8_t second, std::vector<WiiPartEvent>& out) {
    const std::size_t before = out.size();
    const std::array<std::uint8_t, 2> bytes{first, second};
    for (const CoreKey& core : coreKeys) {
        key(WiiRemotePart::Remote, core.code, (bytes.at(core.byte) & core.bit) != 0, out);
    }
    if (out.size() != before) {
        out.push_back({WiiRemotePart::Remote, PadEvent{.type = EV_SYN, .code = SYN_REPORT}});
    }
}

void WiiSlot::takeExtension(std::span<const std::uint8_t> bytes, std::vector<WiiPartEvent>& out) {
    const WiiRemotePart part = extension_.value();
    const std::size_t before = out.size();
    if (part == WiiRemotePart::Nunchuk) {
        axis(part, ABS_HAT0X, static_cast<std::int32_t>(bytes[0]) - stickMiddle, out);
        axis(part, ABS_HAT0Y, static_cast<std::int32_t>(bytes[1]) - stickMiddle, out);
        for (const CoreKey& held : nunchukKeys) {
            key(part, held.code, (bytes[held.byte] & held.bit) == 0, out);
        }
    } else {
        const std::int32_t rightX =
            (((bytes[0] >> 3) & 0x18) | ((bytes[1] >> 5) & 0x06) | ((bytes[2] >> 7) & 0x01)) << 1;
        const std::int32_t rightY = (bytes[2] & 0x1f) << 1;
        const std::int32_t rightTrigger = (bytes[3] & 0x1f) << 1;
        const std::int32_t leftTrigger = (((bytes[2] >> 2) & 0x18) | ((bytes[3] >> 5) & 0x07)) << 1;
        axis(part, ABS_HAT1X, (bytes[0] & 0x3f) - classicMiddle, out);
        axis(part, ABS_HAT1Y, (bytes[1] & 0x3f) - classicMiddle, out);
        axis(part, ABS_HAT2X, rightX - classicMiddle, out);
        axis(part, ABS_HAT2Y, rightY - classicMiddle, out);
        axis(part, ABS_HAT3X, rightTrigger, out);
        axis(part, ABS_HAT3Y, leftTrigger, out);
        for (const CoreKey& held : classicKeys) {
            key(part, held.code, (bytes[held.byte] & held.bit) == 0, out);
        }
    }
    if (out.size() != before) {
        out.push_back({part, PadEvent{.type = EV_SYN, .code = SYN_REPORT}});
    }
}

void WiiSlot::writeRegister(std::uint32_t address, std::uint8_t value) {
    std::array<std::uint8_t, 22> request{};
    request[0] = writeMemory;
    request[1] = registers;
    request[2] = static_cast<std::uint8_t>(address >> 16);
    request[3] = static_cast<std::uint8_t>(address >> 8);
    request[4] = static_cast<std::uint8_t>(address);
    request[5] = 1;
    request[6] = value;
    static_cast<void>(send(request));
}

void WiiSlot::requestReports() {
    const std::array<std::uint8_t, 3> mode{setReportMode, 0x00,
                                           extension_ ? buttonsAndExtension : buttonsOnly};
    static_cast<void>(send(mode));
}

bool WiiSlot::send(std::span<const std::uint8_t> report) {
    return ::write(fd_, report.data(), report.size()) == static_cast<ssize_t>(report.size());
}

void WiiSlot::key(WiiRemotePart part, std::uint16_t code, bool down,
                  std::vector<WiiPartEvent>& out) {
    report(part, PadEvent{.type = EV_KEY, .code = code, .value = down ? 1 : 0}, out);
}

void WiiSlot::axis(WiiRemotePart part, std::uint16_t code, std::int32_t value,
                   std::vector<WiiPartEvent>& out) {
    report(part, PadEvent{.type = EV_ABS, .code = code, .value = value}, out);
}

void WiiSlot::report(WiiRemotePart part, const PadEvent& event, std::vector<WiiPartEvent>& out) {
    const auto known = std::ranges::find_if(now_, [&](const WiiPartEvent& held) {
        return held.part == part && held.event.type == event.type && held.event.code == event.code;
    });
    if (known == now_.end()) {
        if (event.type == EV_KEY && event.value == 0) {
            // A key never pressed is up already.
            return;
        }
        now_.push_back({part, event});
    } else if (known->event.value == event.value) {
        return;
    } else {
        known->event.value = event.value;
    }
    out.push_back({part, event});
}

std::vector<WiiPartEvent> WiiSlot::state() const {
    std::vector<WiiPartEvent> events;
    for (const WiiPartEvent& known : now_) {
        if (known.event.type == EV_ABS || known.event.value != 0) {
            events.push_back(known);
        }
    }
    for (const WiiRemotePart part :
         {WiiRemotePart::Remote, WiiRemotePart::Nunchuk, WiiRemotePart::Classic}) {
        events.push_back({part, PadEvent{.type = EV_SYN, .code = SYN_REPORT}});
    }
    return events;
}

} // namespace opensu::gamepad

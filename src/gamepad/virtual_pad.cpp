#include "virtual_pad.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>

#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "lucent/log.h"

namespace iideck::gamepad {
namespace {

void check(int result, const char* what) {
    if (result < 0) {
        throw std::system_error{errno, std::generic_category(), what};
    }
}

/// xpad's Xbox 360 keys.
constexpr int keys[] = {BTN_SOUTH,  BTN_EAST,  BTN_NORTH, BTN_WEST,   BTN_TL,    BTN_TR,
                        BTN_SELECT, BTN_START, BTN_MODE,  BTN_THUMBL, BTN_THUMBR};

struct Axis {
    int code;
    AxisRange range;
    int fuzz;
    int flat;
};

/// xpad's Xbox 360 axes.
constexpr Axis axes[] = {
    {ABS_X, virtualStick, 16, 128},  {ABS_Y, virtualStick, 16, 128},
    {ABS_RX, virtualStick, 16, 128}, {ABS_RY, virtualStick, 16, 128},
    {ABS_Z, virtualTrigger, 0, 0},   {ABS_RZ, virtualTrigger, 0, 0},
    {ABS_HAT0X, {-1, 1}, 0, 0},      {ABS_HAT0Y, {-1, 1}, 0, 0},
};

} // namespace

VirtualPad::VirtualPad() {
    fd_ = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    check(fd_, "open /dev/uinput");
    try {
        check(ioctl(fd_, UI_SET_EVBIT, EV_KEY), "UI_SET_EVBIT EV_KEY");
        check(ioctl(fd_, UI_SET_EVBIT, EV_ABS), "UI_SET_EVBIT EV_ABS");
        for (const int key : keys) {
            check(ioctl(fd_, UI_SET_KEYBIT, key), "UI_SET_KEYBIT");
        }
        for (const Axis& axis : axes) {
            check(ioctl(fd_, UI_SET_ABSBIT, axis.code), "UI_SET_ABSBIT");
            uinput_abs_setup setup{};
            setup.code = static_cast<std::uint16_t>(axis.code);
            setup.absinfo.minimum = axis.range.min;
            setup.absinfo.maximum = axis.range.max;
            setup.absinfo.fuzz = axis.fuzz;
            setup.absinfo.flat = axis.flat;
            check(ioctl(fd_, UI_ABS_SETUP, &setup), "UI_ABS_SETUP");
        }
        check(ioctl(fd_, UI_SET_PHYS, std::string{phys}.c_str()), "UI_SET_PHYS");
        uinput_setup setup{};
        setup.id.bustype = BUS_USB;
        setup.id.vendor = vendor;
        setup.id.product = product;
        setup.id.version = 0x0110;
        std::copy_n(name.begin(), std::min(name.size(), sizeof(setup.name) - 1), setup.name);
        check(ioctl(fd_, UI_DEV_SETUP, &setup), "UI_DEV_SETUP");
        check(ioctl(fd_, UI_DEV_CREATE), "UI_DEV_CREATE");
    } catch (...) {
        ::close(fd_);
        throw;
    }
    awaitAccess();
}

void VirtualPad::awaitAccess() const {
    std::array<char, 64> sysname{};
    if (ioctl(fd_, UI_GET_SYSNAME(sysname.size()), sysname.data()) < 0) {
        return;
    }
    const std::filesystem::path device =
        std::filesystem::path{"/sys/devices/virtual/input"} / sysname.data();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (std::chrono::steady_clock::now() < deadline) {
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator{device, error}) {
            const std::string name = entry.path().filename().string();
            if (name.starts_with("event") &&
                ::access(("/dev/input/" + name).c_str(), R_OK | W_OK) == 0) {
                return;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    lucent::warn("gamepad", "{} is not yet open to this user; a game may not see it",
                 sysname.data());
}

VirtualPad::~VirtualPad() {
    ioctl(fd_, UI_DEV_DESTROY);
    ::close(fd_);
}

void VirtualPad::write(const std::vector<PadEvent>& events) {
    std::vector<input_event> out(events.size());
    for (std::size_t i = 0; i < events.size(); ++i) {
        out[i].type = events[i].type;
        out[i].code = events[i].code;
        out[i].value = events[i].value;
    }
    const auto bytes = out.size() * sizeof(input_event);
    // uinput takes whole events or fails the write, so a short write cannot happen.
    if (!out.empty() && ::write(fd_, out.data(), bytes) != static_cast<ssize_t>(bytes)) {
        throw std::system_error{errno, std::generic_category(), "write uinput"};
    }
}

} // namespace iideck::gamepad

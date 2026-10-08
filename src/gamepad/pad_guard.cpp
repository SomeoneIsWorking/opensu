#include "pad_guard.hpp"

#include <cerrno>
#include <cstdint>
#include <system_error>
#include <utility>

#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "lucent/log.h"

namespace iideck::gamepad {

PadGuard::Pad::Pad(EvdevDevice held) : device{std::move(held)}, translator{device.capabilities()} {
    std::vector<PadEvent> forward;
    std::vector<Event> ignored;
    for (const PadEvent& event : device.state()) {
        translator.translate(event, forward, ignored);
    }
    output.write(forward);
}

PadGuard::PadGuard(const std::filesystem::path& inputDir) {
    wake_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wake_ < 0) {
        throw std::system_error{errno, std::generic_category(), "eventfd"};
    }
    for (EvdevDevice& device : EvdevDevice::openGamepads(inputDir)) {
        if (!device.grab()) {
            missed_ = true;
            continue;
        }
        const std::string name = device.name();
        try {
            auto pad = std::make_unique<Pad>(std::move(device));
            lucent::info("gamepad", "holding {} ({}) for the game", pad->device.name(),
                         pad->device.node().string());
            pads_.push_back(std::move(pad));
        } catch (const std::system_error& failure) {
            lucent::error("gamepad", "cannot give {} a virtual pad: {}", name, failure.what());
            missed_ = true;
        }
    }
    held_ = pads_.size();
    thread_ = std::jthread{[this](const std::stop_token& stop) {
        run(stop);
    }};
}

PadGuard::~PadGuard() {
    thread_.request_stop();
    const std::uint64_t one = 1;
    [[maybe_unused]] const auto written = ::write(wake_, &one, sizeof(one));
    thread_.join();
    ::close(wake_);
}

std::vector<std::string> PadGuard::environment() const {
    if (held_ == 0 || missed_) {
        return {};
    }
    return {"SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x045e/0x028e"};
}

void PadGuard::setBlocked(bool blocked) {
    blocked_.store(blocked);
    const std::uint64_t one = 1;
    [[maybe_unused]] const auto written = ::write(wake_, &one, sizeof(one));
}

std::vector<Event> PadGuard::takeControls() {
    const std::lock_guard lock{controlsMutex_};
    return std::exchange(controls_, {});
}

void PadGuard::run(const std::stop_token& stop) {
    // A virtual pad that cannot be written is a uinput failure the game cannot recover from
    // either; the pad stays held and the game sees it stop.
    const auto write = [](Pad& pad, const std::vector<PadEvent>& events) {
        try {
            pad.output.write(events);
        } catch (const std::system_error& failure) {
            lucent::error("gamepad", "{}: {}", pad.device.name(), failure.what());
        }
    };
    bool blocked = false;
    while (!stop.stop_requested()) {
        std::vector<pollfd> fds;
        fds.push_back(pollfd{wake_, POLLIN, 0});
        for (const auto& pad : pads_) {
            fds.push_back(pollfd{pad->device.fd(), POLLIN, 0});
        }
        if (::poll(fds.data(), fds.size(), -1) < 0 && errno != EINTR) {
            lucent::error("gamepad", "poll: {}", std::generic_category().message(errno));
            return;
        }
        std::uint64_t count = 0;
        [[maybe_unused]] const auto drained = ::read(wake_, &count, sizeof(count));

        const bool wanted = blocked_.load();
        if (wanted != blocked) {
            blocked = wanted;
            for (const auto& pad : pads_) {
                write(*pad, blocked ? pad->translator.rest() : pad->translator.resume());
            }
        }

        std::vector<Event> controls;
        for (std::size_t i = fds.size() - 1; i > 0; --i) {
            if (fds[i].revents == 0) {
                continue;
            }
            Pad& pad = *pads_[i - 1];
            std::vector<PadEvent> events;
            if (!pad.device.read(events) || (fds[i].revents & (POLLHUP | POLLERR)) != 0) {
                lucent::info("gamepad", "{} is gone", pad.device.name());
                pads_.erase(pads_.begin() + static_cast<std::ptrdiff_t>(i - 1));
                continue;
            }
            std::vector<PadEvent> forward;
            for (const PadEvent& event : events) {
                pad.translator.translate(event, forward, controls);
            }
            if (!blocked) {
                write(pad, forward);
            }
        }
        if (!controls.empty()) {
            const std::lock_guard lock{controlsMutex_};
            controls_.insert(controls_.end(), controls.begin(), controls.end());
        }
    }
}

} // namespace iideck::gamepad

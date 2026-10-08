#include "pads.hpp"

#include <array>
#include <cerrno>
#include <system_error>
#include <utility>

#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "lucent/log.h"

namespace iideck::gamepad {
namespace {

std::system_error lastError(const char* what) {
    return std::system_error{errno, std::generic_category(), what};
}

} // namespace

Pads::Pad::Pad(EvdevDevice opened) : device{std::move(opened)}, translator{device.capabilities()} {
    // The translator starts from the pad's present state, so the first resume is right.
    std::vector<PadEvent> ignored;
    std::vector<Event> controls;
    for (const PadEvent& event : device.state()) {
        translator.translate(event, ignored, controls);
    }
}

Pads::Pads(std::filesystem::path inputDir) : inputDir_{std::move(inputDir)} {
    wake_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wake_ < 0) {
        throw lastError("eventfd");
    }
    watch_ = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (watch_ < 0) {
        ::close(wake_);
        throw lastError("inotify_init1");
    }
    // udev creates a node, then grants access to it: a pad becomes readable on the attribute
    // change, not the creation.
    if (inotify_add_watch(watch_, inputDir_.c_str(), IN_CREATE | IN_ATTRIB) < 0) {
        const std::system_error failure = lastError("inotify_add_watch");
        ::close(watch_);
        ::close(wake_);
        throw failure;
    }
    scan();
    thread_ = std::jthread{[this](const std::stop_token& stop) {
        run(stop);
    }};
}

Pads::~Pads() {
    thread_.request_stop();
    wake();
    thread_.join();
    ::close(watch_);
    ::close(wake_);
}

std::vector<Event> Pads::takeEvents() {
    const std::lock_guard lock{mutex_};
    return std::exchange(events_, {});
}

std::vector<std::string> Pads::hold() {
    std::unique_lock lock{mutex_};
    wanted_.held = true;
    wanted_.blocked = false;
    awaitApplied(lock, ++wanted_.serial);
    if (appliedMissed_) {
        return {};
    }
    return {"SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x045e/0x028e"};
}

void Pads::release() {
    std::unique_lock lock{mutex_};
    wanted_.held = false;
    wanted_.blocked = false;
    awaitApplied(lock, ++wanted_.serial);
}

void Pads::awaitApplied(std::unique_lock<std::mutex>& lock, std::uint64_t serial) {
    wake();
    appliedChanged_.wait(lock, [this, serial] {
        return appliedSerial_ >= serial;
    });
}

void Pads::setBlocked(bool blocked) {
    const std::lock_guard lock{mutex_};
    wanted_.blocked = blocked;
    ++wanted_.serial;
    wake();
}

void Pads::wake() const {
    const std::uint64_t one = 1;
    [[maybe_unused]] const auto written = ::write(wake_, &one, sizeof(one));
}

void Pads::scan() {
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator{inputDir_, error}) {
        add(entry.path());
    }
}

void Pads::add(const std::filesystem::path& node) {
    if (!node.filename().string().starts_with("event")) {
        return;
    }
    for (const auto& pad : pads_) {
        if (pad->device.node() == node) {
            return;
        }
    }
    std::optional<EvdevDevice> device = EvdevDevice::openPad(node);
    if (!device) {
        return;
    }
    auto pad = std::make_unique<Pad>(std::move(*device));
    lucent::info("gamepad", "controller connected: {} ({})", pad->device.name(), node.string());
    if (applied_.held && !apply(*pad, true, applied_.blocked)) {
        missed_ = true;
    }
    std::vector<Event> events{Event{.kind = Event::Kind::Connected, .device = pad->device.name()}};
    publish(events);
    pads_.push_back(std::move(pad));
}

bool Pads::apply(Pad& pad, bool held, bool blocked) {
    if (!held) {
        pad.output.reset();
        pad.device.ungrab();
        return true;
    }
    if (!pad.device.grab()) {
        return false;
    }
    try {
        pad.output = std::make_unique<VirtualPad>();
    } catch (const std::system_error& failure) {
        lucent::error("gamepad", "cannot give {} a virtual pad: {}", pad.device.name(),
                      failure.what());
        pad.device.ungrab();
        return false;
    }
    if (!blocked) {
        write(pad, pad.translator.resume());
    }
    lucent::info("gamepad", "holding {} for the game", pad.device.name());
    return true;
}

void Pads::write(Pad& pad, const std::vector<PadEvent>& events) {
    if (!pad.output) {
        return;
    }
    // A virtual pad that cannot be written is a uinput failure the game cannot recover from
    // either; the pad stays held and the game sees it stop.
    try {
        pad.output->write(events);
    } catch (const std::system_error& failure) {
        lucent::error("gamepad", "{}: {}", pad.device.name(), failure.what());
    }
}

void Pads::publish(std::vector<Event>& events) {
    if (events.empty()) {
        return;
    }
    const std::lock_guard lock{mutex_};
    events_.insert(events_.end(), std::make_move_iterator(events.begin()),
                   std::make_move_iterator(events.end()));
}

void Pads::run(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        std::vector<pollfd> fds;
        fds.push_back(pollfd{wake_, POLLIN, 0});
        fds.push_back(pollfd{watch_, POLLIN, 0});
        for (const auto& pad : pads_) {
            fds.push_back(pollfd{pad->device.fd(), POLLIN, 0});
        }
        if (::poll(fds.data(), fds.size(), -1) < 0 && errno != EINTR) {
            lucent::error("gamepad", "poll: {}", std::generic_category().message(errno));
            // Nothing is read from here on; a waiting hold() is answered with nothing held.
            const std::lock_guard lock{mutex_};
            appliedSerial_ = UINT64_MAX;
            appliedMissed_ = true;
            appliedChanged_.notify_all();
            return;
        }
        std::uint64_t count = 0;
        [[maybe_unused]] const auto drained = ::read(wake_, &count, sizeof(count));

        Wanted wanted;
        {
            const std::lock_guard lock{mutex_};
            wanted = wanted_;
        }
        if (wanted.held != applied_.held) {
            missed_ = false;
            for (const auto& pad : pads_) {
                if (!apply(*pad, wanted.held, wanted.blocked)) {
                    missed_ = true;
                }
            }
        } else if (wanted.held && wanted.blocked != applied_.blocked) {
            for (const auto& pad : pads_) {
                write(*pad, wanted.blocked ? pad->translator.rest() : pad->translator.resume());
            }
        }
        applied_ = wanted;
        {
            const std::lock_guard lock{mutex_};
            appliedSerial_ = wanted.serial;
            appliedMissed_ = missed_;
        }
        appliedChanged_.notify_all();

        std::vector<Event> events;
        // Pads first: fds index into pads_ as it was polled, before any new pad is added.
        for (std::size_t i = fds.size() - 1; i > 1; --i) {
            if (fds[i].revents == 0) {
                continue;
            }
            Pad& pad = *pads_[i - 2];
            std::vector<PadEvent> read;
            if (!pad.device.read(read) || (fds[i].revents & (POLLHUP | POLLERR)) != 0) {
                lucent::info("gamepad", "controller disconnected: {}", pad.device.name());
                events.push_back(
                    Event{.kind = Event::Kind::Disconnected, .device = pad.device.name()});
                pads_.erase(pads_.begin() + static_cast<std::ptrdiff_t>(i - 2));
                continue;
            }
            std::vector<PadEvent> forward;
            for (const PadEvent& event : read) {
                pad.translator.translate(event, forward, events);
            }
            if (!applied_.blocked) {
                write(pad, forward);
            }
        }
        // A batch is forwarded before its controls are reported.
        publish(events);

        if ((fds[1].revents & POLLIN) != 0) {
            std::array<char, 4096> buffer{};
            const ssize_t got = ::read(watch_, buffer.data(), buffer.size());
            for (ssize_t at = 0; at < got;) {
                const auto* change = reinterpret_cast<const inotify_event*>(buffer.data() + at);
                if (change->len > 0) {
                    add(inputDir_ / change->name);
                }
                at += static_cast<ssize_t>(sizeof(inotify_event) + change->len);
            }
        }
    }
}

} // namespace iideck::gamepad

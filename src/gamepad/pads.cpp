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

namespace opensu::gamepad {
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
        translator.translate(event, ignored, controls, ChordGate::Clock::now());
    }
}

Pads::Pads(std::filesystem::path inputDir, OwnVirtualPads own)
    : inputDir_{std::move(inputDir)}, own_{own} {
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

std::vector<PadInfo> Pads::connected() const {
    const std::lock_guard lock{mutex_};
    return connected_;
}

std::vector<std::string> Pads::hold(ForwardMap map) {
    std::unique_lock lock{mutex_};
    wanted_.map = std::move(map);
    wanted_.blocked = false;
    request(lock, Claim::Game);
    if (appliedMissed_) {
        return {};
    }
    return {"SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x045e/0x028e"};
}

void Pads::release(Claim after) {
    std::unique_lock lock{mutex_};
    wanted_.map = {};
    wanted_.blocked = false;
    request(lock, after);
}

void Pads::claim(Claim claim) {
    std::unique_lock lock{mutex_};
    request(lock, claim);
}

void Pads::request(std::unique_lock<std::mutex>& lock, Claim claim) {
    wanted_.claim = claim;
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

void Pads::setChords(ChordTriggers chords) {
    std::unique_lock lock{mutex_};
    wanted_.chords = std::move(chords);
    awaitApplied(lock, ++wanted_.serial);
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
    std::optional<EvdevDevice> device = EvdevDevice::openPad(node, own_);
    if (!device) {
        return;
    }
    auto pad = std::make_unique<Pad>(std::move(*device));
    pad->translator.setChords(applied_.chords);
    pad->translator.setForwardMap(applied_.map);
    lucent::info("gamepad", "controller connected: {} ({})", pad->device.name(), node.string());
    if (!apply(*pad, applied_)) {
        missed_ = true;
    }
    const PadInfo info{node.string(), pad->device.name(), pad->device.uniq()};
    std::vector<Event> events{
        Event{.kind = Event::Kind::Connected, .device = info.name, .source = info.id}};
    {
        const std::lock_guard lock{mutex_};
        connected_.push_back(info);
    }
    publish(events);
    pads_.push_back(std::move(pad));
}

bool Pads::apply(Pad& pad, const Wanted& wanted) {
    const bool grab = wanted.claim == Claim::Shell || wanted.claim == Claim::Game;
    const bool virtualPad = wanted.claim == Claim::Game || wanted.claim == Claim::Away;
    bool held = true;
    if (grab && !pad.grabbed) {
        pad.grabbed = pad.device.grab();
        held = pad.grabbed;
    } else if (!grab && pad.grabbed) {
        pad.device.ungrab();
        pad.grabbed = false;
    }
    if (!virtualPad) {
        pad.output.reset();
        pad.live = false;
    } else if (!pad.output) {
        try {
            pad.output = std::make_unique<VirtualPad>();
        } catch (const std::system_error& failure) {
            lucent::error("gamepad", "cannot give {} a virtual pad: {}", pad.device.name(),
                          failure.what());
            return false;
        }
    }
    const bool live = pad.output && wanted.claim == Claim::Game && !wanted.blocked;
    if (live != pad.live) {
        write(pad, live ? pad.translator.resume() : pad.translator.rest());
        pad.live = live;
    }
    lucent::debug("gamepad", "{}: grabbed {}, virtual pad {}", pad.device.name(), pad.grabbed,
                  pad.output != nullptr);
    return held;
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
        if (wanted.chords != applied_.chords) {
            for (const auto& pad : pads_) {
                pad->translator.setChords(wanted.chords);
            }
        }
        if (wanted.map != applied_.map) {
            // Before a hold, so a new virtual pad resumes from the mapped state.
            for (const auto& pad : pads_) {
                pad->translator.setForwardMap(wanted.map);
            }
        }
        if (wanted.claim != applied_.claim || wanted.blocked != applied_.blocked) {
            missed_ = false;
            for (const auto& pad : pads_) {
                if (!apply(*pad, wanted)) {
                    missed_ = true;
                }
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
        const ChordGate::Clock::time_point now = ChordGate::Clock::now();
        // Pads first: fds index into pads_ as it was polled, before any new pad is added.
        for (std::size_t i = fds.size() - 1; i > 1; --i) {
            if (fds[i].revents == 0) {
                continue;
            }
            Pad& pad = *pads_[i - 2];
            std::vector<PadEvent> read;
            if (!pad.device.read(read) || (fds[i].revents & (POLLHUP | POLLERR)) != 0) {
                lucent::info("gamepad", "controller disconnected: {}", pad.device.name());
                const std::string id = pad.device.node().string();
                events.push_back(Event{
                    .kind = Event::Kind::Disconnected, .device = pad.device.name(), .source = id});
                {
                    const std::lock_guard lock{mutex_};
                    std::erase_if(connected_, [&id](const PadInfo& info) {
                        return info.id == id;
                    });
                }
                pads_.erase(pads_.begin() + static_cast<std::ptrdiff_t>(i - 2));
                continue;
            }
            std::vector<PadEvent> forward;
            const std::size_t before = events.size();
            for (const PadEvent& event : read) {
                pad.translator.translate(event, forward, events, now);
            }
            for (std::size_t i = before; i < events.size(); ++i) {
                events[i].source = pad.device.node().string();
            }
            if (pad.live) {
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

} // namespace opensu::gamepad

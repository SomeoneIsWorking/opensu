#include "host_stream.hpp"

#include <array>
#include <utility>
#include <vector>

#include "lucent/log.h"
#include "secrets/secret.hpp"

namespace opensu::stream {
namespace {

using Clock = std::chrono::steady_clock;

/// How long the guest may stay silent before the session is over; it pings five times a second.
constexpr std::chrono::seconds quietLimit{5};
/// How long the guest has to send its first datagram.
constexpr std::chrono::seconds joinLimit{20};
constexpr std::chrono::milliseconds readWait{100};
/// An End is sent this many times, so one lost datagram does not leave the guest waiting.
constexpr int endRepeats = 3;
/// How often the prober looks for datagrams due: often in a burst, seldom between.
constexpr std::chrono::milliseconds probeTick{1};
constexpr std::chrono::milliseconds probeIdle{20};

} // namespace

HostStream::HostStream(Setup setup)
    : setup_{std::move(setup)}, socket_{0}, opener_{setup_.keys.guest}, seal_{setup_.keys.host},
      link_{setup_.encoders} {
    secrets::wipeBytes(setup_.keys.host.data(), setup_.keys.host.size());
    secrets::wipeBytes(setup_.keys.guest.data(), setup_.keys.guest.size());
    receiver_ = std::jthread{[this](const std::stop_token& stop) {
        receive(stop);
    }};
    prober_ = std::jthread{[this](const std::stop_token& stop) {
        probe(stop);
    }};
}

HostStream::~HostStream() {
    prober_.request_stop();
    receiver_.request_stop();
    if (prober_.joinable()) {
        prober_.join();
    }
    if (receiver_.joinable()) {
        receiver_.join();
    }
}

std::optional<LinkPlan> HostStream::plan() const {
    const std::lock_guard lock{linkMutex_};
    return link_.plan();
}

void HostStream::refuse(VideoCodec codec) {
    const std::lock_guard lock{linkMutex_};
    link_.refuse(codec);
}

std::string HostStream::linkSummary() const {
    const std::lock_guard lock{linkMutex_};
    return link_.summary();
}

void HostStream::probe(const std::stop_token& stop) {
    while (!stop.stop_requested() && !over()) {
        LinkRate::ProbeDue due;
        if (joined_) {
            const std::lock_guard lock{linkMutex_};
            due = link_.probe(wire::nowMicros());
        }
        for (std::uint32_t i = 0; i < due.count; ++i) {
            send(PacketKind::Probe, wire::write(wire::Probe{due.burst, due.first + i}, probeBytes));
        }
        std::this_thread::sleep_for(due.count > 0 ? probeTick : probeIdle);
    }
}

void HostStream::send(PacketKind kind, std::string_view payload) {
    const std::lock_guard lock{sendMutex_};
    if (guest_) {
        static_cast<void>(socket_.send(*guest_, seal_.seal(kind, payload)));
    }
}

void HostStream::end(std::string_view reason) {
    const std::string payload = wire::write(wire::End{std::string{reason}});
    for (int i = 0; i < endRepeats; ++i) {
        send(PacketKind::End, payload);
    }
    finish(std::string{reason});
}

std::optional<std::string> HostStream::over() const {
    const std::lock_guard lock{overMutex_};
    return over_;
}

void HostStream::finish(std::string reason) {
    const std::lock_guard lock{overMutex_};
    if (!over_) {
        lucent::info("stream", "remote play with {} is over: {}", setup_.guest, reason);
        over_ = std::move(reason);
    }
}

void HostStream::receive(const std::stop_token& stop) {
    std::vector<char> buffer(largestDatagram);
    const Clock::time_point started = Clock::now();
    Clock::time_point heard = started;
    while (!stop.stop_requested() && !over()) {
        UdpAddress from;
        const std::optional<std::size_t> got = socket_.receive(buffer, from, readWait);
        const Clock::time_point now = Clock::now();
        if (!got) {
            if (joined_ ? now - heard > quietLimit : now - started > joinLimit) {
                finish(joined_ ? "the guest stopped answering" : "the guest never came");
            }
            continue;
        }
        {
            const std::lock_guard lock{sendMutex_};
            if (guest_ && from != *guest_) {
                continue;
            }
        }
        const std::optional<Opened> opened = opener_.open({buffer.data(), *got});
        if (!opened) {
            continue;
        }
        if (!joined_) {
            const std::lock_guard lock{sendMutex_};
            guest_ = from;
            joined_ = true;
            lucent::info("stream", "{} joined remote play from UDP port {}", setup_.guest,
                         from.port);
        }
        heard = now;
        handle(*opened);
    }
}

void HostStream::handle(const Opened& opened) {
    switch (opened.kind) {
    case PacketKind::Ping:
        if (const std::optional<wire::Ping> ping = wire::readPing(opened.payload)) {
            send(PacketKind::Pong, wire::write(wire::Pong{ping->sentUs, wire::nowMicros()}));
        }
        break;
    case PacketKind::Pad:
        if (const std::optional<wire::PadState> pad = wire::readPadState(opened.payload);
            pad && setup_.pad) {
            setup_.pad(*pad);
        }
        break;
    case PacketKind::Keys:
        if (const std::optional<wire::KeyState> keys = wire::readKeyState(opened.payload);
            keys && setup_.keyboard) {
            setup_.keyboard(*keys);
        }
        break;
    case PacketKind::Mouse:
        if (const std::optional<wire::MouseState> mouse = wire::readMouseState(opened.payload);
            mouse && setup_.mouse) {
            setup_.mouse(*mouse);
        }
        break;
    case PacketKind::End:
        if (const std::optional<wire::End> end = wire::readEnd(opened.payload)) {
            finish("the guest ended it: " + end->reason);
        }
        break;
    case PacketKind::Report:
        if (const std::optional<wire::Report> report = wire::readReport(opened.payload)) {
            const std::lock_guard lock{linkMutex_};
            link_.report(*report, wire::nowMicros());
        }
        break;
    case PacketKind::KeyFrame:
        keyFrame_ = true;
        break;
    case PacketKind::Mapping:
        if (const std::optional<wire::PadMapping> mapping = wire::readPadMapping(opened.payload);
            mapping && setup_.mapping) {
            setup_.mapping(*mapping);
        }
        break;
    case PacketKind::Pong:
    case PacketKind::Video:
    case PacketKind::Audio:
    case PacketKind::Probe:
        break;
    }
}

} // namespace opensu::stream

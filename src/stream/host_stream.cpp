#include "host_stream.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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
/// How often the pacer looks for a probe to start when nothing else wakes it.
constexpr std::chrono::milliseconds probeIdle{20};
/// What the pacer sends video at, over the plan's rate.
constexpr double paceOverRate = 2.0;
/// How far over its reserve the bucket fills, so probe datagrams still go when the pacer wakes
/// late.
constexpr std::size_t padRoom = 4 * probeDatagramBytes;

/// A video payload as it crosses the link: sealed.
std::size_t sealedBytes(const std::string& payload) {
    return payload.size() + sealOverhead;
}

/// The datagram bytes of one frame at `bitsPerSecond`, its headers and seals counted.
double frameDatagramBytes(std::uint64_t bitsPerSecond) {
    const double coded = static_cast<double>(bitsPerSecond) / 8 / videoFramesPerSecond;
    const double packets = std::ceil(coded / static_cast<double>(videoPacketBytes));
    return coded + packets * static_cast<double>(wire::videoHeaderBytes + sealOverhead);
}

} // namespace

HostStream::HostStream(Setup setup)
    : setup_{std::move(setup)}, socket_{0}, opener_{setup_.keys.guest}, seal_{setup_.keys.host},
      link_{setup_.encoders} {
    secrets::wipeBytes(setup_.keys.host.data(), setup_.keys.host.size());
    secrets::wipeBytes(setup_.keys.guest.data(), setup_.keys.guest.size());
    pacer_.set(0, static_cast<double>(reserve_ + padRoom));
    receiver_ = std::jthread{[this](const std::stop_token& stop) {
        receive(stop);
    }};
    sender_ = std::jthread{[this](const std::stop_token& stop) {
        pace(stop);
    }};
}

HostStream::~HostStream() {
    sender_.request_stop();
    receiver_.request_stop();
    if (sender_.joinable()) {
        sender_.join();
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
    std::string summary;
    {
        const std::lock_guard lock{linkMutex_};
        summary = link_.summary();
    }
    const std::lock_guard lock{paceMutex_};
    return summary + " paced=" + std::to_string(paced_) + "/" + std::to_string(videoSent_);
}

void HostStream::pace(const std::stop_token& stop) {
    std::uint32_t padded = 0;
    std::uint32_t paddedBurst = 0;
    while (!stop.stop_requested() && !over()) {
        std::optional<LinkRate::Probing> probing;
        if (joined_) {
            const std::lock_guard lock{linkMutex_};
            link_.probed(paddedBurst, padded);
            probing = link_.probe(wire::nowMicros());
        }
        std::unique_lock lock{paceMutex_};
        const std::uint64_t nowUs = wire::nowMicros();
        probing_ = probing;
        tune(nowUs);
        sendDue(nowUs);
        padded = pad(nowUs);
        paddedBurst = probing ? probing->burst : 0;
        std::chrono::microseconds wait = probeIdle;
        if (!waiting_.empty()) {
            const std::uint64_t dueUs = pacer_.waitUs(sealedBytes(waiting_.front()), nowUs);
            wait = std::min(wait, std::chrono::microseconds{std::max<std::uint64_t>(dueUs, 1)});
        } else if (probing_) {
            const std::uint64_t dueUs = pacer_.waitUs(reserve_ + probeDatagramBytes, nowUs);
            wait = std::min(wait, std::chrono::microseconds{std::max<std::uint64_t>(dueUs, 1)});
        }
        paceWake_.wait_for(lock, stop, wait, [this] {
            return queued_;
        });
        queued_ = false;
    }
}

void HostStream::tune(std::uint64_t nowUs) {
    const double bytesPerSecond = probing_ ? static_cast<double>(probing_->bitsPerSecond) / 8
                                           : static_cast<double>(pacedRate_) / 8 * paceOverRate;
    if (bytesPerSecond != pacerBytesPerSecond_) {
        pacerBytesPerSecond_ = bytesPerSecond;
        pacer_.changeRate(bytesPerSecond, nowUs);
    }
}

void HostStream::sendDue(std::uint64_t nowUs) {
    while (!waiting_.empty() && pacer_.take(sealedBytes(waiting_.front()), nowUs)) {
        send(PacketKind::Video, waiting_.front());
        waiting_.pop_front();
        ++videoSent_;
    }
}

std::uint32_t HostStream::pad(std::uint64_t nowUs) {
    std::uint32_t sent = 0;
    while (probing_ && waiting_.empty() &&
           pacer_.waitUs(reserve_ + probeDatagramBytes, nowUs) == 0 &&
           pacer_.take(probeDatagramBytes, nowUs)) {
        send(PacketKind::Probe, wire::write(wire::Probe{.burst = probing_->burst,
                                                        .index = probing_->sent + sent,
                                                        .sentUs = wire::nowMicros()},
                                            probeBytes));
        ++sent;
    }
    return sent;
}

void HostStream::sendFrame(std::vector<std::string> payloads) {
    const std::optional<LinkPlan> planned = plan();
    const std::uint64_t nowUs = wire::nowMicros();
    const std::lock_guard lock{paceMutex_};
    if (planned && planned->bitsPerSecond != pacedRate_) {
        pacedRate_ = planned->bitsPerSecond;
        reserve_ = static_cast<std::size_t>(
            std::max(frameDatagramBytes(pacedRate_), static_cast<double>(probeDatagramBytes)));
        pacer_.set(pacerBytesPerSecond_, static_cast<double>(reserve_ + padRoom));
        tune(nowUs);
    }
    const bool idle = waiting_.empty();
    const std::size_t frame = payloads.size();
    for (std::string& payload : payloads) {
        waiting_.push_back(std::move(payload));
    }
    sendDue(nowUs);
    paced_ += std::min(waiting_.size(), frame);
    if (idle && !waiting_.empty()) {
        queued_ = true;
        paceWake_.notify_one();
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
                dropped_ = joined_.load();
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
    case PacketKind::Netplay:
        break;
    }
}

} // namespace opensu::stream

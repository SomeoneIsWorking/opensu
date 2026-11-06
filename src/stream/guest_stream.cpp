#include "guest_stream.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <deque>
#include <utility>
#include <vector>

#include "lucent/log.h"
#include "secrets/secret.hpp"

namespace opensu::stream {
namespace {

using Clock = std::chrono::steady_clock;

constexpr std::chrono::milliseconds pingEvery{200};
/// How long the host may stay silent before the session is over.
constexpr std::chrono::seconds quietLimit{5};
constexpr std::chrono::milliseconds readWait{100};
constexpr int endRepeats = 3;
/// The pongs the round trip is taken from: the fastest of the last few seconds.
constexpr std::size_t pongsKept = 25;
constexpr std::uint64_t microsPerSecond = 1000000;
/// Asks for a key frame at most this often; the host's answer takes a round trip.
constexpr std::uint64_t keyAskEveryUs = 100'000;
/// How often a probe under way is reported, so the host stops it soon after the queue grows.
constexpr std::uint64_t probeReportEveryUs = 20'000;

} // namespace

GuestStream::GuestStream(Setup setup)
    : setup_{std::move(setup)}, socket_{0}, opener_{setup_.keys.host}, seal_{setup_.keys.guest},
      received_{.decoders = setup_.decoders,
                .datagrams = 0,
                .bytes = 0,
                .newest = 0,
                .probeBurst = 0,
                .probeDatagrams = 0,
                .probeQueueUs = 0,
                .pictureWidth = 0,
                .pictureHeight = 0} {
    secrets::wipeBytes(setup_.keys.host.data(), setup_.keys.host.size());
    secrets::wipeBytes(setup_.keys.guest.data(), setup_.keys.guest.size());
    receiver_ = std::jthread{[this](const std::stop_token& stop) {
        receive(stop);
    }};
    pinger_ = std::jthread{[this](const std::stop_token& stop) {
        ping(stop);
    }};
}

GuestStream::~GuestStream() {
    pinger_.request_stop();
    receiver_.request_stop();
    changed_.notify_all();
    if (pinger_.joinable()) {
        pinger_.join();
    }
    if (receiver_.joinable()) {
        receiver_.join();
    }
}

void GuestStream::pad(const wire::PadState& state) {
    {
        const std::lock_guard lock{mutex_};
        if (pad_ == state) {
            return;
        }
        pad_ = state;
        inputChanged_ = true;
    }
    changed_.notify_all();
}

void GuestStream::keys(const wire::KeyState& state) {
    {
        const std::lock_guard lock{mutex_};
        if (keys_ == state) {
            return;
        }
        keys_ = state;
        inputChanged_ = true;
    }
    changed_.notify_all();
}

void GuestStream::mouse(const wire::MouseState& state) {
    {
        const std::lock_guard lock{mutex_};
        if (mouse_ == state) {
            return;
        }
        mouse_ = state;
        inputChanged_ = true;
    }
    changed_.notify_all();
}

void GuestStream::mapping(const wire::PadMapping& mapping) {
    {
        const std::lock_guard lock{mutex_};
        if (mapping_ == mapping) {
            return;
        }
        mapping_ = mapping;
        inputChanged_ = true;
    }
    changed_.notify_all();
}

void GuestStream::end(std::string_view reason) {
    const std::string payload = wire::write(wire::End{std::string{reason}});
    for (int i = 0; i < endRepeats; ++i) {
        send(PacketKind::End, payload);
    }
    finish(std::string{reason});
}

std::optional<std::string> GuestStream::over() const {
    const std::lock_guard lock{mutex_};
    return over_;
}

GuestStream::Timing GuestStream::timing() const {
    const std::lock_guard lock{mutex_};
    return timing_;
}

void GuestStream::send(PacketKind kind, std::string_view payload) {
    const std::lock_guard lock{sendMutex_};
    static_cast<void>(socket_.send(setup_.host, seal_.seal(kind, payload)));
}

void GuestStream::finish(std::string reason) {
    {
        const std::lock_guard lock{mutex_};
        if (over_) {
            return;
        }
        lucent::info("stream", "remote play is over: {}", reason);
        over_ = std::move(reason);
    }
    changed_.notify_all();
}

void GuestStream::sendInput() {
    std::optional<wire::PadState> pad;
    std::optional<wire::KeyState> keys;
    std::optional<wire::MouseState> mouse;
    std::optional<wire::PadMapping> mapping;
    {
        const std::lock_guard lock{mutex_};
        pad = pad_;
        keys = keys_;
        mouse = mouse_;
        mapping = mapping_;
        inputChanged_ = false;
    }
    if (mapping) {
        send(PacketKind::Mapping, wire::write(*mapping));
    }
    if (pad) {
        send(PacketKind::Pad, wire::write(*pad));
    }
    if (keys) {
        send(PacketKind::Keys, wire::write(*keys));
    }
    if (mouse) {
        send(PacketKind::Mouse, wire::write(*mouse));
    }
}

float GuestStream::shape() const {
    const std::lock_guard lock{mutex_};
    return shape_;
}

std::string GuestStream::panel() const {
    const std::lock_guard lock{mutex_};
    return panel_;
}

std::string GuestStream::stage() const {
    const std::lock_guard lock{mutex_};
    return stage_;
}

void GuestStream::showsIn(int width, int height) {
    const std::lock_guard lock{mutex_};
    received_.pictureWidth = static_cast<std::uint16_t>(width);
    received_.pictureHeight = static_cast<std::uint16_t>(height);
}

void GuestStream::sendReport() {
    wire::Report report;
    {
        const std::lock_guard lock{mutex_};
        if (probeQuickest_ && probeQuickestSinceReport_) {
            received_.probeQueueUs =
                static_cast<std::uint32_t>(*probeQuickestSinceReport_ - *probeQuickest_);
        }
        probeQuickestSinceReport_.reset();
        reportedUs_ = wire::nowMicros();
        report = received_;
    }
    send(PacketKind::Report, wire::write(report));
}

void GuestStream::ping(const std::stop_token& stop) {
    Clock::time_point next = Clock::now();
    while (!stop.stop_requested() && !over()) {
        if (Clock::now() >= next) {
            send(PacketKind::Ping, wire::write(wire::Ping{wire::nowMicros()}));
            sendReport();
            sendInput();
            next = Clock::now() + pingEvery;
            continue;
        }
        bool changed = false;
        {
            std::unique_lock lock{mutex_};
            changed_.wait_until(lock, stop, next, [this] {
                return inputChanged_ || over_.has_value();
            });
            changed = inputChanged_;
        }
        if (changed) {
            sendInput();
        }
    }
}

void GuestStream::receive(const std::stop_token& stop) {
    std::vector<char> buffer(largestDatagram);
    Clock::time_point heard = Clock::now();
    std::uint64_t secondStart = wire::nowMicros();
    std::uint64_t secondBytes = 0;
    while (!stop.stop_requested() && !over()) {
        std::chrono::milliseconds wait = readWait;
        if (const std::optional<std::uint64_t> nextUs = cap_.nextUs()) {
            const std::uint64_t nowUs = wire::nowMicros();
            const std::uint64_t leftUs = *nextUs > nowUs ? *nextUs - nowUs : 0;
            wait = std::min(wait, std::chrono::ceil<std::chrono::milliseconds>(
                                      std::chrono::microseconds{leftUs}));
        }
        UdpAddress from;
        const std::optional<std::size_t> got = socket_.receive(buffer, from, wait);
        const std::uint64_t receivedUs = wire::nowMicros();
        if (receivedUs - secondStart >= microsPerSecond) {
            const std::lock_guard lock{mutex_};
            timing_.bytesPerSecond = secondBytes * microsPerSecond / (receivedUs - secondStart);
            secondStart = receivedUs;
            secondBytes = 0;
        }
        if (got && from == setup_.host) {
            static_cast<void>(cap_.push({buffer.data(), *got}, receivedUs));
        }
        while (std::optional<LinkCap::Out> out = cap_.pop(wire::nowMicros())) {
            if (take(out->datagram, out->leftUs)) {
                heard = Clock::now();
                secondBytes += out->datagram.size();
            }
        }
        if (Clock::now() - heard > quietLimit) {
            finish("the host stopped answering");
        }
    }
}

bool GuestStream::take(std::string_view datagram, std::uint64_t receivedUs) {
    const std::optional<Opened> opened = opener_.open(datagram);
    if (!opened) {
        return false;
    }
    bool reportNow = false;
    {
        const std::lock_guard lock{mutex_};
        ++received_.datagrams;
        received_.bytes += datagram.size();
        received_.newest = std::max(received_.newest, opened->counter);
        if (opened->kind == PacketKind::Probe) {
            if (const std::optional<wire::Probe> probe = wire::readProbe(opened->payload)) {
                tookProbe(*probe, receivedUs);
                reportNow = receivedUs - reportedUs_ >= probeReportEveryUs;
            }
        }
    }
    if (reportNow) {
        sendReport();
    }
    if (opened->kind == PacketKind::Pong) {
        const std::optional<wire::Pong> pong = wire::readPong(opened->payload);
        if (!pong || pong->pingUs > receivedUs) {
            return true;
        }
        pongs_.push_back(PongSample{receivedUs - pong->pingUs, pong->pingUs, pong->hostUs});
        if (pongs_.size() > pongsKept) {
            pongs_.pop_front();
        }
        // The fastest round trip of the kept ones, the host's answer taken at its middle.
        const auto fastest = std::ranges::min_element(pongs_, {}, &PongSample::roundTripUs);
        const std::lock_guard lock{mutex_};
        timing_.known = true;
        timing_.roundTripUs = fastest->roundTripUs;
        timing_.hostAheadUs = static_cast<std::int64_t>(fastest->hostUs) -
                              static_cast<std::int64_t>(fastest->pingUs + fastest->roundTripUs / 2);
        return true;
    }
    handle(*opened, receivedUs);
    return true;
}

void GuestStream::tookProbe(const wire::Probe& probe, std::uint64_t receivedUs) {
    if (probe.burst != received_.probeBurst) {
        received_.probeBurst = probe.burst;
        received_.probeDatagrams = 0;
        probeQuickest_.reset();
        probeQuickestSinceReport_.reset();
    }
    ++received_.probeDatagrams;
    // The clocks' offset is the same for every datagram, so the difference is the queue's.
    const std::int64_t tookUs =
        static_cast<std::int64_t>(receivedUs) - static_cast<std::int64_t>(probe.sentUs);
    probeQuickest_ = std::min(probeQuickest_.value_or(tookUs), tookUs);
    probeQuickestSinceReport_ = std::min(probeQuickestSinceReport_.value_or(tookUs), tookUs);
}

void GuestStream::handle(const Opened& opened, std::uint64_t receivedUs) {
    switch (opened.kind) {
    case PacketKind::Video:
        if (const std::optional<wire::VideoPacket> packet = wire::readVideoPacket(opened.payload);
            packet && setup_.video && setup_.video(*packet, receivedUs, timing().hostAheadUs) &&
            receivedUs - keyAskedUs_ >= keyAskEveryUs) {
            keyAskedUs_ = receivedUs;
            send(PacketKind::KeyFrame, {});
        }
        break;
    case PacketKind::Audio:
        if (const std::optional<wire::AudioPacket> packet = wire::readAudioPacket(opened.payload);
            packet && setup_.audio) {
            setup_.audio(*packet, receivedUs, timing().hostAheadUs);
        }
        break;
    case PacketKind::End:
        if (const std::optional<wire::End> end = wire::readEnd(opened.payload)) {
            finish(end->reason);
        }
        break;
    case PacketKind::Shape: {
        unsigned shape = 0;
        const std::string_view text = opened.payload;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), shape);
        if (parsed.ec == std::errc{} && shape > 0) {
            const std::lock_guard lock{mutex_};
            shape_ = static_cast<float>(shape) / 10000.0F;
        }
        break;
    }
    case PacketKind::Panel: {
        const std::lock_guard lock{mutex_};
        panel_ = opened.payload;
        break;
    }
    case PacketKind::Stage: {
        const std::lock_guard lock{mutex_};
        stage_ = wire::readText(opened.payload);
        break;
    }
    case PacketKind::Ping:
    case PacketKind::Pong:
    case PacketKind::Pad:
    case PacketKind::Keys:
    case PacketKind::Mouse:
    case PacketKind::Probe:
    case PacketKind::Report:
    case PacketKind::KeyFrame:
    case PacketKind::Mapping:
    case PacketKind::Netplay:
        break;
    }
}

} // namespace opensu::stream

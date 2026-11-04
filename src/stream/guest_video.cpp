#include "guest_video.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>
#include <vector>

#include "clock/percentile.hpp"
#include "lucent/log.h"

namespace opensu::stream {
namespace {

/// The shown frames the percentiles are taken over: the last ten seconds at 60 a second.
constexpr std::size_t framesKept = 600;
/// Decoded frames waiting to be shown that are remembered; older ones were never shown.
constexpr std::size_t waitingKept = 8;
constexpr double microsPerMilli = 1000.0;

} // namespace

GuestVideo::GuestVideo()
    : decoding_{[this](const std::stop_token& stop) {
          decodeLoop(stop);
      }} {
}

void GuestVideo::take(const wire::VideoPacket& packet, std::uint64_t receivedUs,
                      std::int64_t hostAheadUs) {
    {
        const std::lock_guard lock{queueMutex_};
        queue_.push_back(
            Queued{.packet = packet, .receivedUs = receivedUs, .hostAheadUs = hostAheadUs});
    }
    queued_.notify_one();
}

void GuestVideo::decodeLoop(const std::stop_token& stop) {
    std::vector<Queued> batch;
    while (!stop.stop_requested()) {
        {
            std::unique_lock lock{queueMutex_};
            if (!queued_.wait(lock, stop, [this] {
                    return !queue_.empty();
                })) {
                return;
            }
            batch.swap(queue_);
        }
        // A frame two newer ones have begun after would only be shown late.
        std::uint32_t newest = 0;
        for (const Queued& queued : batch) {
            newest = std::max(newest, queued.packet.frame);
        }
        const std::uint32_t oldestKept = newest > 0 ? newest - 1 : 0;
        std::uint32_t skipped = 0;
        std::size_t lateFrames = 0;
        for (const Queued& queued : batch) {
            if (queued.packet.frame < oldestKept) {
                if (queued.packet.frame != skipped) {
                    skipped = queued.packet.frame;
                    ++lateFrames;
                }
                continue;
            }
            decode(queued);
        }
        if (lateFrames > 0) {
            const std::lock_guard lock{mutex_};
            late_ += lateFrames;
        }
        batch.clear();
    }
}

void GuestVideo::decode(const Queued& queued) {
    const wire::VideoPacket& packet = queued.packet;
    const std::int64_t hostAheadUs = queued.hostAheadUs;
    const auto local = [hostAheadUs](std::uint64_t hostUs) {
        return static_cast<double>(static_cast<std::int64_t>(hostUs) - hostAheadUs);
    };
    if (!building_ || building_->number != packet.frame) {
        if (building_ && packet.frame < building_->number) {
            return;
        }
        if (building_) {
            const std::lock_guard lock{mutex_};
            ++dropped_;
        }
        building_ = Times{.number = packet.frame,
                          .capturedUs = local(packet.capturedUs),
                          .encodedUs = local(packet.encodedUs),
                          .sentUs = local(packet.sentUs),
                          .receivedUs = 0,
                          .decodedUs = 0};
    }
    building_->receivedUs = static_cast<double>(queued.receivedUs);
    if (!decoder_.push(packet.width, packet.height, packet.data)) {
        return;
    }
    Frame frame{.number = packet.frame, .picture = {}};
    try {
        decoder_.decode(frame.picture);
    } catch (const std::runtime_error& failure) {
        lucent::warn("video", "frame {} did not decode: {}", packet.frame, failure.what());
        building_.reset();
        return;
    }
    Times times = *building_;
    times.decodedUs = static_cast<double>(wire::nowMicros());
    building_.reset();
    const std::lock_guard lock{mutex_};
    ++decoded_;
    last_ = frame;
    fresh_ = std::move(frame);
    waiting_.push_back(times);
    if (waiting_.size() > waitingKept) {
        waiting_.pop_front();
    }
}

std::optional<GuestVideo::Frame> GuestVideo::takeFrame() {
    const std::lock_guard lock{mutex_};
    return std::exchange(fresh_, std::nullopt);
}

std::optional<GuestVideo::Frame> GuestVideo::lastDecoded() const {
    const std::lock_guard lock{mutex_};
    return last_;
}

void GuestVideo::presented(std::uint32_t number, std::uint64_t submittedUs, bool drawn) {
    const std::lock_guard lock{mutex_};
    const auto found = std::ranges::find(waiting_, number, &Times::number);
    if (found == waiting_.end()) {
        return;
    }
    const Times times = *found;
    waiting_.erase(waiting_.begin(), found + 1);
    const auto shown = static_cast<double>(submittedUs);
    shown_.push_back(Stages{.encode = (times.encodedUs - times.capturedUs) / microsPerMilli,
                            .send = (times.sentUs - times.encodedUs) / microsPerMilli,
                            .network = (times.receivedUs - times.sentUs) / microsPerMilli,
                            .decode = (times.decodedUs - times.receivedUs) / microsPerMilli,
                            .submit = (shown - times.decodedUs) / microsPerMilli,
                            .total = (shown - times.capturedUs) / microsPerMilli});
    if (shown_.size() > framesKept) {
        shown_.pop_front();
    }
    ++presented_;
    if (drawn) {
        ++drawn_;
    }
}

std::string GuestVideo::summary() const {
    const std::lock_guard lock{mutex_};
    std::string out = std::format("decoded={} dropped={} late={} submitted={} drawn={}", decoded_,
                                  dropped_, late_, presented_, drawn_);
    const auto stage = [this, &out](const char* name, double Stages::* field) {
        std::vector<double> values;
        values.reserve(shown_.size());
        for (const Stages& stages : shown_) {
            values.push_back(stages.*field);
        }
        std::ranges::sort(values);
        out += std::format(" {}={:.2f}/{:.2f}", name, clock::percentile(values, 0.5),
                           clock::percentile(values, 0.95));
    };
    if (!shown_.empty()) {
        stage("encode", &Stages::encode);
        stage("send", &Stages::send);
        stage("network", &Stages::network);
        stage("decode", &Stages::decode);
        stage("submit", &Stages::submit);
        stage("total", &Stages::total);
    }
    return out;
}

} // namespace opensu::stream

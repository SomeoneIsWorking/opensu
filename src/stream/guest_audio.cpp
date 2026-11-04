#include "guest_audio.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include "clock/percentile.hpp"

namespace opensu::stream {
namespace {

/// Packets the arrival times are kept of: the last ten seconds.
constexpr std::size_t arrivalsKept = 1000;
/// A gap longer than this is the session restarting its count, not loss to fill in.
constexpr std::uint32_t longestGap = 10;
constexpr double microsPerMilli = 1000.0;

} // namespace

GuestAudio::GuestAudio(std::function<void(std::span<const float>)> play)
    : play_{std::move(play)} {
}

void GuestAudio::take(const wire::AudioPacket& packet, std::uint64_t receivedUs,
                      std::int64_t hostAheadUs) {
    if (last_ != 0 && packet.sequence <= last_) {
        return;
    }
    const std::uint32_t gap = last_ == 0 ? 0 : packet.sequence - last_ - 1;
    last_ = packet.sequence;
    if (gap > 0 && gap <= longestGap) {
        for (std::uint32_t i = 0; i < gap; ++i) {
            decoder_.conceal(samples_);
            if (play_) {
                play_(samples_);
            }
        }
    }
    const bool decoded = decoder_.decode(packet.data, samples_);
    if (decoded && play_) {
        play_(samples_);
    }
    const double arrival =
        (static_cast<double>(receivedUs) -
         static_cast<double>(static_cast<std::int64_t>(packet.capturedUs) - hostAheadUs)) /
        microsPerMilli;
    const std::lock_guard lock{mutex_};
    ++packets_;
    lost_ += gap;
    arrivals_.push_back(arrival);
    if (arrivals_.size() > arrivalsKept) {
        arrivals_.pop_front();
    }
}

std::string GuestAudio::summary() const {
    const std::lock_guard lock{mutex_};
    std::string out = std::format("packets={} lost={}", packets_, lost_);
    if (!arrivals_.empty()) {
        std::vector<double> sorted{arrivals_.begin(), arrivals_.end()};
        std::ranges::sort(sorted);
        out += std::format(" arrival={:.2f}/{:.2f}", clock::percentile(sorted, 0.5),
                           clock::percentile(sorted, 0.95));
    }
    return out;
}

} // namespace opensu::stream

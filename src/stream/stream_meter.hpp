// stream_meter — how a remote play stream is going, as the guest's player sees it: the picture's
// latency from the host's capture to this screen, the link's round trip, the bitrate that came, the
// frames a second shown and those lost or skipped late, read once every `meterPeriodUs` from the
// counts the guest's video and stream keep, and graded Good, Fair or Poor. Pure state with the
// time passed in.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace opensu::stream {

/// How often a reading is taken.
inline constexpr std::uint64_t meterPeriodUs = 1'000'000;
/// The latency, round trip and share of frames lost past which the stream reads Fair, and Poor.
inline constexpr int fairLatencyMs = 50;
inline constexpr int poorLatencyMs = 100;
inline constexpr int fairRoundTripMs = 20;
inline constexpr int poorRoundTripMs = 60;
inline constexpr double fairLost = 0.01;
inline constexpr double poorLost = 0.05;

enum class StreamQuality : std::uint8_t { Good, Fair, Poor };

/// What the guest's video and stream counted so far.
struct StreamCounts {
    /// Frames shown, and those lost on the way or skipped late, since the stream began.
    std::size_t shown{0};
    std::size_t lost{0};
    /// Capture to submit of the frames shown last, in milliseconds; 0 before any.
    double latencyMs{0};
    /// The round trip of the fastest recent ping; 0 before the first pong.
    std::uint64_t roundTripUs{0};
    /// Bytes received in the last whole second.
    std::uint64_t bytesPerSecond{0};
};

/// One second of the stream.
struct StreamReading {
    int latencyMs{0};
    int roundTripMs{0};
    /// Megabits a second, in tenths.
    int tenthsOfMegabits{0};
    int framesPerSecond{0};
    int lostPerSecond{0};
    StreamQuality quality{StreamQuality::Good};

    bool operator==(const StreamReading&) const = default;
};

/// The grade of `reading`: Poor past any poor limit, Fair past any fair one, else Good.
[[nodiscard]] StreamQuality qualityOf(const StreamReading& reading) noexcept;

class StreamMeter {
  public:
    /// Takes the counts at `nowUs`; a new reading once `meterPeriodUs` passed since the last one
    /// (the first only sets the start), nothing otherwise.
    [[nodiscard]] std::optional<StreamReading> take(const StreamCounts& counts,
                                                    std::uint64_t nowUs);

  private:
    std::optional<std::uint64_t> sinceUs_;
    StreamCounts since_;
};

} // namespace opensu::stream

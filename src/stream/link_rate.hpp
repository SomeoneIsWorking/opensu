// link_rate — the codec and rate the host codes at, from what the guest says it received
// (`wire::Report`). A probe burst at PyroWave's top rate opens the session; then each second of
// reports gives the loss and the rate the link delivered. PyroWave while the link carries
// `pyroWaveFloor` with room to spare, its budget a share of what was delivered; below that the
// best hardware codec both ends have, at a smaller share, and a probe now and then (further apart
// each time one fails) to see whether PyroWave fits again. Pure state with the time passed in.
#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <string>

#include "stream_wire.hpp"
#include "video_codec.hpp"

namespace opensu::stream {

/// PyroWave's top budget and the least it is used at, in bits a second.
inline constexpr std::uint64_t pyroWaveTop = 200'000'000;
inline constexpr std::uint64_t pyroWaveFloor = 100'000'000;
/// A hardware codec's top and least rate.
inline constexpr std::uint64_t hardwareTop = 40'000'000;
inline constexpr std::uint64_t hardwareFloor = 2'000'000;
/// The payload of each probe datagram: as big as a full video packet's.
inline constexpr std::size_t probeBytes = wire::videoHeaderBytes + videoPacketBytes;

/// What the host codes at.
struct LinkPlan {
    VideoCodec codec{VideoCodec::PyroWave};
    std::uint64_t bitsPerSecond{0};

    bool operator==(const LinkPlan&) const = default;
};

class LinkRate {
  public:
    /// The codecs the host codes.
    explicit LinkRate(VideoCodecs encoders);

    /// The probe datagrams due now, `nowUs` on the host's clock: their burst and how many, none
    /// outside a burst. The caller sends that many `Probe`s of `probeBytes`.
    struct ProbeDue {
        std::uint32_t burst{0};
        std::uint32_t first{0};
        std::uint32_t count{0};
    };
    [[nodiscard]] ProbeDue probe(std::uint64_t nowUs);
    /// The guest's report, heard at `nowUs`.
    void report(const wire::Report& report, std::uint64_t nowUs);
    /// `codec` cannot code here after all; another is planned.
    void refuse(VideoCodec codec);

    /// What to code at; nothing until the opening probe was heard.
    [[nodiscard]] std::optional<LinkPlan> plan() const noexcept {
        return plan_;
    }
    /// The plan, the link as last measured and the switches, as one line.
    [[nodiscard]] std::string summary() const;

  private:
    struct Burst {
        std::uint32_t number{0};
        std::uint64_t startUs{0};
        std::uint64_t bitsPerSecond{0};
        std::uint64_t durationUs{0};
        std::uint32_t sent{0};
        bool over{false};
        /// Whether its result was taken.
        bool heard{false};
    };
    /// A report as heard.
    struct Heard {
        std::uint64_t atUs{0};
        wire::Report report;
    };

    void startBurst(std::uint64_t nowUs, std::uint64_t bitsPerSecond, std::uint64_t durationUs);
    void probeHeard(Burst& burst, std::uint32_t received, std::uint64_t nowUs);
    /// The plan for a link that carries `capacity` bits a second.
    void choose(double capacity, std::uint64_t nowUs, const char* why);
    void adopt(LinkPlan plan, std::uint64_t nowUs, const char* why);
    /// Judges the last second of reports, outside a probe.
    void judge(std::uint64_t nowUs);
    /// The best hardware codec both ends have, if any.
    [[nodiscard]] std::optional<VideoCodec> hardware() const;

    VideoCodecs encoders_;
    VideoCodecs decoders_{0};
    std::optional<LinkPlan> plan_;
    std::optional<Burst> burst_;
    std::uint32_t bursts_{0};
    std::deque<Heard> heard_;
    /// When the plan last changed, and when the next probe is due on a hardware codec.
    std::uint64_t changedUs_{0};
    std::uint64_t nextProbeUs_{0};
    std::uint64_t probeGapUs_{0};
    std::size_t switches_{0};
    double deliveredBps_{0};
    double loss_{0};
    /// What the link was last found to carry.
    double capacity_{0};
};

} // namespace opensu::stream

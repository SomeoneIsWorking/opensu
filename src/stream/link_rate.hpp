// link_rate — the codec and rate the host codes at, from what the guest says it received
// (`wire::Report`). A probe at PyroWave's top rate opens the session; then each second of reports
// gives the loss and the rate the link delivered. PyroWave while the link carries `pyroWaveFloor`
// with room to spare, its budget a share of what was delivered; below that the best hardware codec
// both ends have, at a smaller share, and now and then (further apart each time one fails) a probe
// to see whether PyroWave fits again: the rate the host sends at, video and probe datagrams
// together, ramps up from the plan's to what PyroWave needs and stops as soon as the guest says
// the link's queue grew or it lost datagrams, before the video does. Pure state with the time
// passed in.
#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <string>

#include "packet_seal.hpp"
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
/// A probe datagram as it crosses the link, its payload sealed: as big as a full video datagram.
inline constexpr std::size_t probeDatagramBytes = probeBytes + sealOverhead;

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

    /// The probe under way at `nowUs` on the host's clock: its burst, the rate the host sends at,
    /// video first and `Probe` datagrams of `probeBytes` filling the rest, and the probe datagrams
    /// sent so far; nothing outside a probe.
    struct Probing {
        std::uint32_t burst{0};
        std::uint64_t bitsPerSecond{0};
        std::uint32_t sent{0};
    };
    [[nodiscard]] std::optional<Probing> probe(std::uint64_t nowUs);
    /// `count` more probe datagrams of `burst` went.
    void probed(std::uint32_t burst, std::uint32_t count);
    /// The guest's report, heard at `nowUs`.
    void report(const wire::Report& report, std::uint64_t nowUs);
    /// `codec` cannot code here after all; another is planned.
    void refuse(VideoCodec codec);

    /// What to code at; nothing until the opening probe was heard.
    [[nodiscard]] std::optional<LinkPlan> plan() const noexcept {
        return plan_;
    }
    /// The guest decodes nothing the host codes, so no picture can come.
    [[nodiscard]] bool noCodec() const noexcept {
        return noCodec_;
    }
    /// The plan, the link as last measured and the switches, as one line.
    [[nodiscard]] std::string summary() const;

  private:
    /// A probe: from `fromBps` it ramps to `toBps` over `rampUs` and holds that `holdUs`. A watched
    /// one stops at the first report of a grown queue or of loss, from its first report on.
    struct Burst {
        std::uint32_t number{0};
        std::uint64_t startUs{0};
        std::uint64_t fromBps{0};
        std::uint64_t toBps{0};
        std::uint64_t rampUs{0};
        std::uint64_t holdUs{0};
        bool watched{false};
        std::uint32_t sent{0};
        bool over{false};
        /// Whether its result was taken.
        bool heard{false};
        /// Its first report, and when it was heard.
        std::optional<wire::Report> first;
        std::uint64_t firstUs{0};
    };
    /// A report as heard.
    struct Heard {
        std::uint64_t atUs{0};
        wire::Report report;
    };

    void startBurst(Burst burst);
    /// The rate `burst` sends at, `elapsedUs` into it.
    [[nodiscard]] static double rateAt(const Burst& burst, std::uint64_t elapsedUs);
    /// Takes a report heard during a watched probe; true when it stopped the probe.
    bool watch(Burst& burst, const wire::Report& report, std::uint64_t nowUs);
    /// A probe that failed: the next one waits twice as long.
    void probeFailed(std::uint64_t nowUs);
    void probeHeard(Burst& burst, const wire::Report& report, std::uint64_t nowUs);
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
    bool noCodec_{false};
    std::optional<Burst> burst_;
    std::uint32_t bursts_{0};
    std::deque<Heard> heard_;
    /// When the plan last changed, and when the next probe is due on a hardware codec.
    std::uint64_t changedUs_{0};
    std::uint64_t nextProbeUs_{0};
    std::uint64_t probeGapUs_{0};
    std::size_t switches_{0};
    /// The judged windows in a row that lost too much.
    int lossyWindows_{0};
    double deliveredBps_{0};
    double loss_{0};
    /// What the link was last found to carry.
    double capacity_{0};
};

} // namespace opensu::stream

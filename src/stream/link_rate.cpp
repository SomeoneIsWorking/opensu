#include "link_rate.hpp"

#include <algorithm>
#include <format>

#include "lucent/log.h"
#include "packet_seal.hpp"

namespace opensu::stream {
namespace {

/// A probe datagram as it crosses the link: its payload sealed.
constexpr std::uint64_t probeDatagramBytes = probeBytes + sealOverhead;
constexpr double microsPerSecond = 1e6;
constexpr double bitsPerByte = 8;

/// The opening probe: a little over PyroWave's top, for 300 ms.
constexpr std::uint64_t openingRate = pyroWaveTop + pyroWaveTop / 10;
constexpr std::uint64_t openingUs = 300'000;
/// A later probe, on a hardware codec: PyroWave's floor with its share and a little room, 200 ms.
constexpr double pyroWaveShare = 0.8;
constexpr double hardwareShare = 0.6;
constexpr auto laterRate = static_cast<std::uint64_t>(pyroWaveFloor / pyroWaveShare * 1.05);
constexpr std::uint64_t laterUs = 200'000;
/// How long the guest's report of a probe may take after its last datagram went.
constexpr std::uint64_t probeAnswerUs = 400'000;
/// The first wait for another probe on a hardware codec, and the longest it grows to.
constexpr std::uint64_t firstProbeGapUs = 10'000'000;
constexpr std::uint64_t longestProbeGapUs = 60'000'000;
/// The reports a judgement spans, and how long a new plan settles before the next one.
constexpr std::uint64_t windowUs = 1'000'000;
constexpr std::uint64_t settleUs = 1'500'000;
/// Datagrams too few to judge a window by.
constexpr std::uint64_t fewestJudged = 30;
/// A probe that lost less than this passed; a window that lost more is over what the link carries,
/// and one that lost less than `quiet` may code more. A hardware codec decodes nothing from a loss
/// to the next key frame, so it stands far less.
constexpr double probeLoss = 0.02;
constexpr double overLoss = 0.05;
constexpr double quiet = 0.01;
constexpr double hardwareOverLoss = 0.01;
constexpr double hardwareQuiet = 0.001;
constexpr double pyroWaveRaise = 1.25;
constexpr double hardwareRaise = 1.15;

double megabits(double bitsPerSecond) {
    return bitsPerSecond / 1e6;
}

} // namespace

LinkRate::LinkRate(VideoCodecs encoders) : encoders_{encoders}, probeGapUs_{firstProbeGapUs} {
}

void LinkRate::startBurst(std::uint64_t nowUs, std::uint64_t bitsPerSecond,
                          std::uint64_t durationUs) {
    burst_ = Burst{.number = ++bursts_,
                   .startUs = nowUs,
                   .bitsPerSecond = bitsPerSecond,
                   .durationUs = durationUs,
                   .sent = 0,
                   .over = false,
                   .heard = false};
    heard_.clear();
}

LinkRate::ProbeDue LinkRate::probe(std::uint64_t nowUs) {
    if (!plan_ && !burst_) {
        startBurst(nowUs, openingRate, openingUs);
    } else if (plan_ && plan_->codec != VideoCodec::PyroWave && (!burst_ || burst_->heard) &&
               nowUs >= nextProbeUs_ && holds(encoders_, VideoCodec::PyroWave) &&
               holds(decoders_, VideoCodec::PyroWave)) {
        startBurst(nowUs, laterRate, laterUs);
    }
    if (!burst_ || burst_->over) {
        return {};
    }
    Burst& burst = *burst_;
    const std::uint64_t total = burst.bitsPerSecond * burst.durationUs /
                                static_cast<std::uint64_t>(microsPerSecond) /
                                (probeDatagramBytes * 8);
    const std::uint64_t elapsed = std::min(nowUs - burst.startUs, burst.durationUs);
    const std::uint64_t due = total * elapsed / burst.durationUs;
    const ProbeDue out{
        .burst = burst.number,
        .first = burst.sent,
        .count = static_cast<std::uint32_t>(due - std::min<std::uint64_t>(due, burst.sent))};
    burst.sent += out.count;
    burst.over = elapsed >= burst.durationUs;
    return out;
}

void LinkRate::report(const wire::Report& report, std::uint64_t nowUs) {
    decoders_ = report.decoders;
    heard_.push_back(Heard{.atUs = nowUs, .report = report});
    while (heard_.size() > 1 && nowUs - heard_.front().atUs > 2 * windowUs) {
        heard_.pop_front();
    }
    if (burst_ && !burst_->heard) {
        if (!burst_->over) {
            return;
        }
        const std::uint64_t endUs = burst_->startUs + burst_->durationUs;
        const bool answered =
            report.probeBurst == burst_->number && report.probeDatagrams >= burst_->sent;
        if (answered || nowUs - endUs >= probeAnswerUs) {
            probeHeard(*burst_, report.probeBurst == burst_->number ? report.probeDatagrams : 0,
                       nowUs);
        }
        return;
    }
    judge(nowUs);
}

void LinkRate::probeHeard(Burst& burst, std::uint32_t received, std::uint64_t nowUs) {
    burst.heard = true;
    heard_.clear();
    const double sent = std::max<double>(burst.sent, 1);
    loss_ = 1.0 - std::min<double>(received, sent) / sent;
    deliveredBps_ = static_cast<double>(received) * probeDatagramBytes * bitsPerByte *
                    microsPerSecond / static_cast<double>(burst.durationUs);
    const bool passed = loss_ < probeLoss;
    lucent::info("stream", "probe {} at {:.0f} Mbit/s: {} of {} came, {:.0f} Mbit/s delivered",
                 burst.number, megabits(static_cast<double>(burst.bitsPerSecond)), received,
                 burst.sent, megabits(deliveredBps_));
    if (!plan_) {
        choose(passed ? std::max(deliveredBps_, static_cast<double>(burst.bitsPerSecond))
                      : deliveredBps_,
               nowUs, "the opening probe");
        return;
    }
    if (passed) {
        probeGapUs_ = firstProbeGapUs;
        choose(static_cast<double>(burst.bitsPerSecond), nowUs, "a probe passed");
        return;
    }
    probeGapUs_ = std::min(2 * probeGapUs_, longestProbeGapUs);
    nextProbeUs_ = nowUs + probeGapUs_;
}

void LinkRate::judge(std::uint64_t nowUs) {
    if (!plan_ || heard_.size() < 2 || nowUs - changedUs_ < settleUs) {
        return;
    }
    const Heard& last = heard_.back();
    const auto from = std::ranges::find_if(heard_, [&last](const Heard& heard) {
        return last.atUs - heard.atUs <= 2 * windowUs;
    });
    if (last.atUs - from->atUs < windowUs) {
        return;
    }
    const std::uint64_t sent = last.report.newest - from->report.newest;
    const std::uint64_t received = last.report.datagrams - from->report.datagrams;
    if (sent < fewestJudged) {
        return;
    }
    const auto spanUs = static_cast<double>(last.atUs - from->atUs);
    loss_ = 1.0 - std::min<double>(static_cast<double>(received), static_cast<double>(sent)) /
                      static_cast<double>(sent);
    deliveredBps_ = static_cast<double>(last.report.bytes - from->report.bytes) * bitsPerByte *
                    microsPerSecond / spanUs;
    heard_.erase(heard_.begin(), heard_.end() - 1);
    const bool pyroWave = plan_->codec == VideoCodec::PyroWave;
    if (loss_ > (pyroWave ? overLoss : hardwareOverLoss)) {
        choose(deliveredBps_, nowUs, "the link lost frames");
        return;
    }
    if (loss_ >= (pyroWave ? quiet : hardwareQuiet)) {
        return;
    }
    LinkPlan raised = *plan_;
    const std::uint64_t top = pyroWave ? pyroWaveTop : hardwareTop;
    if (raised.bitsPerSecond >= top) {
        return;
    }
    raised.bitsPerSecond =
        std::min(top, static_cast<std::uint64_t>(static_cast<double>(raised.bitsPerSecond) *
                                                 (pyroWave ? pyroWaveRaise : hardwareRaise)));
    adopt(raised, nowUs, "the link kept up");
}

std::optional<VideoCodec> LinkRate::hardware() const {
    for (const VideoCodec codec : hardwareCodecs) {
        if (holds(encoders_, codec) && holds(decoders_, codec)) {
            return codec;
        }
    }
    return std::nullopt;
}

void LinkRate::choose(double capacity, std::uint64_t nowUs, const char* why) {
    capacity_ = capacity;
    const bool pyroWave =
        holds(encoders_, VideoCodec::PyroWave) && holds(decoders_, VideoCodec::PyroWave);
    const std::optional<VideoCodec> fallback = hardware();
    const auto share = [capacity](double part, std::uint64_t least, std::uint64_t most) {
        return std::clamp(static_cast<std::uint64_t>(capacity * part), least, most);
    };
    if (pyroWave && (capacity * pyroWaveShare >= static_cast<double>(pyroWaveFloor) || !fallback)) {
        if (!fallback && capacity * pyroWaveShare < static_cast<double>(pyroWaveFloor)) {
            lucent::warn("stream",
                         "the link carries {:.0f} Mbit/s, too little for PyroWave, and "
                         "no hardware codec both ends have",
                         megabits(capacity));
        }
        adopt(LinkPlan{VideoCodec::PyroWave, share(pyroWaveShare, hardwareFloor, pyroWaveTop)},
              nowUs, why);
        return;
    }
    if (fallback) {
        nextProbeUs_ = nowUs + probeGapUs_;
        adopt(LinkPlan{*fallback, share(hardwareShare, hardwareFloor, hardwareTop)}, nowUs, why);
        return;
    }
    lucent::warn("stream", "no video codec both ends have (host {:#x}, guest {:#x})", encoders_,
                 decoders_);
    plan_.reset();
}

void LinkRate::adopt(LinkPlan plan, std::uint64_t nowUs, const char* why) {
    changedUs_ = nowUs;
    heard_.clear();
    if (plan_ == plan) {
        return;
    }
    if (plan_ && plan_->codec != plan.codec) {
        ++switches_;
    }
    lucent::info("stream", "coding with {} at {:.1f} Mbit/s: {}", nameOf(plan.codec),
                 megabits(static_cast<double>(plan.bitsPerSecond)), why);
    plan_ = plan;
}

void LinkRate::refuse(VideoCodec codec) {
    encoders_ = static_cast<VideoCodecs>(encoders_ & ~codecBit(codec));
    choose(capacity_, changedUs_, "a codec could not open");
}

std::string LinkRate::summary() const {
    if (!plan_) {
        return std::format("codec=none probes={}", bursts_);
    }
    return std::format("codec={} rate={:.1f} delivered={:.1f} loss={:.1f}% switches={} probes={}",
                       nameOf(plan_->codec), megabits(static_cast<double>(plan_->bitsPerSecond)),
                       megabits(deliveredBps_), loss_ * 100, switches_, bursts_);
}

} // namespace opensu::stream

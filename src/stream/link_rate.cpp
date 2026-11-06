#include "link_rate.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "lucent/log.h"

namespace opensu::stream {
namespace {

constexpr double microsPerSecond = 1e6;
constexpr double bitsPerByte = 8;

/// The opening probe: a little over PyroWave's top, for 300 ms; nothing is coded yet.
constexpr std::uint64_t openingRate = pyroWaveTop + pyroWaveTop / 10;
constexpr std::uint64_t openingUs = 300'000;
/// A later probe, on a hardware codec: from the plan's rate up to PyroWave's floor with its share
/// and a little room over two seconds, held for 200 ms. Ramping, as BBR's bandwidth probe does,
/// lets the queue grow slowly enough past the link's rate to be heard before it overflows.
constexpr double pyroWaveShare = 0.8;
constexpr double hardwareShare = 0.6;
constexpr auto laterRate = static_cast<std::uint64_t>(pyroWaveFloor / pyroWaveShare * 1.05);
constexpr std::uint64_t laterRampUs = 2'000'000;
constexpr std::uint64_t laterHoldUs = 200'000;
/// A later probe stops when its datagrams met a queue this much longer than its quickest did: well
/// short of a shallow queue, which each video frame's burst already half fills.
constexpr std::uint32_t queueGrownUs = 2'000;
/// How long the guest's report of a probe may take after its last datagram went.
constexpr std::uint64_t probeAnswerUs = 400'000;
/// The first wait for another probe on a hardware codec, and the longest it grows to.
constexpr std::uint64_t firstProbeGapUs = 10'000'000;
constexpr std::uint64_t longestProbeGapUs = 20'000'000;
/// The reports a judgement spans, and how long a new plan settles before the next one.
constexpr std::uint64_t windowUs = 1'000'000;
constexpr std::uint64_t settleUs = 1'500'000;
/// Datagrams too few to judge a window by.
constexpr std::uint64_t fewestJudged = 30;
/// A probe that lost less than this passed; a window that lost more is over what the link carries,
/// and one that lost less than `quiet` may code more. A hardware codec decodes nothing from a loss
/// to the next key frame, so it stands far less, and a later probe stops at its line.
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

/// Of the datagrams the guest heard between two reports, the share lost.
double lostBetween(const wire::Report& from, const wire::Report& to) {
    const std::uint64_t sent = to.newest - from.newest;
    const std::uint64_t received = to.datagrams - from.datagrams;
    if (sent == 0) {
        return 0;
    }
    return 1.0 - std::min<double>(static_cast<double>(received), static_cast<double>(sent)) /
                     static_cast<double>(sent);
}

/// What the link delivered between a report heard at `fromUs` and one heard at `toUs`.
double deliveredBetween(const wire::Report& from, std::uint64_t fromUs, const wire::Report& to,
                        std::uint64_t toUs) {
    if (toUs <= fromUs) {
        return 0;
    }
    return static_cast<double>(to.bytes - from.bytes) * bitsPerByte * microsPerSecond /
           static_cast<double>(toUs - fromUs);
}

} // namespace

LinkRate::LinkRate(VideoCodecs encoders) : encoders_{encoders}, probeGapUs_{firstProbeGapUs} {
}

void LinkRate::startBurst(Burst burst) {
    burst.number = ++bursts_;
    burst_ = burst;
    heard_.clear();
}

double LinkRate::rateAt(const Burst& burst, std::uint64_t elapsedUs) {
    const auto to = static_cast<double>(burst.toBps);
    if (elapsedUs >= burst.rampUs || burst.fromBps >= burst.toBps) {
        return to;
    }
    const auto from = static_cast<double>(std::max<std::uint64_t>(burst.fromBps, 1));
    return from *
           std::pow(to / from, static_cast<double>(elapsedUs) / static_cast<double>(burst.rampUs));
}

std::optional<LinkRate::Probing> LinkRate::probe(std::uint64_t nowUs) {
    if (!plan_ && !burst_) {
        startBurst(Burst{.number = 0,
                         .startUs = nowUs,
                         .fromBps = openingRate,
                         .toBps = openingRate,
                         .rampUs = 0,
                         .holdUs = openingUs,
                         .watched = false,
                         .sent = 0,
                         .over = false,
                         .heard = false,
                         .first = std::nullopt,
                         .firstUs = 0});
    } else if (plan_ && plan_->codec != VideoCodec::PyroWave && (!burst_ || burst_->heard) &&
               nowUs >= nextProbeUs_ && holds(encoders_, VideoCodec::PyroWave) &&
               holds(decoders_, VideoCodec::PyroWave)) {
        startBurst(Burst{.number = 0,
                         .startUs = nowUs,
                         .fromBps = plan_->bitsPerSecond,
                         .toBps = laterRate,
                         .rampUs = laterRampUs,
                         .holdUs = laterHoldUs,
                         .watched = true,
                         .sent = 0,
                         .over = false,
                         .heard = false,
                         .first = std::nullopt,
                         .firstUs = 0});
    }
    if (!burst_ || burst_->over) {
        return std::nullopt;
    }
    Burst& burst = *burst_;
    const std::uint64_t elapsed = nowUs - std::min(nowUs, burst.startUs);
    if (elapsed >= burst.rampUs + burst.holdUs) {
        burst.over = true;
        return std::nullopt;
    }
    return Probing{.burst = burst.number,
                   .bitsPerSecond = static_cast<std::uint64_t>(rateAt(burst, elapsed)),
                   .sent = burst.sent};
}

void LinkRate::probed(std::uint32_t burst, std::uint32_t count) {
    if (burst_ && burst_->number == burst) {
        burst_->sent += count;
    }
}

void LinkRate::report(const wire::Report& report, std::uint64_t nowUs) {
    decoders_ = report.decoders;
    heard_.push_back(Heard{.atUs = nowUs, .report = report});
    while (heard_.size() > 1 && nowUs - heard_.front().atUs > 2 * windowUs) {
        heard_.pop_front();
    }
    if (burst_ && !burst_->heard) {
        Burst& burst = *burst_;
        if (burst.watched && report.probeBurst == burst.number && watch(burst, report, nowUs)) {
            return;
        }
        if (!burst.over) {
            return;
        }
        const std::uint64_t endUs = burst.startUs + burst.rampUs + burst.holdUs;
        const bool answered =
            report.probeBurst == burst.number && report.probeDatagrams >= burst.sent;
        if (answered || nowUs - std::min(nowUs, endUs) >= probeAnswerUs) {
            probeHeard(burst, report, nowUs);
        }
        return;
    }
    judge(nowUs);
}

bool LinkRate::watch(Burst& burst, const wire::Report& report, std::uint64_t nowUs) {
    if (!burst.first) {
        burst.first = report;
        burst.firstUs = nowUs;
        return false;
    }
    const double lost = lostBetween(*burst.first, report);
    const bool lossy =
        report.newest - burst.first->newest >= fewestJudged && lost > hardwareOverLoss;
    if (report.probeQueueUs < queueGrownUs && !lossy) {
        return false;
    }
    burst.over = true;
    burst.heard = true;
    heard_.clear();
    loss_ = lost;
    deliveredBps_ = deliveredBetween(*burst.first, burst.firstUs, report, nowUs);
    lucent::info("stream",
                 "probe {} stopped at {:.0f} Mbit/s: its queue grew {:.1f} ms, {:.1f}% lost, "
                 "{:.0f} Mbit/s delivered",
                 burst.number, megabits(rateAt(burst, nowUs - std::min(nowUs, burst.startUs))),
                 static_cast<double>(report.probeQueueUs) / 1000, lost * 100,
                 megabits(deliveredBps_));
    probeFailed(nowUs);
    return true;
}

void LinkRate::probeFailed(std::uint64_t nowUs) {
    probeGapUs_ = std::min(2 * probeGapUs_, longestProbeGapUs);
    nextProbeUs_ = nowUs + probeGapUs_;
}

void LinkRate::probeHeard(Burst& burst, const wire::Report& report, std::uint64_t nowUs) {
    burst.heard = true;
    heard_.clear();
    const std::uint32_t received = report.probeBurst == burst.number ? report.probeDatagrams : 0;
    const double sent = std::max<double>(burst.sent, 1);
    loss_ = 1.0 - std::min<double>(received, sent) / sent;
    // The opening probe is all that is sent; a later one shares the link with the video.
    deliveredBps_ = burst.first
                        ? deliveredBetween(*burst.first, burst.firstUs, report, nowUs)
                        : static_cast<double>(received) * probeDatagramBytes * bitsPerByte *
                              microsPerSecond / static_cast<double>(burst.holdUs + burst.rampUs);
    const bool passed = loss_ < probeLoss;
    lucent::info("stream", "probe {} up to {:.0f} Mbit/s: {} of {} came, {:.0f} Mbit/s delivered",
                 burst.number, megabits(static_cast<double>(burst.toBps)), received, burst.sent,
                 megabits(deliveredBps_));
    if (!plan_) {
        choose(passed ? std::max(deliveredBps_, static_cast<double>(burst.toBps)) : deliveredBps_,
               nowUs, "the opening probe");
        return;
    }
    if (passed) {
        probeGapUs_ = firstProbeGapUs;
        choose(static_cast<double>(burst.toBps), nowUs, "a probe passed");
        return;
    }
    probeFailed(nowUs);
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
    if (last.report.newest - from->report.newest < fewestJudged) {
        return;
    }
    loss_ = lostBetween(from->report, last.report);
    deliveredBps_ = deliveredBetween(from->report, from->atUs, last.report, last.atUs);
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
    noCodec_ = true;
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

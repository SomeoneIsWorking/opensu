// The fork's pacer fed the ready time `ReadyNeed` plans for, over 64 arrival phases of eight paces,
// each real frame's first generated frame ready after a time drawn from flow's measured ones (and
// twice them, a slower GPU), each further one 1.2 ms after it: no generated frame misses its
// vblank, and the held delay steps at most once a minute more than with a fixed need of a refresh.
//
//   frame_gen_ready_need_replay_test <ready times, ns per line>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <random>
#include <utility>
#include <vector>

#include "check.hpp"
#include "frame_gen/ready_need.hpp"
#include "framegen/FramePacer.h"
#include "support/test_main.hpp"

namespace {

using opensu::frame_gen::ReadyNeed;
using opensu::test::expect;

constexpr std::uint64_t refreshNs = 16'666'667;
constexpr int phases = 64;
constexpr int realFrames = 1800;
constexpr std::uint64_t generateNs = 1'200'000;
constexpr std::uint64_t startNs = 1'000'000'000;

struct Pace {
    const char* name;
    framegen::PacingSettings_t settings;
    double baseFps;
    std::uint64_t jitterNs;
};

struct Run {
    std::uint64_t generated{0};
    std::uint64_t missed{0};
    std::uint64_t steps{0};
    std::uint64_t real{0};
    double realDelayNs{0.0};
    double minutes{0.0};
};

/// One phase: `need` plans every real frame, or a fixed refresh when it is empty.
Run replay(const Pace& pace, std::uint64_t phaseNs, const std::vector<std::uint64_t>& times,
           std::uint32_t scale, bool measured, std::uint32_t seed) {
    framegen::CFramePacer pacer;
    pacer.Configure(pace.settings);
    ReadyNeed need;
    std::mt19937 draw{seed};
    std::uniform_int_distribution<std::size_t> pick{0, times.size() - 1};
    std::vector<std::uint64_t> arrivals;
    std::vector<std::uint64_t> ready;
    // Measured ready times, each known once its frame is done: (done at, took).
    std::vector<std::pair<std::uint64_t, std::uint64_t>> pending;
    Run run;
    std::uint64_t now = 0;
    const auto isReady = [&](const framegen::PacedOutput_t& output) {
        return !output.oGenerated || now >= arrivals[output.ulInterval] + ready[output.ulInterval] +
                                                (std::uint64_t{*output.oGenerated} * generateNs);
    };
    std::optional<std::int64_t> lastRealDelay;
    std::uint64_t nextVBlank = startNs;
    const auto vblanksUntil = [&](std::uint64_t until) {
        for (; nextVBlank <= until; nextVBlank += refreshNs) {
            now = nextVBlank;
            const framegen::PacedOutput_t* head = pacer.PeekNext();
            const bool dueUnready = head != nullptr && head->oGenerated &&
                                    head->ulDesiredPresentNs <= now + (refreshNs / 2) &&
                                    !isReady(*head);
            const std::optional<framegen::PacedOutput_t> shown =
                pacer.OnVBlank(now, refreshNs, isReady);
            if (dueUnready && !shown) {
                ++run.missed;
            }
            if (!shown) {
                continue;
            }
            if (shown->oGenerated) {
                ++run.generated;
                continue;
            }
            const auto delay =
                static_cast<std::int64_t>(now) -
                static_cast<std::int64_t>(
                    arrivals[std::min<std::size_t>(shown->ulRealFrame, arrivals.size() - 1)]);
            if (lastRealDelay && std::abs(delay - *lastRealDelay) > std::int64_t{refreshNs / 2}) {
                ++run.steps;
            }
            ++run.real;
            run.realDelayNs += static_cast<double>(delay);
            lastRealDelay = delay;
        }
    };
    std::uint32_t jitter = 12345;
    const double intervalNs = 1.0e9 / pace.baseFps;
    for (int frame = 0; frame < realFrames; ++frame) {
        jitter = (jitter * 1664525U) + 1013904223U;
        const std::uint64_t arrival = startNs + phaseNs +
                                      static_cast<std::uint64_t>(frame * intervalNs) +
                                      (pace.jitterNs != 0 ? jitter % pace.jitterNs : 0);
        vblanksUntil(arrival);
        if (frame > 2) {
            const auto oldest = static_cast<std::uint64_t>(frame - 2);
            pacer.DropOlderThan(oldest, oldest);
        }
        std::erase_if(pending, [&](const auto& sample) {
            if (sample.first > arrival) {
                return false;
            }
            need.add(sample.second, refreshNs);
            return true;
        });
        pacer.SetReadyNeededNs(measured ? need.needNs(refreshNs) : refreshNs);
        arrivals.push_back(arrival);
        ready.push_back(times[pick(draw)] * scale);
        const framegen::IntervalPlan_t plan =
            pacer.OnRealFrame(static_cast<std::uint64_t>(frame), arrival, arrival, refreshNs);
        if (!plan.flTimestamps.empty()) {
            pending.emplace_back(arrival + ready.back(), ready.back());
        }
    }
    vblanksUntil(arrivals.back() + (4 * refreshNs));
    run.minutes = static_cast<double>(arrivals.back() - startNs) / 60.0e9;
    return run;
}

} // namespace

int main(int argc, char** argv) try {
    expect(argc == 2, "usage: frame_gen_ready_need_replay_test <ready times>");
    std::vector<std::uint64_t> times;
    std::ifstream in{argv[1]};
    for (std::uint64_t value = 0; in >> value;) {
        times.push_back(value);
    }
    expect(!times.empty(), "the ready times are read");
    using framegen::PacingMode;
    const Pace paces[] = {
        {"adaptive 50 fps to 60", {.eMode = PacingMode::Adaptive, .uMaxGenerated = 3}, 50.0, 0},
        {"adaptive 50 fps to 60, 3 ms jitter",
         {.eMode = PacingMode::Adaptive, .uMaxGenerated = 3},
         50.0,
         3'000'000},
        {"adaptive 45 fps to 60", {.eMode = PacingMode::Adaptive, .uMaxGenerated = 3}, 45.0, 0},
        {"adaptive 30 fps to 60",
         {.eMode = PacingMode::Adaptive, .uMaxGenerated = 3},
         30.0,
         1'000'000},
        {"adaptive 24 fps to 60", {.eMode = PacingMode::Adaptive, .uMaxGenerated = 3}, 24.0, 0},
        {"fixed 2x at 30 fps", {.eMode = PacingMode::Fixed, .flRatio = 2.0}, 30.0, 0},
        {"fixed 2x at 30 fps, 3 ms jitter",
         {.eMode = PacingMode::Fixed, .flRatio = 2.0},
         30.0,
         3'000'000},
        {"fixed 3x at 20 fps", {.eMode = PacingMode::Fixed, .flRatio = 3.0}, 20.0, 0},
    };
    bool passed = true;
    for (const std::uint32_t scale : {1U, 2U}) {
        for (const Pace& pace : paces) {
            std::uint64_t generated = 0;
            std::uint64_t worstMissed = 0;
            double steps = 0.0;
            double fixedSteps = 0.0;
            double minutes = 0.0;
            double delay = 0.0;
            double fixedDelay = 0.0;
            for (int phase = 0; phase < phases; ++phase) {
                const std::uint64_t phaseNs =
                    static_cast<std::uint64_t>(phase) * refreshNs / phases;
                const auto seed = static_cast<std::uint32_t>(777 + phase);
                const Run measured = replay(pace, phaseNs, times, scale, true, seed);
                const Run fixed = replay(pace, phaseNs, times, scale, false, seed);
                generated += measured.generated;
                worstMissed = std::max(worstMissed, measured.missed);
                steps += static_cast<double>(measured.steps);
                fixedSteps += static_cast<double>(fixed.steps);
                minutes += measured.minutes;
                delay += measured.realDelayNs / static_cast<double>(measured.real) / phases;
                fixedDelay += fixed.realDelayNs / static_cast<double>(fixed.real) / phases;
            }
            // A measured need may take one more step a minute than a fixed one, on its way down.
            const bool ok = generated > 0 && worstMissed == 0 && steps <= fixedSteps + minutes;
            std::printf(
                "ready times x%u, %s: %llu generated over %d phases, at most %llu missed in "
                "a phase; held delay steps %.2f a minute (%.2f with a refresh's need); real "
                "frames shown %.1f ms after arriving (%.1f)%s\n",
                scale, pace.name, static_cast<unsigned long long>(generated), phases,
                static_cast<unsigned long long>(worstMissed), steps / minutes, fixedSteps / minutes,
                delay / 1.0e6, fixedDelay / 1.0e6, ok ? "" : "  FAIL");
            passed = passed && ok;
        }
    }
    expect(passed, "every phase shows every generated frame, the held delay as steady");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

// The resampler's rate control: within ±0.5 % of nominal, pushing the ring's fill toward its target
// against a device clock that runs fast or slow.
#include "audio_sink.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::retro::AudioSink;
using opensu::test::expect;
using opensu::test::near;

constexpr int outputRate = 48000;
constexpr std::size_t target = 1920;
constexpr double coreRate = 32768.0;

void push(AudioSink& sink, std::size_t frames) {
    const std::vector<std::int16_t> samples(frames * 2, 1000);
    sink.push(samples);
}

void ratioBounds() {
    AudioSink sink{outputRate, target};
    sink.setCoreRate(coreRate);
    const double nominal = outputRate / coreRate;
    near(sink.nominalRatio(), nominal, "the nominal ratio is output over core rate", 1e-9);
    near(sink.ratio(), nominal * (1.0 + AudioSink::maxAdjustment), "empty: the most output", 1e-9);
    push(sink, 1000);
    const double belowTarget = sink.ratio();
    expect(belowTarget > nominal && belowTarget < nominal * 1.005, "below target: a little more");
    push(sink, 2000);
    const double aboveTarget = sink.ratio();
    expect(aboveTarget < nominal && aboveTarget >= nominal * 0.995, "above target: a little less");
    push(sink, 20000);
    near(sink.ratio(), nominal * (1.0 - AudioSink::maxAdjustment), "full: the least output", 1e-9);
}

/// A device `skew` faster than nominal, pulling 1024 frames whenever it has played them.
void holdsFill(double skew) {
    AudioSink sink{outputRate, target};
    sink.setCoreRate(coreRate);
    const double fps = 59.7275;
    const double coreFramesPerFrame = coreRate / fps;
    const double deviceFramesPerFrame = outputRate * (1.0 + skew) / fps;
    std::vector<std::int16_t> chunk(std::size_t{1024} * 2);
    double owedCore = 0.0;
    double owedDevice = 0.0;
    push(sink, static_cast<std::size_t>(target / sink.nominalRatio()));
    std::size_t lowest = SIZE_MAX;
    std::size_t highest = 0;
    for (int frame = 0; frame < 60 * 180; ++frame) {
        owedCore += coreFramesPerFrame;
        const auto frames = static_cast<std::size_t>(owedCore);
        owedCore -= static_cast<double>(frames);
        push(sink, frames);
        owedDevice += deviceFramesPerFrame;
        while (owedDevice >= 1024.0) {
            sink.pull(chunk);
            owedDevice -= 1024.0;
        }
        if (frame > 60 * 60) {
            lowest = std::min(lowest, sink.fill());
            highest = std::max(highest, sink.fill());
        }
    }
    std::printf("skew %+.3f%%: fill %zu..%zu, %zu frames missed, %zu dropped\n", skew * 100, lowest,
                highest, sink.underrunFrames(), sink.overrunFrames());
    expect(sink.underrunFrames() == 0, "a skewed device never runs dry");
    expect(sink.overrunFrames() == 0, "a skewed device never overflows the ring");
    expect(lowest > 0 && highest < target * 2, "the fill settles around its target");
}

void underrun() {
    AudioSink sink{outputRate, target};
    sink.setCoreRate(static_cast<double>(outputRate));
    push(sink, 10);
    std::vector<std::int16_t> out(std::size_t{40} * 2, 7);
    sink.pull(out);
    expect(out[out.size() - 1] == 0 && out[out.size() - 2] == 0, "a dry ring plays silence");
    expect(sink.underrunFrames() >= 29, "the missing frames are counted");
    expect(sink.fill() == 0, "the ring is empty after the pull");
}

void discard() {
    AudioSink sink{outputRate, target};
    sink.setCoreRate(static_cast<double>(outputRate));
    push(sink, 100);
    const std::size_t before = sink.fill();
    sink.discard(40);
    expect(sink.fill() == before - 40, "discarding drains as playing does");
}

void tone() {
    // A constant input resamples to the same constant once the first frame has been passed.
    AudioSink sink{outputRate, target};
    sink.setCoreRate(coreRate);
    push(sink, 400);
    std::vector<std::int16_t> out(std::size_t{400} * 2);
    sink.pull(out);
    expect(out[out.size() - 1] == 1000 && out[out.size() - 2] == 1000,
           "resampling keeps a constant level");
}

} // namespace

int main() try {
    ratioBounds();
    holdsFill(0.003);
    holdsFill(-0.003);
    holdsFill(0.0);
    underrun();
    discard();
    tone();
    std::printf("audio_sink: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

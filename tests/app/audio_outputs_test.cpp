// The audio outputs through the volume owner: the list, the default, choosing, and why there are
// none.
#include <cstdio>

#include "audio_outputs.hpp"
#include "host_fakes.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;

void listsAndChooses() {
    auto mixer = std::make_unique<test::FakeMixer>();
    audio::SystemVolume volume{std::move(mixer)};
    app::AudioOutputs outputs{volume};
    expect(outputs.sinks().empty() && outputs.current() == nullptr, "nothing is read until asked");
    outputs.refresh();
    expect(outputs.sinks().size() == 2 && outputs.current()->name == "Speakers", "the default");
    expect(outputs.choose("2").empty() && outputs.current()->name == "Headphones",
           "choosing makes it the default");
    expect(outputs.unavailable().empty(), "a mixer lists outputs");
}

void withoutAMixerSaysWhy() {
    audio::SystemVolume volume{nullptr};
    const app::AudioOutputs outputs{volume};
    expect(!outputs.unavailable().empty(), "no mixer is said");
}

} // namespace

int main() try {
    listsAndChooses();
    withoutAMixerSaysWhy();
    std::printf("audio_outputs: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

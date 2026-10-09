// The audio outputs through the volume owner: the list, the default, choosing and cycling.
#include <cstdio>

#include "audio_outputs.hpp"
#include "host_fakes.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;

void listsChoosesAndCycles() {
    auto mixer = std::make_unique<test::FakeMixer>();
    audio::SystemVolume volume{std::move(mixer)};
    app::AudioOutputs outputs{volume};
    expect(outputs.sinks().empty() && outputs.current() == nullptr, "nothing is read until asked");
    outputs.refresh();
    expect(outputs.sinks().size() == 2 && outputs.current()->name == "Speakers", "the default");
    expect(outputs.choose("2").empty() && outputs.current()->name == "Headphones",
           "choosing makes it the default");
    expect(outputs.cycle(0).empty() && outputs.current()->name == "Speakers", "A wraps around");
    expect(outputs.cycle(-1).empty() && outputs.current()->name == "Headphones", "Left steps back");
}

void oneOutputHasNoOtherToCycleTo() {
    auto mixer = std::make_unique<test::FakeMixer>();
    mixer->outputs.pop_back();
    audio::SystemVolume volume{std::move(mixer)};
    app::AudioOutputs outputs{volume};
    outputs.refresh();
    expect(!outputs.cycle(1).empty(), "a single output is said");
}

} // namespace

int main() {
    listsChoosesAndCycles();
    oneOutputHasNoOtherToCycleTo();
    std::printf("audio_outputs: all checks passed\n");
    return 0;
}

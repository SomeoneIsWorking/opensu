// The UI sounds' file names and iiSU's repeat rule.
#include "debounce.hpp"
#include "effect.hpp"

#include <cstdio>
#include <set>
#include <string>

#include "ui/check.hpp"

namespace {

using opensu::audio::Debounce;
using opensu::audio::Effect;
using opensu::test::expect;
using std::chrono::milliseconds;

void filesRoundTrip() {
    std::set<std::string> seen;
    for (const Effect effect : opensu::audio::allEffects) {
        const std::string_view file = opensu::audio::assetFile(effect);
        expect(file.ends_with(".wav") ||
                   (file.starts_with("domino_icons") && file.ends_with(".ogg")),
               "every effect is a WAV, or a domino cue's OGG");
        expect(opensu::audio::effectOfFile(file) == effect, "a file names its effect back");
        expect(seen.insert(std::string{file}).second, "no two effects share a file");
    }
    expect(opensu::audio::assetFile(Effect::EnterConsolesApps) == "Enter ConsolesApps.wav",
           "the APK's name keeps its space");
    expect(!opensu::audio::effectOfFile("Friends Tab.wav"), "an effect opensu lacks is nothing");
}

void dominoCueSizes() {
    // xp8.a: 1 and 2 have their own cues, 3 to 5, 6 to 11 and 12 or more share one each.
    expect(!opensu::audio::dominoFor(0), "no tiles, no cue: domino_icons.ogg is unreachable");
    expect(opensu::audio::dominoFor(1) == Effect::DominoOne, "one tile");
    expect(opensu::audio::dominoFor(2) == Effect::DominoTwo, "two tiles");
    for (const std::size_t tiles : {3U, 4U, 5U}) {
        expect(opensu::audio::dominoFor(tiles) == Effect::DominoThreeToFive, "three to five");
    }
    for (const std::size_t tiles : {6U, 8U, 11U}) {
        expect(opensu::audio::dominoFor(tiles) == Effect::DominoSixToEleven, "six to eleven");
    }
    for (const std::size_t tiles : {12U, 13U, 100U}) {
        expect(opensu::audio::dominoFor(tiles) == Effect::DominoTwelvePlus, "twelve or more");
    }
    expect(opensu::audio::assetFile(Effect::DominoThreeToFive) == "domino_icons_0_5.ogg",
           "the 3 to 5 cue is iiSU's _0_5 file");
}

void consoleEffectsDebounce() {
    const Debounce::Clock::time_point start{};
    Debounce debounce;
    expect(debounce.admit(Effect::EnterConsolesApps, start), "the first plays");
    expect(!debounce.admit(Effect::EnterConsolesApps, start + milliseconds{90}),
           "a repeat within 91 ms is dropped");
    expect(!debounce.admit(Effect::EnterConsolesApps, start + milliseconds{90}),
           "a dropped one does not restart the window");
    expect(debounce.admit(Effect::EnterConsolesApps, start + milliseconds{91}),
           "91 ms after the last played one plays");
    expect(debounce.admit(Effect::DominoTwo, start) &&
               !debounce.admit(Effect::DominoTwo, start + milliseconds{90}) &&
               debounce.admit(Effect::DominoTwo, start + milliseconds{91}),
           "every domino cue debounces at 91 ms");
    expect(debounce.admit(Effect::ExitConsolesApps, start + milliseconds{92}),
           "another effect has its own window");
    expect(!debounce.admit(Effect::ExitConsolesApps, start + milliseconds{100}),
           "Exit debounces too");
}

void otherEffectsDoNot() {
    const Debounce::Clock::time_point start{};
    Debounce debounce;
    for (const Effect effect : {Effect::Navigation, Effect::Open, Effect::Close,
                                Effect::OpenContextMenu, Effect::OpenAppRom}) {
        expect(debounce.admit(effect, start) && debounce.admit(effect, start),
               "an effect without a debounce plays every time");
    }
}

} // namespace

int main() {
    filesRoundTrip();
    dominoCueSizes();
    consoleEffectsDebounce();
    otherEffectsDoNot();
    std::printf("effect: all checks passed\n");
    return 0;
}

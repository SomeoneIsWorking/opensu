// The UI sounds' file names and iiSU's repeat rule.
#include "debounce.hpp"
#include "effect.hpp"

#include <cstdio>
#include <set>
#include <string>

#include "ui/check.hpp"

namespace {

using iideck::audio::Debounce;
using iideck::audio::Effect;
using iideck::test::expect;
using std::chrono::milliseconds;

void filesRoundTrip() {
    std::set<std::string> seen;
    for (const Effect effect : iideck::audio::allEffects) {
        const std::string_view file = iideck::audio::assetFile(effect);
        expect(file.ends_with(".wav"), "every effect is a WAV");
        expect(iideck::audio::effectOfFile(file) == effect, "a file names its effect back");
        expect(seen.insert(std::string{file}).second, "no two effects share a file");
    }
    expect(iideck::audio::assetFile(Effect::EnterConsolesApps) == "Enter ConsolesApps.wav",
           "the APK's name keeps its space");
    expect(!iideck::audio::effectOfFile("Friends Tab.wav"), "an effect iideck lacks is nothing");
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
    consoleEffectsDebounce();
    otherEffectsDoNot();
    std::printf("effect: all checks passed\n");
    return 0;
}

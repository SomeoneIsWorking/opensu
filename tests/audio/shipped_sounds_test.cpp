// openSU's own UI sounds as shipped in assets/sounds/<set>/: every set has every effect, and each
// decodes to a short clip.
#include "sdl_sound_player.hpp"

#include <cstdio>
#include <filesystem>
#include <string_view>

#include "effect.hpp"
#include "settings/sound_set.hpp"
#include "sound_clip.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::audio::Effect;
using opensu::settings::SoundSet;
using opensu::test::expect;
using opensu::test::need;

fs::path soundsDirectory() {
    return fs::path{OPENSU_ASSETS_DIR} / opensu::audio::shippedDirectory;
}

void everyEffectLoads(std::string_view set) {
    opensu::audio::SdlSoundPlayer player;
    expect(player.loadSet(soundsDirectory(), set) == opensu::audio::allEffects.size(),
           "a shipped set covers every effect");
    for (const Effect effect : opensu::audio::allEffects) {
        expect(player.loaded(effect), "each effect has its clip");
    }
}

void clipsAreShort(std::string_view set) {
    for (const Effect effect : opensu::audio::allEffects) {
        const auto clip = need(opensu::audio::decodeSound(soundsDirectory() / set /
                                                          opensu::audio::shippedFile(effect)),
                               "a shipped sound decodes");
        expect(clip.spec.freq == 48000, "the set is 48 kHz");
        expect(clip.frames() > 0 && clip.frames() < static_cast<std::size_t>(clip.spec.freq),
               "a UI sound is under a second");
    }
}

} // namespace

int main() try {
    for (const SoundSet set : opensu::settings::allSoundSets) {
        if (set == SoundSet::Iisu) {
            continue;
        }
        everyEffectLoads(opensu::settings::key(set));
        clipsAreShort(opensu::settings::key(set));
    }
    std::printf("shipped_sounds: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

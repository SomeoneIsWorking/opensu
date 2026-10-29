// openSU's own UI sounds as shipped in assets/sounds: every effect has one, and it decodes to a
// short clip.
#include "sdl_sound_player.hpp"

#include <cstdio>
#include <filesystem>

#include "effect.hpp"
#include "sound_clip.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::audio::Effect;
using opensu::test::expect;
using opensu::test::need;

fs::path setDirectory() {
    return fs::path{OPENSU_ASSETS_DIR} / opensu::audio::shippedDirectory;
}

void everyEffectLoads() {
    opensu::audio::SdlSoundPlayer player;
    expect(player.loadSet(setDirectory()) == opensu::audio::allEffects.size(),
           "the shipped set covers every effect");
    for (const Effect effect : opensu::audio::allEffects) {
        expect(player.loaded(effect), "each effect has its clip");
    }
}

void clipsAreShort() {
    for (const Effect effect : opensu::audio::allEffects) {
        const auto clip =
            need(opensu::audio::decodeSound(setDirectory() / opensu::audio::shippedFile(effect)),
                 "a shipped sound decodes");
        expect(clip.spec.freq == 48000, "the set is 48 kHz");
        expect(clip.frames() > 0 && clip.frames() < static_cast<std::size_t>(clip.spec.freq),
               "a UI sound is under a second");
    }
}

} // namespace

int main() try {
    everyEffectLoads();
    clipsAreShort();
    std::printf("shipped_sounds: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

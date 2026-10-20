// Which APK files opensu keeps and where the APK has them.
#include "iisu_assets.hpp"

#include <cstdio>

#include "ui/check.hpp"

namespace {

using opensu::artwork::ApkAsset;
using opensu::audio::Effect;
using opensu::test::expect;

void sounds() {
    const ApkAsset navigation = opensu::artwork::soundAsset(Effect::Navigation);
    expect(navigation.file == "Navigation.wav" && navigation.entry == "assets/Navigation.wav",
           "a sound is its file under the APK's assets");
    expect(opensu::artwork::soundAsset(Effect::DominoThreeToFive).entry ==
               "assets/domino_icons_0_5.ogg",
           "a domino cue is an OGG there");
    expect(opensu::artwork::allApkAssets().size() == opensu::audio::allEffects.size(),
           "every effect's file is kept, and nothing else");
}

} // namespace

int main() {
    sounds();
    std::printf("iisu_assets: all checks passed\n");
    return 0;
}

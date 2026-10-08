// Which APK files iideck keeps, where the store puts them and where the APK has them.
#include "iisu_assets.hpp"

#include <cstdio>
#include <set>
#include <string>

#include "ui/check.hpp"

namespace {

using iideck::artwork::allNavIcons;
using iideck::artwork::ApkAsset;
using iideck::artwork::AssetKind;
using iideck::audio::Effect;
using iideck::test::expect;

void sounds() {
    const ApkAsset navigation = iideck::artwork::soundAsset(Effect::Navigation);
    expect(navigation.kind == AssetKind::Sound && navigation.file == "Navigation.wav" &&
               navigation.entry == "assets/Navigation.wav",
           "a sound is its file under the APK's assets");
    expect(iideck::artwork::soundAsset(Effect::DominoThreeToFive).entry ==
               "assets/domino_icons_0_5.ogg",
           "a domino cue is an OGG there");
    expect(iideck::artwork::folder(AssetKind::Sound) == "sound" &&
               iideck::artwork::folder(AssetKind::NavIcon) == "nav",
           "each kind has a folder of its own");
}

void navIcons() {
    std::set<std::string> files;
    std::set<std::string> entries;
    for (const iideck::artwork::NavIcon icon : allNavIcons) {
        const ApkAsset asset = iideck::artwork::navAsset(icon);
        expect(asset.kind == AssetKind::NavIcon && asset.file.ends_with(".png") &&
                   asset.entry.starts_with("res/") && asset.entry.ends_with(".png"),
               "a dock icon is a PNG resource");
        expect(files.insert(asset.file).second && entries.insert(asset.entry).second,
               "no two icons share a file or an entry");
        expect(iideck::artwork::navIconOfFile(asset.file) == icon, "a file names its icon back");
    }
    expect(files.size() == 4, "home and Library, each normal and selected");
    expect(!iideck::artwork::navIconOfFile("friends.png"), "an icon iideck lacks is nothing");
    expect(iideck::artwork::navAsset({iideck::library::Section::Library, true}).entry ==
               "res/tO.png",
           "Library's selected icon is iiSU's rom_selected, as the pinned APK names it");
}

} // namespace

int main() {
    sounds();
    navIcons();
    std::printf("iisu_assets: all checks passed\n");
    return 0;
}

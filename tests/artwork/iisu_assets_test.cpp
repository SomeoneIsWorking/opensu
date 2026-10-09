// Which APK files opensu keeps, where the store puts them and where the APK has them.
#include "iisu_assets.hpp"

#include <cstdio>
#include <set>
#include <string>

#include "ui/check.hpp"

namespace {

using opensu::artwork::allNavIcons;
using opensu::artwork::ApkAsset;
using opensu::artwork::AssetKind;
using opensu::audio::Effect;
using opensu::test::expect;

void sounds() {
    const ApkAsset navigation = opensu::artwork::soundAsset(Effect::Navigation);
    expect(navigation.kind == AssetKind::Sound && navigation.file == "Navigation.wav" &&
               navigation.entry == "assets/Navigation.wav",
           "a sound is its file under the APK's assets");
    expect(opensu::artwork::soundAsset(Effect::DominoThreeToFive).entry ==
               "assets/domino_icons_0_5.ogg",
           "a domino cue is an OGG there");
    expect(opensu::artwork::folder(AssetKind::Sound) == "sound" &&
               opensu::artwork::folder(AssetKind::NavIcon) == "nav",
           "each kind has a folder of its own");
}

void navIcons() {
    std::set<std::string> files;
    std::set<std::string> entries;
    for (const opensu::artwork::NavIcon icon : allNavIcons) {
        const ApkAsset asset = opensu::artwork::navAsset(icon);
        expect(asset.kind == AssetKind::NavIcon && asset.file.ends_with(".png") &&
                   asset.entry.starts_with("res/") && asset.entry.ends_with(".png"),
               "a dock icon is a PNG resource");
        expect(files.insert(asset.file).second && entries.insert(asset.entry).second,
               "no two icons share a file or an entry");
        expect(opensu::artwork::navIconOfFile(asset.file) == icon, "a file names its icon back");
    }
    expect(files.size() == 4, "home and Library, each normal and selected");
    expect(!opensu::artwork::navIconOfFile("friends.png"), "an icon opensu lacks is nothing");
    expect(opensu::artwork::navAsset({opensu::library::Section::Library, true}).entry ==
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

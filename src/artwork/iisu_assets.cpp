#include "iisu_assets.hpp"

#include <algorithm>

namespace opensu::artwork {
namespace {

constexpr std::string_view soundDirectory = "assets/";

/// A dock icon's store file and its entry in iiSU 0.0.7.4's APK (iiSU `nl2.I`: home, rom and the
/// `_selected` pair of each; `drawable/*.png` named by CRC-32 and size in the APK's directory).
struct NavEntry {
    NavIcon icon;
    std::string_view file;
    std::string_view entry;
};

constexpr std::array navEntries{
    NavEntry{{library::Section::Home, false}, "home.png", "res/TG.png"},
    NavEntry{{library::Section::Home, true}, "home_selected.png", "res/dp.png"},
    NavEntry{{library::Section::Library, false}, "roms.png", "res/TQ.png"},
    NavEntry{{library::Section::Library, true}, "roms_selected.png", "res/tO.png"},
};

} // namespace

std::string_view folder(AssetKind kind) noexcept {
    switch (kind) {
    case AssetKind::Sound:
        return "sound";
    case AssetKind::NavIcon:
        return "nav";
    }
    return {};
}

ApkAsset soundAsset(audio::Effect effect) {
    const std::string file{audio::assetFile(effect)};
    return ApkAsset{AssetKind::Sound, file, std::string{soundDirectory} + file};
}

ApkAsset navAsset(NavIcon icon) {
    const NavEntry& found = *std::ranges::find(navEntries, icon, &NavEntry::icon);
    return ApkAsset{AssetKind::NavIcon, std::string{found.file}, std::string{found.entry}};
}

std::optional<NavIcon> navIconOfFile(std::string_view file) noexcept {
    const auto found = std::ranges::find(navEntries, file, &NavEntry::file);
    return found == navEntries.end() ? std::nullopt : std::optional{found->icon};
}

} // namespace opensu::artwork

#include "iisu_assets.hpp"

#include <string_view>

namespace opensu::artwork {
namespace {

constexpr std::string_view soundDirectory = "assets/";

} // namespace

ApkAsset soundAsset(audio::Effect effect) {
    const std::string file{audio::assetFile(effect)};
    return ApkAsset{file, std::string{soundDirectory} + file};
}

std::vector<ApkAsset> allApkAssets() {
    std::vector<ApkAsset> assets;
    assets.reserve(audio::allEffects.size());
    for (const audio::Effect effect : audio::allEffects) {
        assets.push_back(soundAsset(effect));
    }
    return assets;
}

} // namespace opensu::artwork

// iisu_assets — the files opensu takes out of iiSU's APK as they are and keeps in the artwork
// store: its UI sounds. One description of such a file serves the store, which names where it is
// kept, and the fetcher, which names where it is in the APK.
#pragma once

#include <string>
#include <vector>

#include "audio/effect.hpp"

namespace opensu::artwork {

/// One file of the APK, kept whole.
struct ApkAsset {
    /// The store's file name, and what the shell is told it by.
    std::string file;
    /// The file's entry in the APK.
    std::string entry;

    bool operator==(const ApkAsset&) const = default;
};

/// An effect's WAV or OGG in the APK's `assets/`.
[[nodiscard]] ApkAsset soundAsset(audio::Effect effect);

/// Every APK file opensu keeps: each UI sound.
[[nodiscard]] std::vector<ApkAsset> allApkAssets();

} // namespace opensu::artwork

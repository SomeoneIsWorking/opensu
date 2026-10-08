// iisu_assets — the files iideck takes out of iiSU's APK as they are and keeps in the artwork
// store: its UI sounds and the dock's icons. One description of such a file serves the store, which
// names where it is kept, and the fetcher, which names where it is in the APK.
#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "audio/effect.hpp"
#include "library/sections.hpp"

namespace iideck::artwork {

/// What an asset is for, which is also the store folder it is kept in.
enum class AssetKind : std::uint8_t { Sound, NavIcon };

/// One file of the APK, kept whole.
struct ApkAsset {
    AssetKind kind{AssetKind::Sound};
    /// The store's file name under the kind's folder, and what the shell is told it by.
    std::string file;
    /// The file's entry in the APK.
    std::string entry;

    bool operator==(const ApkAsset&) const = default;
};

/// The store folder of an asset kind.
[[nodiscard]] std::string_view folder(AssetKind kind) noexcept;

/// An effect's WAV or OGG in the APK's `assets/`.
[[nodiscard]] ApkAsset soundAsset(audio::Effect effect);

/// A dock icon: a section's, as drawn when it is not the active one or when it is.
struct NavIcon {
    library::Section section{library::Section::Home};
    bool selected{false};

    bool operator==(const NavIcon&) const = default;
};

inline constexpr std::array allNavIcons{
    NavIcon{library::Section::Home, false},
    NavIcon{library::Section::Home, true},
    NavIcon{library::Section::Library, false},
    NavIcon{library::Section::Library, true},
};

/// A dock icon's PNG in the APK. iiSU 0.0.7.4 ships its resources under shrunk names
/// (`res/TG.png` is `drawable/home.png`), so these are valid for the pinned APK only.
[[nodiscard]] ApkAsset navAsset(NavIcon icon);

/// The dock icon whose store file is `file`, or nothing.
[[nodiscard]] std::optional<NavIcon> navIconOfFile(std::string_view file) noexcept;

} // namespace iideck::artwork

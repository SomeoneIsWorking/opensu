// asset_owner — whose art or sounds the player chose (Settings › Artwork › Console art, Settings ›
// Audio › Interface sounds): openSU's own or iiSU's, and how the settings file spells it.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace opensu::settings {

enum class AssetOwner : std::uint8_t {
    OpenSu,
    /// iiSU's, read from the iiSU file the player gave.
    Iisu,
};

inline constexpr std::array allAssetOwners{AssetOwner::OpenSu, AssetOwner::Iisu};

/// The choice as the settings file spells it: "opensu", "iisu".
[[nodiscard]] std::string_view key(AssetOwner choice) noexcept;

/// The choice a spelling names, or nothing.
[[nodiscard]] std::optional<AssetOwner> assetOwnerOf(std::string_view spelling) noexcept;

/// The choice as the player reads it: "openSU", "iiSU".
[[nodiscard]] std::string_view label(AssetOwner choice) noexcept;

} // namespace opensu::settings

#include "asset_owner.hpp"

#include <algorithm>

namespace opensu::settings {

std::string_view key(AssetOwner choice) noexcept {
    switch (choice) {
    case AssetOwner::OpenSu:
        return "opensu";
    case AssetOwner::Iisu:
        return "iisu";
    }
    return {};
}

std::optional<AssetOwner> assetOwnerOf(std::string_view spelling) noexcept {
    const auto found = std::ranges::find_if(allAssetOwners, [spelling](AssetOwner choice) {
        return key(choice) == spelling;
    });
    return found == allAssetOwners.end() ? std::nullopt : std::optional{*found};
}

std::string_view label(AssetOwner choice) noexcept {
    switch (choice) {
    case AssetOwner::OpenSu:
        return "openSU";
    case AssetOwner::Iisu:
        return "iiSU";
    }
    return {};
}

} // namespace opensu::settings

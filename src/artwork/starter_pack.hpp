// starter_pack — iiSU's console cards. iiSU's starter pack (`platforms/<system>.webp`, a framed
// card per console) is a zip inside its release APK. It is taken out of the APK by ranges
// (ApkArchive), verified against a pin and kept in the store.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "apk_archive.hpp"
#include "artwork_store.hpp"
#include "net/web_client.hpp"

namespace opensu::artwork {

/// The release APK and the starter pack entry in it, as pinned.
struct PackPin {
    std::uint64_t apkSize;
    std::string_view entry;
    std::uint32_t entrySize;
    std::uint32_t entryCrc32;
    /// The store's file for the pack; changes with the pin.
    std::string_view fileName;
};

inline constexpr std::string_view iisuApkUrl =
    "https://github.com/iisu-network/iiSU/releases/download/0.0.7.4/iiSU-Alpha-7.4.apk";

/// iiSU 0.0.7.4's `assets/iiSU_StarterPack.zip`.
inline constexpr PackPin iisuPackPin{128322093, "assets/iiSU_StarterPack.zip", 25028027, 0x86d90fc0,
                                     "starter-pack-0.0.7.4.zip"};

class StarterPack {
  public:
    StarterPack(const ArtworkStore& store, ApkArchive& apk, const PackPin& pin);

    /// Makes sure the pack is in the store, downloading it when it is not. False with `error`
    /// when the download fails or what arrives differs from the pin; nothing is kept then.
    [[nodiscard]] bool ensure(const net::WebClient& web, std::string& error) const;

    /// The PNG of a system's card from the stored pack. Nothing when the pack has no card for it
    /// (`error` empty) or cannot be read (`error` says why).
    [[nodiscard]] std::optional<std::string> card(std::string_view system,
                                                  std::string& error) const;

  private:
    const ArtworkStore& store_;
    ApkArchive& apk_;
    PackPin pin_;
};

} // namespace opensu::artwork

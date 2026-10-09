// console_glyphs — the white console glyphs on iiSU's platform frames. The APK's
// `assets/borders/border_pack.json` maps each system to a logo PNG beside it; this reads the map
// once and takes single logos out of the APK by ranges.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "apk_archive.hpp"
#include "net/web_client.hpp"

namespace opensu::artwork {

class ConsoleGlyphs {
  public:
    explicit ConsoleGlyphs(ApkArchive& apk);

    /// The glyph PNG of an ES-DE system.
    [[nodiscard]] ApkFile fetch(const net::WebClient& web, std::string_view system);

  private:
    /// Reads the pack's console to logo map on first use.
    [[nodiscard]] bool load(const net::WebClient& web, std::string& error);

    ApkArchive& apk_;
    std::optional<std::map<std::string, std::string, std::less<>>> logos_;
};

} // namespace opensu::artwork

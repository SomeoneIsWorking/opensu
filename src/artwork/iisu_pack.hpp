// iisu_pack — iiSU's console cards and tab glyphs from the file the player downloaded and gave
// openSU, never fetched by openSU. The file is iiSU's release APK, whose
// `assets/iiSU_StarterPack.zip` holds a card per console (`platforms/<system>.webp`) and whose
// `assets/borders/border_pack.json` maps each console to a logo PNG beside it, or that starter
// pack zip alone (cards, no glyphs). Any release with that layout is taken; what is read is
// checked as it is read. Read from disk by ranges; safe to read from several threads.
#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "apk_archive.hpp"
#include "zip/zip_archive.hpp"

namespace opensu::artwork {

class IisuPack {
  public:
    /// Opens `file` and checks its layout. Nothing, with `error` saying what is wrong, for a file
    /// that is neither iiSU's release APK nor its starter pack.
    [[nodiscard]] static std::shared_ptr<const IisuPack> open(const std::filesystem::path& file,
                                                              std::string& error);

    [[nodiscard]] const std::filesystem::path& file() const noexcept {
        return file_;
    }
    /// Whether it holds glyphs: the release APK does, a starter pack alone does not.
    [[nodiscard]] bool hasGlyphs() const noexcept {
        return apk_;
    }

    /// A console's card, WebP as the pack holds it. Nothing when the pack has no card for the
    /// system (`error` empty) or it cannot be read (`error` says why).
    [[nodiscard]] std::optional<std::string> card(std::string_view system,
                                                  std::string& error) const;
    /// A console's glyph, PNG as the APK holds it; nothing (`error` empty) from a starter pack.
    [[nodiscard]] std::optional<std::string> glyph(std::string_view system,
                                                   std::string& error) const;

  private:
    IisuPack(std::filesystem::path file, ApkArchive archive, bool apk);

    using Logos = std::map<std::string, std::string, std::less<>>;

    /// The APK's starter pack, read whole on first use. Under `mutex_`.
    [[nodiscard]] const zip::Archive* starterPack(std::string& error) const;
    /// The APK's console to logo map, read on first use. Under `mutex_`.
    [[nodiscard]] const Logos* logos(std::string& error) const;

    std::filesystem::path file_;
    bool apk_;
    mutable std::mutex mutex_;
    mutable ApkArchive archive_;
    mutable std::optional<zip::Archive> starterPack_;
    mutable std::optional<Logos> logos_;
};

} // namespace opensu::artwork

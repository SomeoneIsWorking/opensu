// arcade_names — the games behind arcade short names ("sf2.zip"). The names come from the
// libretro-database MAME and FinalBurn Neo ROM-set listings (CC-BY-SA-4.0, pinned by commit):
// each game block's `name` is the description and its `rom ( name <short>.zip ...)` the file.
// The listings are fetched once by `artwork::ArtworkFetcher` and kept here parsed.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace opensu::library::roms {

/// The libretro-database commit every listing is read at.
inline constexpr std::string_view libretroDatabaseRevision =
    "bf825e3ec48d43557ad024da6a7b93e512043525";

/// Lower-case short name to the listing's description ("sf2" to "Street Fighter II: The World
/// Warrior (World 910522)").
using NameMap = std::unordered_map<std::string, std::string>;

/// The games of a clrmamepro `.dat`. A block without a `.zip` ROM line is skipped.
[[nodiscard]] NameMap parseDat(std::string_view text);

/// The parsed names on disk under the cache.
class NameDb {
  public:
    /// A database that is nowhere: never cached, saves nothing.
    NameDb() = default;
    explicit NameDb(std::filesystem::path dir);

    /// `<cache>/names`.
    [[nodiscard]] static NameDb under(const std::filesystem::path& cacheDir);

    /// Whether a database for the pinned revision is kept.
    [[nodiscard]] bool cached() const;

    /// Whether this database is somewhere to keep names at all.
    [[nodiscard]] bool enabled() const noexcept {
        return !dir_.empty();
    }

    /// The kept names; empty when none are kept or the file is unreadable.
    [[nodiscard]] NameMap load() const;

    /// Keeps `names`. False with `error` when it cannot; what was kept stays.
    [[nodiscard]] bool save(const NameMap& names, std::string& error) const;

  private:
    [[nodiscard]] std::filesystem::path file() const;

    std::filesystem::path dir_;
};

} // namespace opensu::library::roms

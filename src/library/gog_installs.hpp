// gog_installs — where iideck keeps GOG installs and which ones it has made.
//
// gogdl downloads into a folder it names itself and keeps no list of installs, so iideck records
// each finished install (game id, folder, platform) in `<data dir>/gog-installs.json`. The record
// is what makes a GOG game installed and says how it launches.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace iideck::library::gog {

/// Where iideck puts GOG's files under its data directory.
struct Paths {
    /// The install records.
    std::filesystem::path records;
    /// gogdl downloads game `<id>` into `<games>/<id>/<folder gogdl names>`.
    std::filesystem::path games;
    /// Wine prefixes of Windows installs, `<prefixes>/<id>`.
    std::filesystem::path prefixes;
    /// The transient file the token is handed to gogdl in.
    std::filesystem::path gogdlAuth;

    [[nodiscard]] static Paths under(const std::filesystem::path& dataDir);
};

/// One finished install.
struct Installed {
    std::filesystem::path path;
    /// The gogdl platform it was downloaded for: "linux" or "windows".
    std::string platform;
};

class InstallRecords {
  public:
    explicit InstallRecords(std::filesystem::path file);

    /// The record of game `id`; nothing when it was never installed. Throws when the file is
    /// unreadable or not a record list.
    [[nodiscard]] std::optional<Installed> find(std::string_view id) const;

    /// Every record, by game id. Throws as `find` does.
    [[nodiscard]] std::map<std::string, Installed, std::less<>> all() const;

    /// Adds or replaces the record of game `id`. Throws when it cannot be written.
    void add(const std::string& id, const Installed& installed) const;

  private:
    std::filesystem::path file_;
};

} // namespace iideck::library::gog

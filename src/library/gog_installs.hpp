// gog_installs — where opensu keeps GOG installs and which ones it has made.
//
// gogdl downloads into a folder it names itself and keeps no list of installs, so opensu records
// each finished install (game id, folder, platform) in `<data dir>/gog-installs.json`. The record
// is what makes a GOG game installed and says how it launches; its build is what an update check
// compares with what GOG offers now. An uninstall deletes a recorded
// folder only when it lies inside the root openSU gave gogdl for that game; a record pointing
// anywhere else is only forgotten.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace opensu::library::gog {

/// Where opensu puts GOG's files under its data directory.
struct Paths {
    /// The install records.
    std::filesystem::path records;
    /// gogdl downloads game `<id>` into `<games>/<id>/<folder gogdl names>` when no install folder
    /// is set.
    std::filesystem::path games;
    /// The transient file the token is handed to gogdl in.
    std::filesystem::path gogdlAuth;

    [[nodiscard]] static Paths under(const std::filesystem::path& dataDir);

    /// The folder gogdl is given for game `id`: `<folder>/<id>`, or `<games>/<id>` with none.
    [[nodiscard]] std::filesystem::path gameRoot(const std::optional<std::filesystem::path>& folder,
                                                 std::string_view id) const;
};

/// One finished install.
struct Installed {
    std::filesystem::path path;
    /// The gogdl platform it was downloaded for: "linux" or "windows".
    std::string platform;
    /// What was installed, as `installedBuild` reads it; empty when unknown.
    std::string build{};
};

/// The build installed at `installed.path` for game `id`, as gogdl's `import` reads it
/// (`gogdl/imports.py:get_info`): for Windows the content system's build id, from
/// `goggame-<id>.id`, else `goggame-<id>.info`; for Linux the installer's version, the second line
/// of `gameinfo`. Empty when the files do not say.
[[nodiscard]] std::string installedBuild(const Installed& installed, std::string_view id);

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

    /// Forgets game `id`; nothing when it has no record. Throws as `add` does.
    void remove(std::string_view id) const;

  private:
    void write(const std::map<std::string, Installed, std::less<>>& records) const;

    std::filesystem::path file_;
};

/// The folder an uninstall of `installed` may delete: its path with symlinks resolved, when that
/// lies strictly inside `root` (`Paths::gameRoot`), whose last part is taken as it is, so a
/// symlinked game root cannot lead outside the install folder. Nothing when it lies elsewhere or
/// no longer exists, which leaves the record to be forgotten with no file touched.
[[nodiscard]] std::optional<std::filesystem::path> ownedFolder(const Installed& installed,
                                                               const std::filesystem::path& root);

} // namespace opensu::library::gog

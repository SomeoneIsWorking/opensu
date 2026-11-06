#include "lutris_games.hpp"

#include <algorithm>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include <sqlite3.h>

#include "config/config.hpp"
#include "game.hpp"
#include "lutris_config.hpp"
#include "lutris_wine.hpp"

namespace opensu::library::own {
namespace {

namespace fs = std::filesystem;

struct CloseDatabase {
    void operator()(sqlite3* database) const noexcept {
        sqlite3_close(database);
    }
};
struct FinishStatement {
    void operator()(sqlite3_stmt* statement) const noexcept {
        sqlite3_finalize(statement);
    }
};

/// `path` as the path of an SQLite file URI: `%`, `?` and `#` escaped.
std::string uriPath(const std::string& path) {
    std::string out;
    for (const char c : path) {
        if (c == '%') {
            out += "%25";
        } else if (c == '?') {
            out += "%3f";
        } else if (c == '#') {
            out += "%23";
        } else {
            out += c;
        }
    }
    return out;
}

std::string column(sqlite3_stmt* statement, int index) {
    const unsigned char* text = sqlite3_column_text(statement, index);
    return text == nullptr ? std::string{} : std::string{reinterpret_cast<const char*>(text)};
}

/// umu's game id for a Steam app, `umu-<app id>`, or the bare id.
std::string appOfUmuId(std::string_view gameId) {
    if (gameId.starts_with("umu-")) {
        gameId.remove_prefix(4);
    }
    return isSteamAppId(gameId) ? std::string{gameId} : std::string{};
}

/// The Steam app a Lutris game is: Lutris's Steam service id, a Steam runner's `appid`, or the
/// `GAMEID` umu runs a Wine game as.
std::string steamAppOf(const fs::path& lutrisData, const std::string& configpath,
                       const std::string& runner, const std::string& service,
                       const std::string& serviceId) {
    if (service == "steam" && isSteamAppId(serviceId)) {
        return serviceId;
    }
    if (configpath.empty()) {
        return {};
    }
    const fs::path config = lutris::gameConfig(lutrisData, configpath);
    if (runner == "steam") {
        std::string appId = lutris::configValue(config, {"game", "appid"});
        return isSteamAppId(appId) ? appId : std::string{};
    }
    return appOfUmuId(lutris::configValue(config, {"system", "env", "GAMEID"}));
}

/// The file name of the program a Lutris game runs; empty when its config names none.
std::string programOf(const fs::path& lutrisData, const std::string& configpath) {
    if (configpath.empty()) {
        return {};
    }
    const std::string exe =
        lutris::configValue(lutris::gameConfig(lutrisData, configpath), {"game", "exe"});
    return fs::path{exe}.filename().string();
}

/// The value Lutris gives Wine for a game's DLL overrides: `name=value;...`, no spaces.
std::string dllOverrides(const fs::path& config) {
    std::string overrides;
    for (auto& [dll, how] : lutris::configMapping(config, {"wine", "overrides"})) {
        std::erase(how, ' ');
        overrides += overrides.empty() ? "" : ";";
        overrides += dll;
        overrides += '=';
        overrides += how;
    }
    return overrides;
}

/// Fills in how Lutris starts `entry` when openSU can start it the same way: a Linux program, or
/// a Windows `.exe` Lutris runs on the newest GE-Proton in a prefix it names. A game on a Wine
/// build keeps it: a Proton build would upgrade that prefix under Lutris.
void readStart(const fs::path& lutrisData, const std::string& configpath, const std::string& runner,
               Entry& entry) {
    if (configpath.empty()) {
        return;
    }
    const fs::path config = lutris::gameConfig(lutrisData, configpath);
    const fs::path exe = lutris::configValue(config, {"game", "exe"});
    const std::optional<Kind> kind = kindOf(exe);
    if (!exe.is_absolute() || !kind) {
        return;
    }
    std::vector<std::string> environment;
    for (const auto& [name, value] : lutris::configMapping(config, {"system", "env"})) {
        environment.push_back(std::format("{}={}", name, value));
    }
    if (runner == "wine" && *kind == Kind::Windows) {
        std::string version = lutris::configValue(config, {"wine", "version"});
        if (version.empty()) {
            version = lutris::defaultWineVersion(lutrisData);
        }
        const fs::path prefix = lutris::configValue(config, {"game", "prefix"});
        if (version != lutris::geProtonLatest || !prefix.is_absolute()) {
            return;
        }
        entry.winePrefix = prefix;
        if (const std::string overrides = dllOverrides(config); !overrides.empty()) {
            environment.push_back("WINEDLLOVERRIDES=" + overrides);
        }
    } else if (runner != "linux" || *kind == Kind::Windows) {
        return;
    }
    entry.file = exe;
    entry.args = config::splitWords(lutris::configValue(config, {"game", "args"}));
    entry.workingDirectory = lutris::configValue(config, {"game", "working_dir"});
    entry.environment = std::move(environment);
}

} // namespace

std::vector<Entry> lutrisGames(const fs::path& lutrisData) {
    const fs::path file = lutrisData / "pga.db";
    std::error_code error;
    if (!fs::is_regular_file(file, error)) {
        return {};
    }
    // immutable=1: no lock, journal or -shm file is made next to Lutris's database.
    const std::string uri = "file:" + uriPath(file.string()) + "?immutable=1";
    sqlite3* opened = nullptr;
    const int status =
        sqlite3_open_v2(uri.c_str(), &opened, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr);
    const std::unique_ptr<sqlite3, CloseDatabase> database{opened};
    if (status != SQLITE_OK) {
        throw std::runtime_error{"cannot open " + file.string() + ": " + sqlite3_errstr(status)};
    }
    sqlite3_stmt* prepared = nullptr;
    if (sqlite3_prepare_v2(database.get(),
                           "SELECT id, name, configpath, runner, service, service_id FROM games "
                           "WHERE installed = 1 ORDER BY name",
                           -1, &prepared, nullptr) != SQLITE_OK) {
        throw std::runtime_error{"cannot read " + file.string() + ": " +
                                 sqlite3_errmsg(database.get())};
    }
    const std::unique_ptr<sqlite3_stmt, FinishStatement> statement{prepared};
    std::vector<Entry> entries;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        std::string name = column(statement.get(), 1);
        if (name.empty()) {
            continue;
        }
        const std::string configpath = column(statement.get(), 2);
        const std::string runner = column(statement.get(), 3);
        entries.push_back(
            Entry{.id = {},
                  .title = std::move(name),
                  .file = {},
                  .args = {},
                  .workingDirectory = {},
                  .origin = Origin::Lutris,
                  .originId = column(statement.get(), 0),
                  .steamAppId = steamAppOf(lutrisData, configpath, runner,
                                           column(statement.get(), 4), column(statement.get(), 5)),
                  .lutrisProgram = programOf(lutrisData, configpath)});
        readStart(lutrisData, configpath, runner, entries.back());
    }
    return entries;
}

} // namespace opensu::library::own

// fumarole_installs — Steam games fumarole installed (`fumarole install`), read from its records
// (`<data>/installed/<appid>.vdf`: appid, name, path, os, state). A complete install is a Steam
// game openSU starts with `fumarole launch <appid>`, which signs in, syncs the app's cloud and runs
// it with fumarole's own steamclient, no Steam client running: a Windows build under Wine, a macOS
// one as it is (`fumaroleOwnOs`). Also the
// account's owned games (`fumarole list --json`), which `fumarole install` puts here, and what its
// install prints.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game.hpp"

namespace opensu::library::fumarole {

/// Where fumarole is on this machine.
struct Installs {
    /// The `fumarole` program; empty when there is none, and then nothing is read.
    std::filesystem::path program;
    /// fumarole's data directory (`$XDG_DATA_HOME/fumarole`).
    std::filesystem::path dataDir;

    /// `fumarole` on `executablePath`, with its data in `dataDir` (`Config::fumaroleDataDir`).
    [[nodiscard]] static Installs find(const std::vector<std::filesystem::path>& executablePath,
                                       const std::filesystem::path& dataDir);

    /// Each complete install of a build that runs here (`gameOf`) as a Steam game, launched through
    /// fumarole; unreadable records are skipped.
    [[nodiscard]] std::vector<Game> games() const;

    /// The account's owned games fumarole can install here (`parseOwned`), as Steam games not
    /// installed here that `fumarole install` installs. Throws when fumarole is not signed in, cannot
    /// be reached or prints an unreadable list.
    [[nodiscard]] std::vector<Game> owned() const;

    /// The account's Steam web login, the `steamLoginSecure` cookie `fumarole web-session`
    /// prints; a secret, never logged. Nothing when fumarole is not signed in or cannot be reached.
    [[nodiscard]] std::optional<std::string> webLogin() const;

    /// The Steam sign-in fumarole keeps, else the one this PC's Steam client saved, as one line
    /// of JSON (`fumarole auth --export`) another fumarole's `auth --import` takes; a secret, never
    /// logged. Nothing with why when there is none.
    [[nodiscard]] std::optional<std::string> exportedLogin(std::string& why) const;
};

/// The owned games with a Windows build, or this OS's own where fumarole installs it, in
/// `fumarole list --json`'s array, as `Installs::owned`
/// lists them; throws std::runtime_error when `text` is not that array.
[[nodiscard]] std::vector<Game> parseOwned(std::string_view text);

/// How far an install is, from one line `fumarole install --json` prints: its progress event.
[[nodiscard]] std::optional<double> installProgress(std::string_view line);

/// Why an install failed, from one line fumarole logs: its error.
[[nodiscard]] std::optional<std::string> installFailure(std::string_view line);

/// The game of one record, read from `record`; nothing when it is not a complete Windows install
/// or one of this OS's own build (`fumaroleOwnOs`), which fumarole launches without Wine.
[[nodiscard]] std::optional<Game> gameOf(const std::filesystem::path& record,
                                         const std::filesystem::path& program);

} // namespace opensu::library::fumarole

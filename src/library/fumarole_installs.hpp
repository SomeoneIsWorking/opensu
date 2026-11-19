// fumarole_installs — Steam games fumarole installed (`fumarole install`), read from its records
// (`<data>/installed/<appid>.vdf`: appid, name, path, os, state). A complete Windows install is a
// Steam game openSU starts with `fumarole launch <appid>`, which signs in, syncs the app's cloud
// and runs it under Wine with fumarole's own steamclient, no Steam client running. Read only.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
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

    /// Each complete Windows install as a Steam game, launched through fumarole; unreadable
    /// records are skipped.
    [[nodiscard]] std::vector<Game> games() const;
};

/// The game of one record, read from `record`; nothing when it is not a complete Windows install.
[[nodiscard]] std::optional<Game> gameOf(const std::filesystem::path& record,
                                         const std::filesystem::path& program);

} // namespace opensu::library::fumarole

// qt_config_ini — the settings file of a Qt frontend of citra's and yuzu's line (Azahar, Eden,
// Citron, sudachi), as QSettings writes it: a key's `<key>\default` flag, which the frontend reads
// before the value, string values in quotes, and input bindings as param packages
// (`engine:sdl,guid:<GUID>,port:<N>,...`).
#pragma once

#include <string>
#include <string_view>

#include "ini_file.hpp"

namespace opensu::library {

/// `ini` with `key` set to `value` and its `\default` flag false, so the frontend reads the value.
[[nodiscard]] std::string withQtSetting(std::string_view ini, const IniKey& key,
                                        std::string_view value);

/// A QSettings value without the quotes around it.
[[nodiscard]] std::string qtUnquoted(std::string_view value);

/// `value` in quotes, as QSettings writes a string holding a comma.
[[nodiscard]] std::string qtQuoted(std::string_view value);

/// Whether the param package `binding` reads an SDL pad.
[[nodiscard]] bool isSdlBinding(std::string_view binding);

/// The param package `binding` with its `guid` and `port` made `guid` and `port` when it reads an
/// SDL pad; any other binding as it was.
[[nodiscard]] std::string withSdlPad(std::string_view binding, std::string_view guid, int port);

} // namespace opensu::library

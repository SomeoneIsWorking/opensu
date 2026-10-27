// steam_appinfo — Steam's app info cache (`appcache/appinfo.vdf`, formats 28 and 29): the binary
// KeyValues of the apps asked for, as `vdf::Node`s. Read only.
#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>

#include "vdf/node.hpp"

namespace opensu::library::steam {

/// Each of `appIds` found in `file`, by app id: the block under its `appinfo` key, numbers as
/// decimal text. Empty when the file is missing, malformed or in another format, which is logged.
[[nodiscard]] std::map<std::string, vdf::Node, std::less<>>
readAppInfo(const std::filesystem::path& file, const std::set<std::string, std::less<>>& appIds);

} // namespace opensu::library::steam

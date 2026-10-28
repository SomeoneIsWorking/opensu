#include "steam_compat.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <set>
#include <string_view>
#include <system_error>

#include "steam_appinfo.hpp"
#include "vdf/node.hpp"

namespace opensu::library::steam {
namespace {

namespace fs = std::filesystem;

/// The mapping's key for the tool every other Windows-only game runs under.
constexpr std::string_view defaultKey = "0";

/// Each app id's tool name as the player set it in Steam, with the default under `defaultKey`.
std::map<std::string, std::string, std::less<>> mappingOf(const std::vector<fs::path>& roots) {
    std::map<std::string, std::string, std::less<>> mapping;
    for (const fs::path& root : roots) {
        const std::optional<vdf::Node> config = vdf::parseFile(root / "config" / "config.vdf");
        const std::optional<vdf::Node> block =
            config ? config->block(
                         {"InstallConfigStore", "Software", "Valve", "Steam", "CompatToolMapping"})
                   : std::nullopt;
        if (!block) {
            continue;
        }
        for (const std::string& app : block->keys()) {
            if (const std::optional<std::string> name = block->str({app, "name"});
                name && !name->empty()) {
                mapping.try_emplace(app, *name);
            }
        }
    }
    return mapping;
}

/// The tools in each root's `compatibilitytools.d`, by name.
std::map<std::string, fs::path, std::less<>> customTools(const std::vector<fs::path>& roots) {
    std::map<std::string, fs::path, std::less<>> tools;
    for (const fs::path& root : roots) {
        std::error_code ec;
        for (const fs::directory_entry& entry :
             fs::directory_iterator{root / "compatibilitytools.d", ec}) {
            const fs::path file = entry.path() / "compatibilitytool.vdf";
            const std::optional<vdf::Node> doc = vdf::parseFile(file);
            const std::optional<vdf::Node> listed =
                doc ? doc->block({"compatibilitytools", "compat_tools"}) : std::nullopt;
            if (!listed) {
                continue;
            }
            for (const std::string& name : listed->keys()) {
                const fs::path at = listed->str({name, "install_path"}).value_or(".");
                tools.try_emplace(name, (entry.path() / at / "").lexically_normal().parent_path());
            }
        }
    }
    return tools;
}

/// Where an official tool's app is installed, from its manifest; empty when it is not.
fs::path installedApp(const Library& library, const std::string& appId) {
    const std::optional<fs::path> manifest = library.manifestOf(appId);
    if (!manifest || !library.installed(appId)) {
        return {};
    }
    const std::optional<vdf::Node> doc = vdf::parseFile(*manifest);
    const std::optional<std::string> dir =
        doc ? doc->str({"AppState", "installdir"}) : std::nullopt;
    return dir && !dir->empty() ? manifest->parent_path() / "common" / *dir : fs::path{};
}

/// Each official tool's name by itself and by each of its aliases (`proton-stable` is `proton_11`).
std::map<std::string, std::string, std::less<>>
officialNames(const std::optional<vdf::Node>& tools) {
    std::map<std::string, std::string, std::less<>> names;
    if (!tools) {
        return names;
    }
    for (const std::string& name : tools->keys()) {
        names.try_emplace(name, name);
        const std::string aliases = tools->str({name, "aliases"}).value_or("");
        for (std::size_t at = 0; at < aliases.size();) {
            const std::size_t end = std::min(aliases.find(',', at), aliases.size());
            if (end > at) {
                names.try_emplace(aliases.substr(at, end - at), name);
            }
            at = end + 1;
        }
    }
    return names;
}

/// The tool Valve maps `app` to; empty for none, or a mapping Valve emptied.
std::string valveMapping(const std::optional<vdf::Node>& mappings, const std::string& app) {
    return mappings ? mappings->str({app, "tool"}).value_or("") : std::string{};
}

bool windowsOnly(const vdf::Node& app) {
    const std::optional<std::string> platforms = app.str({"common", "oslist"});
    return platforms && platforms->find("windows") != std::string::npos &&
           platforms->find("linux") == std::string::npos;
}

} // namespace

std::map<std::string, CompatTool, std::less<>> compatTools(const Library& library,
                                                           const std::vector<std::string>& appIds) {
    const std::map<std::string, std::string, std::less<>> mapping = mappingOf(library.roots());
    const auto fallback = mapping.find(defaultKey);
    std::set<std::string, std::less<>> wanted{appIds.begin(), appIds.end()};
    wanted.emplace(toolRegistryApp);
    std::map<std::string, vdf::Node, std::less<>> info;
    for (const fs::path& root : library.roots()) {
        info.merge(readAppInfo(root / "appcache" / "appinfo.vdf", wanted));
    }
    const auto registry = info.find(toolRegistryApp);
    const std::optional<vdf::Node> official =
        registry != info.end() ? registry->second.block({"extended", "compat_tools"})
                               : std::nullopt;
    const std::optional<vdf::Node> valve =
        registry != info.end() ? registry->second.block({"extended", "app_mappings"})
                               : std::nullopt;
    const std::map<std::string, std::string, std::less<>> officialName = officialNames(official);
    const std::map<std::string, fs::path, std::less<>> custom = customTools(library.roots());

    std::map<std::string, CompatTool, std::less<>> tools;
    for (const std::string& app : appIds) {
        std::string name;
        if (const auto picked = mapping.find(app); picked != mapping.end()) {
            name = picked->second;
        } else if (std::string mapped = valveMapping(valve, app); !mapped.empty()) {
            name = std::move(mapped);
        } else if (const auto known = info.find(app);
                   fallback != mapping.end() && known != info.end() && windowsOnly(known->second)) {
            name = fallback->second;
        }
        if (name.empty()) {
            continue;
        }
        if (const auto canonical = officialName.find(name); canonical != officialName.end()) {
            name = canonical->second;
        }
        fs::path dir;
        if (const auto found = custom.find(name); found != custom.end()) {
            dir = found->second;
        } else if (const std::optional<std::string> toolApp =
                       official ? official->str({name, "appid"}) : std::nullopt) {
            dir = installedApp(library, *toolApp);
        }
        tools.emplace(app, CompatTool{.name = std::move(name), .dir = std::move(dir)});
    }
    return tools;
}

} // namespace opensu::library::steam

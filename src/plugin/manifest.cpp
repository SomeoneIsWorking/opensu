#include "manifest.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

namespace opensu::plugin {
namespace {

using json = nlohmann::json;

bool isIdCharacter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
}

bool isGroupCharacter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

bool isHostCharacter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

/// A Flatpak application id: `dev.eden_emu.eden`.
bool isAppIdCharacter(char c) {
    return isHostCharacter(c) || (c >= 'A' && c <= 'Z') || c == '_';
}

template <class Predicate> bool spelledWith(std::string_view text, Predicate allowed) {
    return !text.empty() && std::ranges::all_of(text, allowed);
}

/// The string field `name` of `document`, or empty with `error` set.
std::string textField(const json& document, const char* name, std::string& error) {
    const auto found = document.find(name);
    if (found == document.end() || !found->is_string() || found->get<std::string>().empty()) {
        error = std::string{"\""} + name + "\" must be a non-empty string";
        return {};
    }
    return found->get<std::string>();
}

/// Why `pattern` cannot be a read path, or empty.
std::string pathRefusal(const std::string& pattern) {
    const PathTokens probe{"/h", "/d", "/c", "/k"};
    const std::optional<std::filesystem::path> expanded = expandPath(pattern, probe);
    if (!expanded) {
        return "read path " + pattern + " has an unknown token or is not absolute";
    }
    for (const std::filesystem::path& part : *expanded) {
        if (part == "..") {
            return "read path " + pattern + " climbs out with ..";
        }
    }
    return {};
}

std::string readPermissions(const json& permissions, Manifest& manifest) {
    const auto read = permissions.find("read");
    if (read == permissions.end()) {
        return {};
    }
    if (!read->is_object()) {
        return "\"read\" must map names to lists of paths";
    }
    for (const auto& [group, paths] : read->items()) {
        if (!spelledWith(group, isGroupCharacter) || !paths.is_array()) {
            return "read group " + group + " must be [a-z0-9_]+ and hold a list of paths";
        }
        std::vector<std::string>& listed = manifest.read[group];
        for (const json& path : paths) {
            if (!path.is_string()) {
                return "read group " + group + " holds something that is not a path";
            }
            const std::string pattern = path.get<std::string>();
            if (std::string refused = pathRefusal(pattern); !refused.empty()) {
                return refused;
            }
            listed.push_back(pattern);
        }
    }
    return {};
}

std::string httpPermissions(const json& permissions, Manifest& manifest) {
    const auto http = permissions.find("http");
    if (http == permissions.end()) {
        return {};
    }
    if (!http->is_array()) {
        return "\"http\" must be a list of host names";
    }
    for (const json& host : *http) {
        if (!host.is_string() || !spelledWith(host.get<std::string>(), isHostCharacter)) {
            return "\"http\" holds something that is not a lower-case host name";
        }
        manifest.http.push_back(host.get<std::string>());
    }
    return {};
}

} // namespace

std::vector<std::string> Manifest::permissions() const {
    std::vector<std::string> all;
    for (const auto& [group, paths] : read) {
        for (const std::string& path : paths) {
            all.push_back("read:" + path);
        }
    }
    for (const std::string& host : http) {
        all.push_back("http:" + host);
    }
    std::ranges::sort(all);
    all.erase(std::ranges::unique(all).begin(), all.end());
    return all;
}

ManifestRead parseManifest(std::string_view text) {
    const json document = json::parse(text, nullptr, false);
    if (!document.is_object()) {
        return {std::nullopt, "plugin.json is not a JSON object"};
    }
    Manifest manifest;
    std::string error;
    manifest.id = textField(document, "id", error);
    if (error.empty() && !spelledWith(manifest.id, isIdCharacter)) {
        error = "\"id\" must be [a-z0-9-]+";
    }
    if (error.empty()) {
        manifest.name = textField(document, "name", error);
    }
    if (error.empty()) {
        manifest.version = textField(document, "version", error);
    }
    if (!error.empty()) {
        return {std::nullopt, error};
    }
    const auto api = document.find("api");
    if (api == document.end() || !api->is_number_integer()) {
        return {std::nullopt, "\"api\" must be an integer"};
    }
    manifest.api = api->get<int>();
    const auto permissions = document.find("permissions");
    if (permissions != document.end()) {
        if (!permissions->is_object()) {
            return {std::nullopt, "\"permissions\" must be an object"};
        }
        error = readPermissions(*permissions, manifest);
        if (error.empty()) {
            error = httpPermissions(*permissions, manifest);
        }
        if (!error.empty()) {
            return {std::nullopt, error};
        }
    }
    return {std::move(manifest), {}};
}

std::optional<std::filesystem::path> expandPath(std::string_view pattern,
                                                const PathTokens& tokens) {
    if (pattern.starts_with('/')) {
        return std::filesystem::path{pattern};
    }
    if (!pattern.starts_with('{')) {
        return std::nullopt;
    }
    const std::size_t close = pattern.find('}');
    if (close == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view token = pattern.substr(1, close - 1);
    std::string_view rest = pattern.substr(close + 1);
    if (!rest.empty() && !rest.starts_with('/')) {
        return std::nullopt;
    }
    if (rest.starts_with('/')) {
        rest.remove_prefix(1);
    }
    std::filesystem::path base;
    if (token == "home") {
        base = tokens.home;
    } else if (token == "xdg_data") {
        base = tokens.xdgData;
    } else if (token == "xdg_config") {
        base = tokens.xdgConfig;
    } else if (token == "xdg_cache") {
        base = tokens.xdgCache;
    } else if (const std::size_t colon = token.find(':'); colon != std::string_view::npos) {
        const std::string_view kind = token.substr(0, colon);
        const std::string_view app = token.substr(colon + 1);
        if (!spelledWith(app, isAppIdCharacter) ||
            (kind != "flatpak_data" && kind != "flatpak_cache")) {
            return std::nullopt;
        }
        base = tokens.home / ".var" / "app" / std::string{app} /
               (kind == "flatpak_data" ? "data" : "cache");
    } else {
        return std::nullopt;
    }
    if (!base.is_absolute()) {
        return std::nullopt;
    }
    return rest.empty() ? base : base / std::string{rest};
}

} // namespace opensu::plugin

#include "gog_builds.hpp"

#include <stdexcept>

#include <nlohmann/json.hpp>

namespace opensu::library::gog {
namespace {

using nlohmann::json;

json parsed(std::string_view body, std::string_view what) {
    json document = json::parse(body, nullptr, false);
    if (!document.is_object()) {
        throw std::runtime_error{"GOG's " + std::string{what} + " answer has an unexpected shape"};
    }
    return document;
}

std::optional<std::string> text(const json& object, std::string_view key) {
    const json::const_iterator value = object.find(key);
    if (value == object.end() || !value->is_string() || value->get<std::string>().empty()) {
        return std::nullopt;
    }
    return value->get<std::string>();
}

} // namespace

std::optional<std::string> offeredWindowsBuild(std::string_view body) {
    const json document = parsed(body, "builds");
    const json::const_iterator items = document.find("items");
    if (items == document.end() || !items->is_array()) {
        throw std::runtime_error{"GOG's builds answer has an unexpected shape"};
    }
    const json* chosen = nullptr;
    for (const json& build : *items) {
        if (!build.is_object()) {
            continue;
        }
        if (chosen == nullptr) {
            chosen = &build;
        }
        if (const json::const_iterator branch = build.find("branch");
            branch == build.end() || branch->is_null()) {
            chosen = &build;
            break;
        }
    }
    return chosen != nullptr ? text(*chosen, "build_id") : std::nullopt;
}

std::optional<std::string> offeredLinuxVersion(std::string_view body) {
    const json document = parsed(body, "product");
    const json downloads = document.value("downloads", json::object());
    const json installers =
        downloads.is_object() ? downloads.value("installers", json::array()) : json::array();
    if (!installers.is_array()) {
        throw std::runtime_error{"GOG's product answer has an unexpected shape"};
    }
    std::vector<const json*> forLinux;
    for (const json& installer : installers) {
        if (installer.is_object() && installer.value("os", "") == "linux") {
            forLinux.push_back(&installer);
        }
    }
    if (forLinux.size() == 1) {
        return text(*forLinux.front(), "version");
    }
    for (const json* installer : forLinux) {
        if (installer->value("language", "") == "en") {
            return text(*installer, "version");
        }
    }
    return std::nullopt;
}

std::optional<std::string> offeredBuild(const net::WebClient& web, const Endpoints& endpoints,
                                        const std::vector<std::string>& headers,
                                        std::string_view id, std::string_view platform) {
    const bool native = platform == "linux";
    const std::string url =
        native ? endpoints.api + "/products/" + std::string{id} + "?expand=downloads"
               : endpoints.content + "/products/" + std::string{id} + "/os/" +
                     std::string{platform} + "/builds?generation=2";
    std::string error;
    const std::optional<std::string> body = web.get(url, error, headers);
    if (!body) {
        throw std::runtime_error{"cannot read GOG's builds of " + std::string{id} + " (" + error +
                                 ")"};
    }
    return native ? offeredLinuxVersion(*body) : offeredWindowsBuild(*body);
}

bool behind(std::string_view installed, const std::optional<std::string>& offered) {
    return !installed.empty() && offered && *offered != installed;
}

} // namespace opensu::library::gog

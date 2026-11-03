#include "game_matches.hpp"

#include <cctype>

#include <nlohmann/json.hpp>

namespace opensu::library::matches {
namespace {

using nlohmann::json;

/// The last run of four digits in `date` ("23 Mar, 2023", "Q1 2027").
std::string yearIn(std::string_view date) {
    std::size_t digits = 0;
    for (std::size_t i = date.size(); i-- > 0;) {
        if (std::isdigit(static_cast<unsigned char>(date[i])) != 0) {
            ++digits;
            continue;
        }
        if (digits == 4) {
            return std::string{date.substr(i + 1, 4)};
        }
        digits = 0;
    }
    return digits == 4 ? std::string{date.substr(0, 4)} : std::string{};
}

} // namespace

std::string releaseDateUrl(std::string_view appId) {
    return "https://store.steampowered.com/api/appdetails?appids=" + std::string{appId} +
           "&filters=release_date";
}

std::string parseReleaseYear(std::string_view json, std::string_view appId) {
    const nlohmann::json answer = json::parse(json, nullptr, false);
    const std::string key{appId};
    if (!answer.is_object() || !answer.contains(key)) {
        return {};
    }
    const nlohmann::json& app = answer[key];
    if (!app.is_object() || !app.value("success", false) || !app.contains("data") ||
        !app["data"].is_object()) {
        return {};
    }
    const nlohmann::json& data = app["data"];
    if (!data.contains("release_date") || !data["release_date"].is_object()) {
        return {};
    }
    const nlohmann::json& release = data["release_date"];
    const auto date = release.find("date");
    return date != release.end() && date->is_string() ? yearIn(date->get<std::string>())
                                                      : std::string{};
}

} // namespace opensu::library::matches

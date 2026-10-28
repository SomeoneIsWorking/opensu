#include "console_glyphs.hpp"

#include <algorithm>
#include <cctype>

#include <nlohmann/json.hpp>

namespace opensu::artwork {
namespace {

constexpr std::string_view borderDirectory = "assets/borders/";
constexpr std::string_view packEntry = "assets/borders/border_pack.json";

std::string fold(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

} // namespace

ConsoleGlyphs::ConsoleGlyphs(ApkArchive& apk) : apk_{apk} {
}

bool ConsoleGlyphs::load(const net::WebClient& web, std::string& error) {
    if (logos_) {
        return true;
    }
    const zip::Entry* entry = apk_.find(web, packEntry, error);
    if (entry == nullptr) {
        if (error.empty()) {
            error = std::string{packEntry} + " is not in the APK";
        }
        return false;
    }
    const std::optional<std::string> body = apk_.extract(web, *entry, error);
    if (!body) {
        return false;
    }
    const nlohmann::json document = nlohmann::json::parse(*body, nullptr, false);
    if (!document.is_object() || !document.contains("consoles") ||
        !document["consoles"].is_array()) {
        error = std::string{packEntry} + " has no console list";
        return false;
    }
    std::map<std::string, std::string, std::less<>> logos;
    for (const nlohmann::json& console : document["consoles"]) {
        const auto name = console.find("console");
        const auto logo = console.find("logo");
        if (name != console.end() && logo != console.end() && name->is_string() &&
            logo->is_string()) {
            logos.emplace(fold(name->get<std::string>()), logo->get<std::string>());
        }
    }
    logos_ = std::move(logos);
    return true;
}

ApkFile ConsoleGlyphs::fetch(const net::WebClient& web, std::string_view system) {
    std::string error;
    if (!load(web, error) || !logos_) {
        return ApkFile{ApkFile::Status::Failed, {}, error};
    }
    const auto logo = logos_->find(system);
    if (logo == logos_->end()) {
        return ApkFile{};
    }
    return apk_.fetch(web, std::string{borderDirectory} + logo->second);
}

} // namespace opensu::artwork

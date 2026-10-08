#include "console_glyphs.hpp"

#include <algorithm>
#include <cctype>

#include <nlohmann/json.hpp>

namespace iideck::artwork {
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

GlyphResult ConsoleGlyphs::fetch(const net::WebClient& web, std::string_view system) {
    GlyphResult result;
    result.status = GlyphResult::Status::Failed;
    if (!load(web, result.error) || !logos_) {
        return result;
    }
    const auto logo = logos_->find(system);
    if (logo == logos_->end()) {
        result.status = GlyphResult::Status::Missing;
        return result;
    }
    const std::string name = std::string{borderDirectory} + logo->second;
    const zip::Entry* entry = apk_.find(web, name, result.error);
    if (entry == nullptr) {
        if (result.error.empty()) {
            result.status = GlyphResult::Status::Missing;
        }
        return result;
    }
    std::optional<std::string> png = apk_.extract(web, *entry, result.error);
    if (!png) {
        return result;
    }
    result.status = GlyphResult::Status::Found;
    result.png = std::move(*png);
    return result;
}

} // namespace iideck::artwork

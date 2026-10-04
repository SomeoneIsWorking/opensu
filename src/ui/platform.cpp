#include "platform.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <ranges>
#include <span>

#include "config/config.hpp"
#include "library/game.hpp"
#include "lucent/log.h"

namespace iideck::ui {

namespace {

/// Lower-cases ASCII, which is all a console key ever contains. Used to match a
/// key against a pack that spells "steam" and "Steam" and "PC" three ways.
std::string fold(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

/// The sprite paths named for one console, found by reading forward from its
/// entry for the two string fields. The pack is a flat array, so each entry's
/// text runs until the closing brace.
std::string entryText(std::string_view json, std::size_t from) {
    const auto end = json.find('}', from);
    return std::string{
        json.substr(from, (end == std::string_view::npos ? json.size() : end) - from)};
}

/// Reads one string field out of an entry.
///
/// The pack is pretty-printed, so a field may carry spaces around its colon and
/// after its value, and the reference's own entries do both. Searching for a fixed
/// `"name":"value"` needle found nothing at all.
std::optional<std::string> field(std::string_view entry, std::string_view name) {
    const std::string quoted = "\"" + std::string{name} + "\"";
    const auto at = entry.find(quoted);
    if (at == std::string_view::npos) {
        return std::nullopt;
    }
    std::size_t cursor = at + quoted.size();
    const auto spaces = [&entry](std::size_t from) {
        while (from < entry.size() && (entry[from] == ' ' || entry[from] == '\t')) {
            ++from;
        }
        return from;
    };
    cursor = spaces(cursor);
    if (cursor >= entry.size() || entry[cursor] != ':') {
        return std::nullopt;
    }
    cursor = spaces(cursor + 1);
    if (cursor >= entry.size() || entry[cursor] != '"') {
        return std::nullopt;
    }
    const auto start = cursor + 1;
    const auto end = entry.find('"', start);
    if (end == std::string_view::npos) {
        return std::nullopt;
    }
    return std::string{entry.substr(start, end - start)};
}

/// The pack's console name for each store, matching the pack's own spelling. A
/// store title has no console, so it is framed in the store's identity; the pack
/// carries one border per store because a grid shows them side by side.
const char* platformForSource(library::Source source) noexcept {
    switch (source) {
    case library::Source::Steam:
        return "steam";
    case library::Source::Epic:
        return "epic";
    case library::Source::Gog:
        return "gog";
    case library::Source::Rom:
        return nullptr;
    }
    return nullptr;
}

} // namespace

std::size_t strokeGradientCount() noexcept {
    const auto* cursor = kStrokeGradients;
    while (cursor->console != nullptr) {
        ++cursor;
    }
    return static_cast<std::size_t>(cursor - kStrokeGradients);
}

std::filesystem::path defaultPlatformRoot() {
    return config::read().assetsDir / "borders";
}

Platforms Platforms::load(const std::filesystem::path& root) {
    Platforms out;
    const std::filesystem::path pack = root / "border_pack.json";
    std::ifstream in{pack, std::ios::binary};
    if (!in) {
        // Not an error: a shell with no pack draws every tile unframed, which is
        // what it did before the pack existed.
        lucent::info("ui", "no border pack at {}; tiles are unframed", pack.string());
        return out;
    }
    const std::string json{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    in.close();

    std::size_t cursor = 0;
    std::size_t declared = 0;
    while (cursor < json.size()) {
        // The field may be spaced as "console" : "name", so the colon is not
        // part of the search; the field reader handles the spacing.
        const auto at = json.find("\"console\"", cursor);
        if (at == std::string::npos) {
            break;
        }
        const std::string entry = entryText(json, at);
        // The search lands on the field's own opening quote, so the entry has to
        // start there rather than after it, or every field read falls outside it.
        const auto key = field(entry, "console");
        if (!key) {
            lucent::warn("ui", "border pack entry with no readable console name: {}", entry);
            cursor = at + 1;
            continue;
        }
        Platform platform;
        platform.key = fold(*key);
        if (const auto border = field(entry, "border"); border && !border->empty()) {
            platform.border = root / *border;
        }
        if (const auto logo = field(entry, "logo"); logo && !logo->empty()) {
            platform.logo = root / *logo;
        }
        // The gradient is per platform, recovered from the sprite by the
        // generator, so it is looked up rather than decoded here.
        const std::string wanted = platform.key;
        const auto table = std::span{kStrokeGradients, strokeGradientCount()};
        const auto found = std::ranges::find_if(table, [&wanted](const StrokeGradient& gradient) {
            return wanted == gradient.console;
        });
        if (found != table.end()) {
            platform.strokeFrom = found->from;
            platform.strokeTo = found->to;
        }
        out.platforms_.push_back(std::move(platform));
        ++declared;
        cursor = at + entry.size();
    }

    lucent::info("ui", "{} platform borders declared by {}", declared, pack.string());
    return out;
}

const Platform* Platforms::forSource(library::Source source) const {
    const char* key = platformForSource(source);
    if (key == nullptr) {
        return nullptr;
    }
    const std::string wanted = fold(key);
    const auto found = std::ranges::find_if(platforms_, [&wanted](const Platform& platform) {
        return platform.key == wanted;
    });
    return found == platforms_.end() ? nullptr : &*found;
}

std::optional<Platform> Platforms::find(std::string_view key) const {
    const std::string wanted = fold(key);
    const auto found = std::ranges::find_if(platforms_, [&wanted](const Platform& platform) {
        return platform.key == wanted;
    });
    if (found == platforms_.end()) {
        return std::nullopt;
    }
    return *found;
}

} // namespace iideck::ui

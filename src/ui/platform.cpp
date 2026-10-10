#include "platform.hpp"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <variant>

#include "library/game.hpp"
#include "platform_stroke.hpp"

namespace opensu::ui {

namespace {

/// Lower-cases ASCII, which is all a console key ever contains, so "PC" and "pc" match.
std::string fold(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

/// The platform key for each store. A store title has no console, so it is framed in the
/// store's identity.
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

Platforms::Platforms() {
    for (const StrokeGradient& gradient : kStrokeGradients) {
        platforms_.push_back(Platform{fold(gradient.console), gradient.from, gradient.to});
    }
}

const Platform* Platforms::forSource(library::Source source) const {
    const char* key = platformForSource(source);
    if (key == nullptr) {
        return nullptr;
    }
    return find(key);
}

const Platform* Platforms::forItem(const library::ShelfItem& item) const {
    if (const auto* console = std::get_if<library::Console>(&item)) {
        return find(console->system);
    }
    const auto* game = std::get_if<library::Game>(&item);
    if (game == nullptr) {
        return nullptr;
    }
    // A ROM belongs to a system, and the pack's console names are those systems.
    if (game->source == library::Source::Rom && !game->sourceId.empty()) {
        return find(game->sourceId);
    }
    return forSource(game->source);
}

const Platform* Platforms::find(std::string_view key) const {
    const std::string wanted = fold(key);
    const auto found = std::ranges::find_if(platforms_, [&wanted](const Platform& platform) {
        return platform.key == wanted;
    });
    return found == platforms_.end() ? nullptr : &*found;
}

} // namespace opensu::ui

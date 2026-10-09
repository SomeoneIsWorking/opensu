#include "context_menu.hpp"

#include <algorithm>
#include <variant>

namespace opensu::ui {
namespace {

constexpr float cardWidthDp = 320.0f;
constexpr float paddingDp = 16.0f;
constexpr float titleHeightDp = 36.0f;
constexpr float rowHeightDp = 44.0f;
constexpr float rowGapDp = 4.0f;
constexpr float radiusDp = 17.0f;
constexpr float hintBandDp = 30.0f;

/// Whether a launcher tile for a store with its own sign-in has none yet.
bool needsSignIn(const library::Launcher& launcher) noexcept {
    return !launcher.ready && !launcher.loading &&
           (launcher.source == library::Source::Gog || launcher.source == library::Source::Epic);
}

} // namespace

std::vector<ContextItem> contextItemsFor(const library::ShelfItem& item, bool hidden) {
    if (const auto* game = std::get_if<library::Game>(&item)) {
        std::vector<ContextItem> items;
        items.push_back(game->installed ? ContextItem{"Launch", ContextAction::Launch}
                                        : ContextItem{"Install", ContextAction::Install});
        items.push_back(ContextItem{"Details", ContextAction::Details});
        items.push_back(hidden ? ContextItem{"Unhide game", ContextAction::Unhide}
                               : ContextItem{"Hide game", ContextAction::Hide});
        return items;
    }
    std::vector<ContextItem> items{ContextItem{"Open", ContextAction::Open}};
    if (const auto* launcher = std::get_if<library::Launcher>(&item); launcher != nullptr) {
        if (needsSignIn(*launcher)) {
            items.push_back(ContextItem{"Sign in", ContextAction::SignIn});
        }
    }
    items.push_back(ContextItem{"Refresh library", ContextAction::Refresh});
    return items;
}

std::optional<std::size_t> ContextLayout::itemAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

ContextLayout layoutContextMenu(const Rect& frame, float dp, std::size_t items) {
    ContextLayout layout;
    const float pad = paddingDp * dp;
    const float width = std::min(cardWidthDp * dp, frame.width);
    const auto count = static_cast<float>(items);
    const float rows = count * rowHeightDp * dp + std::max(count - 1.0f, 0.0f) * rowGapDp * dp;
    const float height = pad + titleHeightDp * dp + rows + hintBandDp * dp + pad * 0.5f;
    layout.card = Rect{frame.x + (frame.width - width) * 0.5f,
                       frame.y + (frame.height - height) * 0.5f, width, height};
    layout.radius = radiusDp * dp;
    layout.title =
        Rect{layout.card.x + pad, layout.card.y + pad, width - 2.0f * pad, titleHeightDp * dp};
    float y = layout.title.bottom();
    for (std::size_t i = 0; i < items; ++i) {
        layout.rows.push_back(Rect{layout.card.x + pad, y, width - 2.0f * pad, rowHeightDp * dp});
        y += (rowHeightDp + rowGapDp) * dp;
    }
    layout.hints = Rect{layout.card.x + pad, y, width - 2.0f * pad, hintBandDp * dp};
    return layout;
}

void ContextMenu::open(std::string title, std::vector<ContextItem> items) {
    title_ = std::move(title);
    items_ = std::move(items);
    focus_ = 0;
    open_ = true;
}

bool ContextMenu::move(int delta) noexcept {
    const auto last = static_cast<long>(items_.size()) - 1;
    const auto next = static_cast<std::size_t>(
        std::clamp(static_cast<long>(focus_) + delta, 0L, std::max(last, 0L)));
    const bool moved = next != focus_;
    focus_ = next;
    return moved;
}

bool ContextMenu::focusItem(std::size_t index) noexcept {
    if (index >= items_.size() || index == focus_) {
        return false;
    }
    focus_ = index;
    return true;
}

} // namespace opensu::ui

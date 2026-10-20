#include "session_panels.hpp"

#include <variant>

namespace opensu::ui {

void SessionPanels::tick(Clock::time_point now) noexcept {
    dialogFade_.follow(dialog_.isOpen(), now);
    passwordFade_.follow(password_.isOpen(), now);
    chooserFade_.follow(chooser_.isOpen(), now);
}

void SessionPanels::draw(Canvas& canvas, Point size, float dp, Clock::time_point now,
                         double seconds) const {
    chooserPainter_.paint(canvas, chooser_, size, dp, chooserFade_.look(now));
    dialogPainter_.paint(canvas, dialog_, size, dp, dialogFade_.look(now));
    passwordPainter_.paint(canvas, password_, size, dp, passwordFade_.look(now), seconds);
}

PointerTarget SessionPanels::pointAt(Point point, Point size, float dp) const {
    if (password_.isOpen()) {
        const SearchLayout panel =
            layoutSearch(Rect{0.0f, 0.0f, size.x, size.y}, dp, password_.lists());
        if (const std::optional<std::size_t> key = panel.keyAt(point.x, point.y)) {
            return OnSearchKey{*key};
        }
        return {};
    }
    if (chooser_.isOpen()) {
        if (const std::optional<std::size_t> item =
                chooserPainter_.layout(chooser_, size, dp).itemAt(point.x, point.y)) {
            return OnIntegrationItem{*item};
        }
        return {};
    }
    if (dialog_.isOpen() && !dialog_.busy()) {
        if (const std::optional<std::size_t> button =
                dialogPainter_.layout(dialog_, size, dp).buttonAt(point.x, point.y)) {
            return OnDialogButton{*button};
        }
    }
    return {};
}

bool SessionPanels::focusTarget(const PointerTarget& target) {
    if (const auto* key = std::get_if<OnSearchKey>(&target); key != nullptr && password_.isOpen()) {
        return password_.focusKey(key->index);
    }
    if (const auto* item = std::get_if<OnIntegrationItem>(&target)) {
        return chooser_.focusItem(item->index);
    }
    if (const auto* button = std::get_if<OnDialogButton>(&target)) {
        return dialog_.focusButton(button->index);
    }
    return false;
}

} // namespace opensu::ui

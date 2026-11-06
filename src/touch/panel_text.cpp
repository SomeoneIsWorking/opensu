#include "panel_text.hpp"

#include <charconv>
#include <cstdint>
#include <format>

#include "gamepad/event.hpp"

namespace opensu::touch {
namespace {

constexpr unsigned mostWays = 8;

/// The text before the next `separator` in `text`, which then holds what follows it.
std::string_view takeUntil(std::string_view& text, char separator) {
    const std::size_t end = text.find(separator);
    const std::string_view taken = text.substr(0, end);
    text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
    return taken;
}

std::optional<unsigned> numberIn(std::string_view text) {
    unsigned value = 0;
    const auto [end, failure] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (failure != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

} // namespace

std::string panelText(const ArcadePanel& panel) {
    std::string text = std::format("{} {}\n", panel.ways, panel.vertical ? 1 : 0);
    for (const ArcadeButton& button : panel.buttons) {
        text += std::format("{} {} {}\n", gamepad::name(button.button),
                            static_cast<unsigned>(button.limb), button.label);
    }
    return text;
}

std::optional<ArcadePanel> panelFromText(std::string_view text) {
    std::string_view stick = takeUntil(text, '\n');
    const std::optional<unsigned> ways = numberIn(takeUntil(stick, ' '));
    const std::optional<unsigned> vertical = numberIn(stick);
    if (!ways || *ways > mostWays || !vertical || *vertical > 1) {
        return std::nullopt;
    }
    ArcadePanel panel{.ways = *ways, .vertical = *vertical == 1, .buttons = {}};
    while (!text.empty()) {
        std::string_view line = takeUntil(text, '\n');
        const gamepad::Button button = gamepad::buttonNamed(takeUntil(line, ' '));
        const std::optional<unsigned> limb = numberIn(takeUntil(line, ' '));
        if (button == gamepad::Button::None || !limb || *limb > static_cast<unsigned>(Limb::Kick)) {
            return std::nullopt;
        }
        panel.buttons.push_back(ArcadeButton{
            .button = button, .label = std::string{line}, .limb = static_cast<Limb>(*limb)});
    }
    return panel;
}

} // namespace opensu::touch

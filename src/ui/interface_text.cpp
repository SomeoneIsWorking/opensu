#include "interface_text.hpp"

#include <cmath>

#include "input/shortcuts.hpp"
#include "interface_input.hpp"
#include "lucent/log.h"

namespace opensu::ui {

float InterfaceText::measure(std::string_view text, float size) {
    return typeface_.interfaceText().measure(text, gpu2d::TextStyle{.size = size});
}

float InterfaceText::lineHeight(float size) {
    return typeface_.interfaceText().metrics(size).lineBox();
}

void InterfaceText::draw(gpu2d::Canvas& canvas, std::string_view text, gpu2d::Point topLeft,
                         float size, gpu2d::Colour colour) {
    if (text.empty() || colour.a == 0) {
        return;
    }
    const gpu2d::Point at{std::round(topLeft.x), std::round(topLeft.y)};
    if (!typeface_.interfaceText().draw(canvas, text, at, gpu2d::TextStyle{.size = size},
                                        colour)) {
        lucent::error("ui", "could not shape \"{}\": {}", text, SDL_GetError());
    }
}

float InterfacePrompts::width(gpu2d::ui::Action action, float height) {
    return glyphs_.advance(input::glyphOf(buttonOfInterfaceAction(action)), height);
}

void InterfacePrompts::draw(gpu2d::Canvas& canvas, gpu2d::ui::Action action,
                            const gpu2d::Rect& box, gpu2d::Colour ink) {
    glyphs_.paint(canvas, input::glyphOf(buttonOfInterfaceAction(action)),
                  gpu2d::Point{box.x + box.width * 0.5f, box.y + box.height * 0.5f}, box.height,
                  ink);
}

} // namespace opensu::ui

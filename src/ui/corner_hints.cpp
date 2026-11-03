#include "corner_hints.hpp"

namespace opensu::ui {

std::vector<Prompt> startPrompts(const HintContext& context) {
    std::vector<Prompt> prompts;
    // iiSU mw5.h: ("B", "Back"), ("-", "Details"); here the "-" prompt is the context menu.
    if (context.clearSearch) {
        prompts.push_back(Prompt{"B", "Clear search", gamepad::Button::B});
    } else if (context.back) {
        prompts.push_back(Prompt{"B", "Back", gamepad::Button::B});
    }
    if (context.options) {
        prompts.push_back(Prompt{"-", "Options", gamepad::Button::Select});
    }
    return prompts;
}

std::vector<Prompt> endPrompts(const HintContext& context) {
    std::vector<Prompt> prompts;
    // iiSU mw5.k: ("A", "Select"), ("+", "Menu").
    if (context.select) {
        const char* label = context.details ? "Details" : "Select";
        prompts.push_back(Prompt{"A", label, gamepad::Button::A});
    }
    if (context.menu) {
        prompts.push_back(Prompt{"+", "Menu", gamepad::Button::Start});
    }
    return prompts;
}

} // namespace opensu::ui

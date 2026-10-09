#include "corner_hints.hpp"

namespace opensu::ui {

std::vector<Prompt> startPrompts(const HintContext& context) {
    std::vector<Prompt> prompts;
    // iiSU mw5.h: ("B", "Back"), ("-", "Details"); here the "-" prompt is the context menu.
    if (context.clearSearch) {
        prompts.push_back(Prompt{"B", "Clear search"});
    } else if (context.back) {
        prompts.push_back(Prompt{"B", "Back"});
    }
    if (context.options) {
        prompts.push_back(Prompt{"-", "Options"});
    } else if (context.forget) {
        prompts.push_back(Prompt{"-", "Forget"});
    }
    return prompts;
}

std::vector<Prompt> endPrompts(const HintContext& context) {
    std::vector<Prompt> prompts;
    // iiSU mw5.k: ("A", "Select"), ("+", "Menu").
    if (context.select) {
        const char* label = context.change ? "Change" : (context.details ? "Details" : "Select");
        prompts.push_back(Prompt{"A", label});
    }
    if (context.menu) {
        prompts.push_back(Prompt{"+", "Menu"});
    }
    return prompts;
}

} // namespace opensu::ui

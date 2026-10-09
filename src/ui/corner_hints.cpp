#include "corner_hints.hpp"

namespace opensu::ui {

std::vector<Prompt> startPrompts(const HintContext& context) {
    std::vector<Prompt> prompts;
    // iiSU mw5.h: ("B", "Back"), ("-", "Details").
    if (context.clearSearch) {
        prompts.push_back(Prompt{"B", "Clear search"});
    } else if (context.back) {
        prompts.push_back(Prompt{"B", "Back"});
    }
    if (context.details) {
        prompts.push_back(Prompt{"-", "Details"});
    } else if (context.options) {
        prompts.push_back(Prompt{"-", "Options"});
    }
    return prompts;
}

std::vector<Prompt> endPrompts(const HintContext& context) {
    std::vector<Prompt> prompts;
    // iiSU mw5.k: ("A", "Select"), ("+", "Menu").
    if (context.select) {
        prompts.push_back(Prompt{"A", "Select"});
    }
    if (context.menu) {
        prompts.push_back(Prompt{"+", "Menu"});
    }
    return prompts;
}

} // namespace opensu::ui

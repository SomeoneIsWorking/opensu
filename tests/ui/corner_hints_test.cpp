// A corner prompt shows only while its button does something.
#include "corner_hints.hpp"

#include <cstdio>
#include <vector>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::ui::HintContext;
using opensu::ui::Prompt;

void startCorner() {
    expect(opensu::ui::startPrompts(HintContext{}).empty(), "nothing to go back to or detail");
    expect(
        (opensu::ui::startPrompts(HintContext{.back = true}) == std::vector<Prompt>{{"B", "Back"}}),
        "Back shows inside a folder only");
    expect((opensu::ui::startPrompts(HintContext{.details = true}) ==
            std::vector<Prompt>{{"-", "Details"}}),
           "Details shows with a game focused only");
    expect(opensu::ui::startPrompts(HintContext{.back = true, .details = true}).size() == 2,
           "both together");
}

void searchAndOptions() {
    expect((opensu::ui::startPrompts(HintContext{.back = true, .clearSearch = true}) ==
            std::vector<Prompt>{{"B", "Clear search"}}),
           "a search being cleared takes B before Back");
    expect((opensu::ui::startPrompts(HintContext{.options = true}) ==
            std::vector<Prompt>{{"-", "Options"}}),
           "a folder tile offers its options");
    expect((opensu::ui::startPrompts(HintContext{.details = true, .options = true}) ==
            std::vector<Prompt>{{"-", "Details"}}),
           "a game's menu is Details, not Options");
}

void endCorner() {
    expect(opensu::ui::endPrompts(HintContext{}).empty(), "an empty slot selects nothing");
    expect((opensu::ui::endPrompts(HintContext{.select = true}) ==
            std::vector<Prompt>{{"A", "Select"}}),
           "Select shows on a tile");
    expect(
        (opensu::ui::endPrompts(HintContext{.menu = true}) == std::vector<Prompt>{{"+", "Menu"}}),
        "Menu shows where START opens one");
    expect(opensu::ui::endPrompts(HintContext{.select = true, .menu = true}).size() == 2,
           "both together");
}

} // namespace

int main() {
    startCorner();
    searchAndOptions();
    endCorner();
    std::printf("corner_hints: all checks passed\n");
    return 0;
}

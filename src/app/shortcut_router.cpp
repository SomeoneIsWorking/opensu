#include "shortcut_router.hpp"

namespace opensu::app {

void ShortcutRouter::route(input::Action action, bool pressed, std::vector<gamepad::Event>& out) {
    if (const std::optional<gamepad::Button> button = input::buttonOf(action)) {
        gamepad::Event event;
        event.button = *button;
        event.pressed = pressed;
        out.push_back(event);
    } else if (pressed && action == input::Action::Quit) {
        hooks_.quit();
    } else if (pressed && action == input::Action::QuickMenu) {
        hooks_.quickMenu();
    } else if (pressed) {
        volume_.act(action);
    }
}

ShortcutRouter::Keys ShortcutRouter::fromKeys(const input::KeySource& keys) {
    Keys result;
    for (const input::ActionEdge& edge : shortcuts_.keyEdges(keys)) {
        result.any = true;
        route(edge.action, edge.pressed, result.events);
    }
    return result;
}

std::vector<gamepad::Event> ShortcutRouter::fromCombo(const input::Combo& combo) {
    std::vector<gamepad::Event> events;
    if (const std::optional<input::Action> action = shortcuts_.actionFor(combo)) {
        route(*action, true, events);
        route(*action, false, events);
    }
    return events;
}

std::vector<gamepad::Event> ShortcutRouter::fromPad(const std::vector<gamepad::Event>& events) {
    input::PadChords::Result result = chords_.feed(events);
    for (const input::Action action : result.actions) {
        std::vector<gamepad::Event> none;
        route(action, true, none);
    }
    return std::move(result.events);
}

std::vector<gamepad::Event> ShortcutRouter::fromGame(session::GameKeys& keys) {
    std::vector<gamepad::Event> events;
    for (const input::Action action : keys.poll(shortcuts_)) {
        route(action, true, events);
        route(action, false, events);
    }
    return events;
}

} // namespace opensu::app

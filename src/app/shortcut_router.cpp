#include "shortcut_router.hpp"

namespace opensu::app {

void ShortcutRouter::route(input::Action action, bool pressed, std::vector<gamepad::Event>& out) {
    if (const std::optional<gamepad::Button> button = input::buttonOf(action)) {
        gamepad::Event event;
        event.button = *button;
        event.pressed = pressed;
        out.push_back(event);
    } else if (pressed) {
        perform(action);
    }
}

void ShortcutRouter::perform(input::Action action) {
    using input::Action;
    const auto retro = [this](RetroSession::Command command) {
        onGame([this, command] {
            const RetroSession::Outcome outcome = retro_.perform(command);
            hooks_.say(outcome.text, outcome.error);
        });
    };
    switch (action) {
    case Action::Quit:
        hooks_.quit();
        break;
    case Action::QuickMenu:
        hooks_.quickMenu();
        break;
    case Action::VolumeUp:
    case Action::VolumeDown:
    case Action::VolumeMute:
        volume_.act(action);
        break;
    case Action::CloseGame:
        onGame(hooks_.closeGame);
        break;
    case Action::SaveState:
        retro(RetroSession::Command::SaveState);
        break;
    case Action::LoadState:
        retro(RetroSession::Command::LoadState);
        break;
    case Action::ResetGame:
        retro(RetroSession::Command::Reset);
        break;
    case Action::Devices:
        hooks_.openDevices();
        break;
    case Action::Home:
        hooks_.goHome();
        break;
    case Action::Downloads:
        hooks_.openDownloads();
        break;
    // The button actions come out as pad events in route.
    case Action::Up:
    case Action::Down:
    case Action::Left:
    case Action::Right:
    case Action::Confirm:
    case Action::Back:
    case Action::X:
    case Action::Y:
    case Action::Select:
    case Action::Start:
    case Action::L1:
    case Action::R1:
    case Action::Search:
    case Action::Guide:
        break;
    }
}

void ShortcutRouter::onGame(const std::function<void()>& act) {
    if (hooks_.gameRunning()) {
        act();
    } else {
        hooks_.say("no game is running", true);
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

std::vector<gamepad::Event> ShortcutRouter::fromPad(const std::vector<gamepad::Event>& events,
                                                    input::PadChords::Clock::time_point at) {
    input::PadChords::Result result = chords_.feed(events, at);
    for (const input::Action action : result.actions) {
        std::vector<gamepad::Event> none;
        route(action, true, none);
    }
    return std::move(result.events);
}

std::vector<gamepad::Event> ShortcutRouter::fromGame(GameOverlay& overlay) {
    std::vector<gamepad::Event> events;
    for (const input::Action action : overlay.gameKeys(shortcuts_)) {
        // Over a game there is nothing to go back to, so Back (ESC) opens the Guide menu.
        const input::Action effective =
            action == input::Action::Back ? input::Action::Guide : action;
        route(effective, true, events);
        route(effective, false, events);
    }
    return events;
}

} // namespace opensu::app

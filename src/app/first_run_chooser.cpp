#include "first_run_chooser.hpp"

#include <string>
#include <vector>

#include "integration_options.hpp"
#include "lucent/log.h"

namespace opensu::app {

void FirstRunChooser::openIfUnchosen() {
    if (integrations_.chosen()) {
        return;
    }
    std::vector<ui::IntegrationOption> options;
    for (const IntegrationEntry& entry : integrationEntries()) {
        options.push_back(ui::IntegrationOption{.id = std::string{entry.id},
                                                .label = std::string{entry.label},
                                                .note = std::string{entry.note},
                                                .on = false});
    }
    lucent::info("integrations", "no integration choices saved; showing the first-run chooser");
    card_.open(std::move(options));
}

void FirstRunChooser::act(gamepad::Button button) {
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        // input-sound.md 3.4 Navigation: a focus move.
        if (card_.move(button == gamepad::Button::Down ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::A:
        sounds_.play(audio::Effect::Navigation);
        if (!card_.toggleFocused()) {
            done();
        }
        break;
    default:
        break;
    }
}

void FirstRunChooser::done() {
    settings::IntegrationChoices choices;
    for (const ui::IntegrationOption& option : card_.options()) {
        if (bool* on = switchOf(choices, option.id)) {
            *on = option.on;
        }
    }
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    card_.close();
    integrations_.choose(choices);
    chosen_();
}

} // namespace opensu::app

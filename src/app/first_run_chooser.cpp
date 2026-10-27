#include "first_run_chooser.hpp"

#include <algorithm>
#include <memory>

#include "first_run_screen.hpp"
#include "integration_options.hpp"
#include "lucent/log.h"

namespace opensu::app {

void FirstRunChooser::openIfUnchosen() {
    if (integrations_.chosen() || active()) {
        return;
    }
    rows_.clear();
    for (const IntegrationEntry& entry : integrationEntries()) {
        rows_.push_back(ui::makeToggle(std::string{entry.id}, std::string{entry.label},
                                       std::string{entry.note}, false));
    }
    lucent::info("integrations", "no integration choices saved; showing the first-run chooser");
    navigator_.push(std::make_unique<FirstRunScreen>(*this));
}

bool FirstRunChooser::active() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    return std::ranges::any_of(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const FirstRunScreen*>(screen) != nullptr;
    });
}

void FirstRunChooser::run(const std::string& id, int step) {
    const auto row = std::ranges::find(rows_, id, &ui::SettingsRow::id);
    if (step == 0 && row != rows_.end()) {
        row->on = !row->on;
    }
}

void FirstRunChooser::done() {
    settings::IntegrationChoices choices;
    for (const ui::SettingsRow& row : rows_) {
        if (bool* on = switchOf(choices, row.id)) {
            *on = row.on;
        }
    }
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto card = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const FirstRunScreen*>(screen) != nullptr;
    });
    if (card != open.end()) {
        navigator_.closeFrom(**card);
    }
    integrations_.choose(choices);
    chosen_();
}

} // namespace opensu::app

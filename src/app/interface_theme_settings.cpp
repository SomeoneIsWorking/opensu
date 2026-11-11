#include "interface_theme_settings.hpp"

#include <vector>

namespace opensu::app {
namespace {

constexpr const char* interfaceThemeId = "appearance.interfacetheme";

/// The id the settings file keeps for `id`: empty for the default.
std::string kept(const std::string& id) {
    return id == theme::defaultInterfaceTheme ? std::string{} : id;
}

} // namespace

ui::SettingsRow InterfaceThemeSettings::row() const {
    const std::string& chosen = preferences_.values().interfaceTheme;
    ui::Options choices;
    std::string note = "How openSU looks and sounds";
    for (const std::shared_ptr<const theme::InterfaceTheme>& theme : themes_.list()) {
        if (kept(theme->id) == chosen) {
            choices.chosen = choices.labels.size();
            if (!theme->note.empty()) {
                note = theme->note;
            }
        }
        choices.labels.push_back(theme->name);
    }
    return ui::makeChoice(interfaceThemeId, "Interface theme", note, std::move(choices));
}

bool InterfaceThemeSettings::owns(const std::string& id) {
    return id == interfaceThemeId;
}

void InterfaceThemeSettings::run(const std::string& /*id*/, int step) {
    std::vector<std::string> ids;
    for (const std::shared_ptr<const theme::InterfaceTheme>& theme : themes_.list()) {
        ids.push_back(kept(theme->id));
    }
    if (ids.empty()) {
        return;
    }
    settings::Settings& values = preferences_.values();
    values.interfaceTheme = ui::stepped(ids, values.interfaceTheme, step);
    preferences_.save();
    changed_();
}

std::shared_ptr<const theme::InterfaceTheme> InterfaceThemeSettings::current() {
    const std::string& chosen = preferences_.values().interfaceTheme;
    if (current_ == nullptr || chosen != opened_) {
        current_ = themes_.openOrDefault(chosen);
        opened_ = chosen;
    }
    return current_;
}

} // namespace opensu::app

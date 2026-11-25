#include "interface_size_setting.hpp"

#include <utility>

namespace opensu::app {

ui::SettingsRow interfaceSizeRow(std::string id, const settings::Settings& values) {
    ui::Options scales;
    for (int percent = settings::minUiScale; percent <= settings::maxUiScale;
         percent += settings::uiScaleStep) {
        if (percent <= values.uiScale) {
            scales.chosen = scales.labels.size();
        }
        scales.labels.push_back(std::to_string(percent) + "%");
    }
    return ui::makeChoice(std::move(id), "Interface size", {}, std::move(scales));
}

void stepInterfaceSize(Preferences& preferences, int step) {
    settings::Settings& values = preferences.values();
    values.uiScale = settings::steppedUiScale(values.uiScale, step);
    preferences.save();
}

} // namespace opensu::app

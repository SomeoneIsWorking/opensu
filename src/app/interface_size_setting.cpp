#include "interface_size_setting.hpp"

#include <utility>

namespace opensu::app {

ui::SettingsRow interfaceSizeRow(std::string id, const settings::Settings& values) {
    return ui::makeSlider(std::move(id), "Interface size", {},
                          ui::SliderRange{.level = values.uiScale,
                                          .low = settings::minUiScale,
                                          .high = settings::maxUiScale,
                                          .step = settings::uiScaleStep,
                                          .unit = "%"});
}

void setInterfaceSize(Preferences& preferences, int percent) {
    preferences.values().uiScale = settings::offeredUiScale(percent);
    preferences.save();
}

} // namespace opensu::app

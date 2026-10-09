#include "launch_panel.hpp"

#include <algorithm>
#include <utility>

namespace opensu::ui {

void LaunchPanel::open(std::string title) {
    title_ = std::move(title);
    line_ = "Starting";
    fraction_.reset();
    hints_ = {PanelHint{"B", "Cancel"}};
    busy_ = true;
    open_ = true;
}

void LaunchPanel::setHints(std::vector<PanelHint> hints) {
    hints_ = std::move(hints);
}

void LaunchPanel::update(std::string line, std::optional<double> fraction, bool busy) {
    line_ = std::move(line);
    busy_ = busy;
    fraction_.reset();
    if (fraction) {
        fraction_ = std::clamp(*fraction, 0.0, 1.0);
    }
}

void LaunchPanel::close() noexcept {
    open_ = false;
}

} // namespace opensu::ui

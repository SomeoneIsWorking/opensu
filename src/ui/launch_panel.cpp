#include "launch_panel.hpp"

#include <algorithm>
#include <utility>

namespace iideck::ui {

void LaunchPanel::open(std::string title) {
    title_ = std::move(title);
    line_ = "Starting";
    fraction_.reset();
    open_ = true;
}

void LaunchPanel::update(std::string line, std::optional<double> fraction) {
    line_ = std::move(line);
    fraction_.reset();
    if (fraction) {
        fraction_ = std::clamp(*fraction, 0.0, 1.0);
    }
}

void LaunchPanel::close() noexcept {
    open_ = false;
}

} // namespace iideck::ui

// preferences — what the player chose, as the shell holds it and keeps it: the loaded settings,
// and saving them with a toast when the file cannot be written.
#pragma once

#include <functional>
#include <string>

#include "settings/settings.hpp"

namespace opensu::app {

class Preferences {
  public:
    /// `onFailure` is told why when a save cannot be written.
    Preferences(settings::Store store, std::function<void(const std::string&)> onFailure)
        : store_{std::move(store)}, onFailure_{std::move(onFailure)}, values_{store_.load()} {
    }

    [[nodiscard]] settings::Settings& values() noexcept {
        return values_;
    }
    [[nodiscard]] const settings::Settings& values() const noexcept {
        return values_;
    }

    /// Writes the settings; on failure logs it and reports it, and keeps the old file.
    void save();

  private:
    settings::Store store_;
    std::function<void(const std::string&)> onFailure_;
    settings::Settings values_;
};

} // namespace opensu::app

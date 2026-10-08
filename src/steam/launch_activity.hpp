// launch_activity — what Steam's client says about game launches and running games. A
// recorder installed in the SharedJSContext keeps the latest game action per app and each
// app's running state from Steam's own notifications; reading returns that record.
#pragma once

#include <map>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "devtools.hpp"
#include "launch/steam_gate.hpp"

namespace iideck::steam {

/// Every app the record names, by app id.
using Activities = std::map<std::string, launch::SteamAppActivity, std::less<>>;

/// The recorder's state as `LaunchActivity::read` returns it.
[[nodiscard]] Activities parseActivities(const nlohmann::json& record);

class LaunchActivity {
  public:
    explicit LaunchActivity(DevTools& devTools);

    /// Installs the recorder if Steam has none yet, then reads it. Nothing when Steam cannot
    /// be asked; `error` says why.
    [[nodiscard]] std::optional<Activities> read(std::string& error);

  private:
    DevTools& devTools_;
};

} // namespace iideck::steam

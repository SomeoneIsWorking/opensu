// uninstaller — removes a Steam app the way Steam's own library does, through
// `SteamClient.Installs.OpenUninstallWizard([appId], true)` (`steamui/chunk~2dcc5aaf7.js`). Steam
// deletes the files and the app manifest itself; the caller watches the manifest go.
#pragma once

#include <string>
#include <string_view>

#include "devtools.hpp"

namespace opensu::steam {

class Uninstaller {
  public:
    explicit Uninstaller(DevTools& devTools) : devTools_{devTools} {
    }

    /// Asks Steam to uninstall `appId`. Empty when Steam took the request, else why not; a bad
    /// app id asks nothing.
    [[nodiscard]] std::string uninstall(std::string_view appId);

  private:
    DevTools& devTools_;
};

} // namespace opensu::steam

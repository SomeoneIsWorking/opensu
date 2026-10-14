#include "uninstaller.hpp"

#include "lucent/log.h"
#include "steam_json.hpp"

namespace opensu::steam {

std::string Uninstaller::uninstall(std::string_view appId) {
    if (!isAppId(appId)) {
        return "not a Steam app id: " + std::string{appId};
    }
    std::string error;
    // The second argument is what Steam's library passes from its own Uninstall menu entry.
    if (!devTools_.evaluate("SteamClient.Installs.OpenUninstallWizard([" + std::string{appId} +
                                "], true)",
                            error)) {
        return "Steam would not uninstall it: " + error;
    }
    lucent::info("steam", "uninstalling {}", appId);
    return {};
}

} // namespace opensu::steam

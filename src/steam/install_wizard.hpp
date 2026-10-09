// install_wizard — installs a Steam app the way Steam's own library does: open its install
// wizard, take the default library folder, and continue until the app is queued to download.
// Licence agreements are never accepted without the player.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "devtools.hpp"

namespace opensu::steam {

/// A licence agreement an app's install waits on.
struct Eula {
    std::string id;
    std::string url;
    int version{0};
};

/// Where the wizard stands.
struct InstallStep {
    enum class Kind : std::uint8_t {
        /// Steam is still preparing; poll again.
        Working,
        /// The player must accept `eulas` before it continues.
        NeedsEula,
        /// Queued to download; the wizard is finished.
        Queued,
        /// Ended without installing; `failure` says why.
        Failed,
    };
    Kind kind{Kind::Working};
    std::vector<Eula> eulas;
    std::string failure;
};

/// What to do at one of Steam's EInstallMgrState values.
enum class WizardAction : std::uint8_t {
    Wait,
    Continue,
    AskEula,
    Done,
    Fail,
    /// A step only Steam's own dialogs can answer (CD key, password, sign-up).
    Unsupported,
};
[[nodiscard]] WizardAction actionFor(int installState) noexcept;

class InstallWizard {
  public:
    explicit InstallWizard(DevTools& devTools);

    /// Opens the wizard for `appId`. Fails when Steam cannot be asked.
    [[nodiscard]] InstallStep open(std::string_view appId);
    /// Advances the wizard as far as it can go without the player.
    [[nodiscard]] InstallStep poll();
    /// Records the player's acceptance of `eulas` for the open app and continues.
    [[nodiscard]] InstallStep accept(const std::vector<Eula>& eulas);
    /// Closes the wizard without installing.
    void cancel();

  private:
    [[nodiscard]] InstallStep failed(std::string reason);

    DevTools& devTools_;
    std::string appId_;
    /// The wizard was told to continue; it returns to no state once it has handed off.
    bool continued_{false};
};

} // namespace opensu::steam

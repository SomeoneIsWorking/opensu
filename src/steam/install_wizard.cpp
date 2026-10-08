#include "install_wizard.hpp"

#include <algorithm>
#include <cctype>
#include <optional>

#include "lucent/log.h"

namespace iideck::steam {
namespace {

using nlohmann::json;

constexpr std::string_view readState =
    "SteamClient.Installs.GetInstallManagerInfo().then(info => ({ state: info.eInstallState, "
    "app: info.currentAppID, error: info.errorDetail }))";

// Steam's EInstallMgrState (steamui library.js).
enum InstallState : int {
    None = 0,
    Setup = 1,
    WaitLicense = 2,
    FreeLicense = 3,
    ShowCdKey = 4,
    WaitAppInfo = 5,
    ShowPassword = 6,
    ShowConfig = 7,
    ShowEulas = 8,
    CreateApps = 9,
    ReadFromMedia = 10,
    ShowChangeMedia = 11,
    WaitLegacyCdKeys = 12,
    ShowSignup = 13,
    Complete = 14,
    Failed = 15,
    Canceled = 16,
};

bool numeric(std::string_view text) {
    return !text.empty() && std::ranges::all_of(text, [](char c) {
        return std::isdigit(static_cast<unsigned char>(c)) != 0;
    });
}

} // namespace

WizardAction actionFor(int installState) noexcept {
    switch (installState) {
    case None:
    case Setup:
    case WaitLicense:
    case WaitAppInfo:
    case CreateApps:
    case ReadFromMedia:
    case WaitLegacyCdKeys:
        return WizardAction::Wait;
    case ShowConfig:
        return WizardAction::Continue;
    case ShowEulas:
        return WizardAction::AskEula;
    case Complete:
        return WizardAction::Done;
    case Failed:
    case Canceled:
        return WizardAction::Fail;
    case FreeLicense:
    case ShowCdKey:
    case ShowPassword:
    case ShowChangeMedia:
    case ShowSignup:
        break;
    }
    return WizardAction::Unsupported;
}

InstallWizard::InstallWizard(DevTools& devTools) : devTools_{devTools} {
}

InstallStep InstallWizard::failed(std::string reason) {
    lucent::warn("steam", "install of {} failed: {}", appId_, reason);
    return InstallStep{.kind = InstallStep::Kind::Failed, .failure = std::move(reason)};
}

InstallStep InstallWizard::open(std::string_view appId) {
    if (!numeric(appId)) {
        return failed("not a Steam app id: " + std::string{appId});
    }
    appId_ = std::string{appId};
    continued_ = false;
    std::string error;
    if (!devTools_.evaluate("SteamClient.Installs.OpenInstallWizard([" + appId_ + "])", error)) {
        return failed("Steam did not open its installer: " + error);
    }
    lucent::info("steam", "opened Steam's installer for {}", appId_);
    return poll();
}

InstallStep InstallWizard::poll() {
    std::string error;
    const std::optional<json> info = devTools_.evaluate(readState, error);
    if (!info || !info->is_object()) {
        return failed("Steam's installer stopped answering: " + error);
    }
    const int state = info->value("state", 0);
    const std::string app = std::to_string(info->value("app", 0));
    // Before the wizard has loaded the app, and after it hands off, it reports no app.
    if (state != None && app != appId_) {
        return failed("Steam's installer moved on to app " + app);
    }
    if (state == None && continued_) {
        lucent::info("steam", "{} is queued to download", appId_);
        return InstallStep{.kind = InstallStep::Kind::Queued};
    }
    switch (actionFor(state)) {
    case WizardAction::Wait:
        return InstallStep{};
    case WizardAction::Continue:
        if (!devTools_.evaluate("SteamClient.Installs.ContinueInstall()", error)) {
            return failed("Steam's installer would not continue: " + error);
        }
        continued_ = true;
        return InstallStep{};
    case WizardAction::AskEula: {
        const std::optional<json> list =
            devTools_.evaluate("SteamClient.Apps.LoadEula(" + appId_ + ")", error);
        if (!list || !list->is_array()) {
            return failed("Steam did not list the licence agreements: " + error);
        }
        InstallStep step{.kind = InstallStep::Kind::NeedsEula};
        for (const json& eula : *list) {
            step.eulas.push_back(Eula{.id = eula.value("id", ""),
                                      .url = eula.value("url", ""),
                                      .version = eula.value("version", 0)});
        }
        return step;
    }
    case WizardAction::Done:
        lucent::info("steam", "{} is queued to download", appId_);
        return InstallStep{.kind = InstallStep::Kind::Queued};
    case WizardAction::Fail: {
        const std::string detail = info->value("error", "");
        return failed(detail.empty() ? "Steam cancelled the install" : detail);
    }
    case WizardAction::Unsupported:
        cancel();
        return failed("this game needs Steam's own install dialog (state " + std::to_string(state) +
                      ")");
    }
    return InstallStep{};
}

InstallStep InstallWizard::accept(const std::vector<Eula>& eulas) {
    std::string error;
    for (const Eula& eula : eulas) {
        const json call = json::array({std::stoi(appId_), eula.id, eula.version});
        const std::string args = call.dump();
        // The array's brackets become the call's parentheses.
        if (!devTools_.evaluate("SteamClient.Apps.MarkEulaAccepted(" +
                                    args.substr(1, args.size() - 2) + ")",
                                error)) {
            return failed("Steam did not record the licence agreement: " + error);
        }
        lucent::info("steam", "player accepted {} for {}", eula.id, appId_);
    }
    if (!devTools_.evaluate("SteamClient.Installs.ContinueInstall()", error)) {
        return failed("Steam's installer would not continue: " + error);
    }
    continued_ = true;
    return InstallStep{};
}

void InstallWizard::cancel() {
    std::string error;
    if (!devTools_.evaluate("SteamClient.Installs.CancelInstall()", error)) {
        lucent::warn("steam", "could not cancel Steam's installer: {}", error);
    }
}

} // namespace iideck::steam

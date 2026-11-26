#include "integration_wizard.hpp"

#include <memory>

#include "fumarole_qr_screen.hpp"
#include "integration_options.hpp"
#include "row_page_screen.hpp"

namespace opensu::app {
namespace {

using settings::SteamKind;
using settings::SteamMode;

constexpr std::string_view kindFumaroleId = "wizard.kind.fumarole";
constexpr std::string_view kindClientId = "wizard.kind.client";
/// A mode's row is this and the mode's key.
constexpr std::string_view modePrefix = "wizard.mode.";
constexpr std::string_view loginQrId = "wizard.login.qr";
constexpr std::string_view loginSteamId = "wizard.login.steam";
constexpr std::string_view loginLaterId = "wizard.login.later";
/// Every step's page id starts with this.
constexpr std::string_view pagePrefix = "wizard/";
constexpr std::string_view qrPageId = "wizard/fumarole-qr";

std::string_view stepId(IntegrationWizard::Step step) {
    switch (step) {
    case IntegrationWizard::Step::Mode:
        return "steam-mode";
    case IntegrationWizard::Step::Login:
        return "steam-login";
    case IntegrationWizard::Step::Kind:
        break;
    }
    return "steam-kind";
}

ui::SettingsRow action(std::string_view id, std::string label, std::string note) {
    return ui::makeRow(std::string{id}, ui::RowKind::Action, std::move(label), std::move(note), {});
}

} // namespace

void IntegrationWizard::start(std::string_view entry) {
    staged_ = integrations_.choices();
    signingIn_ = false;
    bool* on = switchOf(staged_, entry);
    if (on == nullptr) {
        return;
    }
    *on = true;
    if (entry == "steam") {
        if (integrations_.modesOffered().empty()) {
            staged_.steamKind = SteamKind::Fumarole;
            show(Step::Login);
        } else {
            show(Step::Kind);
        }
        return;
    }
    hooks_.add(staged_);
    if (entry == "epic" || entry == "gog") {
        hooks_.signIn(entry == "epic" ? Store::Epic : Store::Gog);
        return;
    }
    for (const IntegrationEntry& offered : integrationEntries()) {
        if (offered.id == entry) {
            hooks_.toast(std::string{offered.label} + " added", false);
        }
    }
}

ui::SettingsCategory IntegrationWizard::page(Step step) const {
    ui::SettingsCategory page{std::string{stepId(step)}, {}, {}};
    switch (step) {
    case Step::Kind:
        page.label = "How should openSU get Steam games?";
        page.rows.push_back(action(kindFumaroleId, "fumarole",
                                   "Installs and plays Steam's games without the Steam client"));
        page.rows.push_back(action(kindClientId, "Steam client",
                                   "Steam itself, with its overlay, friends and Remote Play"));
        break;
    case Step::Mode:
        page.label = "How should Steam run?";
        for (const SteamMode mode : integrations_.modesOffered()) {
            page.rows.push_back(action(std::string{modePrefix} + std::string{settings::key(mode)},
                                       std::string{settings::label(mode)},
                                       std::string{steamModeNote(mode)}));
        }
        break;
    case Step::Login:
        page.label = "Sign in to Steam";
        page.rows.push_back(
            action(loginQrId, "Sign in with the Steam app", "Scan a code with your phone"));
        page.rows.push_back(action(loginSteamId, "Use this PC's Steam login",
                                   "The account Steam on this PC remembers"));
        page.rows.push_back(
            action(loginLaterId, "Sign in later", "Steam's games show once fumarole is signed in"));
        break;
    }
    return page;
}

void IntegrationWizard::show(Step step) {
    navigator_.push(std::make_unique<RowPageScreen>(
        std::string{pagePrefix} + std::string{stepId(step)},
        [this, step] {
            return page(step);
        },
        *this, chrome_));
}

void IntegrationWizard::run(const std::string& id, int step) {
    if (step != 0) {
        return;
    }
    if (id == kindFumaroleId) {
        staged_.steamKind = SteamKind::Fumarole;
        show(Step::Login);
    } else if (id == kindClientId) {
        staged_.steamKind = SteamKind::Client;
        const std::vector<SteamMode> modes = integrations_.modesOffered();
        if (integrations_.loginSession() || modes.size() < 2) {
            staged_.steamMode = settings::Integrations::defaultMode(integrations_.loginSession());
            finish("Steam added; it asks for your login when it starts");
        } else {
            show(Step::Mode);
        }
    } else if (id.starts_with(modePrefix)) {
        staged_.steamMode = settings::steamModeOf(std::string_view{id}.substr(modePrefix.size()));
        finish("Steam added; it asks for your login when it starts");
    } else if (id == loginQrId) {
        signingIn_ = true;
        fumarole_.start(FumaroleSignIn::Way::Qr);
        navigator_.push(std::make_unique<FumaroleQrScreen>(fumarole_, images_, chrome_));
    } else if (id == loginSteamId) {
        signingIn_ = true;
        fumarole_.start(FumaroleSignIn::Way::FromSteam);
        hooks_.toast("Signing in with this PC's Steam login", false);
    } else if (id == loginLaterId) {
        finish("Steam added");
    }
}

void IntegrationWizard::service() {
    if (!signingIn_) {
        return;
    }
    const std::optional<FumaroleSignIn::Result> result = fumarole_.take();
    if (!result) {
        return;
    }
    signingIn_ = false;
    if (result->ok) {
        finish("Steam added. " + result->message);
        return;
    }
    closeFrom(qrPageId);
    hooks_.toast(result->message, true);
}

void IntegrationWizard::closeFrom(std::string_view id) {
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->id().starts_with(id)) {
            navigator_.closeFrom(*screen);
            return;
        }
    }
}

void IntegrationWizard::finish(const std::string& text) {
    closeFrom(pagePrefix);
    hooks_.add(staged_);
    hooks_.toast(text, false);
}

} // namespace opensu::app

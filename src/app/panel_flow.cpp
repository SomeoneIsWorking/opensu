#include "panel_flow.hpp"

#include <utility>

#include "config/config.hpp"

namespace opensu::app {

PanelFlow::PanelFlow(ui::Shell& shell, audio::SoundPlayer& sounds, steam::Client& steam,
                     Hooks hooks)
    : shell_{shell}, sounds_{sounds},
      install_{steam, "legendary", GogInstallJob::Options{.dataDir = config::read().dataDir},
               hooks.installFolder},
      hooks_{std::move(hooks)} {
}

void PanelFlow::act(gamepad::Button button) {
    switch (use_) {
    case Use::Launch:
        if (button == gamepad::Button::B) {
            hooks_.cancelLaunch();
        }
        break;
    case Use::OfferInstall:
        if (button == gamepad::Button::A || button == gamepad::Button::X) {
            const std::size_t choice = button == gamepad::Button::A ? 0 : 1;
            if (choice < offered_.size()) {
                startInstall(offered_[choice]);
            }
        } else if (button == gamepad::Button::B) {
            dismiss();
            offered_.clear();
        }
        break;
    case Use::Install:
        // The download goes on without the panel.
        if (button == gamepad::Button::B) {
            dismiss();
        }
        break;
    case Use::Eula:
        if (button == gamepad::Button::A || button == gamepad::Button::B) {
            const bool accepted = button == gamepad::Button::A;
            install_.decide(accepted);
            if (accepted) {
                use_ = Use::Install;
                shell_.launchPanel().update("Starting the download", std::nullopt);
                shell_.launchPanel().setHints({{"B", "Hide"}});
            } else {
                dismiss();
            }
        }
        break;
    case Use::None:
        break;
    }
}

void PanelFlow::showLaunch(const std::string& title) {
    shell_.launchPanel().open(title);
    use_ = Use::Launch;
}

void PanelFlow::endLaunch() {
    if (use_ == Use::Launch) {
        shell_.launchPanel().close();
        use_ = Use::None;
    }
}

void PanelFlow::open(const std::string& title) {
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    shell_.launchPanel().open(title);
}

void PanelFlow::dismiss() {
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    shell_.launchPanel().close();
    use_ = Use::None;
}

void PanelFlow::startInstall(const library::Game& game) {
    if (const std::string refused = install_.folderRefusal(game.source); !refused.empty()) {
        shell_.launchPanel().close();
        use_ = Use::None;
        offered_.clear();
        shell_.setToast("cannot install " + game.title + ": " + refused, true);
        return;
    }
    if (install_.start(game)) {
        use_ = Use::Install;
        shell_.launchPanel().update("Starting", std::nullopt);
        shell_.launchPanel().setHints({{"B", "Hide"}});
    } else {
        shell_.launchPanel().close();
        use_ = Use::None;
        shell_.setToast(install_.title() + " is still installing", true);
    }
    offered_.clear();
}

void PanelFlow::offerInstall(const library::Game& game, std::vector<library::Game> copies) {
    const std::string unsupported =
        std::string{library::label(copies.front().source)} + " installs are not supported yet";
    std::erase_if(copies, [](const library::Game& copy) {
        return copy.installed || !Installs::supports(copy.source);
    });
    if (copies.empty()) {
        shell_.setToast(unsupported, true);
        return;
    }
    if (install_.running()) {
        shell_.setToast(install_.title() + " is still installing", true);
        return;
    }
    offered_ = std::move(copies);
    use_ = Use::OfferInstall;
    open(game.title);
    if (offered_.size() == 1) {
        shell_.launchPanel().update("Not installed", std::nullopt, false);
        shell_.launchPanel().setHints({{"A", "Install"}, {"B", "Cancel"}});
        return;
    }
    // Two stores can install it: A is the first, X the second.
    shell_.launchPanel().update("Install from", std::nullopt, false);
    shell_.launchPanel().setHints({{"A", std::string{library::label(offered_[0].source)}},
                                   {"X", std::string{library::label(offered_[1].source)}},
                                   {"B", "Cancel"}});
}

void PanelFlow::presentEula() {
    eulaWaiting_ = false;
    use_ = Use::Eula;
    const std::string title = install_.title();
    open(title);
    shell_.launchPanel().update("Installing " + title + " means accepting its licence agreement",
                                std::nullopt, false);
    shell_.launchPanel().setHints({{"A", "Accept"}, {"B", "Decline"}});
}

void PanelFlow::service() {
    if (eulaWaiting_ && use_ != Use::Launch) {
        presentEula();
    }
    const std::optional<InstallJob::Report> report = install_.take();
    if (!report) {
        return;
    }
    if (report->licence) {
        if (use_ == Use::Launch) {
            eulaWaiting_ = true;
        } else {
            presentEula();
        }
        return;
    }
    if (!report->finished) {
        if (use_ == Use::Install) {
            shell_.launchPanel().update(report->line, report->fraction);
        }
        return;
    }
    if (use_ == Use::Install || use_ == Use::Eula) {
        shell_.launchPanel().close();
        use_ = Use::None;
    }
    eulaWaiting_ = false;
    const std::string title = install_.title();
    if (report->failure.empty()) {
        hooks_.reloadCatalog();
        shell_.setToast(title + " installed");
    } else {
        shell_.setToast("cannot install " + title + ": " + report->failure, true);
    }
}

} // namespace opensu::app

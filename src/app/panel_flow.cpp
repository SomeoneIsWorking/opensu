#include "panel_flow.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "gpu2d/ui/screen.hpp"

namespace opensu::app {
namespace {

/// The card's id on the stack.
constexpr std::string_view cardId = "launch";

/// The licence question, told apart from other dialogs on the stack.
class LicenceDialog final : public gpu2d::ui::Dialog {
  public:
    using Dialog::Dialog;
};

} // namespace

PanelFlow::PanelFlow(gpu2d::ui::Navigator& navigator, audio::SdlSoundPlayer& sounds,
                     steam::Client& steam, const std::filesystem::path& dataDir, Hooks hooks)
    : navigator_{navigator}, sounds_{sounds},
      install_{steam, "legendary", GogInstallJob::Options{.dataDir = dataDir}, hooks.installFolder,
               hooks.playing},
      hooks_{std::move(hooks)} {
}

const gpu2d::ui::Screen* PanelFlow::card() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return screen->id() == cardId;
    });
    return found == open.end() ? nullptr : *found;
}

bool PanelFlow::active() const {
    return card() != nullptr;
}

bool PanelFlow::licenceWaits() const {
    return forLaunch() || hooks_.gameRunning();
}

void PanelFlow::showCard(const std::string& title, std::string line) {
    shown_ = ui::ProgressCard{.title = title, .line = std::move(line), .fraction = {}, .leave = {}};
    if (card() != nullptr) {
        return;
    }
    navigator_.push(std::make_unique<ui::ProgressScreen>(
        std::string{cardId},
        [this] {
            return shown_;
        },
        [this] {
            leave();
        }));
}

void PanelFlow::closeCard() {
    if (const gpu2d::ui::Screen* open = card()) {
        navigator_.closeFrom(*open);
    }
}

void PanelFlow::leave() {
    if (use_ == Use::Launch) {
        hooks_.cancelLaunch();
        // Nothing is left to press while the launch winds down.
        shown_.leave.clear();
        use_ = Use::Cancelling;
    } else if (use_ == Use::Install) {
        // The download goes on without the card.
        // input-sound.md 3.4 Close: the player dismisses a panel.
        sounds_.play(audio::Effect::Close);
        closeCard();
        use_ = Use::None;
    }
}

void PanelFlow::showLaunch(const std::string& title) {
    showCard(title, "Starting");
    shown_.leave = "Cancel";
    use_ = Use::Launch;
}

void PanelFlow::update(std::string line, std::optional<double> fraction) {
    shown_.line = std::move(line);
    shown_.fraction.reset();
    if (fraction) {
        shown_.fraction = static_cast<float>(std::clamp(*fraction, 0.0, 1.0));
    }
}

void PanelFlow::endLaunch() {
    if (forLaunch()) {
        closeCard();
        use_ = Use::None;
    }
}

void PanelFlow::startInstall(const library::Game& game) {
    if (const std::string refused = install_.folderRefusal(game.source); !refused.empty()) {
        hooks_.say("cannot install " + game.title + ": " + refused, true);
        return;
    }
    if (!install_.start(game)) {
        hooks_.say(install_.busy(), true);
        return;
    }
    use_ = Use::Install;
    showCard(game.title, "Starting");
    shown_.leave = "Hide";
}

void PanelFlow::offerInstall(const library::Game& game, std::vector<library::Game> copies) {
    const std::string unsupported =
        std::string{library::label(copies.front().source)} + " installs are not supported yet";
    std::erase_if(copies, [](const library::Game& copy) {
        return copy.installed || !Installs::supports(copy.source);
    });
    if (copies.empty()) {
        hooks_.say(unsupported, true);
        return;
    }
    if (const std::string busy = install_.busy(); !busy.empty()) {
        hooks_.say(busy, true);
        return;
    }
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    for (const library::Game& copy : copies) {
        const std::string label =
            copies.size() == 1 ? "Install" : "From " + std::string{library::label(copy.source)};
        choices.push_back(gpu2d::ui::Dialog::Choice{label,
                                                    [this, copy = copy] {
                                                        startInstall(copy);
                                                    },
                                                    gpu2d::ui::ButtonKind::Primary});
    }
    choices.push_back(gpu2d::ui::Dialog::Choice{"Cancel", {}, gpu2d::ui::ButtonKind::Normal, true});
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>(
        game.title, copies.size() == 1 ? "Not installed." : "Not installed. Install it from:",
        std::move(choices)));
}

void PanelFlow::presentEula() {
    eulaWaiting_ = false;
    use_ = Use::Eula;
    const std::string title = install_.title();
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    std::vector<gpu2d::ui::Dialog::Choice> choices{
        gpu2d::ui::Dialog::Choice{"Accept",
                                  [this, title = title] {
                                      install_.decide(true);
                                      use_ = Use::Install;
                                      showCard(title, "Starting the download");
                                      shown_.leave = "Hide";
                                  },
                                  gpu2d::ui::ButtonKind::Primary},
        gpu2d::ui::Dialog::Choice{"Decline",
                                  [this] {
                                      install_.decide(false);
                                      use_ = Use::None;
                                  },
                                  gpu2d::ui::ButtonKind::Normal, true}};
    navigator_.push(std::make_unique<LicenceDialog>(
        title, "Installing " + title + " means accepting its licence agreement.",
        std::move(choices)));
}

void PanelFlow::service() {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const bool asking = std::ranges::any_of(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const LicenceDialog*>(screen) != nullptr;
    });
    if (use_ == Use::Eula && !asking) {
        // Something closed the question unanswered, as a game starting does: it asks again.
        eulaWaiting_ = true;
        use_ = Use::None;
    }
    if (eulaWaiting_ && !licenceWaits()) {
        presentEula();
    }
    const std::optional<InstallJob::Report> report = install_.take();
    if (!report) {
        return;
    }
    if (const std::optional<Installs::Status> status = install_.status();
        status && status->work != Installs::Work::Install) {
        workEnded(*report, status->work);
        return;
    }
    if (report->licence) {
        if (licenceWaits()) {
            eulaWaiting_ = true;
        } else {
            presentEula();
        }
        return;
    }
    if (!report->finished) {
        if (use_ == Use::Install) {
            update(report->line, report->fraction);
        }
        return;
    }
    if (use_ == Use::Install) {
        closeCard();
        use_ = Use::None;
    }
    eulaWaiting_ = false;
    const std::string title = install_.title();
    if (report->cancelled) {
        hooks_.say(title + " install cancelled", false);
    } else if (report->failure.empty()) {
        hooks_.reloadCatalog();
        hooks_.say(title + " installed", false);
    } else {
        hooks_.say("cannot install " + title + ": " + report->failure, true);
    }
}

void PanelFlow::workEnded(const InstallJob::Report& report, Installs::Work work) {
    if (!report.finished) {
        return;
    }
    const std::string title = install_.title();
    const std::string verb = work == Installs::Work::Update ? "update" : "uninstall";
    if (report.cancelled) {
        hooks_.say(title + " " + verb + " stopped", true);
    } else if (report.failure.empty()) {
        hooks_.reloadCatalog();
        // An uninstall's and Steam's line says how it ended; a store's update says nothing.
        hooks_.say(title + " " + (report.line.empty() ? "updated" : report.line), false);
    } else {
        hooks_.say("cannot " + verb + " " + title + ": " + report.failure, true);
    }
}

} // namespace opensu::app

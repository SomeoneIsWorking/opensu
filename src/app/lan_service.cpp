#include "lan_service.hpp"

#include <algorithm>
#include <utility>

#include "gpu2d/ui/screen.hpp"

#include "lan/dns_sd.hpp"
#include "launch/emulator_running.hpp"
#include "lucent/log.h"
#include "ui/progress_screen.hpp"

namespace opensu::app {
namespace {

using Phase = lan::GuestPairing::Phase;

constexpr std::string_view cardId = "lan-pairing";
/// How often who is found and paired is looked at.
constexpr std::chrono::seconds lookEvery{1};

} // namespace

LanService::LanService(Places places, Preferences& preferences, gpu2d::ui::Navigator& navigator,
                       audio::SdlSoundPlayer& sounds, std::shared_ptr<lan::RemoteLibrary> remote,
                       bool active, Hooks hooks)
    : places_{std::move(places)}, preferences_{preferences}, navigator_{navigator}, sounds_{sounds},
      remote_{std::move(remote)}, hooks_{std::move(hooks)} {
    if (active) {
        start();
    }
}

LanService::~LanService() {
    node_.reset();
}

std::string LanService::name() const {
    const std::string& chosen = preferences_.values().lanName;
    return lan::shownName(chosen.empty() ? lan::machineName() : chosen);
}

void LanService::setActive(bool on) {
    if (on == (node_ != nullptr)) {
        return;
    }
    if (on) {
        start();
    } else {
        stop();
        lucent::info("lan", "LAN is off");
    }
    hooks_.peersChanged();
}

void LanService::rename() {
    if (node_ == nullptr) {
        return;
    }
    stop();
    start();
    hooks_.peersChanged();
}

void LanService::start() {
    lan::SaveSync::Setup saves = places_.saves;
    saves.changed = [news = savesNews_] {
        news->store(true);
    };
    saves.inUse = [playing = playing_](const library::Game& game,
                                       const library::saves::EmulatorSaves& kept) {
        {
            const std::lock_guard lock{playing->mutex};
            if (playing->gameId == game.id) {
                return "Close " + game.title + " first: it is running";
            }
        }
        return launch::runningRefusal(kept);
    };
    try {
        node_ = std::make_unique<lan::LanNode>(lan::LanNode::Setup{.bookFile = places_.bookFile,
                                                                   .secrets = places_.secrets,
                                                                   .coverCache = places_.coverCache,
                                                                   .name = name(),
                                                                   .port = places_.port,
                                                                   .remote = remote_,
                                                                   .changed =
                                                                       [news = news_] {
                                                                           news->store(true);
                                                                       },
                                                                   .saves = std::move(saves)});
    } catch (const std::runtime_error& error) {
        lucent::error("lan", "LAN cannot start: {}", error.what());
        hooks_.say("LAN cannot start: " + std::string{error.what()}, true);
        return;
    }
    node_->share(shared_, catalog_);
}

void LanService::stop() {
    release();
    closeQuestion();
    closeCard();
    node_.reset();
    asked_.reset();
    seen_.clear();
    paired_.clear();
    if (remote_->replace({})) {
        hooks_.remoteChanged();
    }
}

void LanService::share(const std::vector<library::Game>& games,
                       const library::roms::EmulatorCatalog& catalog) {
    shared_ = games;
    catalog_ = catalog;
    if (node_ != nullptr) {
        node_->share(shared_, catalog_);
    }
}

void LanService::shareSaves(const std::string& peerId, bool on) {
    if (node_ == nullptr) {
        return;
    }
    const std::vector<lan::Peer> peers = node_->paired();
    const auto peer = std::ranges::find(peers, peerId, &lan::Peer::id);
    if (peer == peers.end()) {
        return;
    }
    std::string error;
    if (!node_->shareSaves(peerId, on, error)) {
        hooks_.say("Cannot change sharing with " + peer->name + ": " + error, true);
        return;
    }
    lucent::info("lan", "saves {} with {}", on ? "shared" : "no longer shared", peer->name);
    hooks_.say(on ? "Saves are shared with " + peer->name
                  : "Saves are no longer shared with " + peer->name,
               false);
    hooks_.peersChanged();
}

bool LanService::before(const library::Game& game, std::function<void()> launch) {
    if (node_ == nullptr || held_ || !node_->saves().sharing(game)) {
        return false;
    }
    held_ = Held{.ticket = node_->saves().prepare(game), .launch = std::move(launch)};
    lucent::info("saves", "{}: syncing saves before it starts", game.title);
    return true;
}

void LanService::release() {
    if (std::optional<Held> held = std::exchange(held_, std::nullopt)) {
        held->launch();
    }
}

void LanService::playing(const library::Game* game) {
    const std::lock_guard lock{playing_->mutex};
    playing_->gameId = game != nullptr ? game->id : std::string{};
}

void LanService::ended(const library::Game& game) {
    if (node_ != nullptr && node_->saves().sharing(game)) {
        node_->saves().sync(game);
    }
}

void LanService::look(const library::Game& game) {
    if (node_ != nullptr) {
        node_->saves().look(game);
    }
}

void LanService::syncNow(const library::Game& game) {
    if (node_ != nullptr) {
        node_->saves().sync(game);
    }
}

void LanService::keep(const library::Game& game, const std::string& peerId, bool theirs) {
    if (node_ != nullptr) {
        node_->saves().keep(game, peerId, theirs);
    }
}

std::vector<lan::PeerSaves> LanService::standings(const std::string& gameId) const {
    return node_ != nullptr ? node_->saves().standings(gameId) : std::vector<lan::PeerSaves>{};
}

bool LanService::syncing(const std::string& gameId) const {
    return node_ != nullptr && node_->saves().busy(gameId);
}

void LanService::pair(const std::string& peerId) {
    if (node_ == nullptr) {
        return;
    }
    std::string refusal;
    if (!node_->pair(peerId, refusal)) {
        hooks_.say("Cannot pair: " + refusal, true);
        return;
    }
    sounds_.play(audio::Effect::Open);
    auto card = std::make_unique<ui::ProgressScreen>(
        std::string{cardId},
        [this] {
            const std::optional<lan::GuestPairing::State> state =
                node_ != nullptr ? node_->asking() : std::nullopt;
            if (!state) {
                return ui::ProgressCard{};
            }
            ui::ProgressCard shown{.title = "Pairing with " + state->hostName,
                                   .line = "Asking " + state->hostName + "…",
                                   .fraction = std::nullopt,
                                   .leave = "Cancel"};
            if (state->phase == Phase::Waiting) {
                shown.title = "Code " + state->code;
                shown.line = "Accept on " + state->hostName + " if it shows " + state->code;
            }
            return shown;
        },
        [this] {
            closeCard();
            if (node_ != nullptr) {
                node_->endAsking();
            }
        });
    carded_ = true;
    navigator_.push(std::move(card));
}

void LanService::forget(const std::string& peerId) {
    if (node_ == nullptr) {
        return;
    }
    const std::vector<lan::Peer> peers = node_->paired();
    const auto peer = std::ranges::find(peers, peerId, &lan::Peer::id);
    const std::string shown = peer != peers.end() ? peer->name : std::string{"that openSU"};
    std::string error;
    if (!node_->forget(peerId, error)) {
        hooks_.say("Cannot remove " + shown + ": " + error, true);
        return;
    }
    hooks_.say("Removed " + shown, false);
    hooks_.peersChanged();
}

void LanService::service() {
    if (node_ == nullptr) {
        return;
    }
    if (news_->exchange(false)) {
        hooks_.remoteChanged();
        hooks_.peersChanged();
    }
    if (savesNews_->exchange(false)) {
        hooks_.savesChanged();
    }
    if (held_ && node_->saves().prepared(held_->ticket)) {
        release();
    }
    serviceQuestion();
    serviceCard();
    const auto now = std::chrono::steady_clock::now();
    if (now - looked_ < lookEvery) {
        return;
    }
    looked_ = now;
    std::vector<lan::Sighting> seen = node_->sightings();
    std::vector<lan::Peer> paired = node_->paired();
    if (seen != seen_ || paired != paired_) {
        seen_ = std::move(seen);
        paired_ = std::move(paired);
        hooks_.peersChanged();
    }
}

void LanService::serviceQuestion() {
    std::optional<lan::HostPairing::Question> question = node_->question();
    if (question == asked_) {
        return;
    }
    closeQuestion();
    asked_ = std::move(question);
    if (!asked_) {
        return;
    }
    lucent::info("lan", "{} asks to pair", asked_->guest.name);
    const std::string guest = asked_->guest.name;
    const auto answer = [this, id = asked_->guest.id, guest](bool accept) {
        questionTitle_.clear();
        if (node_ == nullptr) {
            return;
        }
        std::string error;
        if (!node_->answer(id, accept, error)) {
            hooks_.say("Cannot pair with " + guest + ": " + error, true);
            return;
        }
        hooks_.say(accept ? "Paired with " + guest : "Declined " + guest, false);
        hooks_.peersChanged();
    };
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Decline",
                                                .chosen =
                                                    [answer] {
                                                        answer(false);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Normal,
                                                .exits = true});
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Accept",
                                                .chosen =
                                                    [answer] {
                                                        answer(true);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Primary,
                                                .exits = false});
    questionTitle_ = "Pair with " + guest + "?";
    auto dialog = std::make_unique<gpu2d::ui::Dialog>(
        questionTitle_,
        "Code " + asked_->code + "\n\nAccept only if " + guest +
            " shows the same code. Paired, each openSU lists the games installed on the other.",
        std::move(choices), 0);
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::move(dialog));
}

void LanService::serviceCard() {
    if (!carded_) {
        return;
    }
    const std::optional<lan::GuestPairing::State> state = node_->asking();
    if (state && (state->phase == Phase::Asking || state->phase == Phase::Waiting)) {
        return;
    }
    closeCard();
    node_->endAsking();
    if (!state) {
        return;
    }
    switch (state->phase) {
    case Phase::Paired:
        hooks_.say("Paired with " + state->hostName, false);
        break;
    case Phase::Declined:
        hooks_.say(state->hostName + " declined to pair", true);
        break;
    case Phase::Failed:
        hooks_.say("Cannot pair with " + state->hostName + ": " + state->problem, true);
        break;
    case Phase::Asking:
    case Phase::Waiting:
    case Phase::Cancelled:
        break;
    }
    hooks_.peersChanged();
}

void LanService::closeQuestion() {
    if (questionTitle_.empty()) {
        return;
    }
    if (const gpu2d::ui::Screen* open = openScreen("dialog", std::exchange(questionTitle_, {}))) {
        navigator_.closeFrom(*open);
    }
}

void LanService::closeCard() {
    if (!std::exchange(carded_, false)) {
        return;
    }
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->id() == cardId) {
            sounds_.play(audio::Effect::Close);
            navigator_.closeFrom(*screen);
            return;
        }
    }
}

const gpu2d::ui::Screen* LanService::openScreen(std::string_view id, std::string_view title) const {
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->id() == id && screen->title() == title) {
            return screen;
        }
    }
    return nullptr;
}

LanService::Glance LanService::glance() const {
    if (node_ == nullptr) {
        return {};
    }
    Glance glance{.on = true, .seen = seen_.size(), .paired = paired_.size(), .code = {}};
    if (asked_) {
        glance.code = asked_->code;
    } else if (const std::optional<lan::GuestPairing::State> state = node_->asking()) {
        glance.code = state->code;
    }
    return glance;
}

} // namespace opensu::app

#include "lan_service.hpp"

#include <algorithm>
#include <utility>

#include "gpu2d/ui/screen.hpp"

#include "lan/dns_sd.hpp"
#include "lucent/log.h"
#include "stream/remote_reach.hpp"
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
    stopAskers();
    node_.reset();
}

void LanService::stopAskers() {
    for (std::jthread* asker : {&asker_, &romAsker_, &peerAsker_}) {
        if (asker->joinable()) {
            asker->request_stop();
            asker->join();
        }
    }
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
    lan::RomImport::Setup roms = places_.roms;
    roms.changed = [news = romsNews_] {
        news->store(true);
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
                                                                   .saves = places_.saves,
                                                                   .profiles = places_.profiles,
                                                                   .play = hooks_.play,
                                                                   .playStatus = hooks_.playStatus,
                                                                   .roms = std::move(roms)});
    } catch (const std::runtime_error& error) {
        lucent::error("lan", "LAN cannot start: {}", error.what());
        hooks_.say("LAN cannot start: " + std::string{error.what()}, true);
        return;
    }
    node_->share(shared_);
    node_->shareRoms(roms_);
}

void LanService::stop() {
    stopAskers();
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

void LanService::share(const std::vector<library::Game>& games) {
    // The listing is what a guest may ask to stream.
    shared_ = stream::hostingMissing().empty() ? games : std::vector<library::Game>{};
    if (node_ != nullptr) {
        node_->share(shared_);
    }
}

bool LanService::askRoms(std::string hostId, RomsAnswer answer) {
    if (node_ == nullptr || askingRoms_->exchange(true)) {
        return false;
    }
    if (romAsker_.joinable()) {
        romAsker_.join();
    }
    romAsker_ = std::jthread{[node = node_.get(), asking = askingRoms_, hostId = std::move(hostId),
                              answer = std::move(answer)](const std::stop_token& stop) {
        std::string error;
        std::optional<std::vector<lan::wire::RomConsole>> consoles =
            node->romsOf(stop, hostId, error);
        asking->store(false);
        if (!stop.stop_requested()) {
            answer(std::move(consoles), std::move(error));
        }
    }};
    return true;
}

void LanService::shareRoms(lan::RomShelf::Contents contents) {
    roms_ = std::move(contents);
    if (node_ != nullptr) {
        node_->shareRoms(roms_);
    }
}

bool LanService::askToPlay(std::string hostId, std::string gameId, std::string profile,
                           std::function<void()> waiting, PlayAnswer answer) {
    if (node_ == nullptr || asking_->exchange(true)) {
        return false;
    }
    if (asker_.joinable()) {
        asker_.join();
    }
    asker_ = std::jthread{[node = node_.get(), asking = asking_, hostId = std::move(hostId),
                           gameId = std::move(gameId), profile = std::move(profile),
                           waiting = std::move(waiting),
                           answer = std::move(answer)](const std::stop_token& stop) {
        std::string error;
        std::optional<lan::LanNode::PlayOffer> offer =
            node->askToPlay(stop, hostId, gameId, profile, waiting, error);
        asking->store(false);
        if (!stop.stop_requested()) {
            answer(std::move(offer), std::move(error));
        }
    }};
    return true;
}

std::string LanService::peerName(const std::string& peerId) const {
    if (node_ != nullptr) {
        if (const std::optional<lan::Peer> peer = node_->find(peerId)) {
            return peer->name;
        }
    }
    return "that openSU";
}

bool LanService::askPeers(std::function<void(const lan::LanNode&, const std::stop_token&)> work) {
    if (node_ == nullptr || askingPeers_->exchange(true)) {
        return false;
    }
    if (peerAsker_.joinable()) {
        peerAsker_.join();
    }
    peerAsker_ = std::jthread{[node = node_.get(), asking = askingPeers_,
                               work = std::move(work)](const std::stop_token& stop) {
        work(*node, stop);
        asking->store(false);
    }};
    return true;
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
    if (romsNews_->exchange(false)) {
        hooks_.romsChanged();
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
    const std::string& guest = asked_->guest.name;
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Decline",
                                                .chosen =
                                                    [this] {
                                                        answerQuestion(false);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Normal,
                                                .exits = true});
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Accept",
                                                .chosen =
                                                    [this] {
                                                        answerQuestion(true);
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

void LanService::answerQuestion(bool accept) {
    questionTitle_.clear();
    if (node_ == nullptr || !asked_) {
        return;
    }
    const lan::Peer guest = asked_->guest;
    std::string error;
    if (!node_->answer(guest.id, accept, error)) {
        hooks_.say("Cannot pair with " + guest.name + ": " + error, true);
        return;
    }
    hooks_.say(accept ? "Paired with " + guest.name : "Declined " + guest.name, false);
    hooks_.peersChanged();
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

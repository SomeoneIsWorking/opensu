#include "lan_netplay.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <stop_token>
#include <system_error>
#include <thread>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "fileio/owner_only_write.hpp"
#include "gpu2d/ui/screen.hpp"
#include "library/linked_play.hpp"
#include "lucent/log.h"
#include "retro/save_paths.hpp"
#include "secrets/crypto.hpp"
#include "secrets/secret.hpp"
#include "stream/udp_socket.hpp"

namespace opensu::app {
namespace {

constexpr auto pollEvery = std::chrono::seconds{1};
constexpr auto waitAtMost = std::chrono::seconds{125};

std::string contentsOf(const std::filesystem::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

std::string sha256Of(const std::filesystem::path& rom) {
    return secrets::hexText(secrets::crypto::sha256(contentsOf(rom)));
}

/// A free UDP port for the runner to bind.
std::uint16_t freeUdpPort() {
    const stream::UdpSocket socket{0};
    return socket.port();
}

/// Player 1's save is the frontend's; the core keeps the others' by player.
std::filesystem::path sessionSaveOf(const std::filesystem::path& folder,
                                    const std::filesystem::path& rom, unsigned slot) {
    const std::string stem = rom.stem().string();
    return folder / (slot == 0 ? stem + ".srm" : stem + ".p" + std::to_string(slot + 1) + ".srm");
}

const library::Game* romOf(const std::vector<library::Game>& games,
                           const lan::wire::NetplayAsk& rom) {
    const auto found = std::ranges::find_if(games, [&rom](const library::Game& game) {
        std::error_code failed;
        return game.source == library::Source::Rom && game.sourceId == rom.console &&
               game.file.filename() == rom.file &&
               std::filesystem::file_size(game.file, failed) == rom.size && !failed &&
               sha256Of(game.file) == rom.sha256;
    });
    return found == games.end() ? nullptr : &*found;
}

} // namespace

LanNetplay::LanNetplay(LanService& lan, gpu2d::ui::Navigator& navigator,
                       audio::SdlSoundPlayer& sounds, Places places, Hooks hooks)
    : lan_{lan}, navigator_{navigator}, sounds_{sounds}, places_{std::move(places)},
      hooks_{std::move(hooks)} {
}

std::vector<ui::GameSettingsView::Partner> LanNetplay::partners() const {
    std::vector<ui::GameSettingsView::Partner> partners;
    const lan::LanNode* node = lan_.node();
    if (node == nullptr) {
        return partners;
    }
    for (const lan::Sighting& seen : node->sightings()) {
        if (const std::optional<lan::Peer> peer = node->find(seen.id)) {
            partners.push_back({peer->id, peer->name});
        }
    }
    return partners;
}

void LanNetplay::play(const library::Game& game, const std::string& peerId) {
    const std::string name = lan_.peerName(peerId);
    const std::filesystem::path own =
        sessionSaveOf(retro::systemSaveFolder(hooks_.profileSaves(), game.sourceId), game.file, 0);
    const bool asking = lan_.askPeers([arrived = arrived_, game, peerId, name,
                                       own](const lan::LanNode& node, const std::stop_token& stop) {
        std::string refusal;
        std::error_code failed;
        const secrets::crypto::Bytes salt =
            secrets::crypto::randomBytes(secrets::crypto::nonceBytes);
        const lan::wire::NetplayAsk ask{.console = game.sourceId,
                                        .file = game.file.filename().string(),
                                        .title = game.title,
                                        .size = std::filesystem::file_size(game.file, failed),
                                        .sha256 = sha256Of(game.file),
                                        .salt = salt};
        std::string ownSave = contentsOf(own);
        node.netplay().handOver(peerId, ask.sha256, ownSave);
        std::optional<Agreed> agreed;
        if (node.askNetplay(stop, peerId, ask, refusal)) {
            const auto until = std::chrono::steady_clock::now() + waitAtMost;
            refusal = "no answer came";
            while (!stop.stop_requested() && std::chrono::steady_clock::now() < until) {
                std::this_thread::sleep_for(pollEvery);
                const std::optional<lan::wire::NetplayVerdict> verdict =
                    node.netplayStatus(stop, peerId, ask.sha256, refusal);
                if (!verdict) {
                    break;
                }
                if (verdict->verdict == "declined") {
                    refusal = verdict->refusal.empty() ? "it was declined" : verdict->refusal;
                    break;
                }
                if (verdict->verdict != "allowed") {
                    continue;
                }
                std::optional<std::string> hostSave =
                    node.netplaySave(stop, peerId, ask.sha256, refusal);
                std::optional<std::string> address = node.addressOf(peerId, refusal);
                std::optional<lan::StreamKeys> keys =
                    node.netplayKeys(peerId, salt, verdict->salt, refusal);
                if (hostSave && address && keys) {
                    agreed = Agreed{.game = game,
                                    .slot = 1,
                                    .peerName = name,
                                    .port = 0,
                                    .peer = *address + ":" + std::to_string(verdict->port),
                                    .keys = std::move(*keys),
                                    .saves = {std::move(*hostSave), std::move(ownSave)}};
                }
                break;
            }
        }
        if (stop.stop_requested()) {
            return;
        }
        const std::lock_guard lock{arrived->mutex};
        if (agreed) {
            arrived->agreed.push_back(std::move(*agreed));
        } else {
            arrived->refusals.push_back("No game with " + name + ": " + refusal);
        }
    });
    if (!asking) {
        hooks_.say("Cannot ask " + name + " now; try again", true);
        return;
    }
    lucent::info("lan", "asking {} to play {} together", name, game.title);
    hooks_.say("Allow it on " + name, false);
}

void LanNetplay::service() {
    serviceQuestion();
    std::vector<Agreed> agreed;
    std::vector<std::string> refusals;
    {
        const std::lock_guard lock{arrived_->mutex};
        agreed.swap(arrived_->agreed);
        refusals.swap(arrived_->refusals);
    }
    for (const std::string& refusal : refusals) {
        lucent::info("lan", "{}", refusal);
        hooks_.say(refusal, true);
    }
    for (Agreed& session : agreed) {
        begin(std::move(session));
    }
}

void LanNetplay::serviceQuestion() {
    lan::LanNode* node = lan_.node();
    std::optional<lan::NetplayAsks::Question> question =
        node != nullptr ? node->netplay().question(lan::NetplayAsks::Clock::now()) : std::nullopt;
    if (question == asked_) {
        return;
    }
    closeQuestion();
    asked_ = std::move(question);
    if (!asked_) {
        return;
    }
    if (romOf(hooks_.games(), asked_->rom) == nullptr) {
        node->netplay().decline(asked_->peer.id, "it does not have this ROM",
                                lan::NetplayAsks::Clock::now());
        hooks_.say(asked_->peer.name + " asked to play " + asked_->rom.title + ", not here", true);
        return;
    }
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Decline",
                                                .chosen =
                                                    [this] {
                                                        answer(false);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Normal,
                                                .exits = true});
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Play",
                                                .chosen =
                                                    [this] {
                                                        answer(true);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Primary,
                                                .exits = true});
    questionTitle_ = "Play " + asked_->rom.title + " with " + asked_->peer.name + "?";
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>(
        questionTitle_, "Each of you plays as your own profile and keeps your own save.",
        std::move(choices), 0));
}

void LanNetplay::answer(bool allow) {
    lan::LanNode* node = lan_.node();
    if (node == nullptr || !asked_) {
        return;
    }
    const lan::NetplayAsks::Question asked = *asked_;
    const auto now = lan::NetplayAsks::Clock::now();
    questionTitle_.clear();
    if (!allow) {
        node->netplay().decline(asked.peer.id, {}, now);
        return;
    }
    const library::Game* found = romOf(hooks_.games(), asked.rom);
    if (found == nullptr || !library::linkable(*found)) {
        node->netplay().decline(asked.peer.id, "it cannot link this game", now);
        return;
    }
    const library::Game game = *found;
    const std::uint16_t port = freeUdpPort();
    const secrets::crypto::Bytes salt = secrets::crypto::randomBytes(secrets::crypto::nonceBytes);
    std::string ownSave = contentsOf(
        sessionSaveOf(retro::systemSaveFolder(hooks_.profileSaves(), game.sourceId), game.file, 0));
    node->netplay().handOver(asked.peer.id, asked.rom.sha256, ownSave);
    node->netplay().allow(asked.peer.id, port, salt, now);
    const bool pulling =
        lan_.askPeers([arrived = arrived_, asked, game, port, salt, ownSave = std::move(ownSave)](
                          const lan::LanNode& node, const std::stop_token& stop) {
            std::string error;
            std::optional<std::string> guestSave =
                node.netplaySave(stop, asked.peer.id, asked.rom.sha256, error);
            std::optional<lan::StreamKeys> keys =
                node.netplayKeys(asked.peer.id, asked.rom.salt, salt, error);
            if (stop.stop_requested()) {
                return;
            }
            const std::lock_guard lock{arrived->mutex};
            if (!guestSave || !keys) {
                arrived->refusals.push_back("No game with " + asked.peer.name + ": " + error);
                return;
            }
            arrived->agreed.push_back(Agreed{.game = game,
                                             .slot = 0,
                                             .peerName = asked.peer.name,
                                             .port = port,
                                             .peer = {},
                                             .keys = std::move(*keys),
                                             .saves = {ownSave, std::move(*guestSave)}});
        });
    if (!pulling) {
        hooks_.say("Cannot reach " + asked.peer.name + " now; try again", true);
    }
}

void LanNetplay::closeQuestion() {
    if (questionTitle_.empty()) {
        return;
    }
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->id() == "dialog" && screen->title() == questionTitle_) {
            navigator_.closeFrom(*screen);
            break;
        }
    }
    questionTitle_.clear();
}

void LanNetplay::begin(Agreed agreed) {
    const auto refuse = [this, &agreed](const std::string& why) {
        hooks_.say("No game with " + agreed.peerName + ": " + why, true);
    };
    std::error_code failed;
    std::filesystem::remove_all(places_.sessionSaves, failed);
    const std::filesystem::path folder =
        retro::systemSaveFolder(places_.sessionSaves, agreed.game.sourceId);
    std::filesystem::create_directories(folder, failed);
    std::string error;
    for (unsigned slot = 0; slot < 2; ++slot) {
        if (!agreed.saves[slot].empty() &&
            !fileio::writeWhole(sessionSaveOf(folder, agreed.game.file, slot), agreed.saves[slot],
                                error)) {
            refuse(error);
            return;
        }
    }
    const bool host = agreed.slot == 0;
    nlohmann::json session{
        {"slot", agreed.slot},
        {"players", 2},
        {"port", agreed.port},
        {"sendKey", secrets::hexText(host ? agreed.keys.host : agreed.keys.guest)},
        {"receiveKey", secrets::hexText(host ? agreed.keys.guest : agreed.keys.host)}};
    if (!agreed.peer.empty()) {
        session["peer"] = agreed.peer;
    }
    const std::optional<library::Game> game =
        library::netplayGame(agreed.game, places_.sessionFile);
    if (!game) {
        refuse("the link core is gone");
        return;
    }
    if (!fileio::writeOwnerOnly(places_.sessionFile, session.dump(), error)) {
        refuse(error);
        return;
    }
    lucent::info("lan", "playing {} with {} as player {}", agreed.game.title, agreed.peerName,
                 agreed.slot + 1);
    if (!hooks_.launch(*game, places_.sessionSaves)) {
        std::filesystem::remove(places_.sessionFile, failed);
        return;
    }
    running_ = Running{
        .gameId = agreed.game.id,
        .own = sessionSaveOf(folder, agreed.game.file, agreed.slot),
        .profile =
            sessionSaveOf(retro::systemSaveFolder(hooks_.profileSaves(), agreed.game.sourceId),
                          agreed.game.file, 0),
    };
}

void LanNetplay::ended(const library::Game& game) {
    if (!running_ || running_->gameId != game.id) {
        return;
    }
    const Running session = *running_;
    running_.reset();
    std::error_code failed;
    std::filesystem::remove(places_.sessionFile, failed);
    if (!std::filesystem::exists(session.own, failed)) {
        lucent::info("lan", "the session left no save of this player");
        return;
    }
    std::string error;
    if (!fileio::writeWhole(session.profile, contentsOf(session.own), error)) {
        lucent::error("lan", "the session's save stays at {}: {}", session.own.string(), error);
        hooks_.say("The session's save could not be kept: " + error, true);
        return;
    }
    lucent::info("lan", "the session's save went back to {}", session.profile.string());
}

} // namespace opensu::app

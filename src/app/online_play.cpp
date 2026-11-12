#include "online_play.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>
#include <system_error>
#include <utility>

#include "gpu2d/ui/screen.hpp"

#include "lan/netplay_asks.hpp"
#include "library/linked_play.hpp"
#include "lucent/log.h"
#include "lobby_screen.hpp"
#include "online_screen.hpp"
#include "secrets/secret.hpp"
#include "ui/entry_screen.hpp"

namespace opensu::app {
namespace {

constexpr std::string_view joinRowId = "online.join";
constexpr std::string_view findRowId = "online.find";
constexpr std::string_view lobbyNameCardId = "online/lobby-name";
constexpr std::string_view zeroTierRowId = "online.zerotier";
constexpr std::string_view codeCardId = "online/code";
/// An invite with every address it can hold, and room for what a chat app adds.
constexpr std::size_t mostCodeBytes = 256;
constexpr std::string_view seesYou =
    "Your friend will see your public IP address, which shows roughly where you are. openSU may "
    "open a port on your router while you play, and closes it when the session ends. Only play "
    "with someone you trust.";
constexpr std::string_view seenByAnyone =
    "Anyone who finds your game in the list and asks for a seat gets your code and sees your "
    "public IP address, which shows roughly where you are. The list shows only the name you type. "
    "The first to ask gets the code; the game leaves the list then, or when you cancel.";
std::string stepName(online::Meeting::Step step) {
    using Step = online::Meeting::Step;
    switch (step) {
    case Step::Gathering:
        return "gathering";
    case Step::Inviting:
        return "inviting";
    case Step::Trying:
        return "trying";
    case Step::Replying:
        return "replying";
    case Step::Punching:
        return "punching";
    case Step::Agreeing:
        return "agreeing";
    case Step::Asking:
        return "asking";
    case Step::Ready:
        return "ready";
    case Step::Failed:
        return "failed";
    }
    return {};
}

std::optional<library::Game> gameAsked(const std::vector<library::Game>& games,
                                       const lan::wire::NetplayAsk& asked) {
    const library::Game* found = NetplayRun::romOf(games, asked);
    return found != nullptr ? std::optional<library::Game>{*found} : std::nullopt;
}

} // namespace

OnlinePlay::OnlinePlay(gpu2d::ui::Navigator& navigator, audio::SdlSoundPlayer& sounds,
                       NetplayRun& run, KeyboardPad& keyboard, Preferences& preferences,
                       std::filesystem::path zeroTierStorage, Hooks hooks)
    : navigator_{navigator}, sounds_{sounds}, run_{run}, keyboard_{keyboard},
      preferences_{preferences}, hooks_{std::move(hooks)},
      zeroTier_{std::move(zeroTierStorage), preferences.values().onlineZeroTierRoots} {
}

ui::SettingsCategory OnlinePlay::page() const {
    ui::SettingsCategory page{std::string{categoryId}, "Play online", {}};
    ui::SettingsRow join;
    join.id = joinRowId;
    join.kind = ui::RowKind::Action;
    join.label = "Join a friend's game";
    join.note = "Paste the code your friend sent you. To host, open a game and pick Play online.";
    page.rows.push_back(std::move(join));
    ui::SettingsRow find;
    find.id = findRowId;
    find.kind = ui::RowKind::Action;
    find.label = "Find a public game";
    find.note = "Games other players listed for anyone to join.";
    page.rows.push_back(std::move(find));
    const bool zeroTier = preferences_.values().onlineZeroTier;
    if (online::ZeroTier::built()) {
        page.rows.push_back(
            ui::makeToggle(std::string{zeroTierRowId}, "ZeroTier", zeroTierNote(), zeroTier));
    }
    ui::SettingsRow fallback;
    fallback.id = "online.fallback";
    fallback.kind = ui::RowKind::Info;
    fallback.label = "If you cannot connect";
    if (!online::ZeroTier::built()) {
        fallback.note =
            "There is no relay server. When two routers will not let you through, install "
            "Tailscale or ZeroTier on both machines and join one network: the same codes "
            "then work over it.";
    } else if (zeroTier) {
        fallback.note =
            "When two routers will not let you through, openSU tries again over ZeroTier, "
            "which it brings along: nothing to install. If that fails too, install Tailscale "
            "on both machines and join one network: the same codes then work over it.";
    } else {
        fallback.note =
            "There is no relay server. When two routers will not let you through, turn "
            "ZeroTier on here on both machines, or install Tailscale on both and join one "
            "network: the same codes then work over it.";
    }
    page.rows.push_back(std::move(fallback));
    return page;
}

std::string OnlinePlay::zeroTierNote() const {
    if (preferences_.values().onlineZeroTier) {
        return "A second way through when two routers block a game";
    }
    if (zeroTier_.paths() != nullptr) {
        return "Off from the next game online; the one started stays until openSU closes";
    }
    return "Games online connect directly only";
}

bool OnlinePlay::owns(const std::string& id) {
    return id == joinRowId || id == findRowId || id == zeroTierRowId;
}

void OnlinePlay::run(const std::string& id, int step) {
    if (id == zeroTierRowId) {
        settings::Settings& values = preferences_.values();
        values.onlineZeroTier = !values.onlineZeroTier;
        preferences_.save();
        return;
    }
    if (step == 0 && id == findRowId) {
        findGames();
    } else if (step == 0) {
        join();
    }
}

online::LobbyPlaces OnlinePlay::lobbyPlaces() const {
    return online::LobbyPlaces::of(preferences_.values().onlineLobbyRelays,
                                   preferences_.values().onlineLobbyServer);
}

online::Network OnlinePlay::network() const {
    online::Network network;
    if (const std::string& servers = preferences_.values().onlineStunServers; !servers.empty()) {
        network.direct.stun_servers = servers;
    }
    if (preferences_.values().onlineZeroTier) {
        network.zerotier = zeroTier_.paths();
    }
    return network;
}

void OnlinePlay::host(const library::Game& game) {
    if (meeting_ || hashing_) {
        hooks_.say("A game online is already being set up", true);
        return;
    }
    if (preferences_.values().onlineZeroTier) {
        zeroTier_.start();
    }
    sounds_.play(audio::Effect::Open);
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    choices.push_back(
        {.label = "Cancel", .chosen = {}, .kind = gpu2d::ui::ButtonKind::Normal, .exits = true});
    choices.push_back({.label = "Make a code",
                       .chosen =
                           [this, game = game] {
                               startHost(game);
                           },
                       .kind = gpu2d::ui::ButtonKind::Primary,
                       .exits = false});
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>(
        "Play " + game.title + " online?", std::string{seesYou}, std::move(choices), 1));
}

void OnlinePlay::startHost(const library::Game& game) {
    if (meeting_ || hashing_) {
        return;
    }
    game_ = game;
    hosting_ = true;
    answered_ = false;
    replyDraft_.clear();
    replyProblem_.clear();
    hashing_ = std::async(std::launch::async, &NetplayRun::sha256Of, game.file);
    openScreen();
}

void OnlinePlay::serviceHash() {
    if (!hashing_ || hashing_->wait_for(std::chrono::seconds{0}) != std::future_status::ready) {
        return;
    }
    const std::string sha256 = hashing_->get();
    hashing_.reset();
    if (!game_) {
        return;
    }
    const library::Game& game = *game_;
    std::error_code failed;
    lan::wire::NetplayAsk ask{.console = game.sourceId,
                              .file = game.file.filename().string(),
                              .title = game.title,
                              .size = std::filesystem::file_size(game.file, failed),
                              .sha256 = sha256,
                              .salt = secrets::crypto::randomBytes(secrets::crypto::nonceBytes),
                              .facts = run_.factsOf(game),
                              .clock = lan::netplayClockNow()};
    meeting_ = std::make_unique<online::Meeting>(network(), online::Patience{}, std::move(ask),
                                                 NetplayRun::contentsOf(run_.profileSave(game)),
                                                 !library::netplayCore(game).empty());
}

void OnlinePlay::join() {
    if (meeting_ || hashing_) {
        hooks_.say("A game online is already being set up", true);
        return;
    }
    if (preferences_.values().onlineZeroTier) {
        zeroTier_.start();
    }
    sounds_.play(audio::Effect::Open);
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    choices.push_back(
        {.label = "Cancel", .chosen = {}, .kind = gpu2d::ui::ButtonKind::Normal, .exits = true});
    choices.push_back({.label = "Enter the code",
                       .chosen =
                           [this] {
                               askCode();
                           },
                       .kind = gpu2d::ui::ButtonKind::Primary,
                       .exits = false});
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>("Join a game online?", std::string{seesYou},
                                                        std::move(choices), 1));
}

void OnlinePlay::askCode() {
    codeDraft_.clear();
    navigator_.push(
        std::make_unique<ui::EntryScreen>(ui::Entry{.id = std::string{codeCardId},
                                                    .title = "Join a friend's game",
                                                    .note = "Paste the code your friend sent you.",
                                                    .label = "Code",
                                                    .placeholder = "OSU...",
                                                    .secret = false,
                                                    .mostBytes = mostCodeBytes,
                                                    .text = [this]() -> const std::string& {
                                                        return codeDraft_;
                                                    },
                                                    .changed =
                                                        [this](std::string text) {
                                                            codeDraft_ = std::move(text);
                                                        },
                                                    .submitted =
                                                        [this] {
                                                            submitCode();
                                                        },
                                                    .cancelled =
                                                        [this] {
                                                            closeCode();
                                                        }}));
}

void OnlinePlay::closeCode() {
    keyboard_.close();
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->id() == codeCardId) {
            navigator_.closeFrom(*screen);
            break;
        }
    }
}

void OnlinePlay::submitCode() {
    lucent::p2p::CodeProblem problem{};
    std::optional<lucent::p2p::Invite> invite =
        lucent::p2p::read_invite(codeDraft_, online::codePrefixes, problem);
    secrets::wipe(codeDraft_);
    codeDraft_.clear();
    if (!invite) {
        hooks_.say(std::string{online::textOf(problem)}, true);
        return;
    }
    closeCode();
    meet(std::move(*invite));
}

void OnlinePlay::meet(lucent::p2p::Invite invite) {
    game_.reset();
    hosting_ = false;
    answered_ = false;
    meeting_ = std::make_unique<online::Meeting>(network(), online::Patience{}, std::move(invite));
    openScreen();
}

void OnlinePlay::findGames() {
    if (meeting_ || hashing_) {
        hooks_.say("A game online is already being set up", true);
        return;
    }
    if (preferences_.values().onlineZeroTier) {
        zeroTier_.start();
    }
    sounds_.play(audio::Effect::Open);
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    choices.push_back(
        {.label = "Cancel", .chosen = {}, .kind = gpu2d::ui::ButtonKind::Normal, .exits = true});
    choices.push_back({.label = "Find games",
                       .chosen =
                           [this] {
                               finder_ = std::make_unique<online::LobbyFinder>(lobbyPlaces());
                               navigator_.push(std::make_unique<LobbyScreen>(*this));
                           },
                       .kind = gpu2d::ui::ButtonKind::Primary,
                       .exits = true});
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>("Join a public game?",
                                                        std::string{seesYou}, std::move(choices),
                                                        1));
}

OnlinePlay::Lobbies OnlinePlay::lobbies() const {
    using Step = online::LobbyFinder::Step;
    Lobbies shown;
    if (!finder_) {
        return shown;
    }
    const online::LobbyFinder::View view = finder_->view();
    shown.step = view.step;
    shown.games = view.games;
    shown.pickable = view.step != Step::Listing && view.step != Step::Asking &&
                     view.step != Step::Invited;
    switch (view.step) {
    case Step::Listing:
        shown.note = "Looking for public games...";
        break;
    case Step::Listed:
        shown.note = view.games.empty() ? "No public games right now. Try again in a moment."
                                        : "Pick a game to ask its host for a seat.";
        break;
    case Step::Asking:
        shown.note = "Asking the host...";
        break;
    case Step::Invited:
        shown.note = "The host answered. Connecting...";
        break;
    case Step::Full:
        shown.note = "Someone else took that seat. Pick another game.";
        break;
    case Step::Failed:
        shown.note = view.problem;
        break;
    }
    return shown;
}

void OnlinePlay::refreshLobbies() {
    if (finder_ && lobbies().pickable) {
        finder_->refresh();
    }
}

void OnlinePlay::pickLobby(std::size_t index) {
    if (finder_) {
        finder_->ask(index);
    }
}

void OnlinePlay::closeLobbies() {
    finder_.reset();
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->id() == LobbyScreen::screenId) {
            navigator_.closeFrom(*screen);
            break;
        }
    }
}

void OnlinePlay::serviceLobbies() {
    if (publicLobby_ && view_.step != online::Meeting::Step::Inviting &&
        !publicLobby_->view().seated) {
        // The code is spent or the meeting is over: the listing goes.
        publicLobby_.reset();
    }
    if (!finder_) {
        return;
    }
    const online::LobbyFinder::View view = finder_->view();
    if (view.step != online::LobbyFinder::Step::Invited) {
        return;
    }
    lucent::p2p::CodeProblem problem{};
    std::optional<lucent::p2p::Invite> invite =
        lucent::p2p::read_invite(view.invite, online::codePrefixes, problem);
    closeLobbies();
    if (!invite) {
        hooks_.say(std::string{online::textOf(problem)}, true);
        return;
    }
    meet(std::move(*invite));
}

void OnlinePlay::listPublicly() {
    if (!meeting_ || view_.step != online::Meeting::Step::Inviting || publicLobby_) {
        return;
    }
    sounds_.play(audio::Effect::Open);
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    choices.push_back(
        {.label = "Cancel", .chosen = {}, .kind = gpu2d::ui::ButtonKind::Normal, .exits = true});
    choices.push_back({.label = "Name the game",
                       .chosen =
                           [this] {
                               askLobbyName();
                           },
                       .kind = gpu2d::ui::ButtonKind::Primary,
                       .exits = true});
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>(
        "List this game publicly?", std::string{seenByAnyone}, std::move(choices), 0));
}

void OnlinePlay::askLobbyName() {
    lobbyNameDraft_ = game_ ? game_->title : std::string{};
    navigator_.push(std::make_unique<ui::EntryScreen>(
        ui::Entry{.id = std::string{lobbyNameCardId},
                  .title = "Name your public game",
                  .note = "Players see only this name in the list.",
                  .label = "Name",
                  .placeholder = "Game name",
                  .secret = false,
                  .mostBytes = lucent::p2p::kMaxLobbyTitleBytes,
                  .text = [this]() -> const std::string& {
                      return lobbyNameDraft_;
                  },
                  .changed =
                      [this](std::string text) {
                          lobbyNameDraft_ = std::move(text);
                      },
                  .submitted =
                      [this] {
                          if (meeting_ && view_.step == online::Meeting::Step::Inviting &&
                              !lobbyNameDraft_.empty()) {
                              publicLobby_ = std::make_unique<online::PublicLobby>(
                                  lobbyPlaces(), lobbyNameDraft_, view_.code);
                          }
                          closeLobbyName();
                      },
                  .cancelled =
                      [this] {
                          closeLobbyName();
                      }}));
}

void OnlinePlay::closeLobbyName() {
    keyboard_.close();
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->id() == lobbyNameCardId) {
            navigator_.closeFrom(*screen);
            break;
        }
    }
}

void OnlinePlay::openScreen() {
    view_ = {};
    navigator_.push(std::make_unique<OnlineScreen>(*this));
}

void OnlinePlay::closeScreen() {
    keyboard_.close();
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->id() == OnlineScreen::screenId) {
            navigator_.closeFrom(*screen);
            break;
        }
    }
}

void OnlinePlay::close() {
    closeQuestion();
    closeScreen();
    publicLobby_.reset();
    meeting_.reset();
    lookup_.reset();
    hashing_.reset();
    game_.reset();
    view_ = {};
}

OnlinePlay::Page OnlinePlay::shown() const {
    using Step = online::Meeting::Step;
    Page page;
    page.title = "Play online";
    switch (view_.step) {
    case Step::Gathering:
        page.note = "Finding how the internet can reach this machine...";
        break;
    case Step::Inviting:
        page.title = "Send this code to your friend";
        page.code = view_.code;
        page.note = "It works once, for ten minutes. If your friend gets a reply code to send "
                    "back, paste it below.";
        if (view_.closed) {
            page.note += " Your router is a strict one: a friend behind another strict router "
                         "will not get through.";
        }
        page.replyField = true;
        page.listable = publicLobby_ == nullptr;
        break;
    case Step::Trying:
        page.note = "Trying to reach your friend...";
        break;
    case Step::Replying:
        page.title = "Send this code back to your friend";
        page.code = view_.code;
        page.note = "Your friend's router did not let this machine in. Your friend pastes this on "
                    "their screen, and you keep waiting here.";
        break;
    case Step::Punching:
        page.note = "Trying to connect both ways...";
        break;
    case Step::Agreeing:
    case Step::Asking:
    case Step::Ready:
        page.note = "Connected. Agreeing on the game...";
        break;
    case Step::Failed:
        page.title = "No game";
        page.note = view_.refusal.empty()
                        ? online::Meeting::textOf(view_.failure)
                        : online::Meeting::textOf(view_.failure) + " " + view_.refusal + ".";
        page.over = true;
        break;
    }
    if (!replyProblem_.empty() && page.replyField) {
        page.note = replyProblem_;
    }
    if (publicLobby_) {
        const online::PublicLobby::View listed = publicLobby_->view();
        page.listing = listed.seated   ? "A player from the public list has your code."
                       : listed.listed ? "Listed publicly as \"" + lobbyNameDraft_ +
                                             "\". Anyone who asks gets your code and your "
                                             "public IP address."
                       : listed.problem.empty() ? "Listing publicly..."
                                                : listed.problem;
    }
    return page;
}

void OnlinePlay::submitReply() {
    if (!meeting_) {
        return;
    }
    const std::optional<lucent::p2p::CodeProblem> problem = meeting_->reply(replyDraft_);
    replyProblem_ = problem ? std::string{online::textOf(*problem)} : std::string{};
    if (!problem) {
        replyDraft_.clear();
    }
}

void OnlinePlay::copyCode() {
    hooks_.copy(view_.code);
    hooks_.say("Copied", false);
}

std::string OnlinePlay::glance() const {
    return meeting_ ? stepName(view_.step) : std::string{};
}

std::string OnlinePlay::code() const {
    return view_.code;
}

void OnlinePlay::lookFor(const lan::wire::NetplayAsk& asked) {
    lookup_ = std::async(std::launch::async, gameAsked, hooks_.games(), asked);
}

void OnlinePlay::serviceLookup() {
    if (!lookup_ || lookup_->wait_for(std::chrono::seconds{0}) != std::future_status::ready) {
        return;
    }
    const std::optional<library::Game> found = lookup_->get();
    lookup_.reset();
    if (!found || !library::netplayable(*found) || !view_.asked) {
        meeting_->decline("it does not have this ROM on a built-in core");
        return;
    }
    const lan::wire::NetplayFacts here = run_.factsOf(*found);
    if (const std::string toHost = lan::netplayDifference(here, view_.asked->facts);
        !toHost.empty()) {
        meeting_->decline(toHost);
        return;
    }
    game_ = found;
    question(*found);
}

void OnlinePlay::question(const library::Game& game) {
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    choices.push_back({.label = "Decline",
                       .chosen =
                           [this] {
                               questionTitle_.clear();
                               if (meeting_) {
                                   meeting_->decline("your friend said no");
                               }
                           },
                       .kind = gpu2d::ui::ButtonKind::Normal,
                       .exits = true});
    choices.push_back({.label = "Play",
                       .chosen =
                           [this] {
                               questionTitle_.clear();
                               if (meeting_ && game_) {
                                   // One console on each machine runs on the host's save.
                                   meeting_->accept(
                                       library::netplayCore(*game_).empty()
                                           ? NetplayRun::contentsOf(run_.profileSave(*game_))
                                           : std::string{});
                               }
                           },
                       .kind = gpu2d::ui::ButtonKind::Primary,
                       .exits = true});
    questionTitle_ = "Play " + game.title + " online?";
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>(
        questionTitle_,
        library::netplayCore(game).empty()
            ? "Your save for this game goes to your friend's machine for the session, and theirs "
              "comes to yours. Each of you keeps your own."
            : "You play on your friend's save. Yours stays as it is, and nothing you do in this "
              "game is kept here.",
        std::move(choices), 0));
}

void OnlinePlay::closeQuestion() {
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

void OnlinePlay::begin(online::Agreement& agreement, const library::Game& game) {
    const bool host = hosting_;
    closeScreen();
    const std::string peer =
        std::format("{}:{}", lucent::net::format_ipv4(agreement.peer.address), agreement.peer.port);
    const std::string why = run_.begin({.game = game,
                                        .slot = host ? 0U : 1U,
                                        .peer = peer,
                                        .port = agreement.port,
                                        .sendKey = agreement.sendKey,
                                        .receiveKey = agreement.receiveKey,
                                        .saves = {agreement.hostSave, agreement.friendSave},
                                        .clock = agreement.clock,
                                        .delay = agreement.delay,
                                        .ahead = agreement.ahead});
    if (!why.empty()) {
        hooks_.say("No game: " + why, true);
        return;
    }
    lease_ = std::move(agreement.lease);
    bridge_ = std::move(agreement.bridge);
}

void OnlinePlay::service() {
    serviceHash();
    serviceLobbies();
    if (!meeting_) {
        return;
    }
    using Step = online::Meeting::Step;
    view_ = meeting_->view();
    if (view_.step == Step::Asking && !answered_ && view_.asked) {
        answered_ = true;
        lookFor(*view_.asked);
    }
    serviceLookup();
    if (view_.step == Step::Failed) {
        closeQuestion();
    }
    if (view_.step == Step::Ready) {
        const std::unique_ptr<online::Agreement> agreement = meeting_->takeAgreement();
        if (agreement && game_) {
            begin(*agreement, *game_);
            meeting_.reset();
            game_.reset();
            view_ = {};
        }
    }
}

void OnlinePlay::ended() {
    lease_.reset();
    bridge_.reset();
}

} // namespace opensu::app

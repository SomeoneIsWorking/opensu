#include "lobbies.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

#include "lucent/log.h"
#include "lucent/p2p/crypto.h"
#include "lucent/p2p/http_directory.h"
#include "lucent/p2p/lobby_seats.h"
#include "lucent/p2p/nostr_directory.h"
#include "lucent/text.h"

namespace opensu::online {
namespace {

/// How often a host looks for asks, and a friend for the answer.
constexpr std::chrono::seconds hostLooks{2};
constexpr std::chrono::seconds friendLooks{1};
/// How long a friend waits for the host's answer.
constexpr std::chrono::seconds answerWait{40};

/// Sleeps `wait` unless asked to stop; true when asked.
bool stopped(const std::stop_token& stop, std::chrono::milliseconds wait) {
    const auto until = std::chrono::steady_clock::now() + wait;
    while (std::chrono::steady_clock::now() < until) {
        if (stop.stop_requested()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
    return stop.stop_requested();
}

} // namespace

LobbyPlaces LobbyPlaces::of(const std::string& relays, const std::string& server) {
    LobbyPlaces places;
    places.server = std::string{lucent::text::trim(server)};
    std::string_view rest = relays;
    while (!rest.empty()) {
        const std::size_t comma = rest.find(',');
        const std::string_view relay = lucent::text::trim(rest.substr(0, comma));
        if (!relay.empty()) {
            places.relays.emplace_back(relay);
        }
        rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
    }
    return places;
}

std::unique_ptr<lucent::p2p::Directory> LobbyPlaces::directory() const {
    if (!server.empty()) {
        return std::make_unique<lucent::p2p::HttpDirectory>(std::string{lobbyGame}, server,
                                                            lucent::p2p::TlsTrust{});
    }
    std::vector<std::string> urls = relays;
    if (urls.empty()) {
        urls.assign(lucent::p2p::kDefaultNostrRelays.begin(),
                    lucent::p2p::kDefaultNostrRelays.end());
    }
    return std::make_unique<lucent::p2p::NostrDirectory>(std::string{lobbyGame}, std::move(urls),
                                                         lucent::p2p::TlsTrust{});
}

PublicLobby::PublicLobby(LobbyPlaces places, std::string title, std::string invite)
    : thread_{[this, places = std::move(places), title = std::move(title),
               invite = std::move(invite)](const std::stop_token& stop) {
          run(stop, places, title, invite);
      }} {
}

PublicLobby::~PublicLobby() {
    thread_.request_stop();
}

PublicLobby::View PublicLobby::view() const {
    const std::lock_guard lock{mutex_};
    return view_;
}

void PublicLobby::show(const View& view) {
    const std::lock_guard lock{mutex_};
    view_ = view;
}

void PublicLobby::run(const std::stop_token& stop, const LobbyPlaces& places,
                      const std::string& title, const std::string& invite) {
    std::unique_ptr<lucent::p2p::Directory> directory;
    try {
        directory = places.directory();
    } catch (const std::invalid_argument& bad) {
        show({.listed = false, .seated = false, .problem = bad.what()});
        return;
    }
    lucent::p2p::Lobby lobby;
    lobby.id = lucent::text::hex(lucent::p2p::crypto::random_bytes(8));
    lobby.title = title.substr(0, lucent::p2p::kMaxLobbyTitleBytes);
    lobby.players = 1;
    lobby.capacity = 2;
    lobby.version = std::string{lobbyVersion};
    lucent::p2p::LobbyHost host{*directory, std::move(lobby)};
    bool seated = false;
    while (!stop.stop_requested() && !seated) {
        std::string error;
        if (!host.keep_listed(error)) {
            lucent::log(lucent::Level::Warn, "online", "the public game is not listed: " + error);
            show({.listed = false, .seated = false, .problem = "Could not list the game: " + error});
        } else {
            show({.listed = true, .seated = false, .problem = {}});
        }
        const std::optional<std::vector<lucent::p2p::LobbyHost::Asker>> asks = host.asks(error);
        for (const lucent::p2p::LobbyHost::Asker& asker : asks.value_or(
                 std::vector<lucent::p2p::LobbyHost::Asker>{})) {
            // The code works once: the first asker gets it, the rest hear the game is full.
            if (host.answer(asker, seated ? std::nullopt : std::optional<std::string>{invite},
                            error) &&
                !seated) {
                seated = true;
                lucent::log(lucent::Level::Info, "online", "a friend asked for the public game");
            }
        }
        if (!seated && stopped(stop, hostLooks)) {
            break;
        }
    }
    std::string error;
    if (!host.withdraw(error)) {
        lucent::log(lucent::Level::Warn, "online", "the public game stays listed until it lapses: " +
                                                       error);
    }
    show({.listed = false, .seated = seated, .problem = {}});
}

LobbyFinder::LobbyFinder(LobbyPlaces places) : places_{std::move(places)} {
    refresh();
}

LobbyFinder::~LobbyFinder() {
    thread_.request_stop();
}

void LobbyFinder::refresh() {
    // The last look ends before the new one shows.
    thread_ = {};
    show({.step = Step::Listing, .games = {}, .invite = {}, .problem = {}});
    thread_ = std::jthread{[this](const std::stop_token& stop) {
        list(stop);
    }};
}

void LobbyFinder::ask(std::size_t index) {
    std::optional<lucent::p2p::Lobby> lobby;
    {
        const std::lock_guard lock{mutex_};
        if (view_.step != Step::Listed || index >= lobbies_.size()) {
            return;
        }
        lobby = lobbies_[index];
        view_.step = Step::Asking;
    }
    thread_ = std::jthread{[this, lobby = std::move(*lobby)](const std::stop_token& stop) {
        request(stop, lobby);
    }};
}

LobbyFinder::View LobbyFinder::view() const {
    const std::lock_guard lock{mutex_};
    return view_;
}

void LobbyFinder::show(const View& view) {
    const std::lock_guard lock{mutex_};
    view_ = view;
}

void LobbyFinder::list(const std::stop_token& stop) {
    std::unique_ptr<lucent::p2p::Directory> directory;
    try {
        directory = places_.directory();
    } catch (const std::invalid_argument& bad) {
        show({.step = Step::Failed, .games = {}, .invite = {}, .problem = bad.what()});
        return;
    }
    std::string error;
    std::optional<std::vector<lucent::p2p::Lobby>> found = directory->list(error);
    if (stop.stop_requested()) {
        return;
    }
    if (!found) {
        show({.step = Step::Failed,
              .games = {},
              .invite = {},
              .problem = "Could not get the list of games: " + error});
        return;
    }
    std::erase_if(*found, [](const lucent::p2p::Lobby& lobby) {
        return lobby.version != lobbyVersion || lobby.players >= lobby.capacity;
    });
    View view{.step = Step::Listed, .games = {}, .invite = {}, .problem = {}};
    for (const lucent::p2p::Lobby& lobby : *found) {
        view.games.push_back(
            {.title = lobby.title, .players = lobby.players, .capacity = lobby.capacity});
    }
    const std::lock_guard lock{mutex_};
    lobbies_ = std::move(*found);
    view_ = std::move(view);
}

void LobbyFinder::request(const std::stop_token& stop, const lucent::p2p::Lobby& lobby) {
    View view = this->view();
    std::unique_ptr<lucent::p2p::Directory> directory;
    std::optional<lucent::p2p::LobbyGuest> guest;
    try {
        directory = places_.directory();
        guest.emplace(*directory, lobby);
    } catch (const std::invalid_argument& bad) {
        view.step = Step::Failed;
        view.problem = bad.what();
        show(view);
        return;
    }
    std::string error;
    if (!guest->ask(error)) {
        view.step = Step::Failed;
        view.problem = "Could not ask the host: " + error;
        show(view);
        return;
    }
    const auto until = std::chrono::steady_clock::now() + answerWait;
    while (std::chrono::steady_clock::now() < until) {
        if (stopped(stop, friendLooks)) {
            return;
        }
        std::optional<lucent::p2p::LobbyGuest::Answer> answer = guest->answer(error);
        if (!answer) {
            continue;
        }
        if (answer->invite) {
            view.step = Step::Invited;
            view.invite = std::move(*answer->invite);
        } else {
            view.step = Step::Full;
        }
        show(view);
        return;
    }
    view.step = Step::Failed;
    view.problem = "The host did not answer. The game may have just started or closed.";
    show(view);
}

} // namespace opensu::online

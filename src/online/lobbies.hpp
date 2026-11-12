// lobbies — Play online's public games, each on a thread of its own: a host lists its game with
// the name the player typed (`PublicLobby`) and hands the meeting's code, sealed, to the first
// player who asks; a friend lists the games (`LobbyFinder`) and asks one for its code. A listing
// carries no code and no address (Lucent's `p2p::LobbyHost`, `p2p::LobbyGuest`); the friend then
// joins by that code as by a pasted one.
#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "lucent/p2p/directory.h"

namespace opensu::online {

/// openSU's name in a directory.
inline constexpr std::string_view lobbyGame = "opensu";
/// The meeting's messages; a lobby of another is not listed. Raise it when they change.
inline constexpr std::string_view lobbyVersion = "1";

/// Where lobbies are listed: the self-hosted lobby server when one is named, else Nostr relays.
struct LobbyPlaces {
    /// `wss://` URLs; empty for Lucent's default relays.
    std::vector<std::string> relays;
    /// A `lucent_lobby_server`'s base URL; empty for none.
    std::string server;

    /// From the settings' comma-separated relay list and server URL.
    [[nodiscard]] static LobbyPlaces of(const std::string& relays, const std::string& server);
    /// A directory for these places; throws std::invalid_argument for a URL that is not one.
    [[nodiscard]] std::unique_ptr<lucent::p2p::Directory> directory() const;
};

class PublicLobby {
  public:
    struct View {
        /// The listing is up.
        bool listed{false};
        /// A friend asked and has the code; the listing is down.
        bool seated{false};
        /// Why the listing is not up, for the player.
        std::string problem;
    };

    /// Lists `title` until a friend asks, then hands that friend `invite`.
    PublicLobby(LobbyPlaces places, std::string title, std::string invite);
    /// Takes the listing down.
    ~PublicLobby();
    PublicLobby(const PublicLobby&) = delete;
    PublicLobby& operator=(const PublicLobby&) = delete;
    PublicLobby(PublicLobby&&) = delete;
    PublicLobby& operator=(PublicLobby&&) = delete;

    [[nodiscard]] View view() const;

  private:
    void run(const std::stop_token& stop, const LobbyPlaces& places, const std::string& title,
             const std::string& invite);
    void show(const View& view);

    mutable std::mutex mutex_;
    View view_;
    std::jthread thread_;
};

class LobbyFinder {
  public:
    enum class Step : std::uint8_t { Listing, Listed, Asking, Invited, Full, Failed };
    struct Game {
        std::string title;
        int players{0};
        int capacity{0};
    };
    struct View {
        Step step{Step::Listing};
        std::vector<Game> games;
        /// The host's code once `Invited`.
        std::string invite;
        /// Why it is `Failed`, for the player.
        std::string problem;
    };

    /// Lists the public games at once.
    explicit LobbyFinder(LobbyPlaces places);
    ~LobbyFinder();
    LobbyFinder(const LobbyFinder&) = delete;
    LobbyFinder& operator=(const LobbyFinder&) = delete;
    LobbyFinder(LobbyFinder&&) = delete;
    LobbyFinder& operator=(LobbyFinder&&) = delete;

    /// Lists again.
    void refresh();
    /// Asks the host of the listed game `index` for its code.
    void ask(std::size_t index);
    [[nodiscard]] View view() const;

  private:
    void list(const std::stop_token& stop);
    void request(const std::stop_token& stop, const lucent::p2p::Lobby& lobby);
    void show(const View& view);

    LobbyPlaces places_;
    mutable std::mutex mutex_;
    View view_;
    std::vector<lucent::p2p::Lobby> lobbies_;
    std::jthread thread_;
};

} // namespace opensu::online

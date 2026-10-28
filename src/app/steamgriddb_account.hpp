// steamgriddb_account — the player's SteamGridDB connection: the API key read back from the
// secret store at start, got through Steam's browser (`steam::SteamGridDbSignIn`) or typed in
// after SteamGridDB's API page opens in the default browser, kept in the secret store and handed
// to the artwork source; Disconnect forgets it. Every step runs on its own worker; the loop takes
// what happened with `take`, the news and the state it left together.
//
// Which browser Connect uses: openSU's own Steam (Managed, Isolated) is started in its mode when
// it is not up and waited for until logged on, then signs in through Big Picture's browser; a
// desktop Steam that refuses it (Blocked) or a Steam that does not start is an error, not the
// default browser. Normal signs in through the desktop Steam when its DevTools answer, else the
// default browser. Only with no Steam to use (off, not installed, a hidden run starting none) does
// Connect go straight to the default browser.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "artwork/steamgriddb.hpp"
#include "host/secret_store.hpp"
#include "launch/steam_gate.hpp"
#include "steam/steamgriddb_sign_in.hpp"

namespace opensu::app {

class SteamGridDbAccount {
  public:
    /// Where the connection is.
    enum class State : std::uint8_t {
        /// The saved key is being read.
        Loading,
        Disconnected,
        /// openSU's own Steam is started, or waited for until it is logged on.
        StartingSteam,
        /// Steam's browser is being asked to open SteamGridDB.
        OpeningSteam,
        /// The player confirms Steam's sign-in page.
        WaitingForPlayer,
        ReadingKey,
        Connected,
    };

    struct Status {
        State state{State::Loading};
        /// Where the key is kept, when connected.
        host::SecretPlace place{host::SecretPlace::File};
        /// SteamGridDB refused the key.
        bool refused{false};

        bool operator==(const Status&) const = default;
    };

    /// Something the player hears about.
    struct News {
        std::string text;
        bool error{false};
        /// The key came or went: artwork is asked for again.
        bool keyChanged{false};
        /// Steam could not get the key: the player types it in.
        bool needsKey{false};
    };

    struct Parts {
        host::SecretStore secrets;
        /// The artwork source the key is handed to; outlives this.
        artwork::SteamGridDb& source;
        std::uint16_t devToolsPort{steam::devToolsPort};
        steam::SteamGridDbSignIn::Timing timing{};
        /// Opens a URL in the player's default browser.
        std::string browser{"xdg-open"};
        /// The Steam client in force, asked on the main loop at each Connect; null when there is
        /// none to use. It outlives this.
        std::function<launch::SteamGate*()> steam{[] {
            return nullptr;
        }};
    };

    /// Starts the worker, which first reads the saved key.
    explicit SteamGridDbAccount(Parts parts);
    ~SteamGridDbAccount();
    SteamGridDbAccount(const SteamGridDbAccount&) = delete;
    SteamGridDbAccount& operator=(const SteamGridDbAccount&) = delete;

    /// Gets the key through Steam's browser; when Steam cannot be asked, opens SteamGridDB's API
    /// page in the default browser and asks for the key (`News::needsKey`).
    void connect();
    /// Opens SteamGridDB's API page in the default browser, for a key typed in.
    void openKeyPage();
    /// Keeps `key`, typed in by the player, when it looks like one.
    void useKey(std::string key);
    /// Ends a sign-in through Steam.
    void cancel();
    /// Forgets the key.
    void disconnect();

    [[nodiscard]] Status status() const;
    /// Whether a sign-in through Steam is under way, whose pages the player answers in Steam's
    /// own windows.
    [[nodiscard]] bool signingIn() const;
    /// Whether `state` is one of a sign-in through Steam.
    [[nodiscard]] static bool signingIn(State state) noexcept;
    /// What happened since the last call and the state it left, read together, so a state is
    /// never seen before the news that came with it. Main loop only.
    struct Taken {
        std::vector<News> news;
        Status status;
    };
    [[nodiscard]] Taken take();

  private:
    struct Load {};
    struct Connect {
        launch::SteamGate* steam{nullptr};
    };
    struct OpenPage {};
    struct UseKey {
        std::string key;
    };
    struct Disconnect {};
    using Work = std::variant<Load, Connect, OpenPage, UseKey, Disconnect>;

    void post(Work work);
    void run(const std::stop_token& stop);
    void perform(const Load& load, const std::stop_token& stop);
    void perform(const Connect& connect, const std::stop_token& stop);
    /// Starts openSU's own Steam when it is not up and waits until it is logged on; what to tell
    /// the player when it does not get there, nothing when it does.
    [[nodiscard]] std::optional<News> bringUp(launch::SteamGate& steam,
                                              const std::stop_token& cancelled);
    /// No Steam to sign in with: the API page in the default browser and the key typed in.
    void askForKey(const std::string& why, const std::stop_token& stop);
    void perform(const OpenPage& page, const std::stop_token& stop);
    void perform(UseKey& use, const std::stop_token& stop);
    void perform(const Disconnect& disconnect, const std::stop_token& stop);
    void keep(std::string& key);
    void tell(News news);
    /// `status` with `mutex_` held.
    [[nodiscard]] Status statusLocked() const;
    /// Ends a step in `state` with `news`, both seen at once.
    void finish(State state, News news);
    void setState(State state);

    Parts parts_;
    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::vector<Work> queue_;
    std::vector<News> news_;
    State state_{State::Loading};
    host::SecretPlace place_{host::SecretPlace::File};
    std::stop_source signIn_;
    /// Last, so the worker stops before anything it uses is destroyed.
    std::jthread worker_;
};

} // namespace opensu::app

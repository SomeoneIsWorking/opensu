// steamgriddb_sign_in — gets the player's SteamGridDB API key through Steam's own browser.
// SteamGridDB signs in only with Steam (OpenID), so the sign-in page opens in a Steam browser view
// (`steam://openurl`), where the client's Steam login is already held: the player confirms
// Steam's "Sign in" page and nothing else. The view then goes to SteamGridDB's API page, makes a
// key when there is none, and the key is read from that page before the view is closed. Scripts
// run only in that view and only while it shows SteamGridDB; no Steam page, cookie or credential
// is read. openSU's own Steam (`-silent`, inside openSU's Gamescope) opens no window for
// `steam://openurl` on its desktop UI, so there the sign-in opens Big Picture first, whose browser
// shows the page, and closes it after.
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

#include "big_picture.hpp"
#include "devtools.hpp"

namespace opensu::steam {

/// SteamGridDB's site; scripts run only on pages of this origin.
inline constexpr std::string_view steamGridDbOrigin = "https://www.steamgriddb.com";
/// The page that shows (and makes) an account's API key.
inline constexpr std::string_view steamGridDbKeyPath = "/profile/preferences/api";

/// Whether `key` looks like a SteamGridDB API key: 32 hex digits.
[[nodiscard]] bool isSteamGridDbKey(std::string_view key) noexcept;

/// How the sign-in waits for the player.
struct SignInTiming {
    /// How often the view is looked at.
    std::chrono::milliseconds poll{std::chrono::milliseconds{500}};
    /// How long the player has to confirm before the sign-in gives up.
    std::chrono::milliseconds limit{std::chrono::minutes{5}};
};

class SteamGridDbSignIn {
  public:
    /// How far the sign-in got, for the player.
    enum class Step : std::uint8_t {
        /// The view is open on SteamGridDB's sign-in; the player confirms Steam's page.
        WaitingForPlayer,
        /// Signed in; the API page is being read.
        ReadingKey,
    };

    /// Where Steam shows the page.
    enum class View : std::uint8_t {
        /// The client's own browser window: a desktop Steam.
        SteamWindow,
        /// Big Picture's browser, opened for the sign-in when it is not up: openSU's own Steam.
        BigPicture,
    };

    using Timing = SignInTiming;

    explicit SteamGridDbSignIn(std::uint16_t port = devToolsPort, Timing timing = {},
                               View view = View::SteamWindow);

    /// Whether Steam answers on DevTools, which the sign-in needs.
    [[nodiscard]] bool reachable(std::string& error);

    /// Runs the sign-in to the end: the key, or nothing with `error` (Steam unreachable, Big
    /// Picture not opening, the view closed by the player, the time limit, `stop`). `step` hears
    /// each step as it starts. The view, and Big Picture when the sign-in opened it, are closed
    /// either way. Blocks; run it off the frame loop.
    [[nodiscard]] std::optional<std::string>
    run(const std::stop_token& stop, const std::function<void(Step)>& step, std::string& error);

  private:
    /// Opens Big Picture unless it shows and waits until it does; false with `error` when it does
    /// not come up before `deadline` or `stop`.
    [[nodiscard]] bool showBigPicture(const std::stop_token& stop,
                                      std::chrono::steady_clock::time_point deadline, bool& opened,
                                      std::string& error);

    std::uint16_t port_;
    Timing timing_;
    View view_;
    DevTools shared_;
    BigPicture bigPicture_{shared_};
};

} // namespace opensu::steam

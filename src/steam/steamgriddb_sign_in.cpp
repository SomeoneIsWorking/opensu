#include "steamgriddb_sign_in.hpp"

#include <algorithm>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

#include "lucent/log.h"

namespace opensu::steam {
namespace {

using nlohmann::json;
using Clock = std::chrono::steady_clock;

/// The page Steam's browser view opens on: SteamGridDB's "Login via Steam".
constexpr std::string_view loginPath = "/login";

/// What the view shows, and one step taken on it. LOGIN and GENERATE say whether a press of
/// "Login via Steam" and "Generate" is still allowed; the sign-in page also links a "Steam
/// Pre-Greenlight Project", so the button is matched by "log in" and "Steam" together. A link is
/// followed in the view itself: clicked, Steam's browser opens it in a popup of its own. Runs only
/// on SteamGridDB's origin.
constexpr std::string_view stepScript = R"js((() => {
  const origin = ORIGIN, keyPath = KEY_PATH;
  if (location.origin !== origin) return { state: "elsewhere" };
  if (document.readyState !== "complete") return { state: "loading" };
  const pressable = pattern => [...document.querySelectorAll("a, button, input[type=submit]")]
    .find(e => pattern.test(String(e.textContent || e.value || "").trim()));
  if (location.pathname.startsWith(LOGIN_PATH)) {
    const steam = LOGIN ? pressable(/log\s*in\b.*\bsteam/i) : null;
    if (steam) {
      if (steam.href) location.assign(steam.href); else steam.click();
      return { state: "login", clicked: true };
    }
    return { state: "login" };
  }
  if (location.pathname !== keyPath) {
    location.assign(origin + keyPath);
    return { state: "navigating" };
  }
  const hex = /\b[0-9a-f]{32}\b/i;
  for (const e of document.querySelectorAll("input, textarea, code, pre, kbd")) {
    const found = hex.exec(String(e.value ?? e.textContent ?? ""));
    if (found) return { state: "key", key: found[0] };
  }
  const generate = GENERATE ? pressable(/generate/i) : null;
  if (generate) { generate.click(); return { state: "nokey", clicked: true }; }
  return { state: "nokey" };
})())js";

std::string jsString(std::string_view text) {
    return json(std::string{text}).dump();
}

std::string scriptFor(bool login, bool generate) {
    std::string script{stepScript};
    const auto fill = [&script](std::string_view name, const std::string& value) {
        const std::size_t at = script.find(name);
        script.replace(at, name.size(), value);
    };
    fill("ORIGIN", jsString(steamGridDbOrigin));
    fill("KEY_PATH", jsString(steamGridDbKeyPath));
    fill("LOGIN_PATH", jsString(loginPath));
    fill("LOGIN ?", login ? "true ?" : "false ?");
    fill("GENERATE ?", generate ? "true ?" : "false ?");
    return script;
}

bool onSteamGridDb(std::string_view url) {
    return url.starts_with(std::string{steamGridDbOrigin} + "/");
}

std::string openScript() {
    const std::string url =
        "steam://openurl/" + std::string{steamGridDbOrigin} + std::string{loginPath};
    return "SteamClient.URL.ExecuteSteamURL(" + jsString(url) + "), true";
}

/// The browser view the sign-in opened: a page on SteamGridDB that was not there before, else a
/// page already on SteamGridDB's sign-in, which Steam may have reused. Steam opens popups of its
/// own at any time, so a new page counts only once it shows SteamGridDB.
std::optional<std::string> openedView(const std::vector<DevToolsTarget>& now,
                                      const std::vector<std::string>& before) {
    for (const DevToolsTarget& target : now) {
        if (target.type == "page" && onSteamGridDb(target.url) &&
            std::ranges::find(before, target.id) == before.end()) {
            return target.id;
        }
    }
    const std::string login = std::string{steamGridDbOrigin} + std::string{loginPath};
    for (const DevToolsTarget& target : now) {
        if (target.type == "page" && target.url.starts_with(login)) {
            return target.id;
        }
    }
    return std::nullopt;
}

/// Waits `poll`; false when `stop` came first.
bool rest(const std::stop_token& stop, std::chrono::milliseconds poll) {
    std::mutex mutex;
    std::condition_variable_any wake;
    std::unique_lock lock{mutex};
    static_cast<void>(wake.wait_for(lock, stop, poll, [] {
        return false;
    }));
    return !stop.stop_requested();
}

/// Closes Big Picture on the way out when the sign-in opened it.
struct LeaveBigPicture {
    explicit LeaveBigPicture(BigPicture& shown) : bigPicture{shown} {
    }
    LeaveBigPicture(const LeaveBigPicture&) = delete;
    LeaveBigPicture(LeaveBigPicture&&) = delete;
    LeaveBigPicture& operator=(LeaveBigPicture&&) = delete;
    LeaveBigPicture& operator=(const LeaveBigPicture&) = delete;
    ~LeaveBigPicture() {
        if (opened) {
            bigPicture.close();
        }
    }
    BigPicture& bigPicture;
    bool opened{false};
};

} // namespace

bool isSteamGridDbKey(std::string_view key) noexcept {
    constexpr std::size_t keyLength = 32;
    return key.size() == keyLength && std::ranges::all_of(key, [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
           });
}

SteamGridDbSignIn::SteamGridDbSignIn(std::uint16_t port, Timing timing, View view)
    : port_{port}, timing_{timing}, view_{view}, shared_{port} {
}

bool SteamGridDbSignIn::showBigPicture(const std::stop_token& stop, Clock::time_point deadline,
                                       bool& opened, std::string& error) {
    std::optional<bool> shown = bigPicture_.shown(error);
    if (!shown) {
        return false;
    }
    if (*shown) {
        return true;
    }
    if (!bigPicture_.open(error)) {
        return false;
    }
    opened = true;
    while (rest(stop, timing_.poll)) {
        if (Clock::now() > deadline) {
            error = "Steam's Big Picture did not open";
            return false;
        }
        shown = bigPicture_.shown(error);
        if (!shown) {
            return false;
        }
        if (*shown) {
            return true;
        }
    }
    error = "cancelled";
    return false;
}

bool SteamGridDbSignIn::reachable(std::string& error) {
    return shared_.evaluate("true", error).has_value();
}

std::optional<std::string> SteamGridDbSignIn::run(const std::stop_token& stop,
                                                  const std::function<void(Step)>& step,
                                                  std::string& error) {
    const Clock::time_point deadline = Clock::now() + timing_.limit;
    LeaveBigPicture leave{bigPicture_};
    if (view_ == View::BigPicture && !showBigPicture(stop, deadline, leave.opened, error)) {
        return std::nullopt;
    }
    const std::optional<std::vector<DevToolsTarget>> before = devToolsTargets(port_, error);
    if (!before) {
        return std::nullopt;
    }
    std::vector<std::string> known;
    for (const DevToolsTarget& target : *before) {
        known.push_back(target.id);
    }
    if (!shared_.evaluate(openScript(), error)) {
        return std::nullopt;
    }
    std::optional<std::string> view;
    std::unique_ptr<DevTools> page;
    std::optional<std::string> key;
    bool loginPressed = false;
    bool generatePressed = false;
    bool reading = false;
    for (;;) {
        if (!rest(stop, timing_.poll)) {
            error = "cancelled";
            break;
        }
        if (Clock::now() > deadline) {
            error = "Steam's sign-in was not confirmed in time";
            break;
        }
        const std::optional<std::vector<DevToolsTarget>> targets = devToolsTargets(port_, error);
        if (!targets) {
            break;
        }
        if (!view) {
            view = openedView(*targets, known);
            if (!view) {
                continue;
            }
            lucent::info("steamgriddb", "sign-in opened in Steam's browser");
            step(Step::WaitingForPlayer);
        }
        const auto shown = std::ranges::find(*targets, *view, &DevToolsTarget::id);
        if (shown == targets->end()) {
            error = "the SteamGridDB window was closed";
            view.reset();
            break;
        }
        if (!onSteamGridDb(shown->url)) {
            continue;
        }
        if (!page) {
            page = std::make_unique<DevTools>(port_, *view);
        }
        std::string pageError;
        const std::optional<json> answer =
            page->evaluate(scriptFor(!loginPressed, !generatePressed), pageError);
        // A navigation can drop the call; the next look asks again.
        if (!answer || !answer->is_object()) {
            continue;
        }
        const std::string state = answer->value("state", "");
        const bool pressed = answer->value("clicked", false);
        if (state == "login") {
            loginPressed = loginPressed || pressed;
            continue;
        }
        if ((state == "nokey" || state == "key") && !reading) {
            reading = true;
            step(Step::ReadingKey);
        }
        if (state == "nokey") {
            generatePressed = generatePressed || pressed;
            continue;
        }
        if (state == "key") {
            std::string found = answer->value("key", "");
            if (isSteamGridDbKey(found)) {
                key = std::move(found);
                break;
            }
        }
    }
    if (view) {
        std::string closeError;
        if (!closeDevToolsTarget(port_, *view, closeError)) {
            lucent::warn("steamgriddb", "{}", closeError);
        }
    }
    if (key) {
        error.clear();
        lucent::info("steamgriddb", "API key read from SteamGridDB");
    }
    return key;
}

} // namespace opensu::steam

#include "steamgriddb_account.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

#include "launch/command.hpp"
#include "lucent/log.h"
#include "secrets/secret.hpp"

namespace opensu::app {
namespace {

/// The secret's name in the store, and what the keyring's own manager shows for it.
constexpr std::string_view keyName = "steamgriddb-api-key";
constexpr std::string_view keyLabel = "openSU: SteamGridDB API key";
constexpr std::chrono::seconds browserTimeout{60};
/// How long Connect waits for openSU's own Steam to log on, as a launch does.
constexpr std::chrono::minutes steamStartLimit{3};

std::string placeText(secrets::SecretPlace place) {
    return place == secrets::SecretPlace::Keyring ? "the keyring" : "a private file";
}

} // namespace

SteamGridDbAccount::SteamGridDbAccount(Parts parts)
    : parts_{std::move(parts)}, worker_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
    post(Load{});
}

SteamGridDbAccount::~SteamGridDbAccount() {
    {
        const std::lock_guard lock{mutex_};
        signIn_.request_stop();
    }
    worker_.request_stop();
    wake_.notify_all();
}

void SteamGridDbAccount::connect() {
    launch::SteamGate* steam = parts_.steam();
    {
        const std::lock_guard lock{mutex_};
        signIn_ = std::stop_source{};
        state_ =
            steam != nullptr && steam->ownsClient() ? State::StartingSteam : State::OpeningSteam;
    }
    post(Connect{steam});
}

void SteamGridDbAccount::openKeyPage() {
    post(OpenPage{});
}

void SteamGridDbAccount::useKey(std::string key) {
    post(UseKey{std::move(key)});
}

void SteamGridDbAccount::cancel() {
    const std::lock_guard lock{mutex_};
    signIn_.request_stop();
}

void SteamGridDbAccount::disconnect() {
    post(Disconnect{});
}

SteamGridDbAccount::Status SteamGridDbAccount::status() const {
    const std::lock_guard lock{mutex_};
    return statusLocked();
}

SteamGridDbAccount::Status SteamGridDbAccount::statusLocked() const {
    return Status{state_, place_, state_ == State::Connected && parts_.source.refused()};
}

bool SteamGridDbAccount::signingIn() const {
    const std::lock_guard lock{mutex_};
    return signingIn(state_);
}

bool SteamGridDbAccount::signingIn(State state) noexcept {
    return state == State::StartingSteam || state == State::OpeningSteam ||
           state == State::WaitingForPlayer || state == State::ReadingKey;
}

SteamGridDbAccount::Taken SteamGridDbAccount::take() {
    const std::lock_guard lock{mutex_};
    return Taken{.news = std::exchange(news_, {}), .status = statusLocked()};
}

void SteamGridDbAccount::post(Work work) {
    {
        const std::lock_guard lock{mutex_};
        queue_.push_back(std::move(work));
    }
    wake_.notify_all();
}

void SteamGridDbAccount::tell(News news) {
    const std::lock_guard lock{mutex_};
    news_.push_back(std::move(news));
}

void SteamGridDbAccount::finish(State state, News news) {
    const std::lock_guard lock{mutex_};
    state_ = state;
    news_.push_back(std::move(news));
}

void SteamGridDbAccount::setState(State state) {
    const std::lock_guard lock{mutex_};
    state_ = state;
}

void SteamGridDbAccount::run(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
        Work work;
        {
            std::unique_lock lock{mutex_};
            if (!wake_.wait(lock, stop, [this] {
                    return !queue_.empty();
                })) {
                return;
            }
            work = std::move(queue_.front());
            queue_.erase(queue_.begin());
        }
        std::visit(
            [this, &stop](auto& job) {
                perform(job, stop);
            },
            work);
    }
}

void SteamGridDbAccount::perform(const Load& /*load*/, const std::stop_token& /*stop*/) {
    std::string error;
    std::optional<secrets::SecretStore::Found> found = parts_.secrets.read(keyName, error);
    if (!error.empty()) {
        lucent::warn("steamgriddb", "saved key: {}", error);
    }
    if (!found || !steam::isSteamGridDbKey(found->value)) {
        if (found) {
            secrets::wipe(found->value);
            lucent::warn("steamgriddb", "the saved key is not a SteamGridDB key");
        }
        setState(State::Disconnected);
        return;
    }
    parts_.source.setKey(std::move(found->value));
    {
        const std::lock_guard lock{mutex_};
        place_ = found->place;
        state_ = State::Connected;
    }
    lucent::info("steamgriddb", "connected, key kept in {}", placeText(found->place));
}

void SteamGridDbAccount::perform(const Connect& connect, const std::stop_token& stop) {
    std::stop_token cancelled;
    {
        const std::lock_guard lock{mutex_};
        cancelled = signIn_.get_token();
    }
    // Shutting down ends the sign-in too.
    const std::stop_callback ending{stop, [this] {
                                        const std::lock_guard lock{mutex_};
                                        signIn_.request_stop();
                                    }};
    if (connect.steam == nullptr) {
        askForKey("no Steam to sign in with", stop);
        return;
    }
    // openSU's own Steam has no desktop window in openSU's Gamescope; Big Picture shows the page.
    const bool own = connect.steam->ownsClient();
    if (own) {
        if (std::optional<News> failed = bringUp(*connect.steam, cancelled)) {
            finish(State::Disconnected, std::move(*failed));
            return;
        }
    }
    setState(State::OpeningSteam);
    steam::SteamGridDbSignIn signIn{parts_.devToolsPort, parts_.timing,
                                    own ? steam::SteamGridDbSignIn::View::BigPicture
                                        : steam::SteamGridDbSignIn::View::SteamWindow};
    std::string error;
    if (!signIn.reachable(error)) {
        if (!own) {
            askForKey("the desktop Steam's DevTools do not answer: " + error, stop);
            return;
        }
        lucent::warn("steamgriddb", "sign-in through Steam: {}", error);
        finish(State::Disconnected, {"SteamGridDB sign-in failed: " + error, true});
        return;
    }
    lucent::info("steamgriddb", "signing in through {} Steam",
                 own ? "openSU's own" : "the desktop");
    std::optional<std::string> key = signIn.run(
        cancelled,
        [this](steam::SteamGridDbSignIn::Step step) {
            setState(step == steam::SteamGridDbSignIn::Step::WaitingForPlayer
                         ? State::WaitingForPlayer
                         : State::ReadingKey);
        },
        error);
    if (!key) {
        lucent::warn("steamgriddb", "sign-in through Steam: {}", error);
        finish(State::Disconnected, {error == "cancelled" ? "SteamGridDB sign-in cancelled"
                                                          : "SteamGridDB sign-in failed: " + error,
                                     error != "cancelled"});
        return;
    }
    keep(*key);
}

std::optional<SteamGridDbAccount::News>
SteamGridDbAccount::bringUp(launch::SteamGate& steam, const std::stop_token& cancelled) {
    steam.ensureStarted();
    const launch::SteamState state = steam.waitReady(steamStartLimit, [&cancelled] {
        return cancelled.stop_requested();
    });
    if (cancelled.stop_requested()) {
        lucent::warn("steamgriddb", "sign-in through Steam: cancelled while Steam started");
        return News{"SteamGridDB sign-in cancelled", false};
    }
    if (state == launch::SteamState::Ready) {
        return std::nullopt;
    }
    const std::string why = launch::notReadyReason(state);
    lucent::warn("steamgriddb", "sign-in through Steam: {}", why);
    return News{"SteamGridDB sign-in failed: " + why, true};
}

void SteamGridDbAccount::askForKey(const std::string& why, const std::stop_token& stop) {
    lucent::info("steamgriddb", "Steam cannot sign in ({}); asking for the key", why);
    setState(State::Disconnected);
    perform(OpenPage{}, stop);
    tell({"Steam is not available: copy the key from SteamGridDB's page in your browser", false,
          false, true});
}

void SteamGridDbAccount::perform(const OpenPage& /*page*/, const std::stop_token& /*stop*/) {
    const std::string url =
        std::string{steam::steamGridDbOrigin} + std::string{steam::steamGridDbKeyPath};
    const std::optional<int> status = launch::runCommand(parts_.browser, {url}, browserTimeout);
    if (!status || *status != 0) {
        lucent::warn("steamgriddb", "the browser did not open");
        tell({"cannot open a browser with " + parts_.browser + "; the key is on " + url, true});
    }
}

void SteamGridDbAccount::perform(UseKey& use, const std::stop_token& /*stop*/) {
    // A pasted key can bring the line's spaces with it.
    const auto blank = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };
    while (!use.key.empty() && blank(use.key.back())) {
        use.key.pop_back();
    }
    const auto first = std::ranges::find_if_not(use.key, blank);
    use.key.erase(use.key.begin(), first);
    if (!steam::isSteamGridDbKey(use.key)) {
        secrets::wipe(use.key);
        tell({"that is not a SteamGridDB key: it is 32 letters and digits", true});
        return;
    }
    keep(use.key);
}

void SteamGridDbAccount::keep(std::string& key) {
    std::string error;
    const std::optional<secrets::SecretPlace> place =
        parts_.secrets.save(keyName, keyLabel, key, error);
    if (!place) {
        secrets::wipe(key);
        lucent::warn("steamgriddb", "cannot keep the key: {}", error);
        finish(State::Disconnected, {"cannot keep the SteamGridDB key: " + error, true});
        return;
    }
    parts_.source.setKey(std::exchange(key, {}));
    {
        const std::lock_guard lock{mutex_};
        place_ = *place;
    }
    lucent::info("steamgriddb", "connected, key kept in {}", placeText(*place));
    finish(State::Connected, {"SteamGridDB connected", false, true});
}

void SteamGridDbAccount::perform(const Disconnect& /*disconnect*/,
                                 const std::stop_token& /*stop*/) {
    std::string error;
    const bool forgotten = parts_.secrets.forget(keyName, error);
    parts_.source.setKey({});
    if (!forgotten) {
        lucent::warn("steamgriddb", "forgetting the key: {}", error);
        finish(State::Disconnected, {"SteamGridDB disconnected, but " + error, true, true});
        return;
    }
    lucent::info("steamgriddb", "disconnected, key forgotten");
    finish(State::Disconnected, {"SteamGridDB disconnected", false, true});
}

} // namespace opensu::app

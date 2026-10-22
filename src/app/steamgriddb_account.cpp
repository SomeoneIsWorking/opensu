#include "steamgriddb_account.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

#include "host/secret.hpp"
#include "launch/command.hpp"
#include "lucent/log.h"

namespace opensu::app {
namespace {

/// The secret's name in the store, and what the keyring's own manager shows for it.
constexpr std::string_view keyName = "steamgriddb-api-key";
constexpr std::string_view keyLabel = "openSU: SteamGridDB API key";
constexpr std::chrono::seconds browserTimeout{60};

std::string placeText(host::SecretPlace place) {
    return place == host::SecretPlace::Keyring ? "the keyring" : "a private file";
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
    {
        const std::lock_guard lock{mutex_};
        signIn_ = std::stop_source{};
        state_ = State::OpeningSteam;
    }
    post(Connect{});
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
    return Status{state_, place_, state_ == State::Connected && parts_.source.refused()};
}

std::vector<SteamGridDbAccount::News> SteamGridDbAccount::take() {
    const std::lock_guard lock{mutex_};
    return std::exchange(news_, {});
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
    std::optional<host::SecretStore::Found> found = parts_.secrets.read(keyName, error);
    if (!error.empty()) {
        lucent::warn("steamgriddb", "saved key: {}", error);
    }
    if (!found || !steam::isSteamGridDbKey(found->value)) {
        if (found) {
            host::wipe(found->value);
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

void SteamGridDbAccount::perform(const Connect& /*connect*/, const std::stop_token& stop) {
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
    steam::SteamGridDbSignIn signIn{parts_.devToolsPort, parts_.timing};
    std::string error;
    if (!signIn.reachable(error)) {
        lucent::info("steamgriddb", "Steam cannot sign in ({}); asking for the key", error);
        setState(State::Disconnected);
        perform(OpenPage{}, stop);
        tell({"Steam is not available: copy the key from SteamGridDB's page in your browser", false,
              false, true});
        return;
    }
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
        setState(State::Disconnected);
        tell({error == "cancelled" ? "SteamGridDB sign-in cancelled"
                                   : "SteamGridDB sign-in failed: " + error,
              error != "cancelled"});
        return;
    }
    keep(*key);
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
        host::wipe(use.key);
        tell({"that is not a SteamGridDB key: it is 32 letters and digits", true});
        return;
    }
    keep(use.key);
}

void SteamGridDbAccount::keep(std::string& key) {
    std::string error;
    const std::optional<host::SecretPlace> place =
        parts_.secrets.save(keyName, keyLabel, key, error);
    if (!place) {
        host::wipe(key);
        lucent::warn("steamgriddb", "cannot keep the key: {}", error);
        setState(State::Disconnected);
        tell({"cannot keep the SteamGridDB key: " + error, true});
        return;
    }
    parts_.source.setKey(std::exchange(key, {}));
    {
        const std::lock_guard lock{mutex_};
        place_ = *place;
        state_ = State::Connected;
    }
    lucent::info("steamgriddb", "connected, key kept in {}", placeText(*place));
    tell({"SteamGridDB connected", false, true});
}

void SteamGridDbAccount::perform(const Disconnect& /*disconnect*/,
                                 const std::stop_token& /*stop*/) {
    std::string error;
    const bool forgotten = parts_.secrets.forget(keyName, error);
    parts_.source.setKey({});
    setState(State::Disconnected);
    if (!forgotten) {
        lucent::warn("steamgriddb", "forgetting the key: {}", error);
        tell({"SteamGridDB disconnected, but " + error, true, true});
        return;
    }
    lucent::info("steamgriddb", "disconnected, key forgotten");
    tell({"SteamGridDB disconnected", false, true});
}

} // namespace opensu::app

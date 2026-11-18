// passed_asks — asks to join a game this openSU plays from another, passed here by its host
// (`/lan/join/ask`) because the player of that game plays it here: put to the player by
// `TogetherQuestions` and the answer sent back to the host (`/lan/join/answer`). Only the host
// the game streams from now may pass one; it lapses with the stream or after `RemotePlay`'s
// patience.
#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "lan/peer_book.hpp"
#include "lan/wire.hpp"

namespace opensu::app {

class PassedAsks {
  public:
    struct Hooks {
        /// Sends host `hostId` the player's answer, `refused` called with why off the loop when it
        /// was not taken; false when it cannot be sent now.
        std::function<bool(const std::string& hostId, const lan::wire::JoinAnswer& answer,
                           std::function<void(std::string)> refused)>
            answer;
        /// Tells the player something; `error` marks a failure.
        std::function<void(const std::string& text, bool error)> say;
    };

    /// An ask the player here answers.
    struct Passed {
        lan::Peer host;
        lan::Peer asker;
        std::string title;
        std::chrono::steady_clock::time_point at;
        bool operator==(const Passed&) const = default;
    };

    explicit PassedAsks(Hooks hooks) : hooks_{std::move(hooks)} {
    }

    /// `host` passes `ask`: why the player here will not be asked, or nothing. On the LAN
    /// listener's thread.
    [[nodiscard]] std::optional<std::string> passed(const lan::Peer& host,
                                                    const lan::wire::JoinAsk& ask);

    /// The host the game here streams from now, empty for none; drops what lapsed. Every frame,
    /// main loop only.
    void service(const std::string& streamingFrom);

    /// The oldest ask that waits. Main loop only.
    [[nodiscard]] std::optional<Passed> next() const;

    /// The player's answer to `askerId`'s ask. Main loop only.
    void answer(const std::string& askerId, bool allow);

  private:
    Hooks hooks_;
    mutable std::mutex mutex_;
    std::string streamingFrom_;
    std::vector<Passed> asks_;
    /// What the answering threads have for the player, under `newsMutex_`.
    std::shared_ptr<std::mutex> newsMutex_{std::make_shared<std::mutex>()};
    std::shared_ptr<std::vector<std::string>> news_{std::make_shared<std::vector<std::string>>()};
};

} // namespace opensu::app

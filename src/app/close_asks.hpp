// close_asks — a paired openSU's asks to close the game running here (`/lan/close`). The profile
// playing it closes it from any device as if it sat here, nobody asked: a profile linked with one
// in its seats. Another profile is refused; it may ask to join instead. The game is closed on the
// loop through `close`, saved first when it runs on a built-in core, and the asker hears "closed"
// once nothing runs here, or why it did not close.
#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "lan/peer_book.hpp"
#include "lan/wire.hpp"

namespace opensu::app {

class CloseAsks {
  public:
    /// Closes the game running here, saving it first where it can; why it did not, or empty.
    using Close = std::function<std::string()>;

    explicit CloseAsks(Close close) : close_{std::move(close)} {
    }

    /// The game running here now (an empty id, none), its title and the profiles in its seats.
    /// Main loop.
    void playing(std::string id, std::string title, std::vector<std::string> seats);

    /// `peer` asks, as `profile` (`linked` with it), to close `gameId`; how that stands. Listener
    /// thread.
    [[nodiscard]] lan::wire::PlayVerdict ask(const lan::Peer& peer, const std::string& gameId,
                                             const std::string& profile, bool linked);

    /// Closes the game when it was asked to. Main loop.
    void service();

  private:
    Close close_;
    mutable std::mutex mutex_;
    std::string runningId_;
    std::string runningTitle_;
    std::vector<std::string> seats_;
    /// The peers waiting for the close, by id, each with why it failed once it did.
    std::map<std::string, std::string> waiting_;
    /// A close asked for and not yet done.
    bool closing_{false};
};

} // namespace opensu::app

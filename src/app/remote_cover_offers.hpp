// remote_cover_offers — "Choose artwork" for a game on a paired host: the host lists SteamGridDB's
// covers with its own key and keeps the pick, so its cover changes on every openSU. This one asks
// about once a second, off the loop, and fetches each thumbnail as it is offered.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <variant>
#include <vector>

#include "artwork/cover_offers.hpp"
#include "lan/lan_node.hpp"

namespace opensu::app {

class RemoteCoverOffers final : public artwork::CoverSource {
  public:
    using Work = std::function<void(const lan::LanNode&, const std::stop_token&)>;
    /// Runs `work` on the node off the loop; false while LAN is off.
    using Ask = std::function<bool(Work)>;

    explicit RemoteCoverOffers(Ask ask) : ask_{std::move(ask)} {
    }

    void list(const library::Game& game) override;
    void choose(const std::string& gameId, const artwork::SteamGridDbCover& cover) override;
    void chooseOwn(const std::string& gameId) override;
    [[nodiscard]] std::optional<artwork::CoverListing> takeListing() override;
    [[nodiscard]] std::vector<artwork::CoverChoice> takeChoices() override;

  private:
    /// The game on its host.
    struct Hosted {
        std::string hostId;
        std::string game;
    };
    struct List {
        Hosted hosted;
        std::string gameId;
        std::uint64_t generation{0};
    };
    struct Choose {
        Hosted hosted;
        std::string gameId;
        std::optional<std::int64_t> cover;
    };
    using Job = std::variant<List, Choose>;

    void enqueue(Job job);
    void drain(const lan::LanNode& node, const std::stop_token& stop);
    void perform(const lan::LanNode& node, const std::stop_token& stop, const List& list);
    void perform(const lan::LanNode& node, const std::stop_token& stop, const Choose& choose);
    void publish(artwork::CoverListing listing, std::uint64_t generation);
    void finish(artwork::CoverChoice choice);
    /// Waits out a poll's interval; false when another job waits or `stop` came.
    [[nodiscard]] bool pause(const std::stop_token& stop);

    Ask ask_;
    Hosted hosted_;
    std::mutex mutex_;
    std::condition_variable_any wake_;
    std::vector<Job> jobs_;
    bool draining_{false};
    std::uint64_t generation_{0};
    std::optional<artwork::CoverListing> listing_;
    std::vector<artwork::CoverChoice> choices_;
};

} // namespace opensu::app

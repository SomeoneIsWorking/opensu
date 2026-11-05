// peer_save_target — a paired openSU as a `save_sync::SaveTarget` for the profiles linked with it:
// asked over its signed save routes (`/lan/saves/status`, `/get`, `/news`, `/synced`) while it is
// announced. Saves given to it are told of (`/news`), and it takes them itself. A profile not
// linked with it is not shared, and it is not asked.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "dns_sd.hpp"
#include "net/web_client.hpp"
#include "peer_book.hpp"
#include "profiles/profile_book.hpp"
#include "save_sync/save_target.hpp"

namespace opensu::lan {

class PeerSaveTarget final : public save_sync::SaveTarget {
  public:
    /// `online` lists the openSUs announced now.
    PeerSaveTarget(Peer peer, const PeerBook& book, const profiles::ProfileBook& profiles,
                   const net::WebClient& web, std::function<std::vector<Sighting>()> online);

    [[nodiscard]] std::string id() const override {
        return peer_.id;
    }
    [[nodiscard]] std::string name() const override {
        return peer_.name;
    }
    [[nodiscard]] bool reachable() const override;
    [[nodiscard]] Held status(const std::stop_token& stop,
                              const library::saves::SaveKey& key) override;
    [[nodiscard]] std::optional<save_sync::SaveBundle> fetch(const std::stop_token& stop,
                                                             const library::saves::SaveKey& key,
                                                             std::string& error) override;
    [[nodiscard]] Given give(const std::stop_token& stop, const library::saves::SaveKey& key,
                             const save_sync::SaveBundle& bundle,
                             const save_sync::SaveStatus& status, bool picked,
                             std::string& error) override;
    void took(const std::stop_token& stop, const library::saves::SaveKey& key,
              const save_sync::SaveStatus& status) override;

  private:
    /// `route` asked with `body`; nothing, with `error`, when it is not announced, its secret is
    /// gone or the call did not arrive.
    [[nodiscard]] std::optional<net::WebClient::Answer> call(std::string_view route,
                                                             const std::string& body,
                                                             std::size_t largest,
                                                             std::string& error) const;

    Peer peer_;
    const PeerBook& book_;
    const profiles::ProfileBook& profiles_;
    const net::WebClient& web_;
    std::function<std::vector<Sighting>()> online_;
};

} // namespace opensu::lan

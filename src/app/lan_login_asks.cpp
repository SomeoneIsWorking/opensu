#include "lan_login_asks.hpp"

namespace opensu::app {

void LanLoginAsks::service() {
    const lan::LanNode* node = lan_.node();
    std::optional<lan::Peer> asking = node != nullptr ? node->loginQuestion() : std::nullopt;
    if (asking == asked_) {
        return;
    }
    question_.close();
    asked_ = std::move(asking);
    if (!asked_) {
        return;
    }
    const std::string& peer = asked_->name;
    question_.ask(peer + " asks for your Steam sign-in",
                  "Allowed, " + peer +
                      " signs in to your Steam account through fumarole and plays your Steam "
                      "games there. Decline unless it is your own device.",
                  "Allow", [this](bool allow) {
                      answer(allow);
                  });
}

void LanLoginAsks::answer(bool allow) {
    lan::LanNode* node = lan_.node();
    if (node == nullptr || !asked_) {
        return;
    }
    const lan::Peer asker = *asked_;
    if (!node->answerLogin(asker.id, allow)) {
        say_("The ask from " + asker.name + " lapsed", true);
        return;
    }
    say_(allow ? "Steam sign-in sent to " + asker.name : "Declined " + asker.name, false);
}

} // namespace opensu::app

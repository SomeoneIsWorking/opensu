#include "cover_desk.hpp"

#include <span>
#include <utility>

#include "secrets/crypto.hpp"
#include "secrets/secret.hpp"

namespace opensu::lan {

std::string CoverDesk::tokenOf(const std::filesystem::path& file) {
    const secrets::crypto::Bytes digest = secrets::crypto::sha256(file.string());
    return secrets::hexText(std::span{digest}.first(wire::coverTokenBytes));
}

wire::CoverOffersAnswer CoverDesk::offers(const wire::CoverAsk& ask) {
    const std::lock_guard lock{mutex_};
    if (ask.fresh || answer_.game != ask.game) {
        wanted_ = ask.game;
        answer_ =
            wire::CoverOffersAnswer{.game = ask.game, .status = std::string{wire::coversAsking}};
        thumbs_.clear();
    }
    return answer_;
}

void CoverDesk::pick(wire::CoverPick pick) {
    const std::lock_guard lock{mutex_};
    if (answer_.game == pick.game) {
        answer_.choice = wire::coversAsking;
    }
    picks_.push_back(std::move(pick));
}

void CoverDesk::match(wire::GameMatch match) {
    const std::lock_guard lock{mutex_};
    matches_.push_back(std::move(match));
}

std::optional<std::filesystem::path> CoverDesk::thumb(std::string_view token) const {
    const std::lock_guard lock{mutex_};
    const auto found = thumbs_.find(token);
    if (found == thumbs_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::optional<std::string> CoverDesk::takeWanted() {
    const std::lock_guard lock{mutex_};
    return std::exchange(wanted_, std::nullopt);
}

std::vector<wire::CoverPick> CoverDesk::takePicks() {
    const std::lock_guard lock{mutex_};
    return std::exchange(picks_, {});
}

std::vector<wire::GameMatch> CoverDesk::takeMatches() {
    const std::lock_guard lock{mutex_};
    return std::exchange(matches_, {});
}

void CoverDesk::publish(wire::CoverOffersAnswer answer, Thumbs thumbs) {
    const std::lock_guard lock{mutex_};
    if (answer.game != answer_.game) {
        return;
    }
    answer.choice = std::move(answer_.choice);
    answer_ = std::move(answer);
    thumbs_ = std::move(thumbs);
}

void CoverDesk::picked(const std::string& game, std::string outcome) {
    const std::lock_guard lock{mutex_};
    if (answer_.game == game) {
        answer_.choice = std::move(outcome);
    }
}

} // namespace opensu::lan

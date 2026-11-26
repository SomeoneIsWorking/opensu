#include "home_choices.hpp"

#include <utility>

#include "shelf.hpp"

namespace opensu::library {

std::string_view key(HomeChoice choice) noexcept {
    return choice == HomeChoice::Added ? "added" : "removed";
}

std::optional<HomeChoice> homeChoiceOf(std::string_view spelling) noexcept {
    for (const HomeChoice choice : {HomeChoice::Added, HomeChoice::Removed}) {
        if (key(choice) == spelling) {
            return choice;
        }
    }
    return std::nullopt;
}

bool onHomeByDefault(const Game& game) noexcept {
    return game.installed && game.source != Source::Rom && game.source != Source::Remote;
}

HomeChoices::HomeChoices(Entries entries) : entries_{std::move(entries)} {
    const auto joined = entries_.find("launcher:" + key(Source::Own));
    if (joined == entries_.end()) {
        return;
    }
    const HomeChoice choice = joined->second;
    entries_.erase(joined);
    for (const bool wine : {false, true}) {
        entries_.try_emplace(library::key(Folder{Launcher{.source = Source::Own, .wine = wine}}),
                             choice);
    }
}

std::string HomeChoices::consoleKey(std::string_view system) {
    return "console:" + std::string{system};
}

bool HomeChoices::holds(const std::string& key, bool byDefault) const {
    const auto found = entries_.find(key);
    if (found == entries_.end()) {
        return byDefault;
    }
    return found->second == HomeChoice::Added;
}

void HomeChoices::set(const std::string& key, bool byDefault, bool onHome) {
    if (onHome == byDefault) {
        entries_.erase(key);
        return;
    }
    entries_.insert_or_assign(key, onHome ? HomeChoice::Added : HomeChoice::Removed);
}

} // namespace opensu::library

#include "console_art_choice.hpp"

#include <algorithm>

namespace opensu::settings {

std::string_view key(ConsoleArtChoice choice) noexcept {
    switch (choice) {
    case ConsoleArtChoice::OpenSu:
        return "opensu";
    case ConsoleArtChoice::Iisu:
        return "iisu";
    }
    return {};
}

std::optional<ConsoleArtChoice> consoleArtChoiceOf(std::string_view spelling) noexcept {
    const auto found =
        std::ranges::find_if(allConsoleArtChoices, [spelling](ConsoleArtChoice choice) {
            return key(choice) == spelling;
        });
    return found == allConsoleArtChoices.end() ? std::nullopt : std::optional{*found};
}

std::string_view label(ConsoleArtChoice choice) noexcept {
    switch (choice) {
    case ConsoleArtChoice::OpenSu:
        return "openSU";
    case ConsoleArtChoice::Iisu:
        return "iiSU";
    }
    return {};
}

} // namespace opensu::settings

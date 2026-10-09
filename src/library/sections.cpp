#include "sections.hpp"

#include <algorithm>

namespace iideck::library {

std::string_view key(Section section) noexcept {
    switch (section) {
    case Section::Home:
        return "home";
    case Section::Library:
        return "library";
    }
    return {};
}

std::string_view key(LibraryMode mode) noexcept {
    switch (mode) {
    case LibraryMode::Standard:
        return "standard";
    case LibraryMode::Xmb:
        return "xmb";
    case LibraryMode::Carousel:
        return "carousel";
    }
    return {};
}

std::optional<LibraryMode> libraryModeOf(std::string_view spelling) noexcept {
    const auto found = std::ranges::find_if(allLibraryModes, [spelling](LibraryMode mode) {
        return key(mode) == spelling;
    });
    return found == allLibraryModes.end() ? std::nullopt : std::optional{*found};
}

std::string_view label(LibraryMode mode) noexcept {
    switch (mode) {
    case LibraryMode::Standard:
        return "Standard";
    case LibraryMode::Xmb:
        return "XMB";
    case LibraryMode::Carousel:
        return "Carousel";
    }
    return {};
}

int Sections::stepsBetween(Section from, Section to) noexcept {
    const int count = static_cast<int>(allSections.size());
    return (static_cast<int>(to) - static_cast<int>(from) + count) % count;
}

Section Sections::cycle(int delta) noexcept {
    const int count = static_cast<int>(allSections.size());
    const int next = (static_cast<int>(active_) + delta) % count;
    active_ = allSections[static_cast<std::size_t>(next < 0 ? next + count : next)];
    return active_;
}

} // namespace iideck::library

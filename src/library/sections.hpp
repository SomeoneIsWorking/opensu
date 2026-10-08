// sections — the dock's places and how Library lays out its games.
//
// iiSU's primary navigation (navigation.md §1) cycles its sections with L1 and R1 and wraps. iideck
// has two: Home, the games installed here, and Library, every launcher, console and store library.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace iideck::library {

/// The dock's sections, in dock order (iiSU `tg3`: Home, Roms).
enum class Section : std::uint8_t {
    Home,
    Library,
};

inline constexpr std::array allSections{Section::Home, Section::Library};

/// How Library lays out its folders and the games inside them (iiSU `gz6`: Grid, Xmb with
/// `xmbHorizontalPath` false and true).
enum class LibraryMode : std::uint8_t {
    Standard,
    Xmb,
    Carousel,
};

inline constexpr std::array allLibraryModes{LibraryMode::Standard, LibraryMode::Xmb,
                                            LibraryMode::Carousel};

/// The section as the control channel and the settings file spell it: "home", "library".
[[nodiscard]] std::string_view key(Section section) noexcept;

/// The mode as the control channel and the settings file spell it: "standard", "xmb", "carousel".
[[nodiscard]] std::string_view key(LibraryMode mode) noexcept;

/// The mode a spelling names, or nothing.
[[nodiscard]] std::optional<LibraryMode> libraryModeOf(std::string_view key) noexcept;

/// The mode as the player reads it (iiSU `roms_layout_cards`): "Standard", "XMB", "Carousel".
[[nodiscard]] std::string_view label(LibraryMode mode) noexcept;

/// Which section is showing, and L1 and R1 moving between them (iiSU `jk2` case 16).
class Sections {
  public:
    [[nodiscard]] Section active() const noexcept {
        return active_;
    }

    /// Moves `delta` sections along the dock, wrapping at both ends, and returns the new one.
    Section cycle(int delta) noexcept;

    /// Makes `section` the active one.
    void select(Section section) noexcept {
        active_ = section;
    }

  private:
    Section active_{Section::Home};
};

} // namespace iideck::library

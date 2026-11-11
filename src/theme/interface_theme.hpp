// interface_theme — one interface theme: a folder holding `theme.json` and the face, sounds and
// music it names (docs/design/interface-themes.md). Every token has a default, the iiSU look's,
// so a theme states only what it changes. A theme is data: `parseTheme` reads it strictly and
// refuses, with why, a file that is too large, has an unknown key, a value out of range or a path
// leading out of its folder.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "settings/sound_set.hpp"
#include "theme_folders.hpp"

namespace opensu::theme {

struct Colour {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{255};

    bool operator==(const Colour&) const = default;
};

/// The toolkit's colours (gpu2d `ui::Palette`) and the shell's own.
struct Palette {
    Colour ground{0xF4, 0xF3, 0xF7, 255};
    Colour surface{0xF8, 0xF8, 0xFB, 255};
    Colour control{0xFF, 0xFF, 0xFF, 255};
    Colour controlFocused{0xFF, 0xFF, 0xFF, 255};
    Colour selected{0xE6, 0xE4, 0xEE, 255};
    Colour ink{0x2B, 0x27, 0x33, 255};
    Colour inkMuted{0x6F, 0x68, 0x80, 255};
    Colour inkFaint{0x93, 0x8C, 0xA1, 255};
    Colour accent{0x4D, 0x46, 0x55, 255};
    Colour onAccent{0xFF, 0xFF, 0xFF, 255};
    Colour track{0xB7, 0xB2, 0xC0, 255};
    Colour focusRing{0x4D, 0x46, 0x55, 255};
    Colour scrim{0x2B, 0x27, 0x33, 120};
    Colour toast{0x2B, 0x27, 0x33, 240};
    Colour toastError{0xB3, 0x26, 0x1E, 240};
    Colour onToast{0xFF, 0xFF, 0xFF, 255};
    Colour danger{0xB3, 0x26, 0x1E, 255};
    /// Home's ground, washed from the top colour to the bottom one.
    Colour groundTop{0xF4, 0xF3, 0xF7, 255};
    Colour groundBottom{0xF4, 0xF3, 0xF7, 255};
    /// iiSU's dot grid over Home's ground.
    bool groundDots{true};
    /// The top bar's title, the corner prompts and their glyphs.
    Colour chromeInk{0x4D, 0x46, 0x55, 255};

    bool operator==(const Palette&) const = default;
};

/// Ems in dp, and the face they are set in.
struct Type {
    /// A TTF or OTF in the theme's folder; empty for the shell's own face (Cal Sans).
    std::filesystem::path face;
    float title{22.0f};
    float heading{11.5f};
    float body{15.5f};
    float caption{11.5f};
    float value{14.0f};
    float hint{13.0f};
    float toast{14.0f};

    bool operator==(const Type&) const = default;
};

/// Lengths in dp.
struct Shape {
    float radius{12.0f};
    float smallRadius{9.0f};
    float outline{1.8f};
    float focusRing{2.2f};
    float focusGap{2.0f};

    bool operator==(const Shape&) const = default;
};

/// Lengths in dp.
struct Spacing {
    float gap{5.0f};
    float pad{14.0f};
    float section{16.0f};
    float rowHeight{50.0f};
    float compactRowHeight{40.0f};
    float sidebarWidth{210.0f};
    float contentWidth{600.0f};
    float hintBarHeight{34.0f};
    float dialogWidth{380.0f};

    bool operator==(const Spacing&) const = default;
};

enum class TileStyle : std::uint8_t {
    /// iiSU's glass chrome with its turning cyan-to-violet selection ring.
    Glass,
    /// A white glossy body with a thin edge, an empty slot a faint well, focus an accent outline.
    Channel,
};

struct Tiles {
    TileStyle style{TileStyle::Glass};
    /// How large the focused tile grows, over its rest size: 1 keeps iiSU's focus pulse alone.
    float grow{1.0f};
    /// A channel tile's body, its edge, an empty slot's well and the focused tile's outline.
    Colour body{0xFF, 0xFF, 0xFF, 255};
    Colour edge{0xC9, 0xCD, 0xD3, 255};
    Colour well{0xE4, 0xE7, 0xEB, 255};
    Colour focus{0x3C, 0xB4, 0xE5, 255};

    bool operator==(const Tiles&) const = default;
};

struct Home {
    /// The scroll mode the theme gives Home until the player picks one: pages, else scrolling.
    bool pages{false};
    /// A band along the bottom edge with the time large and the date under it.
    bool clockBar{false};
    /// Whether the top bar's status pill shows the time.
    bool pillClock{true};

    bool operator==(const Home&) const = default;
};

/// The interface sounds the theme brings: one of openSU's shipped sets, or a folder of the theme
/// holding the effects' files (`audio::shippedFile`'s names).
struct Sounds {
    settings::SoundSet set{settings::SoundSet::Marimba};
    /// In the theme's folder; empty for `set`. An effect without a file there plays `set`'s.
    std::filesystem::path folder;

    bool operator==(const Sounds&) const = default;
};

struct InterfaceTheme {
    /// How the setting names it (the folder's name, or `user/<folder>`).
    std::string id;
    std::string name;
    std::string author;
    /// One line for the settings row: whose it is and its licence.
    std::string note;
    std::filesystem::path folder;
    Palette palette;
    Type type;
    Shape shape;
    Spacing spacing;
    Tiles tiles;
    Home home;
    Sounds sounds;
    /// A loop played behind the menus, in the theme's folder; empty for none.
    std::filesystem::path music;

    bool operator==(const InterfaceTheme&) const = default;
};

/// The most a manifest may hold.
inline constexpr std::size_t manifestLimit = std::size_t{64} << 10U;
/// The most a theme's face, a sound and its music may each hold.
inline constexpr std::uintmax_t faceLimit = std::uintmax_t{16} << 20U;
inline constexpr std::uintmax_t soundLimit = std::uintmax_t{4} << 20U;
inline constexpr std::uintmax_t musicLimit = std::uintmax_t{16} << 20U;

/// The theme `manifest` describes, its files in `folder`, under `id`; nothing, with `error`
/// saying why, when it breaks the format.
[[nodiscard]] std::optional<InterfaceTheme> parseTheme(std::string_view manifest,
                                                       const std::filesystem::path& folder,
                                                       std::string id, std::string& error);

/// The theme in `folder`: its manifest read (at most `manifestLimit`) and parsed.
[[nodiscard]] std::optional<InterfaceTheme> readTheme(const std::filesystem::path& folder,
                                                      std::string id, std::string& error);

} // namespace opensu::theme

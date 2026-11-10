// wii_controls — how each Wii game is played on a pad (`assets/wii-controls.json`): the emulated
// Wii Remote's extension, how it is held, the pointer on the right stick, shake on a button and the
// rest of its inputs on an Xbox-style pad, from a named layout and the game's own changes to it.
// Games are keyed by their disc's game id, or its first three characters for every region; a game
// not listed gets the catalog's default layout.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::library {

/// The library's key for Wii games.
inline constexpr std::string_view wiiSystem{"wii"};

enum class WiiExtension : std::uint8_t {
    None,
    Nunchuk,
    Classic,
};

/// How the remote is held: at the screen, sideways like a pad, or standing up.
enum class WiiOrientation : std::uint8_t {
    Pointing,
    Sideways,
    Upright,
};

/// One emulated Wii Remote as a game wants it, every input a Dolphin expression over an SDL pad.
struct WiiRemote {
    /// The game's catalog title, or the default layout's name for a game not listed.
    std::string title;
    WiiExtension extension{WiiExtension::Nunchuk};
    WiiOrientation orientation{WiiOrientation::Pointing};
    /// The pointer (IR) on the right stick, moved relative and recentred by its click.
    bool pointer{true};
    /// The input that shakes the remote; empty for none.
    std::string shake;
    /// Every other input, by Dolphin's `<group>/<input>` key.
    std::map<std::string, std::string, std::less<>> inputs;

    bool operator==(const WiiRemote&) const = default;
};

class WiiControls {
  public:
    /// No catalog: every game gets a remote and Nunchuk with nothing bound.
    WiiControls() = default;

    /// The catalog in `text`; nothing with `error` when it is malformed.
    [[nodiscard]] static std::optional<WiiControls> parse(std::string_view text,
                                                          std::string& error);
    /// The catalog shipped in `assetsDir`; none, said in the log, when it cannot be read.
    [[nodiscard]] static WiiControls shipped(const std::filesystem::path& assetsDir);

    /// The remote the game with disc id `gameId` is played with.
    [[nodiscard]] WiiRemote remoteFor(std::string_view gameId) const;

  private:
    std::map<std::string, WiiRemote, std::less<>> games_;
    WiiRemote fallback_;
};

} // namespace opensu::library

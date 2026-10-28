// effect — the UI sounds openSU plays at iiSU's events: which shipped file each is and how often it
// may repeat. Pure, so tests link it alone.
#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace opensu::audio {

/// The sound effects of iiSU's `yp8` that opensu has an event for (input-sound.md 3.2, 3.4).
/// MenuNavigate, KeyClick, MessageSend, FriendsTab and the count-less `domino_icons.ogg` have no
/// opensu event: the last is unreachable in iiSU too (`xp8.a` is never called with a count of 0).
enum class Effect : std::uint8_t {
    Open,
    Close,
    OpenContextMenu,
    Navigation,
    OpenAppRom,
    EnterConsolesApps,
    ExitConsolesApps,
    DominoOne,
    DominoTwo,
    DominoThreeToFive,
    DominoSixToEleven,
    DominoTwelvePlus,
};

inline constexpr std::array allEffects{
    Effect::Open,
    Effect::Close,
    Effect::OpenContextMenu,
    Effect::Navigation,
    Effect::OpenAppRom,
    Effect::EnterConsolesApps,
    Effect::ExitConsolesApps,
    Effect::DominoOne,
    Effect::DominoTwo,
    Effect::DominoThreeToFive,
    Effect::DominoSixToEleven,
    Effect::DominoTwelvePlus,
};

/// Where openSU's own sounds are installed, under the assets directory.
inline constexpr std::string_view shippedDirectory = "sounds";

/// The effect's Ogg Vorbis file in openSU's own set (`assets/sounds/`).
[[nodiscard]] std::string_view shippedFile(Effect effect) noexcept;

/// The domino cue for a section change that shows `tiles` tiles at once (`xp8.a`), or nothing for
/// none.
[[nodiscard]] std::optional<Effect> dominoFor(std::size_t tiles) noexcept;

/// How soon after itself the effect is dropped (`xp8.java:1559-1586`); zero for none.
[[nodiscard]] std::chrono::milliseconds debounceOf(Effect effect) noexcept;

} // namespace opensu::audio

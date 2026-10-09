// effect — iiSU's UI sounds that opensu plays: which file each is and how often it may repeat.
// Pure, so the artwork store can name the files and tests link it alone.
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

/// The effect's file name in the APK's `assets/` (`xp8.I`): a WAV, or an OGG for a domino cue.
[[nodiscard]] std::string_view assetFile(Effect effect) noexcept;

/// The effect whose file is `file`, or nothing.
[[nodiscard]] std::optional<Effect> effectOfFile(std::string_view file) noexcept;

/// The domino cue for a section change that shows `tiles` tiles at once (`xp8.a`), or nothing for
/// none.
[[nodiscard]] std::optional<Effect> dominoFor(std::size_t tiles) noexcept;

/// How soon after itself the effect is dropped (`xp8.java:1559-1586`); zero for none.
[[nodiscard]] std::chrono::milliseconds debounceOf(Effect effect) noexcept;

} // namespace opensu::audio

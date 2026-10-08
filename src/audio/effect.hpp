// effect — iiSU's UI sounds that iideck plays: which file each is and how often it may repeat.
// Pure, so the artwork store can name the files and tests link it alone.
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>

namespace iideck::audio {

/// The sound effects of iiSU's `yp8` that iideck has an event for (input-sound.md 3.2, 3.4).
/// MenuNavigate, KeyClick, MessageSend, FriendsTab and the Domino cues have no iideck event.
enum class Effect : std::uint8_t {
    Open,
    Close,
    OpenContextMenu,
    Navigation,
    OpenAppRom,
    EnterConsolesApps,
    ExitConsolesApps,
};

inline constexpr std::array allEffects{
    Effect::Open,
    Effect::Close,
    Effect::OpenContextMenu,
    Effect::Navigation,
    Effect::OpenAppRom,
    Effect::EnterConsolesApps,
    Effect::ExitConsolesApps,
};

/// The effect's WAV name in the APK's `assets/` (`xp8.I`).
[[nodiscard]] std::string_view assetFile(Effect effect) noexcept;

/// The effect whose file is `file`, or nothing.
[[nodiscard]] std::optional<Effect> effectOfFile(std::string_view file) noexcept;

/// How soon after itself the effect is dropped (`xp8.java:1559-1586`); zero for none.
[[nodiscard]] std::chrono::milliseconds debounceOf(Effect effect) noexcept;

} // namespace iideck::audio

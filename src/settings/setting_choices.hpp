// setting_choices — a layered setting whose value is one of a fixed list: its key, its label and
// its choices as kept and as the player reads them, and which one is the built-in default. The
// video settings and the frame generation settings are such keys.
#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace opensu::settings {

/// A choice as it is kept and as the player reads it.
struct ChoiceText {
    std::string_view key;
    std::string_view label;
};

/// One setting: its key in the layers, its label, its choices and which of them is the default.
struct ChoiceKey {
    std::string_view key;
    std::string_view label;
    std::span<const ChoiceText> choices;
    /// The built-in default's place among `choices`.
    std::size_t fallback{0};

    /// The built-in default as it is kept.
    [[nodiscard]] constexpr std::string_view defaultKey() const noexcept {
        return choices[fallback].key;
    }
};

/// Where `spelling` stands among `setting`'s choices, or nothing when it is none of them.
[[nodiscard]] std::optional<std::size_t> choiceIndex(const ChoiceKey& setting,
                                                     std::string_view spelling);
/// The label of `spelling` among `setting`'s choices; the spelling itself when it is none of them.
[[nodiscard]] std::string_view choiceLabel(const ChoiceKey& setting, std::string_view spelling);
/// `spelling`'s place among `setting`'s choices, or the default's with a warning naming
/// `where`.
[[nodiscard]] std::size_t choiceIndexOrDefault(const ChoiceKey& setting, std::string_view spelling,
                                               std::string_view where);

} // namespace opensu::settings

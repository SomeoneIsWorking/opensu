// home_choices — what the player put on Home or took off it, against what Home holds unasked
// (`onHomeByDefault`): a game by its `choiceKey`, a console by its system. A choice is kept only
// while it differs from the default, so a game the player never touched follows the default.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "game.hpp"

namespace opensu::library {

enum class HomeChoice : std::uint8_t {
    Added,
    Removed,
};

/// The choice as the settings file spells it: "added", "removed".
[[nodiscard]] std::string_view key(HomeChoice choice) noexcept;
[[nodiscard]] std::optional<HomeChoice> homeChoiceOf(std::string_view spelling) noexcept;

/// Whether `game`, a title's preferred copy, is on Home unless the player chose otherwise: a store
/// game or one of the player's own, installed on this machine.
[[nodiscard]] bool onHomeByDefault(const Game& game) noexcept;

class HomeChoices {
  public:
    /// A game's `choiceKey` or a console's `consoleKey`, to the player's choice.
    using Entries = std::map<std::string, HomeChoice>;

    HomeChoices() = default;
    explicit HomeChoices(Entries entries) : entries_{std::move(entries)} {
    }

    /// What a console is kept under: "console:<system>".
    [[nodiscard]] static std::string consoleKey(std::string_view system);

    /// Whether what `key` names is on Home; `byDefault` when the player has not chosen.
    [[nodiscard]] bool holds(const std::string& key, bool byDefault) const;
    /// Puts what `key` names on Home or takes it off; a choice that matches `byDefault` is
    /// forgotten.
    void set(const std::string& key, bool byDefault, bool onHome);

    [[nodiscard]] const Entries& entries() const noexcept {
        return entries_;
    }

    bool operator==(const HomeChoices&) const = default;

  private:
    Entries entries_;
};

} // namespace opensu::library

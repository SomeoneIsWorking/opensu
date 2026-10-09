// corner_hints — which prompts the two bottom corners name (iiSU mw5.h and mw5.k). A prompt shows
// only while its button does something where the player is.
#pragma once

#include <vector>

namespace opensu::ui {

/// What each button does now, as the shell's own handling of it says.
struct HintContext {
    /// B leaves a folder.
    bool back{false};
    /// A opens a folder or launches a game.
    bool select{false};
    /// Y or Select shows a game's details.
    bool details{false};
    /// START opens a menu.
    bool menu{false};
};

struct Prompt {
    /// The glyph's key as `ButtonGlyphPainter` names it.
    const char* key;
    const char* label;

    bool operator==(const Prompt&) const = default;
};

/// The bottom-left panel: B Back, - Details.
[[nodiscard]] std::vector<Prompt> startPrompts(const HintContext& context);
/// The bottom-right panel: A Select, + Menu.
[[nodiscard]] std::vector<Prompt> endPrompts(const HintContext& context);

} // namespace opensu::ui

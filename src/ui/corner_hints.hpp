// corner_hints — which prompts the two bottom corners name (iiSU mw5.h and mw5.k). A prompt shows
// only while its button does something where the player is.
#pragma once

#include <vector>

namespace opensu::ui {

/// What each button does now, as the shell's own handling of it says.
struct HintContext {
    /// B leaves a folder.
    bool back{false};
    /// B clears a search whose results are showing; it comes before leaving a folder.
    bool clearSearch{false};
    /// A opens a folder or launches a game.
    bool select{false};
    /// Select opens a game's menu, whose first entries are its details.
    bool details{false};
    /// Select opens a folder tile's menu.
    bool options{false};
    /// START opens a menu.
    bool menu{false};
};

struct Prompt {
    /// The glyph's key as `ButtonGlyphPainter` names it.
    const char* key;
    const char* label;

    bool operator==(const Prompt&) const = default;
};

/// The bottom-left panel: B Back or Clear search, - Details or Options.
[[nodiscard]] std::vector<Prompt> startPrompts(const HintContext& context);
/// The bottom-right panel: A Select, + Menu.
[[nodiscard]] std::vector<Prompt> endPrompts(const HintContext& context);

} // namespace opensu::ui

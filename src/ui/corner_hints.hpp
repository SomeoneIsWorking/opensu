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
    /// A does something: opens a folder, a game's details, or the details page's focused button.
    bool select{false};
    /// A opens the focused game's details page, which the prompt says.
    bool details{false};
    /// Select opens the focused tile's context menu.
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

/// The bottom-left panel: B Back or Clear search, - Options.
[[nodiscard]] std::vector<Prompt> startPrompts(const HintContext& context);
/// The bottom-right panel: A Select or Details, + Menu.
[[nodiscard]] std::vector<Prompt> endPrompts(const HintContext& context);

} // namespace opensu::ui

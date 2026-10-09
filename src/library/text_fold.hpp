// text_fold — the form titles are compared in when the player searches: lower case, accents
// removed, runs of white space one space.
#pragma once

#include <string>
#include <string_view>

namespace opensu::library {

/// `text` (UTF-8) with Latin letters folded to unaccented lower case ("Pokémon" -> "pokemon",
/// "Æon" -> "aeon"), combining marks dropped, the right single quote read as an apostrophe, runs
/// of white space made one space and the ends trimmed. Other characters are kept as they are.
[[nodiscard]] std::string foldText(std::string_view text);

} // namespace opensu::library

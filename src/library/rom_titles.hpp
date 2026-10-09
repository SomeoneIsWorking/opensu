// rom_titles — a ROM's display title from the name it is stored under. No-Intro, Redump,
// GoodTools, Switch dump names and MAME descriptions all carry their tags after the title.
#pragma once

#include <string>
#include <string_view>

namespace opensu::library::roms {

/// Drops a scene release number ("0881 - Castlevania" is "Castlevania").
[[nodiscard]] std::string_view withoutReleaseNumber(std::string_view name);

/// The title in a dump name: everything from the first parenthesis or bracket group on goes
/// (region, language, revision, dump flags, MAME's "(World 910522)"), a release number goes, a
/// trailing ", The" moves to the front ("Legend of Zelda, The - Wind Waker" is "The Legend of
/// Zelda - Wind Waker"), and a name with no spaces and underscores reads with spaces. A name
/// that is nothing but tags is returned as it is.
[[nodiscard]] std::string cleanTitle(std::string_view name);

} // namespace opensu::library::roms

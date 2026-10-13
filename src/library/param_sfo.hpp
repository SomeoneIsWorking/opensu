// param_sfo — the PlayStation PARAM.SFO key-value table PS3, PS4 and Vita games carry.
#pragma once

#include <map>
#include <string>
#include <string_view>

namespace opensu::library::roms {

/// The UTF-8 values of a PARAM.SFO by key ("TITLE", "TITLE_ID"); empty when the bytes are not
/// one. Integer values are left out.
[[nodiscard]] std::map<std::string, std::string> sfoStrings(std::string_view bytes);

} // namespace opensu::library::roms

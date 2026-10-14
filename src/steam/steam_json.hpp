// steam_json — reading the values Steam's SharedJSContext hands back, and the app ids put into the
// scripts sent to it.
#pragma once

#include <cstdint>
#include <string_view>

#include <nlohmann/json.hpp>

namespace opensu::steam {

/// The unsigned size at `key` of `object`, which Steam sends as a number or, past 32 bits, as a
/// string; 0 when it is missing or neither.
[[nodiscard]] std::uint64_t uint64Of(const nlohmann::json& object, const char* key);

/// Whether `text` is a Steam app id: digits only, so it can stand in a script as a number.
[[nodiscard]] bool isAppId(std::string_view text) noexcept;

} // namespace opensu::steam

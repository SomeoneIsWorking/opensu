// steam_json — reading the values Steam's SharedJSContext hands back.
#pragma once

#include <cstdint>

#include <nlohmann/json.hpp>

namespace opensu::steam {

/// The unsigned size at `key` of `object`, which Steam sends as a number or, past 32 bits, as a
/// string; 0 when it is missing or neither.
[[nodiscard]] std::uint64_t uint64Of(const nlohmann::json& object, const char* key);

} // namespace opensu::steam

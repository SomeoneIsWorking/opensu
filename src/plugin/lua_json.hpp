// lua_json — JSON text to Lua values and back, for `opensu.json`. Depth is capped, and JSON null
// is one sentinel table kept in the registry.
#pragma once

#include <string>
#include <string_view>

#include "lua.h"

namespace opensu::plugin {

/// The deepest nesting decode and encode take.
inline constexpr int jsonDepthLimit = 64;

/// Pushes the value `text` holds and returns true; or pushes nothing and returns false with
/// `error`. JSON null is the value with registry reference `nullReference`.
bool pushJson(lua_State* state, std::string_view text, int nullReference, std::string& error);

/// The value at `index` as JSON text, or false with `error` for a value JSON cannot hold (a
/// function, a table with mixed keys, nesting past the limit). A table whose keys are 1..n is an
/// array; an empty table is an object.
bool encodeJson(lua_State* state, int index, int nullReference, std::string& out,
                std::string& error);

} // namespace opensu::plugin

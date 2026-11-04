// json_text — a text field of a JSON object read from another openSU or a share, where any field
// may be missing or of another type.
#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace opensu::save_sync {

/// `document[key]` when it is a string; empty otherwise.
[[nodiscard]] inline std::string textField(const nlohmann::json& document, const char* key) {
    const auto found = document.find(key);
    return found != document.end() && found->is_string() ? found->get<std::string>()
                                                         : std::string{};
}

} // namespace opensu::save_sync

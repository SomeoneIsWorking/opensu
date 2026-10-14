#include "steam_json.hpp"

#include <cstdlib>
#include <string>

namespace opensu::steam {

std::uint64_t uint64Of(const nlohmann::json& object, const char* key) {
    if (!object.is_object()) {
        return 0;
    }
    const auto found = object.find(key);
    if (found == object.end()) {
        return 0;
    }
    if (found->is_number_unsigned() || found->is_number_integer()) {
        return found->get<std::uint64_t>();
    }
    if (found->is_string()) {
        return std::strtoull(found->get<std::string>().c_str(), nullptr, 10);
    }
    return 0;
}

} // namespace opensu::steam

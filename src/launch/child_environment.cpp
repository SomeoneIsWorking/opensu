#include "child_environment.hpp"

namespace opensu::launch {

ChildEnvironment::ChildEnvironment(char** entries) {
    for (char** entry = entries; *entry != nullptr; ++entry) {
        const std::string_view value{*entry};
        if (value.substr(0, value.find('=')) != gamescopeWsiOff) {
            owned_.emplace_back(value);
        }
    }
    pointers_.reserve(owned_.size() + 1);
    for (std::string& value : owned_) {
        pointers_.push_back(value.data());
    }
    pointers_.push_back(nullptr);
}

} // namespace opensu::launch

#include "child_environment.hpp"

#include <algorithm>

// POSIX leaves its declaration to the program; macOS's headers do not declare it.
extern char** environ;

namespace opensu::launch {
namespace {

std::string_view nameOf(std::string_view entry) {
    return entry.substr(0, entry.find('='));
}

} // namespace

ChildEnvironment::ChildEnvironment(const std::vector<std::string>& added) {
    for (char** entry = environ; *entry != nullptr; ++entry) {
        const std::string_view value{*entry};
        const bool replaced = std::ranges::any_of(added, [value](const std::string& set) {
            return nameOf(set) == nameOf(value);
        });
        if (nameOf(value) != gamescopeWsiOff && !replaced) {
            owned_.emplace_back(value);
        }
    }
    owned_.insert(owned_.end(), added.begin(), added.end());
    pointers_.reserve(owned_.size() + 1);
    for (std::string& value : owned_) {
        pointers_.push_back(value.data());
    }
    pointers_.push_back(nullptr);
}

} // namespace opensu::launch

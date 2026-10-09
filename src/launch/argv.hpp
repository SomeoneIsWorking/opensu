// launch — the argument vector for exec.
#pragma once

#include <string>
#include <vector>

namespace opensu::launch {

/// The argument vector for execvp, owning the strings the pointers refer to.
///
/// Built in the parent before a fork: allocating in a forked child of a threaded
/// process is not safe, and a pointer vector beside a dead string vector dangles.
class Argv {
  public:
    Argv(const std::string& program, const std::vector<std::string>& args) {
        owned_.reserve(args.size() + 1);
        owned_.push_back(program);
        for (const std::string& arg : args) {
            owned_.push_back(arg);
        }
        pointers_.reserve(owned_.size() + 1);
        for (std::string& value : owned_) {
            pointers_.push_back(value.data());
        }
        pointers_.push_back(nullptr);
    }

    [[nodiscard]] char** data() noexcept {
        return pointers_.data();
    }

    [[nodiscard]] const std::string& program() const noexcept {
        return owned_.front();
    }

  private:
    std::vector<std::string> owned_;
    std::vector<char*> pointers_;
};

} // namespace opensu::launch

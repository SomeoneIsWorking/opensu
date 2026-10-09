// library — a fixed list of names that can be a constant.
#pragma once

#include <array>
#include <cstddef>
#include <initializer_list>
#include <string_view>

namespace opensu::library {

/// Up to `capacity` names, built at compile time so tables of them need no dynamic initialization.
class NameList {
  public:
    static constexpr std::size_t capacity = 8;

    constexpr NameList() = default;
    consteval NameList(std::initializer_list<std::string_view> names) {
        for (const std::string_view name : names) {
            names_[count_++] = name;
        }
    }

    [[nodiscard]] constexpr const std::string_view* begin() const {
        return names_.data();
    }
    [[nodiscard]] constexpr const std::string_view* end() const {
        return names_.data() + count_;
    }
    [[nodiscard]] constexpr std::size_t size() const {
        return count_;
    }
    [[nodiscard]] constexpr bool empty() const {
        return count_ == 0;
    }

  private:
    std::array<std::string_view, capacity> names_{};
    std::size_t count_ = 0;
};

} // namespace opensu::library

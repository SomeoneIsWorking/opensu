#include "rom_titles.hpp"

#include <array>
#include <cctype>
#include <cstddef>

namespace opensu::library::roms {
namespace {

/// The articles No-Intro and Redump move behind a comma in every language they name titles in.
constexpr std::array<std::string_view, 12> articles{"The", "A",  "An",  "Le",  "La",  "Les",
                                                    "Il",  "El", "Der", "Die", "Das", "Los"};

std::string_view trimmed(std::string_view text) {
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    return text;
}

/// `title` with the article before a trailing comma (or before the comma that ends the part
/// ahead of " - ") moved to the front: "Legend of Zelda, The - Wind Waker" and "Hunt, The".
std::string withArticleFirst(std::string_view title) {
    const std::size_t dash = title.find(" - ");
    const std::string_view head = title.substr(0, dash);
    const std::size_t comma = head.rfind(", ");
    if (comma == std::string_view::npos) {
        return std::string{title};
    }
    const std::string_view word = head.substr(comma + 2);
    for (const std::string_view article : articles) {
        if (word == article) {
            std::string out{article};
            out += ' ';
            out += head.substr(0, comma);
            out += title.substr(head.size());
            return out;
        }
    }
    // French and Italian elisions: "Aventure, L'" is "L'Aventure".
    if (word == "L'") {
        std::string out{"L'"};
        out += head.substr(0, comma);
        out += title.substr(head.size());
        return out;
    }
    return std::string{title};
}

} // namespace

std::string_view withoutReleaseNumber(std::string_view name) {
    std::size_t digits = 0;
    while (digits < name.size() && std::isdigit(static_cast<unsigned char>(name[digits])) != 0) {
        ++digits;
    }
    constexpr std::string_view separator = " - ";
    if (digits > 0 && name.substr(digits).starts_with(separator)) {
        return name.substr(digits + separator.size());
    }
    return name;
}

std::string cleanTitle(std::string_view name) {
    const std::string_view numberless = withoutReleaseNumber(name);
    const std::size_t tags = numberless.find_first_of("([");
    std::string_view title = trimmed(numberless.substr(0, tags));
    if (title.empty()) {
        return std::string{trimmed(name)};
    }
    std::string out = withArticleFirst(title);
    if (out.find(' ') == std::string::npos) {
        for (char& c : out) {
            if (c == '_') {
                c = ' ';
            }
        }
    }
    return out;
}

} // namespace opensu::library::roms

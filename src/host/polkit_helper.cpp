#include "polkit_helper.hpp"

#include <utility>

namespace opensu::host::polkit {
namespace {

using Kind = HelperLine::Kind;

constexpr std::array<std::pair<std::string_view, Kind>, 4> prompts{{
    {"PAM_PROMPT_ECHO_OFF", Kind::AskHidden},
    {"PAM_PROMPT_ECHO_ON", Kind::AskShown},
    {"PAM_ERROR_MSG", Kind::Error},
    {"PAM_TEXT_INFO", Kind::Info},
}};

bool isOctal(char c) {
    return c >= '0' && c <= '7';
}

char escaped(char c) {
    switch (c) {
    case 'b':
        return '\b';
    case 'f':
        return '\f';
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 't':
        return '\t';
    case 'v':
        return '\v';
    default:
        return c;
    }
}

} // namespace

HelperLine parseHelperLine(std::string_view line) {
    if (line == "SUCCESS") {
        return {Kind::Success, {}};
    }
    if (line == "FAILURE") {
        return {Kind::Failure, {}};
    }
    for (const auto& [word, kind] : prompts) {
        if (line.starts_with(word) && line.size() > word.size() && line[word.size()] == ' ') {
            return {kind, unescape(line.substr(word.size() + 1))};
        }
    }
    return {Kind::Unknown, std::string{line}};
}

std::string unescape(std::string_view text) {
    constexpr int octalDigits = 3;
    constexpr int octalBase = 8;
    std::string out;
    out.reserve(text.size());
    for (std::size_t at = 0; at < text.size(); ++at) {
        if (text[at] != '\\' || at + 1 == text.size()) {
            out.push_back(text[at]);
            continue;
        }
        ++at;
        if (isOctal(text[at])) {
            int value = 0;
            int digits = 0;
            while (digits < octalDigits && at < text.size() && isOctal(text[at])) {
                value = value * octalBase + (text[at] - '0');
                ++at;
                ++digits;
            }
            --at;
            out.push_back(static_cast<char>(value));
            continue;
        }
        out.push_back(escaped(text[at]));
    }
    return out;
}

} // namespace opensu::host::polkit

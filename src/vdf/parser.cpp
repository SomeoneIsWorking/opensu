// The parser. KeyValues is a token stream, so an explicit offset over the source
// string is enough; there is no case that needs pushback.
#include "parser.hpp"

#include <charconv>
#include <fstream>
#include <utility>

namespace opensu::vdf::detail {
namespace {

/// True for the characters that end a bare token.
bool isDelimiter(char c) noexcept {
    switch (c) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
    case '{':
    case '}':
    case '"':
        return true;
    default:
        return false;
    }
}

} // namespace

Parser::Parser(std::string_view source) noexcept : source_{source} {
}

void Parser::skipSpace() {
    while (offset_ < source_.size()) {
        const char c = source_[offset_];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++offset_;
            continue;
        }
        // "//" runs to end of line. A lone slash is an ordinary token character.
        if (c == '/' && offset_ + 1 < source_.size() && source_[offset_ + 1] == '/') {
            while (offset_ < source_.size() && source_[offset_] != '\n') {
                ++offset_;
            }
            continue;
        }
        return;
    }
}

std::optional<std::string> Parser::readQuoted() {
    ++offset_; // opening quote
    std::string out;
    while (offset_ < source_.size()) {
        const char c = source_[offset_];
        if (c == '"') {
            ++offset_;
            return out;
        }
        if (c != '\\') {
            out.push_back(c);
            ++offset_;
            continue;
        }
        if (offset_ + 1 >= source_.size()) {
            break;
        }
        const char escaped = source_[offset_ + 1];
        offset_ += 2;
        switch (escaped) {
        case 'n':
            out.push_back('\n');
            break;
        case 't':
            out.push_back('\t');
            break;
        case '\\':
        case '"':
            out.push_back(escaped);
            break;
        default:
            // Unknown escapes keep both characters, which is what a path
            // containing one needs.
            out.push_back('\\');
            out.push_back(escaped);
            break;
        }
    }
    error_ = ParseError{offset_, "unterminated string"};
    return std::nullopt;
}

std::optional<std::string> Parser::readToken() {
    skipSpace();
    if (atEnd()) {
        error_ = ParseError{offset_, "expected a token"};
        return std::nullopt;
    }
    if (peek() == '"') {
        return readQuoted();
    }
    const std::size_t start = offset_;
    while (offset_ < source_.size() && !isDelimiter(source_[offset_])) {
        ++offset_;
    }
    if (offset_ == start) {
        error_ = ParseError{offset_, "empty token"};
        return std::nullopt;
    }
    return std::string{source_.substr(start, offset_ - start)};
}

bool Parser::parseBlock(std::vector<std::pair<std::string, Node::Value>>& out, bool nested) {
    while (true) {
        skipSpace();
        if (atEnd()) {
            if (nested) {
                error_ = ParseError{offset_, "unterminated block"};
                return false;
            }
            return true;
        }
        if (peek() == '}') {
            if (!nested) {
                error_ = ParseError{offset_, "unexpected close brace"};
                return false;
            }
            ++offset_;
            return true;
        }
        if (peek() == '{') {
            error_ = ParseError{offset_, "unexpected open brace"};
            return false;
        }

        const std::optional<std::string> key = readToken();
        if (!key) {
            return false;
        }

        skipSpace();
        if (!atEnd() && peek() == '{') {
            ++offset_;
            std::vector<std::pair<std::string, Node::Value>> child;
            if (!parseBlock(child, true)) {
                return false;
            }
            out.emplace_back(*key, Node::from(std::move(child)));
            continue;
        }

        // A key with no value is legal; record it as empty so callers testing
        // for presence rather than content still find it.
        if (!atEnd() && peek() != '}') {
            const std::optional<std::string> value = readToken();
            if (!value) {
                return false;
            }
            out.emplace_back(*key, *value);
            continue;
        }
        out.emplace_back(*key, std::string{});
    }
}

} // namespace opensu::vdf::detail

namespace opensu::vdf {

std::optional<Node> parse(std::string_view source, ParseError& error) {
    detail::Parser parser{source};
    std::vector<std::pair<std::string, Node::Value>> entries;
    if (!parser.parseBlock(entries, false)) {
        error = parser.error();
        return std::nullopt;
    }
    return Node::from(std::move(entries));
}

std::optional<Node> parseFile(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    ParseError error;
    return parse(buffer.str(), error);
}

} // namespace opensu::vdf
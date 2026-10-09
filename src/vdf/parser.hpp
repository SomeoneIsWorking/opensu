// Parser. KeyValues is a token stream, so an explicit offset over the source
// string is enough; there is no case that needs pushback.
#pragma once

#include "node.hpp"

namespace opensu::vdf::detail {

/// Reads a document into a flat entry list.
class Parser {
  public:
    explicit Parser(std::string_view source) noexcept;

    /// Parses one block, appending its entries to `out`. When `nested` is false
    /// the block runs to end-of-input rather than to a closing brace, which is
    /// what the document root does.
    bool parseBlock(std::vector<std::pair<std::string, Node::Value>>& out, bool nested);

    [[nodiscard]] const ParseError& error() const noexcept {
        return error_;
    }

  private:
    void skipSpace();
    [[nodiscard]] bool atEnd() const noexcept {
        return offset_ >= source_.size();
    }
    [[nodiscard]] char peek() const noexcept {
        return source_[offset_];
    }
    [[nodiscard]] std::optional<std::string> readToken();
    [[nodiscard]] std::optional<std::string> readQuoted();

    std::string_view source_;
    std::size_t offset_{};
    ParseError error_{};
};

} // namespace opensu::vdf::detail
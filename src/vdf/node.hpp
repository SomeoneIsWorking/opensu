// vdf — Valve's KeyValues text format, the shape Steam writes app manifests,
// library folders and per-user configuration in.
//
// The format is a token stream: quoted or bare keys, brace-delimited blocks, and
// `//` comments. A document is an unbraced root block whose keys are top-level
// sections.
#pragma once

#include <filesystem>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace iideck::vdf {

// A parsed document. Values are either a string or a nested block.
class Node {
  public:
    using Value = std::variant<std::string, Node>;

    Node() = default;

    /// Builds a node from an ordered list of key/value pairs.
    static Node from(std::vector<std::pair<std::string, Value>> entries);

    /// Looks up a key, case-insensitively, because Steam is inconsistent about
    /// casing between files. Returns the value or nothing.
    [[nodiscard]] std::optional<Value> find(std::string_view key) const;

    /// True when every key in the path exists, whatever the leaf's value.
    [[nodiscard]] bool has(std::initializer_list<std::string_view> path) const;

    /// Walks a key path and returns the block found there.
    [[nodiscard]] std::optional<Node> block(std::initializer_list<std::string_view> path) const;

    /// Walks a key path and returns the string found there.
    [[nodiscard]] std::optional<std::string>
    str(std::initializer_list<std::string_view> path) const;

    /// Walks a key path and parses the string as a signed integer.
    [[nodiscard]] std::optional<long long>
    integer(std::initializer_list<std::string_view> path) const;

    /// The keys in sorted order, so iteration is reproducible.
    [[nodiscard]] std::vector<std::string> keys() const;

    [[nodiscard]] bool empty() const noexcept {
        return entries_.empty();
    }

  private:
    std::vector<std::pair<std::string, Value>> entries_;

    [[nodiscard]] const Value* findValue(std::string_view key) const;
};

/// The error a malformed document produces.
struct ParseError {
    /// Byte offset where parsing stopped.
    std::size_t offset{};
    /// What was expected there.
    std::string message;
};

/// Parses a document. Returns nothing and fills `error` when the input is not
/// valid KeyValues.
[[nodiscard]] std::optional<Node> parse(std::string_view source, ParseError& error);

/// Parses a file, returning nothing when it cannot be read or is malformed.
[[nodiscard]] std::optional<Node> parseFile(const std::filesystem::path& path);

} // namespace iideck::vdf
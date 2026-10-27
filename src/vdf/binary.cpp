#include "binary.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace opensu::vdf {
namespace {

constexpr char blockType = 0x00;
constexpr char stringType = 0x01;
constexpr char int32Type = 0x02;
constexpr char endType = 0x08;
/// Deeper than any file Steam writes; stops a hostile file from exhausting the stack.
constexpr int mostDepth = 32;

class Reader {
  public:
    explicit Reader(std::string_view bytes) : bytes_{bytes} {
    }

    /// The entries up to the block's end byte; the root's may also end with the input.
    std::optional<Node> block(int depth) {
        if (depth > mostDepth) {
            return std::nullopt;
        }
        std::vector<std::pair<std::string, Node::Value>> entries;
        while (at_ < bytes_.size()) {
            const char type = bytes_[at_++];
            if (type == endType) {
                return Node::from(std::move(entries));
            }
            std::optional<std::string> key = text();
            if (!key) {
                return std::nullopt;
            }
            if (type == blockType) {
                std::optional<Node> inner = block(depth + 1);
                if (!inner) {
                    return std::nullopt;
                }
                entries.emplace_back(std::move(*key), std::move(*inner));
            } else if (type == stringType) {
                std::optional<std::string> value = text();
                if (!value) {
                    return std::nullopt;
                }
                entries.emplace_back(std::move(*key), std::move(*value));
            } else if (type == int32Type) {
                std::optional<std::int32_t> value = int32();
                if (!value) {
                    return std::nullopt;
                }
                entries.emplace_back(std::move(*key), std::to_string(*value));
            } else {
                return std::nullopt;
            }
        }
        return depth == 0 ? std::optional{Node::from(std::move(entries))} : std::nullopt;
    }

  private:
    std::optional<std::string> text() {
        const std::size_t end = bytes_.find('\0', at_);
        if (end == std::string_view::npos) {
            return std::nullopt;
        }
        std::string out{bytes_.substr(at_, end - at_)};
        at_ = end + 1;
        return out;
    }

    std::optional<std::int32_t> int32() {
        if (bytes_.size() - at_ < 4) {
            return std::nullopt;
        }
        std::uint32_t value = 0;
        for (std::size_t i = 0; i < 4; ++i) {
            value |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes_[at_ + i]))
                     << (8U * i);
        }
        at_ += 4;
        return static_cast<std::int32_t>(value);
    }

    std::string_view bytes_;
    std::size_t at_{0};
};

} // namespace

std::optional<Node> parseBinary(std::string_view bytes) {
    return Reader{bytes}.block(0);
}

std::optional<Node> parseBinaryFile(const std::filesystem::path& path) {
    std::ifstream in{path, std::ios::binary};
    if (!in) {
        return std::nullopt;
    }
    const std::string bytes{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    return parseBinary(bytes);
}

} // namespace opensu::vdf

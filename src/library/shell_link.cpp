#include "shell_link.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <vector>

#include "fileio/checked_bytes.hpp"

namespace opensu::library {
namespace {

using fileio::CheckedBytes;

constexpr std::size_t headerSize = 0x4C;
/// 00021401-0000-0000-C000-000000000046, as a link's header stores it.
constexpr std::array<std::uint8_t, 16> linkClsid{0x01, 0x14, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};
constexpr std::size_t flagsAt = 20;
constexpr std::size_t iconIndexAt = 56;

constexpr std::uint32_t hasTargetIdList = 0x01;
constexpr std::uint32_t hasLinkInfo = 0x02;
constexpr std::uint32_t hasName = 0x04;
constexpr std::uint32_t hasRelativePath = 0x08;
constexpr std::uint32_t hasWorkingDir = 0x10;
constexpr std::uint32_t hasArguments = 0x20;
constexpr std::uint32_t hasIconLocation = 0x40;
constexpr std::uint32_t isUnicode = 0x80;

constexpr std::uint32_t volumeIdAndLocalBasePath = 0x01;
constexpr std::uint32_t commonNetworkRelativeLink = 0x02;

constexpr std::uint32_t environmentBlock = 0xA0000001U;
constexpr std::uint32_t iconEnvironmentBlock = 0xA0000007U;
/// An environment block's two 260-character paths follow its size and signature.
constexpr std::size_t blockAnsiAt = 8;
constexpr std::size_t blockUnicodeAt = blockAnsiAt + 260;
constexpr std::size_t blockPathChars = 260;

/// The most bytes of a shortcut read; real ones are a few kilobytes.
constexpr std::size_t mostLinkBytes = 1U << 20U;

/// LinkInfo's base path and suffix joined; empty when it has none.
std::string linkInfoTarget(const CheckedBytes& bytes, std::size_t at) {
    const auto headerLength = bytes.u32(at + 4);
    const auto flags = bytes.u32(at + 8);
    if (!headerLength || !flags) {
        return {};
    }
    const auto fieldAt = [&](std::size_t field) -> std::optional<std::size_t> {
        const auto offset = bytes.u32(at + field);
        return offset && *offset != 0 ? std::optional<std::size_t>{at + *offset} : std::nullopt;
    };
    const std::size_t most = bytes.size();
    std::string suffix;
    if (const auto wide = *headerLength >= 0x24 ? fieldAt(32) : std::nullopt) {
        suffix = bytes.utf16(*wide, most);
    } else if (const auto narrow = fieldAt(24)) {
        suffix = bytes.ansi(*narrow, most);
    }
    if ((*flags & volumeIdAndLocalBasePath) != 0) {
        if (const auto wide = *headerLength >= 0x24 ? fieldAt(28) : std::nullopt) {
            return bytes.utf16(*wide, most) + suffix;
        }
        if (const auto narrow = fieldAt(16)) {
            return bytes.ansi(*narrow, most) + suffix;
        }
    }
    if ((*flags & commonNetworkRelativeLink) != 0) {
        const auto network = bytes.u32(at + 20);
        const auto nameOffset = network ? bytes.u32(at + *network + 8) : std::nullopt;
        if (nameOffset) {
            std::string share = bytes.ansi(at + *network + *nameOffset, most);
            return suffix.empty() ? share : share + "\\" + suffix;
        }
    }
    return {};
}

/// A StringData field at `at`, moving `at` past it; nothing when it runs past the bytes.
std::optional<std::string> stringAt(const CheckedBytes& bytes, std::size_t& at, bool unicode) {
    const auto count = bytes.u16(at);
    if (!count) {
        return std::nullopt;
    }
    const std::size_t length = std::size_t{*count} * (unicode ? 2 : 1);
    if (!bytes.holds(at + 2, length)) {
        return std::nullopt;
    }
    std::string text = unicode ? bytes.utf16(at + 2, *count) : bytes.ansi(at + 2, *count);
    at += 2 + length;
    return text;
}

/// An environment block's path: its Unicode copy, else its ANSI one.
std::string blockPath(const CheckedBytes& bytes, std::size_t at) {
    std::string wide = bytes.utf16(at + blockUnicodeAt, blockPathChars);
    return wide.empty() ? bytes.ansi(at + blockAnsiAt, blockPathChars) : wide;
}

/// Takes what the extra data blocks at `at` add; a block's size of under 4 ends them.
void readExtraData(const CheckedBytes& bytes, std::size_t at, ShellLink& link) {
    for (;;) {
        const auto size = bytes.u32(at);
        const auto signature = bytes.u32(at + 4);
        if (!size || !signature || *size < 8 || !bytes.holds(at, *size)) {
            return;
        }
        if (*signature == environmentBlock && link.target.empty()) {
            link.target = blockPath(bytes, at);
        } else if (*signature == iconEnvironmentBlock && link.iconLocation.empty()) {
            link.iconLocation = blockPath(bytes, at);
        }
        at += *size;
    }
}

} // namespace

std::optional<ShellLink> parseShellLink(std::span<const std::uint8_t> raw) {
    const CheckedBytes bytes{raw};
    const auto clsid = bytes.slice(4, linkClsid.size());
    if (bytes.u32(0) != headerSize || !clsid || !std::ranges::equal(*clsid, linkClsid)) {
        return std::nullopt;
    }
    const auto flags = bytes.u32(flagsAt);
    const auto iconIndex = bytes.u32(iconIndexAt);
    if (!flags || !iconIndex || !bytes.holds(0, headerSize)) {
        return std::nullopt;
    }
    ShellLink link;
    link.iconIndex = static_cast<std::int32_t>(*iconIndex);
    std::size_t at = headerSize;
    if ((*flags & hasTargetIdList) != 0) {
        const auto size = bytes.u16(at);
        if (!size || !bytes.holds(at + 2, *size)) {
            return link;
        }
        at += 2 + std::size_t{*size};
    }
    if ((*flags & hasLinkInfo) != 0) {
        const auto size = bytes.u32(at);
        if (!size || !bytes.holds(at, *size)) {
            return link;
        }
        link.target = linkInfoTarget(bytes, at);
        at += *size;
    }
    const bool unicode = (*flags & isUnicode) != 0;
    const auto field = [&](std::uint32_t flag, std::string* into) {
        if ((*flags & flag) == 0) {
            return true;
        }
        std::optional<std::string> text = stringAt(bytes, at, unicode);
        if (text && into != nullptr) {
            *into = std::move(*text);
            return true;
        }
        return text.has_value();
    };
    std::string relative;
    if (!field(hasName, &link.description) || !field(hasRelativePath, &relative) ||
        !field(hasWorkingDir, &link.workingDirectory) || !field(hasArguments, &link.arguments) ||
        !field(hasIconLocation, &link.iconLocation)) {
        return link;
    }
    readExtraData(bytes, at, link);
    if (link.target.empty()) {
        link.target = relative;
    }
    return link;
}

std::optional<ShellLink> readShellLink(const std::filesystem::path& file) {
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes(mostLinkBytes);
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<std::size_t>(in.gcount()));
    return parseShellLink(bytes);
}

} // namespace opensu::library

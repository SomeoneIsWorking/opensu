// shell_link_bytes — a Windows shortcut built byte by byte from MS-SHLLINK for tests: the file a
// real `.lnk` is, without Windows to write one.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::test {

struct LinkFields {
    /// LinkInfo's local base path.
    std::string target;
    std::string arguments{};
    std::string workingDirectory{};
    std::string iconLocation{};
    std::int32_t iconIndex{0};
    std::string description{};
    /// LinkInfo's common path suffix.
    std::string suffix{};
    /// Strings in UTF-16, as Windows writes them, or in the ANSI code page.
    bool unicode{true};
};

namespace link_detail {

inline void put(std::vector<std::uint8_t>& out, std::uint64_t value, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU));
    }
}

/// `text`, UTF-8, as UTF-16 code units.
inline std::vector<std::uint16_t> units(std::string_view text) {
    std::vector<std::uint16_t> out;
    for (std::size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c < 0x80U) {
            out.push_back(c);
            i += 1;
        } else if (c < 0xE0U) {
            out.push_back(static_cast<std::uint16_t>(
                ((c & 0x1FU) << 6U) | (static_cast<unsigned char>(text[i + 1]) & 0x3FU)));
            i += 2;
        } else {
            out.push_back(static_cast<std::uint16_t>(
                ((c & 0x0FU) << 12U) | ((static_cast<unsigned char>(text[i + 1]) & 0x3FU) << 6U) |
                (static_cast<unsigned char>(text[i + 2]) & 0x3FU)));
            i += 3;
        }
    }
    return out;
}

inline void putZ(std::vector<std::uint8_t>& out, std::string_view text, bool unicode) {
    if (unicode) {
        for (const std::uint16_t unit : units(text)) {
            put(out, unit, 2);
        }
        put(out, 0, 2);
    } else {
        out.insert(out.end(), text.begin(), text.end());
        out.push_back(0);
    }
}

inline void putCounted(std::vector<std::uint8_t>& out, std::string_view text, bool unicode) {
    if (unicode) {
        const std::vector<std::uint16_t> wide = units(text);
        put(out, wide.size(), 2);
        for (const std::uint16_t unit : wide) {
            put(out, unit, 2);
        }
    } else {
        put(out, text.size(), 2);
        out.insert(out.end(), text.begin(), text.end());
    }
}

} // namespace link_detail

[[nodiscard]] inline std::vector<std::uint8_t> shellLinkBytes(const LinkFields& link) {
    using link_detail::put;
    constexpr std::uint32_t hasLinkInfo = 0x02;
    constexpr std::uint32_t hasName = 0x04;
    constexpr std::uint32_t hasWorkingDir = 0x10;
    constexpr std::uint32_t hasArguments = 0x20;
    constexpr std::uint32_t hasIconLocation = 0x40;
    constexpr std::uint32_t isUnicode = 0x80;
    std::uint32_t flags = hasLinkInfo;
    flags |= link.description.empty() ? 0 : hasName;
    flags |= link.workingDirectory.empty() ? 0 : hasWorkingDir;
    flags |= link.arguments.empty() ? 0 : hasArguments;
    flags |= link.iconLocation.empty() ? 0 : hasIconLocation;
    flags |= link.unicode ? isUnicode : 0;

    std::vector<std::uint8_t> out;
    put(out, 0x4C, 4);
    for (const std::uint8_t byte : {0x01, 0x14, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x46}) {
        out.push_back(byte);
    }
    put(out, flags, 4);
    put(out, 0, 4 + 24 + 4);
    put(out, static_cast<std::uint32_t>(link.iconIndex), 4);
    put(out, 0, 4 + 2 + 2 + 8);

    // LinkInfo: its Unicode form has the two optional offsets, so a 0x24-byte header.
    const std::size_t headerSize = link.unicode ? 0x24 : 0x1C;
    std::vector<std::uint8_t> strings;
    std::vector<std::uint8_t> volume;
    put(volume, 0x10, 4);
    put(volume, 3, 4);
    put(volume, 0, 8);
    const std::size_t volumeAt = headerSize;
    const std::size_t baseAt = volumeAt + volume.size();
    strings = volume;
    link_detail::putZ(strings, link.target, false);
    const std::size_t suffixAt = headerSize + strings.size();
    link_detail::putZ(strings, link.suffix, false);
    std::size_t baseWideAt = 0;
    std::size_t suffixWideAt = 0;
    if (link.unicode) {
        baseWideAt = headerSize + strings.size();
        link_detail::putZ(strings, link.target, true);
        suffixWideAt = headerSize + strings.size();
        link_detail::putZ(strings, link.suffix, true);
    }
    put(out, headerSize + strings.size(), 4);
    put(out, headerSize, 4);
    put(out, 1, 4);
    put(out, volumeAt, 4);
    put(out, baseAt, 4);
    put(out, 0, 4);
    put(out, suffixAt, 4);
    if (link.unicode) {
        put(out, baseWideAt, 4);
        put(out, suffixWideAt, 4);
    }
    out.insert(out.end(), strings.begin(), strings.end());

    for (const std::string* field :
         {&link.description, &link.workingDirectory, &link.arguments, &link.iconLocation}) {
        if (!field->empty()) {
            link_detail::putCounted(out, *field, link.unicode);
        }
    }
    put(out, 0, 4);
    return out;
}

} // namespace opensu::test

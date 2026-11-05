// pe_bytes — a Windows program's bytes for tests: PE headers, one resource section, the resource
// directory's three levels, and icons (DIB or PNG), built byte by byte as a linker lays them out.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace opensu::test {

using PeBytes = std::vector<std::uint8_t>;

inline void putLe(PeBytes& out, std::size_t at, std::uint64_t value, std::size_t width) {
    if (out.size() < at + width) {
        out.resize(at + width);
    }
    for (std::size_t i = 0; i < width; ++i) {
        out[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
    }
}

/// One resource: its type and id, its one language, and its bytes.
struct PeResource {
    std::uint32_t type;
    std::uint32_t id;
    PeBytes bytes;
};

/// The resource section of `resources`, its first byte at `rva`.
inline PeBytes peResourceSection(const std::vector<PeResource>& resources, std::uint32_t rva) {
    constexpr std::uint32_t directoryBit = 0x80000000U;
    std::map<std::uint32_t, std::vector<const PeResource*>> byType;
    for (const PeResource& resource : resources) {
        byType[resource.type].push_back(&resource);
    }
    const auto directorySize = [](std::size_t entries) {
        return 16 + (entries * 8);
    };
    std::size_t at = directorySize(byType.size());
    std::map<std::uint32_t, std::size_t> typeAt;
    std::map<const PeResource*, std::size_t> nameAt;
    for (const auto& [type, list] : byType) {
        typeAt[type] = at;
        at += directorySize(list.size());
    }
    for (const auto& [type, list] : byType) {
        for (const PeResource* resource : list) {
            nameAt[resource] = at;
            at += directorySize(1);
        }
    }
    std::map<const PeResource*, std::size_t> entryAt;
    for (const PeResource& resource : resources) {
        entryAt[&resource] = at;
        at += 16;
    }
    PeBytes out(at);
    const auto header = [&out](std::size_t offset, std::size_t numbered) {
        putLe(out, offset + 14, numbered, 2);
    };
    header(0, byType.size());
    std::size_t slot = 16;
    for (const auto& [type, list] : byType) {
        putLe(out, slot, type, 4);
        putLe(out, slot + 4, directoryBit | typeAt[type], 4);
        slot += 8;
        header(typeAt[type], list.size());
        std::size_t inner = typeAt[type] + 16;
        for (const PeResource* resource : list) {
            putLe(out, inner, resource->id, 4);
            putLe(out, inner + 4, directoryBit | nameAt[resource], 4);
            inner += 8;
            header(nameAt[resource], 1);
            putLe(out, nameAt[resource] + 16, 1033, 4);
            putLe(out, nameAt[resource] + 20, entryAt[resource], 4);
        }
    }
    for (const PeResource& resource : resources) {
        const std::size_t dataAt = out.size();
        out.insert(out.end(), resource.bytes.begin(), resource.bytes.end());
        putLe(out, entryAt[&resource], rva + dataAt, 4);
        putLe(out, entryAt[&resource] + 4, resource.bytes.size(), 4);
    }
    return out;
}

/// A whole program holding `resources`, PE32+ with `wide`.
inline PeBytes peProgram(const std::vector<PeResource>& resources, bool wide = false) {
    constexpr std::size_t peAt = 0x80;
    constexpr std::uint32_t rva = 0x1000;
    constexpr std::size_t rawAt = 0x400;
    const std::size_t optional = wide ? 240 : 224;
    const PeBytes section = peResourceSection(resources, rva);
    PeBytes out(rawAt);
    out[0] = 'M';
    out[1] = 'Z';
    putLe(out, 0x3C, peAt, 4);
    putLe(out, peAt, 0x00004550U, 4);
    putLe(out, peAt + 6, 1, 2);
    putLe(out, peAt + 20, optional, 2);
    const std::size_t opt = peAt + 24;
    putLe(out, opt, wide ? 0x20B : 0x10B, 2);
    putLe(out, opt + (wide ? 112 : 96) + 16, rva, 4);
    putLe(out, opt + (wide ? 112 : 96) + 20, section.size(), 4);
    const std::size_t header = opt + optional;
    putLe(out, header + 8, section.size(), 4);
    putLe(out, header + 12, rva, 4);
    putLe(out, header + 16, section.size(), 4);
    putLe(out, header + 20, rawAt, 4);
    out.insert(out.end(), section.begin(), section.end());
    return out;
}

/// A square 32-bit DIB as an icon stores it: bottom row first, an AND mask after it. The colour is
/// (r, g, b), and the top left quarter is clear when `alpha` is false by its mask, by alpha else.
inline PeBytes iconDib(std::uint32_t side, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                       bool alpha = true) {
    PeBytes out(40);
    putLe(out, 0, 40, 4);
    putLe(out, 4, side, 4);
    putLe(out, 8, std::uint64_t{side} * 2, 4);
    putLe(out, 12, 1, 2);
    putLe(out, 14, 32, 2);
    for (std::uint32_t row = 0; row < side; ++row) {
        const std::uint32_t y = side - 1 - row;
        for (std::uint32_t x = 0; x < side; ++x) {
            const bool clear = x < side / 2 && y < side / 2;
            out.push_back(b);
            out.push_back(g);
            out.push_back(r);
            out.push_back(alpha ? (clear ? 0 : 255) : 0);
        }
    }
    const std::uint32_t maskStride = ((side + 31) / 32) * 4;
    for (std::uint32_t row = 0; row < side; ++row) {
        const std::uint32_t y = side - 1 - row;
        PeBytes mask(maskStride);
        for (std::uint32_t x = 0; x < side; ++x) {
            if (!alpha && x < side / 2 && y < side / 2) {
                mask[x / 8] |= static_cast<std::uint8_t>(0x80U >> (x % 8));
            }
        }
        out.insert(out.end(), mask.begin(), mask.end());
    }
    return out;
}

/// A square 8-bit palette DIB of one colour, for the palette path.
inline PeBytes iconPaletteDib(std::uint32_t side, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    PeBytes out(40);
    putLe(out, 0, 40, 4);
    putLe(out, 4, side, 4);
    putLe(out, 8, std::uint64_t{side} * 2, 4);
    putLe(out, 12, 1, 2);
    putLe(out, 14, 8, 2);
    putLe(out, 32, 2, 4);
    const std::uint8_t palette[] = {0, 0, 0, 0, b, g, r, 0};
    out.insert(out.end(), std::begin(palette), std::end(palette));
    const std::uint32_t stride = ((side * 8 + 31) / 32) * 4;
    out.insert(out.end(), std::size_t{stride} * side, 1);
    out.insert(out.end(), std::size_t{(side + 31) / 32} * 4 * side, 0);
    return out;
}

/// An icon group naming resources `ids`, of sides `sides`.
inline PeBytes iconGroup(const std::vector<std::uint16_t>& ids,
                         const std::vector<std::uint32_t>& sides, std::uint16_t depth = 32) {
    PeBytes out(6);
    putLe(out, 2, 1, 2);
    putLe(out, 4, ids.size(), 2);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const std::size_t at = out.size();
        out.resize(at + 14);
        out[at] = static_cast<std::uint8_t>(sides[i] >= 256 ? 0 : sides[i]);
        out[at + 1] = out[at];
        putLe(out, at + 6, depth, 2);
        putLe(out, at + 12, ids[i], 2);
    }
    return out;
}

/// An `.ico` file of `images`, of sides `sides`.
inline PeBytes icoFile(const std::vector<PeBytes>& images,
                       const std::vector<std::uint32_t>& sides) {
    PeBytes out(6 + (16 * images.size()));
    putLe(out, 2, 1, 2);
    putLe(out, 4, images.size(), 2);
    for (std::size_t i = 0; i < images.size(); ++i) {
        const std::size_t at = 6 + (16 * i);
        out[at] = static_cast<std::uint8_t>(sides[i] >= 256 ? 0 : sides[i]);
        out[at + 1] = out[at];
        putLe(out, at + 6, 32, 2);
        putLe(out, at + 8, images[i].size(), 4);
        putLe(out, at + 12, out.size(), 4);
        out.insert(out.end(), images[i].begin(), images[i].end());
    }
    return out;
}

} // namespace opensu::test

#include "pe_resources.hpp"

#include <algorithm>
#include <fstream>

#include "fileio/checked_bytes.hpp"

namespace opensu::library::pe {
namespace {

namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;
using fileio::CheckedBytes;

/// The most bytes of a resource section read.
constexpr std::uint32_t mostSectionBytes = 32U << 20U;
constexpr std::uint32_t peSignature = 0x00004550U;
constexpr std::uint16_t pe32Plus = 0x20B;
constexpr std::uint32_t directoryBit = 0x80000000U;
constexpr std::uint32_t entriesAt = 16;
constexpr std::uint32_t entrySize = 8;
constexpr std::size_t sectionHeaderSize = 40;
constexpr std::size_t optionalHeaderAt = 24;
/// PE's most sections, by the loader's limit.
constexpr std::size_t mostSections = 96;

Bytes readAt(std::ifstream& in, std::uint64_t offset, std::size_t size) {
    Bytes bytes(size);
    in.clear();
    in.seekg(static_cast<std::streamoff>(offset));
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    bytes.resize(static_cast<std::size_t>(std::max<std::streamsize>(in.gcount(), 0)));
    return bytes;
}

} // namespace

std::optional<Resources> Resources::of(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        return std::nullopt;
    }
    const Bytes dos = readAt(in, 0, 64);
    const CheckedBytes dosHeader{dos};
    const auto peAt = dosHeader.u32(0x3C);
    if (dosHeader.u16(0) != 0x5A4D || !peAt) {
        return std::nullopt;
    }
    const Bytes coff = readAt(in, *peAt, optionalHeaderAt);
    const CheckedBytes coffHeader{coff};
    const auto sections = coffHeader.u16(6);
    const auto optionalSize = coffHeader.u16(20);
    if (coffHeader.u32(0) != peSignature || !sections || !optionalSize ||
        *sections > mostSections) {
        return std::nullopt;
    }
    const Bytes rest = readAt(in, std::uint64_t{*peAt} + optionalHeaderAt,
                              *optionalSize + (*sections * sectionHeaderSize));
    const CheckedBytes headers{rest};
    // The resource directory is data directory 2.
    const std::size_t directories = headers.u16(0) == pe32Plus ? 112 : 96;
    const auto resourceRva = headers.u32(directories + 16);
    if (!resourceRva || *resourceRva == 0) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < *sections; ++i) {
        const std::size_t header = *optionalSize + (i * sectionHeaderSize);
        const auto virtualAddress = headers.u32(header + 12);
        const auto rawSize = headers.u32(header + 16);
        const auto rawAt = headers.u32(header + 20);
        if (!virtualAddress || !rawSize || !rawAt) {
            return std::nullopt;
        }
        if (*resourceRva < *virtualAddress ||
            *resourceRva - *virtualAddress >= std::uint64_t{*rawSize}) {
            continue;
        }
        const std::uint32_t skip = *resourceRva - *virtualAddress;
        return Resources{
            readAt(in, std::uint64_t{*rawAt} + skip, std::min(*rawSize - skip, mostSectionBytes)),
            *resourceRva};
    }
    return std::nullopt;
}

std::vector<Resources::Entry> Resources::directory(std::uint32_t offset) const {
    const CheckedBytes bytes{section_};
    const auto named = bytes.u16(std::size_t{offset} + 12);
    const auto numbered = bytes.u16(std::size_t{offset} + 14);
    if (!named || !numbered) {
        return {};
    }
    std::vector<Entry> entries;
    const std::size_t count = std::size_t{*named} + *numbered;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t at = std::size_t{offset} + entriesAt + (i * entrySize);
        const auto name = bytes.u32(at);
        const auto target = bytes.u32(at + 4);
        if (!name || !target) {
            break;
        }
        const bool isNamed = i < *named;
        entries.push_back(Entry{isNamed ? 0 : *name, isNamed, *target});
    }
    return entries;
}

std::vector<Resources::Entry> Resources::ofType(std::uint32_t type) const {
    for (const Entry& entry : directory(0)) {
        if (!entry.named && entry.id == type && (entry.target & directoryBit) != 0) {
            return directory(entry.target & ~directoryBit);
        }
    }
    return {};
}

std::span<const std::uint8_t> Resources::data(const Entry& entry) const {
    if ((entry.target & directoryBit) == 0) {
        return {};
    }
    const std::vector<Entry> languages = directory(entry.target & ~directoryBit);
    if (languages.empty() || (languages.front().target & directoryBit) != 0) {
        return {};
    }
    const CheckedBytes bytes{section_};
    const auto dataRva = bytes.u32(languages.front().target);
    const auto size = bytes.u32(std::size_t{languages.front().target} + 4);
    if (!dataRva || !size || *dataRva < rva_) {
        return {};
    }
    return bytes.slice(*dataRva - rva_, *size).value_or(std::span<const std::uint8_t>{});
}

std::vector<std::uint32_t> Resources::ids(std::uint32_t type) const {
    std::vector<std::uint32_t> found;
    for (const Entry& entry : ofType(type)) {
        found.push_back(entry.id);
    }
    return found;
}

std::span<const std::uint8_t> Resources::at(std::uint32_t type, std::size_t index) const {
    const std::vector<Entry> entries = ofType(type);
    return index < entries.size() ? data(entries[index]) : std::span<const std::uint8_t>{};
}

std::span<const std::uint8_t> Resources::byId(std::uint32_t type, std::uint32_t id) const {
    for (const Entry& entry : ofType(type)) {
        if (!entry.named && entry.id == id) {
            return data(entry);
        }
    }
    return {};
}

} // namespace opensu::library::pe

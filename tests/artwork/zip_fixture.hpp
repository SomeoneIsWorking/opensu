// Builds small zips in memory for the artwork tests.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <zlib.h>

namespace iideck::artwork::fixture {

struct FixtureEntry {
    std::string name;
    std::string data;
    bool deflate{false};
    /// Written in place of the data's real CRC-32.
    std::optional<std::uint32_t> crc;
};

inline void put16(std::string& out, std::uint32_t value) {
    out.push_back(static_cast<char>(value & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
}

inline void put32(std::string& out, std::uint32_t value) {
    put16(out, value & 0xffff);
    put16(out, value >> 16);
}

inline std::uint32_t crcOf(const std::string& data) {
    return static_cast<std::uint32_t>(
        crc32(0, reinterpret_cast<const Bytef*>(data.data()), static_cast<uInt>(data.size())));
}

inline std::string rawDeflate(const std::string& data) {
    z_stream stream{};
    deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    std::string out(deflateBound(&stream, static_cast<uLong>(data.size())), '\0');
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_out = reinterpret_cast<Bytef*>(out.data());
    stream.avail_out = static_cast<uInt>(out.size());
    deflate(&stream, Z_FINISH);
    out.resize(stream.total_out);
    deflateEnd(&stream);
    return out;
}

/// A zip of `entries`, ending in `comment`; `zip64Locator` puts a zip64 end locator before the
/// end record.
inline std::string buildZip(const std::vector<FixtureEntry>& entries,
                            const std::string& comment = {}, bool zip64Locator = false) {
    std::string out;
    std::string directory;
    for (const FixtureEntry& entry : entries) {
        const std::string stored = entry.deflate ? rawDeflate(entry.data) : entry.data;
        const std::uint32_t crc = entry.crc.value_or(crcOf(entry.data));
        const auto offset = static_cast<std::uint32_t>(out.size());
        const auto method = static_cast<std::uint32_t>(entry.deflate ? 8 : 0);
        put32(out, 0x04034b50);
        put16(out, 20);
        put16(out, 0);
        put16(out, method);
        put32(out, 0);
        put32(out, crc);
        put32(out, static_cast<std::uint32_t>(stored.size()));
        put32(out, static_cast<std::uint32_t>(entry.data.size()));
        put16(out, static_cast<std::uint32_t>(entry.name.size()));
        put16(out, 4);
        out += entry.name;
        out += "xxxx";
        out += stored;

        put32(directory, 0x02014b50);
        put16(directory, 20);
        put16(directory, 20);
        put16(directory, 0);
        put16(directory, method);
        put32(directory, 0);
        put32(directory, crc);
        put32(directory, static_cast<std::uint32_t>(stored.size()));
        put32(directory, static_cast<std::uint32_t>(entry.data.size()));
        put16(directory, static_cast<std::uint32_t>(entry.name.size()));
        put16(directory, 0);
        put16(directory, 0);
        put16(directory, 0);
        put16(directory, 0);
        put32(directory, 0);
        put32(directory, offset);
        directory += entry.name;
    }
    const auto directoryOffset = static_cast<std::uint32_t>(out.size());
    out += directory;
    if (zip64Locator) {
        put32(out, 0x07064b50);
        out.append(16, '\0');
    }
    put32(out, 0x06054b50);
    put16(out, 0);
    put16(out, 0);
    put16(out, static_cast<std::uint32_t>(entries.size()));
    put16(out, static_cast<std::uint32_t>(entries.size()));
    put32(out, static_cast<std::uint32_t>(directory.size()));
    put32(out, directoryOffset);
    put16(out, static_cast<std::uint32_t>(comment.size()));
    out += comment;
    return out;
}

} // namespace iideck::artwork::fixture

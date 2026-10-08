#include "zip_archive.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include <zlib.h>

namespace iideck::artwork::zip {
namespace {

constexpr std::uint32_t endSignature = 0x06054b50;
constexpr std::uint32_t locatorSignature = 0x07064b50;
constexpr std::uint32_t directorySignature = 0x02014b50;
constexpr std::uint32_t localSignature = 0x04034b50;
constexpr std::size_t endSize = 22;
constexpr std::size_t locatorSize = 20;
constexpr std::size_t directoryHeaderSize = 46;
constexpr std::uint16_t methodStored = 0;
constexpr std::uint16_t methodDeflate = 8;
constexpr std::uint16_t encryptedFlag = 0x1;
// A file larger than this is not something iideck reads out of a zip.
constexpr std::uint32_t maxFileSize = 256U * 1024U * 1024U;

std::uint32_t le16(std::string_view bytes, std::size_t at) {
    return static_cast<std::uint8_t>(bytes[at]) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[at + 1])) << 8);
}

std::uint32_t le32(std::string_view bytes, std::size_t at) {
    return le16(bytes, at) | (le16(bytes, at + 2) << 16);
}

void fail(std::string& error, std::string message) {
    error = std::move(message);
}

} // namespace

std::optional<Directory> findDirectory(std::string_view tail, std::uint64_t tailOffset,
                                       std::string& error) {
    // The end record is the last one whose comment length runs exactly to the end of the file.
    std::size_t at = std::string_view::npos;
    for (std::size_t from = tail.size() >= endSize ? tail.size() - endSize + 1 : 0; from > 0;) {
        --from;
        if (le32(tail, from) == endSignature &&
            from + endSize + le16(tail, from + 20) == tail.size()) {
            at = from;
            break;
        }
    }
    if (at == std::string_view::npos) {
        fail(error, "no zip end record");
        return std::nullopt;
    }
    const std::uint32_t disk = le16(tail, at + 4);
    const std::uint32_t directoryDisk = le16(tail, at + 6);
    const std::uint32_t onDisk = le16(tail, at + 8);
    const std::uint32_t entries = le16(tail, at + 10);
    const std::uint32_t size = le32(tail, at + 12);
    const std::uint32_t offset = le32(tail, at + 16);
    const bool locator = at >= locatorSize && le32(tail, at - locatorSize) == locatorSignature;
    if (locator || entries == 0xffff || size == 0xffffffffU || offset == 0xffffffffU) {
        fail(error, "zip64 is not supported");
        return std::nullopt;
    }
    if (disk != 0 || directoryDisk != 0 || onDisk != entries) {
        fail(error, "split zips are not supported");
        return std::nullopt;
    }
    if (offset + static_cast<std::uint64_t>(size) > tailOffset + at) {
        fail(error, "zip central directory overruns its end record");
        return std::nullopt;
    }
    return Directory{offset, size, static_cast<std::uint16_t>(entries)};
}

std::optional<std::vector<Entry>> parseDirectory(std::string_view bytes, const Directory& directory,
                                                 std::string& error) {
    std::vector<Entry> entries;
    entries.reserve(directory.entries);
    std::size_t at = 0;
    for (std::uint16_t i = 0; i < directory.entries; ++i) {
        if (at + directoryHeaderSize > bytes.size() || le32(bytes, at) != directorySignature) {
            fail(error, "bad zip central directory record");
            return std::nullopt;
        }
        const std::size_t nameLength = le16(bytes, at + 28);
        const std::size_t extraLength = le16(bytes, at + 30);
        const std::size_t commentLength = le16(bytes, at + 32);
        const std::size_t next =
            at + directoryHeaderSize + nameLength + extraLength + commentLength;
        if (next > bytes.size()) {
            fail(error, "zip central directory record overruns the directory");
            return std::nullopt;
        }
        Entry entry;
        entry.name = std::string{bytes.substr(at + directoryHeaderSize, nameLength)};
        entry.method = static_cast<std::uint16_t>(le16(bytes, at + 10));
        entry.crc32 = le32(bytes, at + 16);
        entry.compressedSize = le32(bytes, at + 20);
        entry.size = le32(bytes, at + 24);
        entry.localHeaderOffset = le32(bytes, at + 42);
        if ((le16(bytes, at + 8) & encryptedFlag) != 0) {
            fail(error, entry.name + " is encrypted");
            return std::nullopt;
        }
        if (entry.compressedSize == 0xffffffffU || entry.size == 0xffffffffU ||
            entry.localHeaderOffset == 0xffffffffU) {
            fail(error, "zip64 is not supported");
            return std::nullopt;
        }
        entries.push_back(std::move(entry));
        at = next;
    }
    return entries;
}

std::optional<std::uint64_t> dataOffset(const Entry& entry, std::string_view header,
                                        std::string& error) {
    if (header.size() < localHeaderSize || le32(header, 0) != localSignature) {
        fail(error, "bad zip local header for " + entry.name);
        return std::nullopt;
    }
    return entry.localHeaderOffset + localHeaderSize + le16(header, 26) + le16(header, 28);
}

std::optional<std::string> extract(const Entry& entry, std::string_view data, std::string& error) {
    if (entry.size > maxFileSize) {
        fail(error, entry.name + " is too large");
        return std::nullopt;
    }
    if (data.size() != entry.compressedSize) {
        fail(error, entry.name + " data is not its compressed size");
        return std::nullopt;
    }
    std::string file;
    if (entry.method == methodStored) {
        if (entry.compressedSize != entry.size) {
            fail(error, entry.name + " is stored at the wrong size");
            return std::nullopt;
        }
        file.assign(data);
    } else if (entry.method == methodDeflate) {
        file.resize(entry.size);
        z_stream stream{};
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            fail(error, "zlib could not start");
            return std::nullopt;
        }
        stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
        stream.avail_in = static_cast<uInt>(data.size());
        stream.next_out = reinterpret_cast<Bytef*>(file.data());
        stream.avail_out = static_cast<uInt>(file.size());
        const int result = inflate(&stream, Z_FINISH);
        const uLong produced = stream.total_out;
        inflateEnd(&stream);
        if (result != Z_STREAM_END || produced != entry.size) {
            fail(error, entry.name + " does not inflate to its size");
            return std::nullopt;
        }
    } else {
        fail(error, entry.name + " uses an unsupported compression method");
        return std::nullopt;
    }
    const uLong crc =
        crc32(0, reinterpret_cast<const Bytef*>(file.data()), static_cast<uInt>(file.size()));
    if (crc != entry.crc32) {
        fail(error, entry.name + " fails its CRC-32");
        return std::nullopt;
    }
    return file;
}

std::optional<Archive> Archive::open(std::string bytes, std::string& error) {
    const std::optional<Directory> directory = findDirectory(bytes, 0, error);
    if (!directory) {
        return std::nullopt;
    }
    std::optional<std::vector<Entry>> entries = parseDirectory(
        std::string_view{bytes}.substr(directory->offset, directory->size), *directory, error);
    if (!entries) {
        return std::nullopt;
    }
    return Archive{std::move(bytes), std::move(*entries)};
}

std::optional<std::string> Archive::read(std::string_view name, std::string& error) const {
    error.clear();
    const auto found = std::ranges::find(entries_, name, &Entry::name);
    if (found == entries_.end()) {
        return std::nullopt;
    }
    const std::string_view header = std::string_view{bytes_}.substr(
        std::min<std::size_t>(found->localHeaderOffset, bytes_.size()), localHeaderSize);
    const std::optional<std::uint64_t> start = dataOffset(*found, header, error);
    if (!start) {
        return std::nullopt;
    }
    if (*start + found->compressedSize > bytes_.size()) {
        fail(error, found->name + " data runs past the end of the zip");
        return std::nullopt;
    }
    return extract(*found, std::string_view{bytes_}.substr(*start, found->compressedSize), error);
}

} // namespace iideck::artwork::zip

#include "zip_writer.hpp"

#include <limits>

#include "zip_archive.hpp"

namespace opensu::zip {
namespace {

constexpr std::uint32_t localSignature = 0x04034b50;
constexpr std::uint32_t centralSignature = 0x02014b50;
constexpr std::uint32_t endSignature = 0x06054b50;
constexpr std::uint16_t version = 20;
/// 1980-01-01, so the same files make the same zip.
constexpr std::uint16_t dosDate = 0x0021;
constexpr std::uint64_t maxOffset = std::numeric_limits<std::uint32_t>::max();
constexpr std::size_t maxEntries = std::numeric_limits<std::uint16_t>::max();

void put16(std::string& out, std::uint16_t value) {
    out.push_back(static_cast<char>(value & 0xff));
    out.push_back(static_cast<char>(value >> 8));
}

void put32(std::string& out, std::uint32_t value) {
    put16(out, static_cast<std::uint16_t>(value & 0xffff));
    put16(out, static_cast<std::uint16_t>(value >> 16));
}

/// What a local header and a central record share, from the version needed on.
void putCommon(std::string& out, std::uint32_t crc32, std::uint32_t size, std::size_t nameSize) {
    put16(out, version);
    put16(out, 0); // flags
    put16(out, 0); // stored
    put16(out, 0); // time
    put16(out, dosDate);
    put32(out, crc32);
    put32(out, size);
    put32(out, size);
    put16(out, static_cast<std::uint16_t>(nameSize));
    put16(out, 0); // extra field
}

} // namespace

bool Writer::add(std::string_view name, std::string_view bytes, std::string& error) {
    const std::uint64_t end = position_ + localHeaderSize + name.size() + bytes.size();
    if (end > maxOffset || written_.size() == maxEntries ||
        name.size() > std::numeric_limits<std::uint16_t>::max()) {
        error = "the zip would need zip64";
        return false;
    }
    const std::uint32_t crc32 = crc32Of(bytes);
    const auto size = static_cast<std::uint32_t>(bytes.size());
    std::string header;
    put32(header, localSignature);
    putCommon(header, crc32, size, name.size());
    header += name;
    out_->write(header.data(), static_cast<std::streamsize>(header.size()));
    out_->write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    written_.push_back(
        Written{std::string{name}, crc32, size, static_cast<std::uint32_t>(position_)});
    position_ = end;
    return true;
}

bool Writer::finish(std::string& error) {
    std::string directory;
    for (const Written& file : written_) {
        put32(directory, centralSignature);
        put16(directory, version);
        putCommon(directory, file.crc32, file.size, file.name.size());
        put16(directory, 0); // comment
        put16(directory, 0); // disk
        put16(directory, 0); // internal attributes
        put32(directory, 0); // external attributes
        put32(directory, file.offset);
        directory += file.name;
    }
    if (position_ + directory.size() > maxOffset) {
        error = "the zip would need zip64";
        return false;
    }
    const auto entries = static_cast<std::uint16_t>(written_.size());
    const auto directorySize = static_cast<std::uint32_t>(directory.size());
    put32(directory, endSignature);
    put16(directory, 0);
    put16(directory, 0);
    put16(directory, entries);
    put16(directory, entries);
    put32(directory, directorySize);
    put32(directory, static_cast<std::uint32_t>(position_));
    put16(directory, 0);
    out_->write(directory.data(), static_cast<std::streamsize>(directory.size()));
    out_->flush();
    if (!*out_) {
        error = "the zip could not be written";
        return false;
    }
    return true;
}

} // namespace opensu::zip

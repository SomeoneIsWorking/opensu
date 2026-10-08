// The zip reader on zips built in memory.
#include "zip_archive.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "zip_fixture.hpp"

namespace {

using iideck::artwork::fixture::buildZip;
using iideck::artwork::fixture::crcOf;
using iideck::artwork::fixture::FixtureEntry;
namespace zip = iideck::artwork::zip;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

bool mentions(const std::string& error, const char* word) {
    return error.find(word) != std::string::npos;
}

std::string repeated(const std::string& unit, int times) {
    std::string out;
    for (int i = 0; i < times; ++i) {
        out += unit;
    }
    return out;
}

void testArchive() {
    const std::string packed = repeated("compressible ", 200);
    std::string error;
    const auto archive = zip::Archive::open(
        buildZip({{"stored.txt", "stored bytes", false, {}}, {"packed.txt", packed, true, {}}}),
        error);
    expect(archive.has_value() && archive->entries().size() == 2, "a zip opens");
    expect(archive->read("stored.txt", error) == "stored bytes", "a stored entry reads back");
    expect(archive->read("packed.txt", error) == packed, "a deflated entry inflates");
    expect(!archive->read("missing.txt", error) && error.empty(),
           "an entry the zip lacks is nothing, without an error");
}

void testChecks() {
    std::string error;
    const auto badCrc =
        zip::Archive::open(buildZip({{"a.txt", "payload", true, 0x12345678}}), error);
    expect(badCrc.has_value(), "a wrong CRC still lists");
    expect(!badCrc->read("a.txt", error) && mentions(error, "CRC"), "a CRC mismatch is refused");

    const std::string zip64 = buildZip({{"a.txt", "payload", false, {}}}, {}, true);
    expect(!zip::Archive::open(zip64, error) && mentions(error, "zip64"), "zip64 is refused");

    expect(!zip::Archive::open("not a zip at all, but long enough", error) &&
               mentions(error, "end record"),
           "bytes without an end record are refused");

    // A comment that contains an end record signature must not be taken for the end record.
    const std::string trap = std::string{"PK\x05\x06"} + std::string(16, '\0') +
                             std::string{"\x05\0"
                                         "abc",
                                         5};
    const auto commented =
        zip::Archive::open(buildZip({{"a.txt", "payload", false, {}}}, trap), error);
    expect(commented.has_value() && commented->read("a.txt", error) == "payload",
           "the end record is found past a comment that imitates one");
}

void testRanges() {
    const std::string packed = repeated("range data ", 300);
    const std::string file = buildZip(
        {{"first.bin", "first", false, {}}, {"second.bin", packed, true, {}}}, "a comment");
    std::string error;
    const std::uint64_t tailOffset = file.size() - 200;
    const std::string tail = file.substr(tailOffset);
    const auto directory = zip::findDirectory(tail, tailOffset, error);
    expect(directory.has_value() && directory->entries == 2, "the end record is read from a tail");
    expect(directory->offset + directory->size < file.size(), "the directory lies inside the file");

    const std::string listing = file.substr(directory->offset, directory->size);
    const auto entries = zip::parseDirectory(listing, *directory, error);
    expect(entries.has_value() && entries->size() == 2 && (*entries)[1].name == "second.bin",
           "the directory parses on its own");
    const zip::Entry& entry = (*entries)[1];
    expect(entry.crc32 == crcOf(packed) && entry.size == packed.size(), "size and CRC are kept");

    const auto start =
        zip::dataOffset(entry, file.substr(entry.localHeaderOffset, zip::localHeaderSize), error);
    expect(start.has_value(), "the data offset follows the local header");
    expect(zip::extract(entry, file.substr(*start, entry.compressedSize), error) == packed,
           "an entry extracts from its data alone");

    zip::Entry wrongSize = entry;
    wrongSize.size += 1;
    expect(!zip::extract(wrongSize, file.substr(*start, entry.compressedSize), error),
           "a size mismatch is refused");
    expect(!zip::dataOffset(entry, "short", error), "a cut-off local header is refused");
}

} // namespace

int main() {
    testArchive();
    testChecks();
    testRanges();
    std::printf("zip_archive: all checks passed\n");
    return 0;
}

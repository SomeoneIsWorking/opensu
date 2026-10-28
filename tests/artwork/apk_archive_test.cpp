// A zip read by ranges: entries come out checked and inflated, and the file is never read whole.
#include "apk_archive.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include "support/zip_fixture.hpp"

namespace {

using opensu::artwork::ApkArchive;
using opensu::artwork::ApkFile;
using opensu::test::zipfixture::buildZip;
using opensu::test::zipfixture::FixtureEntry;

constexpr std::string_view navigationWav = "RIFF navigation click";
constexpr std::string_view openWav = "RIFF open";
constexpr std::string_view domeOgg = "OggS domino cue";

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

/// Reads ranges of `bytes`, adding every byte read to `read`.
ApkArchive::RangeRead reader(const std::string& bytes, std::uint64_t& read) {
    return [&bytes, &read](std::uint64_t first, std::uint64_t last,
                           std::string& error) -> std::optional<std::string> {
        if (last >= bytes.size()) {
            error = "past the end";
            return std::nullopt;
        }
        read += last - first + 1;
        return bytes.substr(first, last - first + 1);
    };
}

/// An APK holding three entries; `fillerEntries` push the directory out of the tail, and `openCrc`
/// is written in place of Open.wav's CRC-32.
std::string apk(int fillerEntries, std::optional<std::uint32_t> openCrc = {}) {
    std::vector<FixtureEntry> entries{
        {"assets/Navigation.wav", std::string{navigationWav}, false, {}},
        {"assets/Open.wav", std::string{openWav}, true, openCrc},
        {"assets/domino_icons_2.ogg", std::string{domeOgg}, false, {}},
        {"classes.dex", std::string(std::size_t{2} * 1024 * 1024, 'd'), false, {}},
    };
    for (int i = 0; i < fillerEntries; ++i) {
        entries.push_back(
            {"res/filler/" + std::string(60, 'a') + std::to_string(i), {}, false, {}});
    }
    return buildZip(entries);
}

void testRanges(int fillerEntries) {
    const std::string served = apk(fillerEntries);
    std::uint64_t read = 0;
    ApkArchive archive{served.size(), reader(served, read)};
    const ApkFile open = archive.fetch("assets/Open.wav");
    expect(open.status == ApkFile::Status::Found && open.bytes == openWav,
           "a deflated entry comes out inflated");
    expect(archive.fetch("assets/Navigation.wav").bytes == navigationWav,
           "a stored entry comes out as stored");
    expect(read < served.size() / 2, "the APK is not read whole");
    expect(archive.fetch("assets/Close.wav").status == ApkFile::Status::Missing,
           "an entry the APK lacks is missing");
}

void testRefusals() {
    {
        const std::string served = apk(0, 0x0badf00d);
        std::uint64_t read = 0;
        ApkArchive archive{served.size(), reader(served, read)};
        const ApkFile open = archive.fetch("assets/Open.wav");
        expect(open.status == ApkFile::Status::Failed &&
                   open.error.find("CRC") != std::string::npos,
               "an entry that fails its CRC-32 is refused");
    }
    {
        const std::string served = apk(0);
        std::uint64_t read = 0;
        ApkArchive archive{served.size() + 1, reader(served, read)};
        const ApkFile open = archive.fetch("assets/Open.wav");
        expect(open.status == ApkFile::Status::Failed && !open.error.empty(),
               "a zip that cannot be read fails rather than misses");
    }
    std::string error;
    expect(!ApkArchive::overFile("/nonexistent/iiSU.apk", error) && !error.empty(),
           "a file that cannot be opened is refused");
}

} // namespace

int main() {
    try {
        testRanges(0);
        testRanges(1200);
        testRefusals();
        std::printf("apk_archive: all checks passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}

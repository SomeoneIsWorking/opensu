#include "resume_file.hpp"

#include <cstring>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"

namespace opensu::retro {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::system_clock;

constexpr std::string_view formatLine = "opensu-resume 1";

/// The header on `in`'s first two lines; nothing when they are not a resume state's.
std::optional<ResumeHeader> headerOn(std::istream& in) {
    std::string format;
    std::string line;
    if (!std::getline(in, format) || format != formatLine || !std::getline(in, line)) {
        return std::nullopt;
    }
    const nlohmann::json json = nlohmann::json::parse(line, nullptr, false);
    if (!json.is_object() || !json.contains("core") || !json["core"].is_string() ||
        !json.contains("saved") || !json["saved"].is_number_integer() ||
        !json.contains("saveRam") || !json["saveRam"].is_number_unsigned()) {
        return std::nullopt;
    }
    return ResumeHeader{
        .core = json["core"].get<std::string>(),
        .coreLabel = json.value("label", std::string{}),
        .coreVersion = json.value("version", std::string{}),
        .saved = Clock::time_point{std::chrono::seconds{json["saved"].get<std::int64_t>()}},
        .saveRam = json["saveRam"].get<std::uint32_t>()};
}

} // namespace

bool writeResume(const fs::path& file, const ResumeHeader& header, std::span<const std::byte> state,
                 std::string& error) {
    const nlohmann::json json{
        {"core", header.core},
        {"label", header.coreLabel},
        {"version", header.coreVersion},
        {"saved",
         std::chrono::duration_cast<std::chrono::seconds>(header.saved.time_since_epoch()).count()},
        {"saveRam", header.saveRam}};
    std::string bytes{formatLine};
    bytes += '\n';
    bytes += json.dump();
    bytes += '\n';
    bytes.append(reinterpret_cast<const char*>(state.data()), state.size());
    return fileio::writeWhole(file, bytes, error);
}

std::optional<ResumeContents> readResume(const fs::path& file, std::string& error) {
    error.clear();
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        std::error_code missing;
        if (fs::exists(file, missing)) {
            error = "cannot read " + file.string();
        }
        return std::nullopt;
    }
    std::optional<ResumeHeader> header = headerOn(in);
    if (!header) {
        error = file.string() + " is not a resume state";
        return std::nullopt;
    }
    const std::vector<char> bytes{std::istreambuf_iterator<char>{in},
                                  std::istreambuf_iterator<char>{}};
    if (bytes.empty()) {
        error = file.string() + " holds no state";
        return std::nullopt;
    }
    ResumeContents contents{.header = std::move(*header), .state = {}};
    contents.state.resize(bytes.size());
    std::memcpy(contents.state.data(), bytes.data(), bytes.size());
    return contents;
}

std::optional<ResumeHeader> readResumeHeader(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        return std::nullopt;
    }
    return headerOn(in);
}

bool discardResume(const fs::path& file, std::string& error) {
    std::error_code failed;
    fs::remove(file, failed);
    if (failed) {
        error = "cannot remove " + file.string() + ": " + failed.message();
        return false;
    }
    return true;
}

} // namespace opensu::retro

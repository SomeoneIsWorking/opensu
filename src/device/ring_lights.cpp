#include "ring_lights.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

namespace opensu::device {
namespace {

constexpr std::string_view suffix{":rgb:joystick_rings"};
/// The largest packed 0xRRGGBB a zone must take to carry any colour.
constexpr long long packedWhite = 0xFFFFFF;

std::vector<std::string> wordsOf(const std::filesystem::path& file) {
    std::ifstream in{file};
    std::vector<std::string> words;
    for (std::string word; in >> word;) {
        words.push_back(word);
    }
    return words;
}

/// The first number in `words`, or `fallback` when there is none.
long long firstNumber(const std::vector<std::string>& words, long long fallback) {
    long long value = fallback;
    if (!words.empty()) {
        std::from_chars(words.front().data(), words.front().data() + words.front().size(), value);
    }
    return value;
}

std::string write(const std::filesystem::path& file, const std::string& text) {
    std::ofstream out{file};
    out << text;
    out.close();
    return out ? std::string{} : "could not write " + file.string();
}

} // namespace

std::optional<RingLights> RingLights::find(const std::filesystem::path& leds) {
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator{leds, error}) {
        const std::string name = entry.path().filename().string();
        if (!name.ends_with(suffix)) {
            continue;
        }
        const std::vector<std::string> index = wordsOf(entry.path() / "multi_index");
        const int maxBrightness = static_cast<int>(
            std::max(1LL, firstNumber(wordsOf(entry.path() / "max_brightness"), 255)));
        // Without the file the LED class does not cap a zone.
        const long long zoneMax =
            firstNumber(wordsOf(entry.path() / "multi_max_intensity"), packedWhite);
        if (!index.empty() && std::ranges::all_of(index, [](const std::string& colour) {
                return colour == "rgb";
            })) {
            return RingLights{entry.path(), index.size(), true, maxBrightness,
                              zoneMax >= packedWhite};
        }
        if (index == std::vector<std::string>{"red", "green", "blue"}) {
            return RingLights{entry.path(), 1, false, maxBrightness, true};
        }
    }
    return std::nullopt;
}

std::string RingLights::set(RingColour colour, int percent) const {
    std::ostringstream intensity;
    if (packed_) {
        const unsigned value = (unsigned{colour.red} << 16U) | (unsigned{colour.green} << 8U) |
                               unsigned{colour.blue};
        for (std::size_t zone = 0; zone < zones_; ++zone) {
            intensity << (zone == 0 ? "" : " ") << value;
        }
    } else {
        intensity << unsigned{colour.red} << ' ' << unsigned{colour.green} << ' '
                  << unsigned{colour.blue};
    }
    if (std::string refused = write(dir_ / "multi_intensity", intensity.str()); !refused.empty()) {
        return refused;
    }
    const int level = std::clamp(percent, 0, 100) * maxBrightness_ / 100;
    return write(dir_ / "brightness", std::to_string(level));
}

} // namespace opensu::device

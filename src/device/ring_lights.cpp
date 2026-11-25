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

std::vector<std::string> wordsOf(const std::filesystem::path& file) {
    std::ifstream in{file};
    std::vector<std::string> words;
    for (std::string word; in >> word;) {
        words.push_back(word);
    }
    return words;
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
        const std::vector<std::string> max = wordsOf(entry.path() / "max_brightness");
        int maxBrightness = 255;
        if (!max.empty()) {
            std::from_chars(max.front().data(), max.front().data() + max.front().size(),
                            maxBrightness);
        }
        maxBrightness = std::max(1, maxBrightness);
        if (!index.empty() && std::ranges::all_of(index, [](const std::string& colour) {
                return colour == "rgb";
            })) {
            return RingLights{entry.path(), index.size(), true, maxBrightness};
        }
        if (index == std::vector<std::string>{"red", "green", "blue"}) {
            return RingLights{entry.path(), 1, false, maxBrightness};
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

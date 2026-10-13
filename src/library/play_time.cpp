#include "play_time.hpp"

namespace opensu::library {

void PlayTimeReports::set(const std::string& source, Report report) {
    sources_[source] = std::move(report);
}

void PlayTimeReports::erase(const std::string& source) {
    sources_.erase(source);
}

void PlayTimeReports::apply(std::vector<Game>& games) const {
    constexpr std::int64_t secondsPerMinute = 60;
    for (Game& game : games) {
        if (game.source != Source::Rom) {
            continue;
        }
        std::int64_t seconds = 0;
        for (const auto& [source, report] : sources_) {
            if (const auto found = report.find(game.id); found != report.end()) {
                seconds += found->second;
            }
        }
        game.playtimeMinutes = static_cast<int>(seconds / secondsPerMinute);
    }
}

} // namespace opensu::library

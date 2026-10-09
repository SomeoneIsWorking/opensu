#include "audio_outputs.hpp"

#include <algorithm>

namespace opensu::app {

void AudioOutputs::refresh() {
    sinks_ = volume_.sinks();
}

const audio::AudioSink* AudioOutputs::current() const {
    const auto found = std::ranges::find(sinks_, true, &audio::AudioSink::isDefault);
    return found == sinks_.end() ? nullptr : &*found;
}

std::string AudioOutputs::choose(const std::string& id) {
    std::string refused = volume_.setDefaultSink(id);
    refresh();
    return refused;
}

std::string AudioOutputs::cycle(int step) {
    if (sinks_.size() < 2) {
        return "there is no other output";
    }
    const auto count = static_cast<int>(sinks_.size());
    const auto now = std::ranges::find(sinks_, true, &audio::AudioSink::isDefault);
    const int from = now == sinks_.end() ? 0 : static_cast<int>(now - sinks_.begin());
    const int to = (from + (step == 0 ? 1 : step) + count) % count;
    return choose(sinks_[static_cast<std::size_t>(to)].id);
}

} // namespace opensu::app

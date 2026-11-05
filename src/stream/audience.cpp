#include "audience.hpp"

#include <algorithm>
#include <iterator>

namespace opensu::stream {

void Audience::add(HostStream& stream) {
    const std::lock_guard lock{mutex_};
    streams_.push_back(&stream);
}

void Audience::remove(const HostStream& stream) {
    const std::lock_guard lock{mutex_};
    std::erase(streams_, &stream);
}

void Audience::visit(const std::function<void(std::span<HostStream* const>)>& visit) const {
    const std::lock_guard lock{mutex_};
    std::vector<HostStream*> live;
    live.reserve(streams_.size());
    std::ranges::copy_if(streams_, std::back_inserter(live), [](const HostStream* stream) {
        return stream->joined() && !stream->over();
    });
    visit(live);
}

} // namespace opensu::stream

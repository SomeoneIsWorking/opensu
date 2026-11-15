#include "remote_frame_gen.hpp"

namespace opensu::app {

bool RemoteFrameGen::built() noexcept {
    return false;
}

std::optional<std::string> RemoteFrameGen::pack(const std::filesystem::path& /*dll*/,
                                                std::string& why) {
    why = "this build has no frame generation";
    return std::nullopt;
}

std::unique_ptr<RemoteFrameGen> RemoteFrameGen::open(const settings::FrameGenSettings& /*settings*/,
                                                     std::string_view /*packed*/,
                                                     stream::GuestVideo& /*video*/,
                                                     std::string& why) {
    why = "this build has no frame generation";
    return nullptr;
}

} // namespace opensu::app

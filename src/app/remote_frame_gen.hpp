// remote_frame_gen — frame generation on the remote play guest: the host's frames taken on a
// thread of its own as each is decoded, a picture for each present out, the frames between two real
// ones made by openSU's generator or the Gamescope fork's framegen library (the LSFG 3 shaders of
// the player's Lossless.dll, which their host sends, `pack`) and chosen for each present by its
// pacer (`framegen::CFramePacer`). Stage A of docs/plans/remote-frame-generation.md: a Vulkan
// device of its own, each real frame's RGBA uploaded and each generated one read back. Built where
// framegen is (Linux); elsewhere `open` says why not.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "settings/framegen_settings.hpp"
#include "stream/guest_video.hpp"

namespace opensu::app {

class RemoteFrameGen {
  public:
    /// The LSFG 3 modules of the player's `dll` in the form `open` takes, for the host to send
    /// its guest; nothing with why when it cannot be read or this build has no frame generation.
    [[nodiscard]] static std::optional<std::string> pack(const std::filesystem::path& dll,
                                                         std::string& why);
    /// A generator for `settings` taking `video`'s frames, which must outlive it: openSU's, or
    /// Lossless's from the modules `packed`; nothing with why when this build, the GPU or the
    /// modules cannot generate.
    [[nodiscard]] static std::unique_ptr<RemoteFrameGen>
    open(const settings::FrameGenSettings& settings, std::string_view packed,
         stream::GuestVideo& video, std::string& why);

    /// Whether this build generates frames at all.
    [[nodiscard]] static bool built() noexcept;

    RemoteFrameGen() = default;
    virtual ~RemoteFrameGen() = default;
    RemoteFrameGen(const RemoteFrameGen&) = delete;
    RemoteFrameGen& operator=(const RemoteFrameGen&) = delete;
    RemoteFrameGen(RemoteFrameGen&&) = delete;
    RemoteFrameGen& operator=(RemoteFrameGen&&) = delete;

    /// The picture the present at `nowUs` shows, on a display refreshing every `refreshNs`: a real
    /// frame, or one generated between two, which carries the number of the later; nothing keeps
    /// the one shown. Real frames in CPU RGBA have their rate measured by `Frame::capturedUs` (on
    /// `wire::nowMicros`'s clock) and are placed by when they reach the generator.
    [[nodiscard]] virtual std::optional<stream::GuestVideo::Frame>
    present(std::uint64_t nowUs, std::uint64_t refreshNs) = 0;
    /// Whether generation failed, with why logged; the frames should then be shown as they come.
    [[nodiscard]] virtual bool failed() const = 0;
    /// Real frames in, pictures shown and how many of them generated, the outputs the pacer
    /// dropped and its state, as one line for the control channel.
    [[nodiscard]] virtual std::string summary() const = 0;
};

} // namespace opensu::app

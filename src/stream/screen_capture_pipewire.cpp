#include "screen_capture.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include <pipewire/pipewire.h>
#include <spa/param/buffers.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>

#include "lucent/log.h"
#include "pipewire_session.hpp"
#include "stream_wire.hpp"

namespace opensu::stream {
namespace {

/// Gamescope's format key for the size a consumer wants (`SPA_FORMAT_VIDEO_requested_size`).
constexpr std::uint32_t requestedSizeKey = 0x70000;
/// The openSU fork's key for a size of the consumer's own shape.
constexpr std::uint32_t exactSizeKey = 0x70002;

/// The process at the other end of the Unix socket `path`.
pid_t peerOf(const std::filesystem::path& path) {
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const std::string name = path.string();
    if (name.size() >= sizeof(address.sun_path)) {
        throw std::runtime_error{"Gamescope's socket path is too long: " + name};
    }
    std::memcpy(address.sun_path, name.c_str(), name.size() + 1);
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        throw std::system_error{errno, std::generic_category(), "socket"};
    }
    ucred peer{};
    socklen_t size = sizeof(peer);
    const bool known =
        ::connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0 &&
        ::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &size) == 0;
    const int failure = errno;
    ::close(fd);
    if (!known) {
        throw std::runtime_error{"cannot reach Gamescope at " + name + ": " +
                                 std::generic_category().message(failure)};
    }
    return peer.pid;
}

/// `key` in `props`, or empty.
std::string_view property(const spa_dict* props, const char* key) {
    const char* value = props != nullptr ? spa_dict_lookup(props, key) : nullptr;
    return value != nullptr ? std::string_view{value} : std::string_view{};
}

std::optional<std::uint64_t> number(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        value = value * 10 + static_cast<std::uint64_t>(c - '0');
    }
    return value;
}

} // namespace

class ScreenCapture::Stream {
  public:
    explicit Stream(Setup setup)
        : setup_{std::move(setup)}, gamescope_{peerOf(setup_.gamescopeSocket)},
          session_{"opensu-capture"} {
        const PipeWireSession::Lock lock{session_};
        try {
            connect(lock);
        } catch (...) {
            drop();
            throw;
        }
    }

    ~Stream() {
        const PipeWireSession::Lock lock{session_};
        drop();
    }

    void fit(PictureSize size, bool exact) {
        const PipeWireSession::Lock lock{session_};
        if (size == requested_ && exact == exact_) {
            return;
        }
        requested_ = size;
        exact_ = exact;
        lucent::info("capture", "asking Gamescope for {}x{}", size.width, size.height);
        pw_stream_destroy(stream_);
        stream_ = nullptr;
        openStream();
    }
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;
    Stream(Stream&&) = delete;
    Stream& operator=(Stream&&) = delete;

  private:
    struct Node {
        std::uint32_t client{0};
        std::string serial;
    };

    /// Lists PipeWire's objects, finds Gamescope's screen among them and connects to it.
    void connect(const PipeWireSession::Lock& lock) {
        registry_ = pw_core_get_registry(session_.core(), PW_VERSION_REGISTRY, 0);
        pw_registry_add_listener(registry_, &registryListener_, &registryEvents, this);
        session_.settle(lock);
        serial_ = gamescopeNode(gamescope_);
        openStream();
    }

    /// The serial of the video source Gamescope's process `gamescope` publishes.
    [[nodiscard]] std::string gamescopeNode(pid_t gamescope) const {
        for (const auto& [id, node] : nodes_) {
            const auto client = clients_.find(node.client);
            if (client != clients_.end() && client->second == gamescope) {
                lucent::info("capture", "Gamescope {} streams on PipeWire node {}", gamescope, id);
                return node.serial;
            }
        }
        throw std::runtime_error{"Gamescope (process " + std::to_string(gamescope) +
                                 ") offers no screen on PipeWire"};
    }

    void openStream() {
        pw_properties* props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY,
                                                 "Capture", PW_KEY_MEDIA_ROLE, "Screen",
                                                 PW_KEY_TARGET_OBJECT, serial_.c_str(), nullptr);
        stream_ = pw_stream_new(session_.core(), "openSU remote play", props);
        if (stream_ == nullptr) {
            throw std::runtime_error{"cannot make a PipeWire stream"};
        }
        pw_stream_add_listener(stream_, &streamListener_, &streamEvents, this);

        std::array<std::uint8_t, 1024> buffer{};
        spa_pod_builder builder{};
        spa_pod_builder_init(&builder, buffer.data(), buffer.size());
        spa_pod_frame object{};
        spa_pod_builder_push_object(&builder, &object, SPA_TYPE_OBJECT_Format,
                                    SPA_PARAM_EnumFormat);
        const auto id = [&builder](std::uint32_t key, std::uint32_t value) {
            spa_pod_builder_prop(&builder, key, 0);
            spa_pod_builder_id(&builder, value);
        };
        id(SPA_FORMAT_mediaType, SPA_MEDIA_TYPE_video);
        id(SPA_FORMAT_mediaSubtype, SPA_MEDIA_SUBTYPE_raw);
        id(SPA_FORMAT_VIDEO_format, SPA_VIDEO_FORMAT_NV12);
        // Any size and rate Gamescope gives: the range's default, then its ends.
        spa_pod_frame choice{};
        spa_pod_builder_prop(&builder, SPA_FORMAT_VIDEO_size, 0);
        spa_pod_builder_push_choice(&builder, &choice, SPA_CHOICE_Range, 0);
        spa_pod_builder_rectangle(&builder, 1280, 720);
        spa_pod_builder_rectangle(&builder, 2, 2);
        spa_pod_builder_rectangle(&builder, 8192, 8192);
        spa_pod_builder_pop(&builder, &choice);
        spa_pod_builder_prop(&builder, SPA_FORMAT_VIDEO_framerate, 0);
        spa_pod_builder_push_choice(&builder, &choice, SPA_CHOICE_Range, 0);
        spa_pod_builder_fraction(&builder, 0, 1);
        spa_pod_builder_fraction(&builder, 0, 1);
        spa_pod_builder_fraction(&builder, 1000, 1);
        spa_pod_builder_pop(&builder, &choice);
        if (requested_.width > 0 && requested_.height > 0) {
            spa_pod_builder_prop(&builder, exact_ ? exactSizeKey : requestedSizeKey, 0);
            spa_pod_builder_rectangle(&builder, static_cast<std::uint32_t>(requested_.width),
                                      static_cast<std::uint32_t>(requested_.height));
        }
        id(SPA_FORMAT_VIDEO_colorMatrix, SPA_VIDEO_COLOR_MATRIX_BT709);
        id(SPA_FORMAT_VIDEO_colorRange, SPA_VIDEO_COLOR_RANGE_0_255);
        std::array<const spa_pod*, 1> params{
            static_cast<const spa_pod*>(spa_pod_builder_pop(&builder, &object))};
        PipeWireSession::connectInput(stream_, params, "cannot connect to Gamescope's stream");
    }

    /// Lets go of the stream and the registry; under the loop's lock.
    void drop() noexcept {
        if (stream_ != nullptr) {
            pw_stream_destroy(stream_);
            stream_ = nullptr;
        }
        if (registry_ != nullptr) {
            pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry_));
            registry_ = nullptr;
        }
    }

    static void onGlobal(void* data, std::uint32_t id, std::uint32_t /*permissions*/,
                         const char* type, std::uint32_t /*version*/, const spa_dict* props) {
        auto& self = *static_cast<Stream*>(data);
        const std::string_view kind{type};
        if (kind == PW_TYPE_INTERFACE_Client) {
            if (const std::optional<std::uint64_t> pid = number(property(props, PW_KEY_SEC_PID))) {
                self.clients_[id] = static_cast<pid_t>(*pid);
            }
        } else if (kind == PW_TYPE_INTERFACE_Node &&
                   property(props, PW_KEY_MEDIA_CLASS) == "Video/Source") {
            const std::optional<std::uint64_t> client = number(property(props, PW_KEY_CLIENT_ID));
            const std::string_view serial = property(props, PW_KEY_OBJECT_SERIAL);
            if (client && !serial.empty()) {
                self.nodes_[id] = Node{.client = static_cast<std::uint32_t>(*client),
                                       .serial = std::string{serial}};
            }
        }
    }

    static void onParamChanged(void* data, std::uint32_t id, const spa_pod* param) {
        auto& self = *static_cast<Stream*>(data);
        if (param == nullptr || id != SPA_PARAM_Format) {
            return;
        }
        spa_video_info_raw format{};
        if (spa_format_video_raw_parse(param, &format) < 0) {
            lucent::warn("capture", "Gamescope's stream has a format this cannot read");
            return;
        }
        self.format_ = format;
        lucent::info("capture", "Gamescope streams {}x{} NV12 (matrix {}, range {})",
                     format.size.width, format.size.height, static_cast<int>(format.color_matrix),
                     static_cast<int>(format.color_range));
        std::array<std::uint8_t, 256> buffer{};
        spa_pod_builder builder{};
        spa_pod_builder_init(&builder, buffer.data(), buffer.size());
        spa_pod_frame object{};
        spa_pod_builder_push_object(&builder, &object, SPA_TYPE_OBJECT_ParamBuffers,
                                    SPA_PARAM_Buffers);
        spa_pod_builder_prop(&builder, SPA_PARAM_BUFFERS_dataType, 0);
        spa_pod_builder_int(&builder, (1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr));
        std::array<const spa_pod*, 1> params{
            static_cast<const spa_pod*>(spa_pod_builder_pop(&builder, &object))};
        pw_stream_update_params(self.stream_, params.data(), params.size());
    }

    static void onStateChanged(void* /*data*/, pw_stream_state /*old*/, pw_stream_state state,
                               const char* error) {
        lucent::info("capture", "Gamescope's stream is {}{}{}", pw_stream_state_as_string(state),
                     error != nullptr ? ": " : "", error != nullptr ? error : "");
    }

    static void onProcess(void* data) {
        auto& self = *static_cast<Stream*>(data);
        // Only the newest frame is wanted; older queued ones go straight back.
        pw_buffer* newest = nullptr;
        while (pw_buffer* next = pw_stream_dequeue_buffer(self.stream_)) {
            if (newest != nullptr) {
                pw_stream_queue_buffer(self.stream_, newest);
            }
            newest = next;
        }
        if (newest == nullptr) {
            return;
        }
        self.deliver(*newest->buffer);
        pw_stream_queue_buffer(self.stream_, newest);
    }

    void deliver(const spa_buffer& buffer) const {
        const std::uint64_t capturedUs = wire::nowMicros();
        if (buffer.n_datas < 1 || buffer.datas[0].data == nullptr ||
            buffer.datas[0].chunk == nullptr || !setup_.frame) {
            return;
        }
        const spa_data& plane = buffer.datas[0];
        const spa_chunk& chunk = *plane.chunk;
        if ((chunk.flags & SPA_CHUNK_FLAG_CORRUPTED) != 0 || chunk.stride <= 0) {
            return;
        }
        const auto width = static_cast<std::size_t>(format_.size.width);
        const auto height = static_cast<std::size_t>(format_.size.height);
        const auto stride = static_cast<std::size_t>(chunk.stride);
        const std::size_t lumaBytes = stride * height;
        const std::size_t chromaBytes = stride * ((height + 1) / 2);
        if (width < 2 || height < 2 || stride < width ||
            static_cast<std::size_t>(chunk.offset) + lumaBytes + chromaBytes > plane.maxsize) {
            return;
        }
        const auto* bytes = static_cast<const std::uint8_t*>(plane.data) + chunk.offset;
        setup_.frame(CapturedFrame{.width = static_cast<int>(width),
                                   .height = static_cast<int>(height),
                                   .stride = stride,
                                   .luma = {bytes, lumaBytes},
                                   .chroma = {bytes + lumaBytes, chromaBytes},
                                   .capturedUs = capturedUs});
    }

    static constexpr pw_registry_events registryEvents{
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = &Stream::onGlobal,
        .global_remove = nullptr,
    };
    static constexpr pw_stream_events streamEvents{
        .version = PW_VERSION_STREAM_EVENTS,
        .destroy = nullptr,
        .state_changed = &Stream::onStateChanged,
        .control_info = nullptr,
        .io_changed = nullptr,
        .param_changed = &Stream::onParamChanged,
        .add_buffer = nullptr,
        .remove_buffer = nullptr,
        .process = &Stream::onProcess,
        .drained = nullptr,
        .command = nullptr,
        .trigger_done = nullptr,
    };

    Setup setup_;
    pid_t gamescope_;
    PipeWireSession session_;
    pw_registry* registry_{};
    pw_stream* stream_{};
    spa_hook registryListener_{};
    spa_hook streamListener_{};
    /// What the registry listed: each client's process, and the video sources by node id.
    std::map<std::uint32_t, pid_t> clients_;
    std::map<std::uint32_t, Node> nodes_;
    spa_video_info_raw format_{};
    /// Gamescope's node, and the size asked of it.
    std::string serial_;
    PictureSize requested_;
    bool exact_{false};
};

std::string ScreenCapture::missing() {
    return {};
}

ScreenCapture::ScreenCapture(Setup setup) : stream_{std::make_unique<Stream>(std::move(setup))} {
}

ScreenCapture::~ScreenCapture() = default;

void ScreenCapture::fit(PictureSize size) {
    stream_->fit(size, false);
}

void ScreenCapture::fitExactly(PictureSize size) {
    stream_->fit(size, true);
}

} // namespace opensu::stream

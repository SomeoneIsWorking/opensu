#include <array>
#include <stdexcept>
#include <utility>

#include <spa/param/audio/raw-utils.h>

#include "game_sound.hpp"
#include "lucent/log.h"
#include "pipewire_session.hpp"
#include "stream_wire.hpp"

namespace opensu::stream {

class GameSound::Sink {
  public:
    Sink(const std::string& name,
         std::function<void(std::span<const float>, std::uint64_t)> samples)
        : samples_{std::move(samples)}, session_{"opensu-sound"} {
        const PipeWireSession::Lock lock{session_};
        try {
            open(name, lock);
        } catch (...) {
            drop();
            throw;
        }
    }
    ~Sink() {
        const PipeWireSession::Lock lock{session_};
        drop();
    }
    Sink(const Sink&) = delete;
    Sink& operator=(const Sink&) = delete;
    Sink(Sink&&) = delete;
    Sink& operator=(Sink&&) = delete;

  private:
    void open(const std::string& name, const PipeWireSession::Lock& lock) {
        if (!name.empty()) {
            pw_properties* sinkProps =
                pw_properties_new("factory.name", "support.null-audio-sink", PW_KEY_NODE_NAME,
                                  name.c_str(), PW_KEY_NODE_DESCRIPTION, "openSU remote play",
                                  PW_KEY_MEDIA_CLASS, "Audio/Sink", "audio.position", "FL,FR",
                                  "audio.rate", "48000", "object.linger", "false", nullptr);
            sink_ = static_cast<pw_proxy*>(
                pw_core_create_object(session_.core(), "adapter", PW_TYPE_INTERFACE_Node,
                                      PW_VERSION_NODE, &sinkProps->dict, 0));
            pw_properties_free(sinkProps);
            if (sink_ == nullptr) {
                throw std::runtime_error{"cannot make the game's sound a sink of its own"};
            }
            session_.settle(lock);
        }

        // Without a target the session manager links the capture to the default output's monitor.
        pw_properties* props = pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE, "Game",
            PW_KEY_STREAM_CAPTURE_SINK, "true", PW_KEY_NODE_LATENCY, "480/48000", nullptr);
        if (!name.empty()) {
            pw_properties_set(props, PW_KEY_TARGET_OBJECT, name.c_str());
        }
        stream_ = pw_stream_new(session_.core(), "openSU remote play sound", props);
        if (stream_ == nullptr) {
            throw std::runtime_error{"cannot listen to the game's sound"};
        }
        static constexpr pw_stream_events events{
            .version = PW_VERSION_STREAM_EVENTS,
            .destroy = nullptr,
            .state_changed = &Sink::onStateChanged,
            .control_info = nullptr,
            .io_changed = nullptr,
            .param_changed = nullptr,
            .add_buffer = nullptr,
            .remove_buffer = nullptr,
            .process = &Sink::onProcess,
            .drained = nullptr,
            .command = nullptr,
            .trigger_done = nullptr,
        };
        pw_stream_add_listener(stream_, &listener_, &events, this);
        std::array<std::uint8_t, 1024> buffer{};
        spa_pod_builder builder{};
        spa_pod_builder_init(&builder, buffer.data(), buffer.size());
        spa_audio_info_raw info{};
        info.format = SPA_AUDIO_FORMAT_F32;
        info.rate = 48000;
        info.channels = 2;
        info.position[0] = SPA_AUDIO_CHANNEL_FL;
        info.position[1] = SPA_AUDIO_CHANNEL_FR;
        std::array<const spa_pod*, 1> params{
            spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};
        PipeWireSession::connectInput(stream_, params, "cannot listen to the game's sound");
        if (name.empty()) {
            lucent::info("sound", "the game's sound is what this computer's output plays");
        } else {
            lucent::info("sound", "the game plays into PipeWire sink {}", name);
        }
    }

    void drop() noexcept {
        if (stream_ != nullptr) {
            pw_stream_destroy(stream_);
            stream_ = nullptr;
        }
        if (sink_ != nullptr) {
            pw_proxy_destroy(sink_);
            sink_ = nullptr;
        }
    }

    static void onStateChanged(void* /*data*/, pw_stream_state /*old*/, pw_stream_state state,
                               const char* error) {
        lucent::info("sound", "the game's sound is {}{}{}", pw_stream_state_as_string(state),
                     error != nullptr ? ": " : "", error != nullptr ? error : "");
    }

    static void onProcess(void* data) {
        auto& self = *static_cast<Sink*>(data);
        pw_buffer* buffer = pw_stream_dequeue_buffer(self.stream_);
        if (buffer == nullptr) {
            return;
        }
        const spa_data& plane = buffer->buffer->datas[0];
        if (plane.data != nullptr && plane.chunk != nullptr && self.samples_) {
            const std::uint32_t offset = std::min(plane.chunk->offset, plane.maxsize);
            const std::uint32_t size = std::min(plane.chunk->size, plane.maxsize - offset);
            self.samples_({reinterpret_cast<const float*>(
                               static_cast<const std::uint8_t*>(plane.data) + offset),
                           size / sizeof(float)},
                          wire::nowMicros());
        }
        pw_stream_queue_buffer(self.stream_, buffer);
    }

    std::function<void(std::span<const float>, std::uint64_t)> samples_;
    PipeWireSession session_;
    pw_proxy* sink_{};
    pw_stream* stream_{};
    spa_hook listener_{};
};

GameSound::GameSound(Setup setup)
    : sinkName_{std::move(setup.sinkName)},
      sink_{std::make_unique<Sink>(sinkName_, std::move(setup.samples))} {
}

GameSound::~GameSound() = default;

std::vector<std::string> GameSound::environment() const {
    if (sinkName_.empty()) {
        return {};
    }
    return {"PIPEWIRE_NODE=" + sinkName_, "PULSE_SINK=" + sinkName_};
}

} // namespace opensu::stream

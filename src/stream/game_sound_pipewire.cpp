#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>

#include <pipewire/extensions/metadata.h>
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

    void adopt(std::vector<std::int32_t> processes) {
        const PipeWireSession::Lock lock{session_};
        processes_ = std::move(processes);
        moveStreams();
    }

  private:
    void open(const std::string& name, const PipeWireSession::Lock& lock) {
        if (!name.empty()) {
            sinkName_ = name;
            registry_ = pw_core_get_registry(session_.core(), PW_VERSION_REGISTRY, 0);
            pw_registry_add_listener(registry_, &registryListener_, &registryEvents, this);
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
        for (auto& [id, output] : outputs_) {
            pw_proxy_destroy(output.node);
        }
        outputs_.clear();
        if (metadata_ != nullptr) {
            pw_proxy_destroy(reinterpret_cast<pw_proxy*>(metadata_));
            metadata_ = nullptr;
        }
        if (registry_ != nullptr) {
            pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry_));
            registry_ = nullptr;
        }
        if (stream_ != nullptr) {
            pw_stream_destroy(stream_);
            stream_ = nullptr;
        }
        if (sink_ != nullptr) {
            pw_proxy_destroy(sink_);
            sink_ = nullptr;
        }
    }

    /// Points each stream of the adopted processes at the sink, once.
    void moveStreams() {
        if (metadata_ == nullptr || sinkSerial_.empty()) {
            return;
        }
        for (auto& [node, stream] : outputs_) {
            if (!stream.moved &&
                std::ranges::find(processes_, stream.process) != processes_.end()) {
                stream.moved = true;
                lucent::info("sound", "the game's stream {} goes to the remote sink", node);
                pw_metadata_set_property(metadata_, node, "target.object", "Spa:Id",
                                         sinkSerial_.c_str());
            }
        }
    }

    static void onGlobal(void* data, std::uint32_t id, std::uint32_t /*permissions*/,
                         const char* type, std::uint32_t /*version*/, const spa_dict* props) {
        auto* self = static_cast<Sink*>(data);
        const auto value = [props](const char* key) {
            const char* found = props != nullptr ? spa_dict_lookup(props, key) : nullptr;
            return std::string_view{found != nullptr ? found : ""};
        };
        if (std::string_view{type} == PW_TYPE_INTERFACE_Metadata &&
            value(PW_KEY_METADATA_NAME) == "default" && self->metadata_ == nullptr) {
            self->metadata_ = static_cast<pw_metadata*>(
                pw_registry_bind(self->registry_, id, type, PW_VERSION_METADATA, 0));
        } else if (std::string_view{type} == PW_TYPE_INTERFACE_Node) {
            if (value(PW_KEY_NODE_NAME) == self->sinkName_) {
                self->sinkSerial_ = value(PW_KEY_OBJECT_SERIAL);
            } else if (value(PW_KEY_MEDIA_CLASS) == "Stream/Output/Audio") {
                self->watchOutput(id, type);
            }
        }
        self->moveStreams();
    }

    /// The process of a stream is only in the node's own info, never in the registry's.
    void watchOutput(std::uint32_t id, const char* type) {
        const auto [found, added] = outputs_.try_emplace(id);
        if (!added) {
            return;
        }
        Output& output = found->second;
        output.sink = this;
        output.node =
            static_cast<pw_proxy*>(pw_registry_bind(registry_, id, type, PW_VERSION_NODE, 0));
        pw_node_add_listener(reinterpret_cast<pw_node*>(output.node), &output.listener, &nodeEvents,
                             &output);
    }

    static void onNodeInfo(void* data, const pw_node_info* info) {
        auto& output = *static_cast<Output*>(data);
        const char* text =
            info->props != nullptr ? spa_dict_lookup(info->props, PW_KEY_APP_PROCESS_ID) : nullptr;
        if (text == nullptr) {
            return;
        }
        const std::string_view digits{text};
        std::from_chars(digits.data(), digits.data() + digits.size(), output.process);
        output.sink->moveStreams();
    }

    static void onGlobalRemove(void* data, std::uint32_t id) {
        auto& outputs = static_cast<Sink*>(data)->outputs_;
        if (const auto found = outputs.find(id); found != outputs.end()) {
            pw_proxy_destroy(found->second.node);
            outputs.erase(found);
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
    struct Output {
        Sink* sink{nullptr};
        pw_proxy* node{nullptr};
        spa_hook listener{};
        std::int32_t process{0};
        bool moved{false};
    };

    static constexpr pw_node_events nodeEvents{
        .version = PW_VERSION_NODE_EVENTS,
        .info = &Sink::onNodeInfo,
        .param = nullptr,
    };

    static constexpr pw_registry_events registryEvents{
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = &Sink::onGlobal,
        .global_remove = &Sink::onGlobalRemove,
    };

    std::string sinkName_;
    std::string sinkSerial_;
    std::map<std::uint32_t, Output> outputs_;
    std::vector<std::int32_t> processes_;
    pw_registry* registry_{};
    pw_metadata* metadata_{};
    spa_hook registryListener_{};
    pw_proxy* sink_{};
    pw_stream* stream_{};
    spa_hook listener_{};
};

GameSound::GameSound(Setup setup)
    : sinkName_{std::move(setup.sinkName)},
      sink_{std::make_unique<Sink>(sinkName_, std::move(setup.samples))} {
}

GameSound::~GameSound() = default;

void GameSound::adopt(std::vector<std::int32_t> processes) {
    sink_->adopt(std::move(processes));
}

std::vector<std::string> GameSound::environment() const {
    if (sinkName_.empty()) {
        return {};
    }
    return {"PIPEWIRE_NODE=" + sinkName_, "PULSE_SINK=" + sinkName_};
}

} // namespace opensu::stream

// core — one libretro core loaded with dlopen: its retro_* symbols, init, load, run and unload in
// order, and the callbacks that hand its frames, samples and input queries to the runner's owners.
#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <libretro.h>

#include "audio_sink.hpp"
#include "core_environment.hpp"
#include "joypad.hpp"
#include "video_frame.hpp"

namespace opensu::retro {

/// A core or game that cannot be loaded; the message says why.
class CoreError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

/// Where the core's callbacks go.
struct CoreHost {
    CoreEnvironment& environment;
    VideoFrame& video;
    AudioSink& audio;
    const Joypad& joypad;
};

class Core {
  public:
    /// Opens the library and initialises the core. Throws CoreError when the library or a symbol
    /// is missing, its API version differs, or another core is loaded in this process.
    Core(const std::filesystem::path& library, CoreHost host);
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;
    Core(Core&&) = delete;
    Core& operator=(Core&&) = delete;
    /// Unloads the game, deinitialises the core and closes the library.
    ~Core();

    /// Loads the game; throws CoreError when the file cannot be read or the core refuses it.
    void loadGame(const std::filesystem::path& rom);

    /// One frame.
    void run();

    /// The core's save RAM; empty when it has none.
    [[nodiscard]] std::span<std::byte> saveRam();

    /// The game's geometry and timing as the core reported them at load.
    [[nodiscard]] const retro_system_av_info& avInfo() const {
        return avInfo_;
    }
    [[nodiscard]] const std::string& name() const {
        return name_;
    }

    // The callbacks, reached from the trampolines in core.cpp.
    bool environment(unsigned command, void* data);
    void video(const void* data, unsigned width, unsigned height, std::size_t pitch);
    void audio(std::span<const std::int16_t> interleaved);
    [[nodiscard]] std::int16_t input(unsigned port, unsigned device, unsigned id) const;

  private:
    struct Api {
        void (*init)();
        void (*deinit)();
        unsigned (*apiVersion)();
        void (*getSystemInfo)(retro_system_info*);
        void (*getSystemAvInfo)(retro_system_av_info*);
        void (*setEnvironment)(retro_environment_t);
        void (*setVideoRefresh)(retro_video_refresh_t);
        void (*setAudioSample)(retro_audio_sample_t);
        void (*setAudioSampleBatch)(retro_audio_sample_batch_t);
        void (*setInputPoll)(retro_input_poll_t);
        void (*setInputState)(retro_input_state_t);
        void (*setControllerPortDevice)(unsigned, unsigned);
        bool (*loadGame)(const retro_game_info*);
        void (*unloadGame)();
        void (*run)();
        void* (*getMemoryData)(unsigned);
        std::size_t (*getMemorySize)(unsigned);
    };

    void close();

    CoreHost host_;
    void* library_{nullptr};
    Api api_{};
    bool initialised_{false};
    bool loaded_{false};
    std::string name_;
    retro_system_av_info avInfo_{};
    /// The game's bytes, kept while it is loaded.
    std::vector<char> content_;
    std::string contentPath_;
};

} // namespace opensu::retro

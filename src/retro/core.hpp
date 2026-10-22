// core — one libretro core loaded as a `SharedLibrary`: its retro_* symbols, init, load, run and
// unload in order, and the callbacks that hand its frames, samples and input queries to the
// runner's owners.
#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <libretro.h>

#include "audio_sink.hpp"
#include "core_environment.hpp"
#include "joypad.hpp"
#include "shared_library.hpp"
#include "video_frame.hpp"

namespace opensu::retro {

/// A core or game that cannot be loaded; the message says why.
class CoreError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

/// One retro_input_state_t call: which control of which device on which port.
struct InputQuery {
    unsigned port{};
    unsigned device{};
    unsigned index{};
    unsigned id{};
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

    /// Unloads the game, if loaded; a GPU core's context is destroyed before this.
    void unloadGame();

    /// One frame.
    void run();

    /// Restarts the game, as the console's reset button.
    void reset();

    /// The game's whole state: empty when the core cannot serialize.
    [[nodiscard]] std::vector<std::byte> serialize();
    /// Puts back a state from serialize(); false when the core refuses it.
    [[nodiscard]] bool unserialize(std::span<const std::byte> state);

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
    [[nodiscard]] std::int16_t input(const InputQuery& query) const;

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
        void (*reset)();
        std::size_t (*serializeSize)();
        bool (*serialize)(void*, std::size_t);
        bool (*unserialize)(const void*, std::size_t);
        void* (*getMemoryData)(unsigned);
        std::size_t (*getMemorySize)(unsigned);
    };

    void close();

    CoreHost host_;
    std::unique_ptr<SharedLibrary> library_;
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

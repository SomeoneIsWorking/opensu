#include "core.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

#include "lucent/log.h"

namespace opensu::retro {
namespace {

// libretro's callbacks carry no context, so the trampolines reach the one loaded core here.
Core* active = nullptr;

bool environmentTrampoline(unsigned command, void* data) {
    return active->environment(command, data);
}

void videoTrampoline(const void* data, unsigned width, unsigned height, std::size_t pitch) {
    active->video(data, width, height, pitch);
}

void sampleTrampoline(std::int16_t left, std::int16_t right) {
    const std::int16_t frame[] = {left, right};
    active->audio(frame);
}

std::size_t batchTrampoline(const std::int16_t* data, std::size_t frames) {
    active->audio({data, frames * 2});
    return frames;
}

void pollTrampoline() {
}

std::int16_t inputTrampoline(unsigned port, unsigned device, unsigned index, unsigned id) {
    return active->input({.port = port, .device = device, .index = index, .id = id});
}

template <class Function> void bind(const SharedLibrary& library, const char* name, Function& out) {
    const SDL_FunctionPointer found = library.symbol(name);
    if (found == nullptr) {
        throw CoreError{std::string{"the core has no "} + name};
    }
    out = reinterpret_cast<Function>(found);
}

} // namespace

Core::Core(const std::filesystem::path& library, CoreHost host) : host_{host} {
    if (active != nullptr) {
        throw CoreError{"another core is already loaded"};
    }
    std::string reason;
    library_ = SharedLibrary::open(library, reason);
    if (library_ == nullptr) {
        throw CoreError{"cannot open " + library.string() + ": " + reason};
    }
    bind(*library_, "retro_init", api_.init);
    bind(*library_, "retro_deinit", api_.deinit);
    bind(*library_, "retro_api_version", api_.apiVersion);
    bind(*library_, "retro_get_system_info", api_.getSystemInfo);
    bind(*library_, "retro_get_system_av_info", api_.getSystemAvInfo);
    bind(*library_, "retro_set_environment", api_.setEnvironment);
    bind(*library_, "retro_set_video_refresh", api_.setVideoRefresh);
    bind(*library_, "retro_set_audio_sample", api_.setAudioSample);
    bind(*library_, "retro_set_audio_sample_batch", api_.setAudioSampleBatch);
    bind(*library_, "retro_set_input_poll", api_.setInputPoll);
    bind(*library_, "retro_set_input_state", api_.setInputState);
    bind(*library_, "retro_set_controller_port_device", api_.setControllerPortDevice);
    bind(*library_, "retro_load_game", api_.loadGame);
    bind(*library_, "retro_unload_game", api_.unloadGame);
    bind(*library_, "retro_run", api_.run);
    bind(*library_, "retro_reset", api_.reset);
    bind(*library_, "retro_serialize_size", api_.serializeSize);
    bind(*library_, "retro_serialize", api_.serialize);
    bind(*library_, "retro_unserialize", api_.unserialize);
    bind(*library_, "retro_get_memory_data", api_.getMemoryData);
    bind(*library_, "retro_get_memory_size", api_.getMemorySize);
    if (api_.apiVersion() != RETRO_API_VERSION) {
        throw CoreError{"the core speaks libretro API " + std::to_string(api_.apiVersion()) +
                        ", not " + std::to_string(RETRO_API_VERSION)};
    }
    active = this;
    retro_system_info info{};
    api_.getSystemInfo(&info);
    name_ = info.library_name != nullptr ? info.library_name : library.stem().string();
    lucent::info("retro", "core {} {}", name_,
                 info.library_version != nullptr ? info.library_version : "");
    api_.setEnvironment(environmentTrampoline);
    api_.init();
    initialised_ = true;
    // After init, as RetroArch does: Mesen keeps its video callback in an object init makes.
    api_.setVideoRefresh(videoTrampoline);
    api_.setAudioSample(sampleTrampoline);
    api_.setAudioSampleBatch(batchTrampoline);
    api_.setInputPoll(pollTrampoline);
    api_.setInputState(inputTrampoline);
}

Core::~Core() {
    close();
}

void Core::unloadGame() {
    if (loaded_) {
        api_.unloadGame();
        loaded_ = false;
    }
}

void Core::close() {
    unloadGame();
    if (initialised_) {
        api_.deinit();
        initialised_ = false;
    }
    library_.reset();
    active = nullptr;
}

void Core::loadGame(const std::filesystem::path& rom) {
    retro_system_info info{};
    api_.getSystemInfo(&info);
    contentPath_ = rom.string();
    retro_game_info game{};
    game.path = contentPath_.c_str();
    if (!info.need_fullpath) {
        std::ifstream in{rom, std::ios::binary};
        if (!in) {
            throw CoreError{"cannot read " + contentPath_};
        }
        content_.assign(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{});
        game.data = content_.data();
        game.size = content_.size();
    } else {
        std::error_code error;
        if (!std::filesystem::is_regular_file(rom, error)) {
            throw CoreError{"no game at " + contentPath_};
        }
    }
    if (!api_.loadGame(&game)) {
        throw CoreError{name_ + " cannot load " + contentPath_};
    }
    loaded_ = true;
    api_.setControllerPortDevice(0, RETRO_DEVICE_JOYPAD);
    api_.getSystemAvInfo(&avInfo_);
    lucent::info("retro", "{}x{} at {:.3f} fps, audio {:.0f} Hz", avInfo_.geometry.base_width,
                 avInfo_.geometry.base_height, avInfo_.timing.fps, avInfo_.timing.sample_rate);
}

void Core::run() {
    api_.run();
}

void Core::reset() {
    api_.reset();
}

std::vector<std::byte> Core::serialize() {
    std::vector<std::byte> state(api_.serializeSize());
    if (state.empty() || !api_.serialize(state.data(), state.size())) {
        return {};
    }
    return state;
}

bool Core::unserialize(std::span<const std::byte> state) {
    return !state.empty() && api_.unserialize(state.data(), state.size());
}

std::span<std::byte> Core::saveRam() {
    void* data = api_.getMemoryData(RETRO_MEMORY_SAVE_RAM);
    const std::size_t size = api_.getMemorySize(RETRO_MEMORY_SAVE_RAM);
    if (data == nullptr || size == 0) {
        return {};
    }
    return {static_cast<std::byte*>(data), size};
}

bool Core::environment(unsigned command, void* data) {
    return host_.environment.answer(command, data);
}

void Core::video(const void* data, unsigned width, unsigned height, std::size_t pitch) {
    if (data == RETRO_HW_FRAME_BUFFER_VALID) {
        host_.environment.hardware().frameReady(width, height);
        return;
    }
    host_.video.take(data, width, height, pitch, host_.environment.pixelFormat());
}

void Core::audio(std::span<const std::int16_t> interleaved) {
    host_.audio.push(interleaved);
}

std::int16_t Core::input(const InputQuery& query) const {
    if (query.port != 0) {
        return 0;
    }
    switch (query.device & RETRO_DEVICE_MASK) {
    case RETRO_DEVICE_JOYPAD:
        return host_.joypad.state(query.id);
    case RETRO_DEVICE_ANALOG:
        return host_.joypad.analog(query.index, query.id);
    default:
        return 0;
    }
}

} // namespace opensu::retro

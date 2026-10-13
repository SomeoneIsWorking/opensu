// A libretro core for the runner's tests: four colour bars in RGB565 (yellow when its option is not
// the default), a 440 Hz tone at 32 kHz, the joypad read each frame, and save RAM holding the frame
// count, the last joypad bits and the ROM's first byte. Its state is the save RAM and the frame
// counter; reset zeroes the frame count. Built with FAKE_CORE_CRASH it crashes in retro_run, with
// FAKE_CORE_NO_STATE it cannot serialize.
#include <array>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <numbers>

#include <libretro.h>

namespace {

constexpr unsigned width = 64;
constexpr unsigned height = 48;
constexpr double sampleRate = 32000.0;
constexpr double fps = 60.0;
constexpr std::uint16_t red = 0xF800;
constexpr std::uint16_t green = 0x07E0;
constexpr std::uint16_t blue = 0x001F;
constexpr std::uint16_t white = 0xFFFF;
constexpr std::uint16_t yellow = 0xFFE0;

struct FakeCore {
    retro_environment_t environment{nullptr};
    retro_video_refresh_t video{nullptr};
    retro_audio_sample_batch_t audio{nullptr};
    retro_input_poll_t poll{nullptr};
    retro_input_state_t input{nullptr};
    std::array<std::uint16_t, static_cast<std::size_t>(width) * height> pixels{};
    std::array<std::uint8_t, 256> saveRam{};
    std::array<std::int16_t, 1068> samples{};
    double phase{0.0};
    std::uint64_t frames{0};
};

FakeCore core;

constexpr std::size_t stateSize = sizeof(core.saveRam) + sizeof(core.frames);

retro_core_option_v2_definition definitions[] = {
    {"fake_pattern",
     "Pattern",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     {{"bars", nullptr}, {"yellow", nullptr}, {nullptr, nullptr}},
     "bars"},
    {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, {{nullptr, nullptr}}, nullptr},
};

retro_core_options_v2 options{nullptr, definitions};

void draw(bool bars) {
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            const std::uint16_t colours[] = {red, green, blue, white};
            core.pixels[static_cast<std::size_t>(y) * width + x] =
                bars ? colours[x / (width / 4)] : yellow;
        }
    }
}

} // namespace

extern "C" {

RETRO_API unsigned retro_api_version() {
    return RETRO_API_VERSION;
}

RETRO_API void retro_set_environment(retro_environment_t callback) {
    core.environment = callback;
    unsigned version = 0;
    if (callback(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && version >= 2) {
        callback(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &options);
    }
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t callback) {
    core.video = callback;
}

RETRO_API void retro_set_audio_sample(retro_audio_sample_t /*callback*/) {
}

RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t callback) {
    core.audio = callback;
}

RETRO_API void retro_set_input_poll(retro_input_poll_t callback) {
    core.poll = callback;
}

RETRO_API void retro_set_input_state(retro_input_state_t callback) {
    core.input = callback;
}

RETRO_API void retro_set_controller_port_device(unsigned /*port*/, unsigned /*device*/) {
}

RETRO_API void retro_init() {
}

RETRO_API void retro_deinit() {
}

RETRO_API void retro_get_system_info(retro_system_info* info) {
    std::memset(info, 0, sizeof(*info));
    info->library_name = "fake";
    info->library_version = "1";
    info->valid_extensions = "bin";
    info->need_fullpath = false;
}

RETRO_API void retro_get_system_av_info(retro_system_av_info* info) {
    std::memset(info, 0, sizeof(*info));
    info->geometry = {width, height, width, height, 4.0F / 3.0F};
    info->timing = {fps, sampleRate};
}

RETRO_API bool retro_load_game(const retro_game_info* game) {
    if (game == nullptr || game->data == nullptr || game->size == 0) {
        return false;
    }
    retro_pixel_format format = RETRO_PIXEL_FORMAT_RGB565;
    if (!core.environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format)) {
        return false;
    }
    retro_variable pattern{"fake_pattern", nullptr};
    const bool bars = core.environment(RETRO_ENVIRONMENT_GET_VARIABLE, &pattern) &&
                      pattern.value != nullptr && std::strcmp(pattern.value, "bars") == 0;
    draw(bars);
    core.saveRam[6] = static_cast<const std::uint8_t*>(game->data)[0];
    core.saveRam[7] = 0xA5;
    return true;
}

RETRO_API void retro_unload_game() {
}

RETRO_API void retro_run() {
#ifdef FAKE_CORE_CRASH
    std::raise(SIGSEGV);
#endif
    core.poll();
    const auto buttons = static_cast<std::uint16_t>(
        core.input(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK));
    core.saveRam[4] = static_cast<std::uint8_t>(buttons);
    core.saveRam[5] = static_cast<std::uint8_t>(buttons >> 8U);

    std::uint32_t count = 0;
    std::memcpy(&count, core.saveRam.data(), sizeof(count));
    ++count;
    std::memcpy(core.saveRam.data(), &count, sizeof(count));

    // Every other frame is a duplicate.
    core.video((core.frames % 2) == 0 ? core.pixels.data() : nullptr, width, height,
               width * sizeof(std::uint16_t));
    const auto frames = static_cast<std::size_t>(sampleRate / fps);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const auto value = static_cast<std::int16_t>(8000.0 * std::sin(core.phase));
        core.samples[frame * 2] = value;
        core.samples[frame * 2 + 1] = value;
        core.phase += 2.0 * std::numbers::pi * 440.0 / sampleRate;
    }
    core.audio(core.samples.data(), frames);
    ++core.frames;
}

RETRO_API void retro_reset() {
    std::memset(core.saveRam.data(), 0, sizeof(std::uint32_t));
    core.frames = 0;
}

RETRO_API std::size_t retro_serialize_size() {
#ifdef FAKE_CORE_NO_STATE
    return 0;
#else
    return stateSize;
#endif
}

RETRO_API bool retro_serialize(void* data, std::size_t size) {
    if (size < stateSize) {
        return false;
    }
    auto* out = static_cast<std::uint8_t*>(data);
    std::memcpy(out, core.saveRam.data(), core.saveRam.size());
    std::memcpy(out + core.saveRam.size(), &core.frames, sizeof(core.frames));
    return true;
}

RETRO_API bool retro_unserialize(const void* data, std::size_t size) {
    if (size != stateSize) {
        return false;
    }
    const auto* in = static_cast<const std::uint8_t*>(data);
    std::memcpy(core.saveRam.data(), in, core.saveRam.size());
    std::memcpy(&core.frames, in + core.saveRam.size(), sizeof(core.frames));
    return true;
}

RETRO_API void retro_cheat_reset() {
}

RETRO_API void retro_cheat_set(unsigned /*index*/, bool /*enabled*/, const char* /*code*/) {
}

RETRO_API bool retro_load_game_special(unsigned /*type*/, const retro_game_info* /*info*/,
                                       std::size_t /*count*/) {
    return false;
}

RETRO_API unsigned retro_get_region() {
    return RETRO_REGION_NTSC;
}

RETRO_API void* retro_get_memory_data(unsigned id) {
    return id == RETRO_MEMORY_SAVE_RAM ? core.saveRam.data() : nullptr;
}

RETRO_API std::size_t retro_get_memory_size(unsigned id) {
    return id == RETRO_MEMORY_SAVE_RAM ? core.saveRam.size() : 0;
}

} // extern "C"

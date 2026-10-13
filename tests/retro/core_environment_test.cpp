// The environment commands the runner answers, and the ones it refuses.
#include "core_environment.hpp"

#include <cstdio>
#include <cstring>

#include "check.hpp"

namespace {

using opensu::retro::CoreEnvironment;
using opensu::test::expect;

bool same(const char* a, const char* b) {
    return a != nullptr && b != nullptr && std::strcmp(a, b) == 0;
}

void directories() {
    CoreEnvironment environment{"/sys/dir", "/save/dir"};
    const char* path = nullptr;
    expect(environment.answer(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, static_cast<void*>(&path)) &&
               same(path, "/sys/dir"),
           "the system directory is answered");
    expect(environment.answer(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, static_cast<void*>(&path)) &&
               same(path, "/save/dir"),
           "the save directory is answered");
}

void pixelFormats() {
    CoreEnvironment environment{"", ""};
    expect(environment.pixelFormat() == RETRO_PIXEL_FORMAT_0RGB1555, "0RGB1555 by default");
    for (retro_pixel_format format :
         {RETRO_PIXEL_FORMAT_XRGB8888, RETRO_PIXEL_FORMAT_RGB565, RETRO_PIXEL_FORMAT_0RGB1555}) {
        expect(environment.answer(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format) &&
                   environment.pixelFormat() == format,
               "each of the three formats is taken");
    }
    retro_pixel_format unknown = RETRO_PIXEL_FORMAT_UNKNOWN;
    expect(!environment.answer(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &unknown) &&
               environment.pixelFormat() == RETRO_PIXEL_FORMAT_0RGB1555,
           "an unknown format is refused and changes nothing");
}

void flags() {
    CoreEnvironment environment{"", ""};
    bool value = false;
    expect(environment.answer(RETRO_ENVIRONMENT_GET_CAN_DUPE, &value) && value,
           "duplicate frames are allowed");
    value = true;
    expect(environment.answer(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &value) && !value,
           "options never change in a run");
    expect(environment.answer(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, nullptr),
           "input bitmasks are supported");
    unsigned version = 0;
    expect(environment.answer(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && version == 2,
           "core options version 2");
    retro_log_callback log{};
    expect(environment.answer(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log) && log.log != nullptr,
           "a log interface is given");
    log.log(RETRO_LOG_INFO, "core says %d\n", 42);
    retro_input_descriptor descriptors[] = {{0, 0, 0, 0, nullptr}};
    expect(environment.answer(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, descriptors),
           "input descriptors are accepted");
    expect(!environment.answer(RETRO_ENVIRONMENT_SET_HW_RENDER, nullptr), "GPU cores are refused");
    expect(!environment.answer(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, nullptr),
           "anything else is refused");
}

void variables() {
    CoreEnvironment environment{"", ""};
    retro_variable defined[] = {{"core_speed", "Speed; normal|fast|slow"},
                                {"core_broken", "no values"},
                                {nullptr, nullptr}};
    expect(environment.answer(RETRO_ENVIRONMENT_SET_VARIABLES, defined), "v0 variables are taken");
    retro_variable query{"core_speed", nullptr};
    expect(environment.answer(RETRO_ENVIRONMENT_GET_VARIABLE, &query) &&
               same(query.value, "normal"),
           "a v0 variable defaults to its first value");
    query = {"core_broken", nullptr};
    expect(!environment.answer(RETRO_ENVIRONMENT_GET_VARIABLE, &query) && query.value == nullptr,
           "a variable without values is not defined");
    query = {"core_unknown", nullptr};
    expect(!environment.answer(RETRO_ENVIRONMENT_GET_VARIABLE, &query),
           "an unknown key is refused");
}

void definitions() {
    CoreEnvironment environment{"", ""};
    retro_core_option_definition v1[] = {
        {"core_named", "Named", nullptr, {{"a", nullptr}, {"b", nullptr}, {nullptr, nullptr}}, "b"},
        {"core_first",
         "First",
         nullptr,
         {{"x", nullptr}, {"y", nullptr}, {nullptr, nullptr}},
         nullptr},
        {nullptr, nullptr, nullptr, {{nullptr, nullptr}}, nullptr},
    };
    expect(environment.answer(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, v1), "v1 options are taken");
    retro_variable query{"core_named", nullptr};
    expect(environment.answer(RETRO_ENVIRONMENT_GET_VARIABLE, &query) && same(query.value, "b"),
           "a v1 option takes its named default");
    query = {"core_first", nullptr};
    expect(environment.answer(RETRO_ENVIRONMENT_GET_VARIABLE, &query) && same(query.value, "x"),
           "a v1 option without a default takes its first value");

    retro_core_options_intl intl{v1, nullptr};
    expect(environment.answer(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &intl),
           "v1 international options are taken");
    query = {"core_named", nullptr};
    expect(environment.answer(RETRO_ENVIRONMENT_GET_VARIABLE, &query) && same(query.value, "b"),
           "international options use the US definitions");

    retro_core_option_v2_definition v2[] = {
        {"core_v2",
         "V2",
         nullptr,
         nullptr,
         nullptr,
         "video",
         {{"on", nullptr}, {"off", nullptr}, {nullptr, nullptr}},
         "off"},
        {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, {{nullptr, nullptr}}, nullptr},
    };
    retro_core_option_v2_category categories[] = {{"video", "Video", nullptr},
                                                  {nullptr, nullptr, nullptr}};
    retro_core_options_v2 options{categories, v2};
    expect(environment.answer(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &options),
           "v2 options are taken");
    query = {"core_v2", nullptr};
    expect(environment.answer(RETRO_ENVIRONMENT_GET_VARIABLE, &query) && same(query.value, "off"),
           "a v2 option takes its default");
    query = {"core_named", nullptr};
    expect(!environment.answer(RETRO_ENVIRONMENT_GET_VARIABLE, &query),
           "a new definition replaces the old");
    retro_core_options_v2_intl v2intl{&options, nullptr};
    expect(environment.answer(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL, &v2intl) &&
               environment.options().size() == 1,
           "v2 international options are taken");
}

void avChanges() {
    CoreEnvironment environment{"", ""};
    expect(!environment.takeTiming() && !environment.takeGeometry(), "nothing changed yet");
    retro_game_geometry geometry{320, 240, 320, 240, 4.0F / 3.0F};
    expect(environment.answer(RETRO_ENVIRONMENT_SET_GEOMETRY, &geometry), "geometry is taken");
    const auto changed = environment.takeGeometry();
    expect(changed && changed->base_width == 320, "the new geometry is handed on");
    expect(!environment.takeGeometry(), "once");
    retro_system_av_info info{{256, 224, 512, 448, 0.0F}, {50.0, 44100.0}};
    expect(environment.answer(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &info), "timing is taken");
    const auto timing = environment.takeTiming();
    expect(timing && timing->fps == 50.0 && timing->sample_rate == 44100.0,
           "the new timing is handed on");
    const auto withTiming = environment.takeGeometry();
    expect(withTiming && withTiming->base_width == 256, "with its geometry");
}

} // namespace

int main() {
    directories();
    pixelFormats();
    flags();
    variables();
    definitions();
    avChanges();
    std::printf("core_environment: all checks passed\n");
    return 0;
}

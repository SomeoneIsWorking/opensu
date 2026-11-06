// The environment commands the runner answers, and the ones it refuses.
#include "core_environment.hpp"

#include <cstdio>
#include <cstring>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::retro::CoreEnvironment;
using opensu::retro::CoreOptions;
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

void rotation() {
    CoreEnvironment environment{"", ""};
    unsigned quarterTurns = 3;
    expect(environment.answer(RETRO_ENVIRONMENT_SET_ROTATION, nullptr),
           "a software core may ask for a rotation");
    expect(environment.answer(RETRO_ENVIRONMENT_SET_ROTATION, &quarterTurns) &&
               environment.rotation() == 3,
           "the rotation a core asks for is kept");
    quarterTurns = 4;
    expect(!environment.answer(RETRO_ENVIRONMENT_SET_ROTATION, &quarterTurns) &&
               environment.rotation() == 3,
           "a rotation past three quarters is refused");
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
    retro_hw_render_callback render{};
    render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
    render.version_major = 3;
    render.version_minor = 3;
    expect(!environment.answer(RETRO_ENVIRONMENT_SET_HW_RENDER, &render) &&
               environment.hardware().refusal().find("OpenGL core 3.3") != std::string::npos,
           "an OpenGL core is refused, named in the reason");
    render.context_type = RETRO_HW_CONTEXT_VULKAN;
    expect(environment.answer(RETRO_ENVIRONMENT_SET_HW_RENDER, &render) &&
               environment.hardware().requested() && environment.hardware().refusal().empty(),
           "a Vulkan core is taken");
    unsigned preferred = 0;
    expect(environment.answer(RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER, &preferred) &&
               preferred == RETRO_HW_CONTEXT_VULKAN,
           "Vulkan is the preferred context");
    expect(!environment.answer(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, nullptr),
           "anything else is refused");
}

void inputDescriptors() {
    CoreEnvironment environment{"", ""};
    expect(!environment.takeInputDescriptors(), "nothing is named before the core says");
    const retro_input_descriptor named[] = {
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "Attack"},
        {0, RETRO_DEVICE_ANALOG, 0, RETRO_DEVICE_ID_ANALOG_X, "Left/Right"},
        {1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "Jump"},
        {0, 0, 0, 0, nullptr}};
    expect(environment.answer(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS,
                              const_cast<retro_input_descriptor*>(named)),
           "the core's descriptors are taken");
    const auto taken = environment.takeInputDescriptors();
    expect(taken && taken->size() == 1 && taken->front().id == RETRO_DEVICE_ID_JOYPAD_B &&
               taken->front().description == "Attack",
           "port 0's joypad controls are kept, copied");
    expect(!environment.takeInputDescriptors(), "they are handed over once");

    const retro_controller_description pad{"Classic", RETRO_DEVICE_JOYPAD};
    const retro_controller_info ports[] = {{&pad, 1}, {&pad, 1}, {&pad, 1}, {nullptr, 0}};
    expect(environment.answer(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO,
                              const_cast<retro_controller_info*>(ports)) &&
               environment.controllerPorts() == 3,
           "the core's ports are counted");
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
    const CoreOptions::Option& listed = environment.options().options().front();
    expect(listed.key == "core_v2" && listed.label == "V2" && listed.current == "off" &&
               listed.values.size() == 2,
           "the option's label, values and current value are listed");
    expect(environment.options().set("core_v2", "on") &&
               !environment.options().set("core_v2", "on"),
           "setting an option changes it once");
    bool updated = false;
    expect(environment.answer(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated,
           "the core is told its variables changed");
    expect(environment.answer(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && !updated,
           "and told once");
    query = {"core_v2", nullptr};
    expect(environment.answer(RETRO_ENVIRONMENT_GET_VARIABLE, &query) && same(query.value, "on"),
           "the core reads the new value");
    expect(!environment.options().set("core_v2", "maybe") &&
               !environment.options().set("nope", "on"),
           "an offered value on a known key is required");
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

void savestates() {
    CoreEnvironment environment{"", ""};
    int context = -1;
    expect(environment.answer(RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT, &context) &&
               context == RETRO_SAVESTATE_CONTEXT_NORMAL,
           "a state is an ordinary one outside a session");
    environment.joinNetplay(1, {});
    expect(environment.answer(RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT, &context) &&
               context == RETRO_SAVESTATE_CONTEXT_ROLLBACK_NETPLAY,
           "a session's state says it may cross machines");
    expect(environment.canRollBack(), "a core with no quirks can roll back");
    std::uint64_t quirks = RETRO_SERIALIZATION_QUIRK_FRONT_VARIABLE_SIZE;
    expect(environment.answer(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS, &quirks) && quirks == 0,
           "a size of the frontend's own choosing is not offered");
    expect(environment.canRollBack(), "and rules nothing out");
    for (const std::uint64_t quirk :
         {RETRO_SERIALIZATION_QUIRK_INCOMPLETE, RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE,
          RETRO_SERIALIZATION_QUIRK_CORE_VARIABLE_SIZE, RETRO_SERIALIZATION_QUIRK_SINGLE_SESSION}) {
        quirks = quirk;
        expect(environment.answer(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS, &quirks) &&
                   !environment.canRollBack(),
               "a quirk keeps the core on lockstep");
    }
}

void withoutData() {
    CoreEnvironment environment{"", ""};
    for (const unsigned command :
         {RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY,
          RETRO_ENVIRONMENT_GET_LOG_INTERFACE, RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS,
          RETRO_ENVIRONMENT_GET_CAN_DUPE, RETRO_ENVIRONMENT_GET_INPUT_BITMASKS,
          RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT, RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS,
          RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS_2, RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION,
          RETRO_ENVIRONMENT_GET_VARIABLE, RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE,
          RETRO_ENVIRONMENT_SET_GEOMETRY, RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER,
          RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE,
          RETRO_ENVIRONMENT_GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT}) {
        expect(environment.answer(command, nullptr), "an answered command is available");
    }
    expect(!environment.takeGeometry(), "and nothing was taken");

    for (const unsigned command :
         {RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO,
          RETRO_ENVIRONMENT_SET_HW_RENDER}) {
        expect(!environment.answer(command, nullptr), "no data is not data taken");
    }
    expect(environment.pixelFormat() == RETRO_PIXEL_FORMAT_0RGB1555 && !environment.takeTiming() &&
               !environment.hardware().requested(),
           "and changes nothing");

    expect(!environment.answer(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, nullptr),
           "no render interface before the presenter is up");
    expect(!environment.answer(RETRO_ENVIRONMENT_GET_NETPLAY_CLIENT_INDEX, nullptr) &&
               !environment.answer(opensu::retro::getNetplayClock, nullptr),
           "no netplay answers outside a session");
    environment.joinNetplay(1, {});
    expect(environment.answer(RETRO_ENVIRONMENT_GET_NETPLAY_CLIENT_INDEX, nullptr) &&
               environment.answer(opensu::retro::getNetplayClock, nullptr),
           "and both in one");
    expect(!environment.answer(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, nullptr),
           "a refused command stays refused");

    retro_variable defined[] = {{"core_speed", "Speed; normal|fast"}, {nullptr, nullptr}};
    for (const unsigned command :
         {RETRO_ENVIRONMENT_SET_VARIABLES, RETRO_ENVIRONMENT_SET_CORE_OPTIONS,
          RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,
          RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL}) {
        environment.answer(RETRO_ENVIRONMENT_SET_VARIABLES, defined);
        expect(environment.answer(command, nullptr) && environment.options().size() == 0,
               "no definitions removes the options");
    }

    environment.answer(RETRO_ENVIRONMENT_SET_VARIABLES, defined);
    expect(environment.options().set("core_speed", "fast") &&
               environment.answer(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, nullptr),
           "asking about the update flag");
    bool updated = false;
    expect(environment.answer(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated,
           "leaves it set");
}

} // namespace

int main() try {
    withoutData();
    savestates();
    directories();
    pixelFormats();
    rotation();
    flags();
    inputDescriptors();
    variables();
    definitions();
    avChanges();
    std::printf("core_environment: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

#include "core_environment.hpp"

#include <cstdarg>
#include <cstdio>
#include <string_view>
#include <utility>
#include <vector>

#include "lucent/log.h"

namespace opensu::retro {
namespace {

/// The core's log lines, through Lucent's "core" channel.
void logLine(retro_log_level level, const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    va_list measuring;
    va_copy(measuring, arguments);
    const int length = std::vsnprintf(nullptr, 0, format, measuring);
    va_end(measuring);
    std::vector<char> text(length > 0 ? static_cast<std::size_t>(length) + 1 : 1, '\0');
    if (length > 0) {
        std::vsnprintf(text.data(), text.size(), format, arguments);
    }
    va_end(arguments);
    std::string_view line{text.data()};
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.remove_suffix(1);
    }
    switch (level) {
    case RETRO_LOG_DEBUG:
        lucent::debug("core", "{}", line);
        break;
    case RETRO_LOG_INFO:
        lucent::info("core", "{}", line);
        break;
    case RETRO_LOG_WARN:
        lucent::warn("core", "{}", line);
        break;
    default:
        lucent::error("core", "{}", line);
        break;
    }
}

} // namespace

CoreEnvironment::CoreEnvironment(const std::filesystem::path& systemDir,
                                 const std::filesystem::path& saveDir)
    : systemDir_{systemDir.string()}, saveDir_{saveDir.string()} {
}

bool CoreEnvironment::answer(unsigned command, void* data) {
    switch (command) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *static_cast<const char**>(data) = systemDir_.c_str();
        return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char**>(data) = saveDir_.c_str();
        return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
        const auto format = *static_cast<const retro_pixel_format*>(data);
        if (format != RETRO_PIXEL_FORMAT_0RGB1555 && format != RETRO_PIXEL_FORMAT_XRGB8888 &&
            format != RETRO_PIXEL_FORMAT_RGB565) {
            return false;
        }
        pixelFormat_ = format;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        static_cast<retro_log_callback*>(data)->log = logLine;
        return true;
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *static_cast<bool*>(data) = true;
        return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
        return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *static_cast<unsigned*>(data) = 2;
        return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
        options_.defineVariables(static_cast<const retro_variable*>(data));
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
        options_.defineDefinitions(static_cast<const retro_core_option_definition*>(data));
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
        options_.defineDefinitions(static_cast<const retro_core_options_intl*>(data)->us);
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
        options_.defineV2(*static_cast<const retro_core_options_v2*>(data));
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
        options_.defineV2(*static_cast<const retro_core_options_v2_intl*>(data)->us);
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        auto* variable = static_cast<retro_variable*>(data);
        variable->value = variable->key != nullptr ? options_.value(variable->key) : nullptr;
        return variable->value != nullptr;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool*>(data) = false;
        return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
        geometry_ = *static_cast<const retro_game_geometry*>(data);
        return true;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
        const auto& info = *static_cast<const retro_system_av_info*>(data);
        timing_ = info.timing;
        geometry_ = info.geometry;
        return true;
    }
    default:
        lucent::debug("retro", "refused environment command {}", command);
        return false;
    }
}

std::optional<retro_system_timing> CoreEnvironment::takeTiming() {
    return std::exchange(timing_, std::nullopt);
}

std::optional<retro_game_geometry> CoreEnvironment::takeGeometry() {
    return std::exchange(geometry_, std::nullopt);
}

} // namespace opensu::retro

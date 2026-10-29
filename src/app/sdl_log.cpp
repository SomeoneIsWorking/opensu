#include "sdl_log.hpp"

#include <SDL3/SDL_log.h>

#include "lucent/log.h"

namespace opensu::app {
namespace {

void toLucent(void* /*userdata*/, int /*category*/, SDL_LogPriority priority, const char* message) {
    if (priority >= SDL_LOG_PRIORITY_ERROR) {
        lucent::error("sdl", "{}", message);
    } else if (priority == SDL_LOG_PRIORITY_WARN) {
        lucent::warn("sdl", "{}", message);
    } else {
        lucent::info("sdl", "{}", message);
    }
}

} // namespace

SdlLog::SdlLog() {
    SDL_SetLogOutputFunction(toLucent, nullptr);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_WARN);
}

SdlLog::~SdlLog() {
    SDL_SetLogOutputFunction(SDL_GetDefaultLogOutputFunction(), nullptr);
}

} // namespace opensu::app

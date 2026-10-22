#include "shared_library.hpp"

#include <SDL3/SDL_error.h>

namespace opensu::retro {

std::unique_ptr<SharedLibrary> SharedLibrary::open(const std::filesystem::path& file,
                                                   std::string& error) {
    SDL_SharedObject* object = SDL_LoadObject(file.string().c_str());
    if (object == nullptr) {
        error = SDL_GetError();
        return nullptr;
    }
    return std::unique_ptr<SharedLibrary>{new SharedLibrary{object}};
}

SharedLibrary::~SharedLibrary() {
    SDL_UnloadObject(object_);
}

SDL_FunctionPointer SharedLibrary::symbol(const char* name) const {
    return SDL_LoadFunction(object_, name);
}

} // namespace opensu::retro

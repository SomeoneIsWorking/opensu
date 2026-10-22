// shared_library — a library loaded at run time by path through SDL's loader (`SDL_LoadObject`:
// dlopen with RTLD_NOW | RTLD_LOCAL on Linux, LoadLibrary on Windows), and its symbols. Cores and
// librashader load through it.
#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include <SDL3/SDL_loadso.h>

namespace opensu::retro {

class SharedLibrary {
  public:
    /// Loads `file`; null, with `error` the loader's reason, when it cannot be loaded.
    [[nodiscard]] static std::unique_ptr<SharedLibrary> open(const std::filesystem::path& file,
                                                             std::string& error);
    SharedLibrary(const SharedLibrary&) = delete;
    SharedLibrary& operator=(const SharedLibrary&) = delete;
    SharedLibrary(SharedLibrary&&) = delete;
    SharedLibrary& operator=(SharedLibrary&&) = delete;
    ~SharedLibrary();

    /// The function `name`, or null when the library has none.
    [[nodiscard]] SDL_FunctionPointer symbol(const char* name) const;

  private:
    explicit SharedLibrary(SDL_SharedObject* object) noexcept : object_{object} {
    }

    SDL_SharedObject* object_;
};

} // namespace opensu::retro

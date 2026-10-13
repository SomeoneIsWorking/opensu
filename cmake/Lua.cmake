# Lua 5.4, the plugin runtime, compiled from the lua.org release as C++ so a Lua error unwinds as
# an exception and the bindings can hold RAII objects. Pinned by release and SHA-256 (the sum
# lua.org publishes); FETCHCONTENT_SOURCE_DIR_LUA points at an unpacked copy for an offline build.
# Only the core and the libraries a plugin may use are built: no io, os, debug or package.

include(FetchContent)

set(OPENSU_LUA_VERSION "5.4.9")
set(OPENSU_LUA_SHA256 "2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6")

FetchContent_Declare(lua
    URL "https://www.lua.org/ftp/lua-${OPENSU_LUA_VERSION}.tar.gz"
    URL_HASH "SHA256=${OPENSU_LUA_SHA256}"
    # The tarball has no CMakeLists.txt; this only unpacks it.
    SOURCE_SUBDIR "no-cmake"
)
FetchContent_MakeAvailable(lua)

set(OPENSU_LUA_SOURCES
    lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c lmem.c lobject.c
    lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c lvm.c lzio.c
    lauxlib.c lbaselib.c lstrlib.c ltablib.c lmathlib.c lutf8lib.c)
list(TRANSFORM OPENSU_LUA_SOURCES PREPEND "${lua_SOURCE_DIR}/src/")
set_source_files_properties(${OPENSU_LUA_SOURCES} PROPERTIES LANGUAGE CXX)

add_library(opensu_lua STATIC ${OPENSU_LUA_SOURCES})
add_library(opensu::lua ALIAS opensu_lua)
target_include_directories(opensu_lua SYSTEM PUBLIC "${lua_SOURCE_DIR}/src")
target_compile_features(opensu_lua PUBLIC cxx_std_20)

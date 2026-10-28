# The Lua plugin host: sandboxed runtimes on their own threads, the `opensu` API, the I/O pool.
add_library(opensu_plugin STATIC
    manifest.cpp
    permissions.cpp
    lua_runtime.cpp
    lua_json.cpp
    sources.cpp
    host_api.cpp
    io_pool.cpp
    plugin_worker.cpp
    plugin_host.cpp
)
add_library(opensu::plugin ALIAS opensu_plugin)
target_link_libraries(opensu_plugin PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(opensu_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_plugin
    PUBLIC opensu::library opensu::net opensu::lua
    PRIVATE lucent::lucent
)
target_compile_features(opensu_plugin PUBLIC cxx_std_20)
target_compile_options(opensu_plugin PRIVATE -Wall -Wextra -Wpedantic)

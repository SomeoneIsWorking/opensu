# The iideck shell.
#
# Layout follows one owner per subsystem: the library model and its per-store
# sources, the controller reader, the launch handoff, the Steam client, the nested
# session, host device state, and the drawn shell. Entry points compose; nothing here reaches into
# another's internals.

add_subdirectory(vdf)
add_subdirectory(config)
add_subdirectory(library)
add_subdirectory(gamepad)
add_subdirectory(launch)
add_subdirectory(steam)
add_subdirectory(session)
add_subdirectory(device)
add_subdirectory(ui)
add_subdirectory(app)

add_executable(iideck main.cpp)
target_link_libraries(iideck
    PRIVATE
        iideck::app
        iideck::ui
        iideck::gamepad
        iideck::launch
        iideck::steam
        iideck::session
        iideck::library
        raylib
        lucent::lucent
)
target_include_directories(iideck PRIVATE "${RAYLIB_ROOT}/include")
target_link_directories(iideck PRIVATE "${RAYLIB_ROOT}/lib")
# Subsystem headers are included as "library/game.hpp" and "vdf/node.hpp", so
# the source root is on the include path for every target.
target_include_directories(iideck_vdf INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck_library INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck_ui INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck_gamepad INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck_launch INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck_steam INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck_session INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck_app INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck INTERFACE "${CMAKE_SOURCE_DIR}/src")

# raylib's static archive leaves the platform libraries to the consumer.
find_package(X11 REQUIRED)
target_link_libraries(iideck PRIVATE X11::X11 GL m Threads::Threads ${CMAKE_DL_LIBS})

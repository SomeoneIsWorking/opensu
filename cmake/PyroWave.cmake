# PyroWave, remote play's video codec (Vulkan compute wavelets, intra-only): the pinned fork's C API,
# `libpyrowave-shared`, from the shared third-party prefix (cmake/thirdparty, built by
# `tools.check deps`), found as `find_package(OpensuPyroWave)`. The library is staged where the
# executables load it, in the build tree as installed: `<prefix>/<libdir>/opensu/` on Linux, which
# openSU's RPATH names; the app's Frameworks on macOS (`@executable_path/../Frameworks`); beside
# `opensu.exe` on Windows, whose tests find it through PATH (`opensu_pyrowave_path`).
#
# OPENSU_BUILD_PYROWAVE=OFF builds without it: remote play then refuses to start with the reason.
# Built on Linux, Windows and macOS; Android builds without it.

include(GNUInstallDirs)
include(ThirdParty)

option(OPENSU_BUILD_PYROWAVE
    "Use the pinned PyroWave fork remote play encodes and decodes video with. OFF builds openSU \
without remote play's video" ON)

if(WIN32)
    set(OPENSU_PYROWAVE_DIR "${CMAKE_INSTALL_BINDIR}")
elseif(APPLE)
    set(OPENSU_PYROWAVE_DIR "${CMAKE_INSTALL_LIBDIR}")
else()
    set(OPENSU_PYROWAVE_DIR "${CMAKE_INSTALL_LIBDIR}/opensu")
endif()

function(opensu_add_pyrowave)
    opensu_require_thirdparty(OpensuPyroWave "${OPENSU_PYROWAVE_COMMIT}" OPENSU_BUILD_PYROWAVE)
    # Beside opensu.exe in the build tree, where Windows looks first.
    if(WIN32)
        set(staged "${CMAKE_BINARY_DIR}/src/${OpensuPyroWave_SONAME}")
    else()
        set(staged "${CMAKE_BINARY_DIR}/${OPENSU_PYROWAVE_DIR}/${OpensuPyroWave_SONAME}")
    endif()
    add_custom_command(OUTPUT "${staged}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${OpensuPyroWave_LIBRARY}" "${staged}"
        DEPENDS "${OpensuPyroWave_LIBRARY}"
        COMMENT "Staging PyroWave"
        VERBATIM)
    add_custom_target(opensu_pyrowave DEPENDS "${staged}")

    add_library(opensu_pyrowave_library SHARED IMPORTED GLOBAL)
    add_library(opensu::pyrowave ALIAS opensu_pyrowave_library)
    set_target_properties(opensu_pyrowave_library PROPERTIES
        IMPORTED_LOCATION "${staged}"
        IMPORTED_SONAME "${OpensuPyroWave_SONAME}"
        INTERFACE_INCLUDE_DIRECTORIES "${OpensuPyroWave_INCLUDE_DIR}")
    if(WIN32)
        set_target_properties(opensu_pyrowave_library PROPERTIES
            IMPORTED_IMPLIB "${OpensuPyroWave_IMPORT_LIBRARY}")
    elseif(APPLE)
        # So CMake gives the build tree's executables an rpath to the staged library.
        set_target_properties(opensu_pyrowave_library PROPERTIES
            IMPORTED_SONAME "@rpath/${OpensuPyroWave_SONAME}")
    endif()
    set(OPENSU_PYROWAVE_STAGED "${staged}" PARENT_SCOPE)

    install(FILES "${staged}" DESTINATION "${OPENSU_PYROWAVE_DIR}")
endfunction()

# On Windows, the tests in `directory` and below find the staged PyroWave through PATH.
function(opensu_pyrowave_path directory)
    if(NOT WIN32 OR NOT TARGET opensu::pyrowave)
        return()
    endif()
    get_filename_component(folder "${OPENSU_PYROWAVE_STAGED}" DIRECTORY)
    get_property(tests DIRECTORY "${directory}" PROPERTY TESTS)
    if(tests)
        set_property(TEST ${tests} DIRECTORY "${directory}" APPEND PROPERTY
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${folder}")
    endif()
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        opensu_pyrowave_path("${child}")
    endforeach()
endfunction()

if(OPENSU_BUILD_PYROWAVE AND NOT ANDROID)
    opensu_add_pyrowave()
endif()

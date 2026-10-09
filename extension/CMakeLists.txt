# The sign-in extension as one .xpi: a sandboxed browser's file picker hands over only the file
# chosen, so a bare manifest.json loads without its scripts.
set(OPENSU_SIGNIN_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/opensu-signin/manifest.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/opensu-signin/background.js")
set(OPENSU_SIGNIN_XPI "${CMAKE_CURRENT_BINARY_DIR}/opensu-signin.xpi")

add_custom_command(
    OUTPUT "${OPENSU_SIGNIN_XPI}"
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${OPENSU_SIGNIN_XPI}" --format=zip manifest.json
            background.js
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/opensu-signin"
    DEPENDS ${OPENSU_SIGNIN_SOURCES}
    COMMENT "Packing the sign-in extension")
add_custom_target(opensu_signin_xpi ALL DEPENDS "${OPENSU_SIGNIN_XPI}")

install(FILES "${OPENSU_SIGNIN_XPI}" DESTINATION "${CMAKE_INSTALL_DATADIR}/opensu")

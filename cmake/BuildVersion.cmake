# Writes the build version header at build time, so About names the commit the build was made
# from. Run with -DSOURCE_DIR, -DINPUT, -DOUTPUT and, for a release, -DOPENSU_VERSION.
if(NOT OPENSU_VERSION)
    execute_process(
        COMMAND git -C "${SOURCE_DIR}" describe --tags --always --dirty
        OUTPUT_VARIABLE OPENSU_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    string(REGEX REPLACE "^v" "" OPENSU_VERSION "${OPENSU_VERSION}")
endif()
# configure_file leaves the header untouched when the version did not change.
configure_file("${INPUT}" "${OUTPUT}" @ONLY)

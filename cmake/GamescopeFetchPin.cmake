# Fetches the pinned commit into an existing clone when it is not there yet, so the disconnected
# update step can check it out. Run with cmake -P and GIT, SOURCE and COMMIT defined.
execute_process(
    COMMAND "${GIT}" -C "${SOURCE}" cat-file -e "${COMMIT}^{commit}"
    RESULT_VARIABLE missing OUTPUT_QUIET ERROR_QUIET)
if(missing)
    execute_process(
        COMMAND "${GIT}" -C "${SOURCE}" fetch origin "${COMMIT}"
        RESULT_VARIABLE fetched)
    if(NOT fetched EQUAL 0)
        message(FATAL_ERROR "git fetch of the Gamescope pin ${COMMIT} failed (exit ${fetched})")
    endif()
endif()

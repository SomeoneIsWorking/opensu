# Fetches a pinned commit into an existing clone when it is not there yet, so ExternalProject's
# disconnected update step can check it out. Run with cmake -P and GIT, SOURCE, COMMIT and NAME
# (the project, for the error) defined.
execute_process(
    COMMAND "${GIT}" -C "${SOURCE}" cat-file -e "${COMMIT}^{commit}"
    RESULT_VARIABLE missing OUTPUT_QUIET ERROR_QUIET)
if(missing)
    execute_process(
        COMMAND "${GIT}" -C "${SOURCE}" fetch origin "${COMMIT}"
        RESULT_VARIABLE fetched)
    if(NOT fetched EQUAL 0)
        message(FATAL_ERROR "git fetch of the ${NAME} pin ${COMMIT} failed (exit ${fetched})")
    endif()
endif()

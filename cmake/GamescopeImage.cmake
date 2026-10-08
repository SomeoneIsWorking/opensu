# Builds the Gamescope build image unless it already exists. Run with cmake -P and
# PODMAN, IMAGE, CONTAINERFILE, CONTEXT, FEDORA_VERSION and STAMP defined.
execute_process(COMMAND "${PODMAN}" image exists "${IMAGE}" RESULT_VARIABLE exists)
if(NOT exists EQUAL 0)
    execute_process(
        COMMAND "${PODMAN}" build --build-arg "FEDORA_VERSION=${FEDORA_VERSION}"
            -t "${IMAGE}" -f "${CONTAINERFILE}" "${CONTEXT}"
        RESULT_VARIABLE built)
    if(NOT built EQUAL 0)
        message(FATAL_ERROR "podman build of ${IMAGE} failed (exit ${built})")
    endif()
endif()
file(TOUCH "${STAMP}")

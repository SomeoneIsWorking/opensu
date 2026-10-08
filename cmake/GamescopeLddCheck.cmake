# Fails when the staged Gamescope has runtime libraries the host cannot resolve.
# Run with cmake -P and BINARY defined.
execute_process(COMMAND ldd "${BINARY}" OUTPUT_VARIABLE listing RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "ldd ${BINARY} failed (exit ${status})")
endif()
string(REGEX MATCHALL "[^\n]*not found" unresolved "${listing}")
if(unresolved)
    list(JOIN unresolved "\n" lines)
    message(FATAL_ERROR
        "The built Gamescope needs runtime libraries this host lacks:\n${lines}\n"
        "Find the package with `dnf provides '*/<library>'` and install it. The distribution's "
        "gamescope package pulls in every runtime library the fork needs: `sudo dnf install "
        "gamescope` installs them.")
endif()

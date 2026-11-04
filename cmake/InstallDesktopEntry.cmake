# Run by `cmake --install`: the application menu entry, whose Exec is the installed opensu under the
# prefix this install goes to (`--prefix` included), or OPENSU_DESKTOP_EXEC when the build set one.
# Set by the install(CODE) before it: opensu_desktop_template, opensu_desktop_exec (may be empty),
# opensu_desktop_bindir, opensu_desktop_datadir, opensu_desktop_stage.
if(opensu_desktop_exec)
    set(OPENSU_DESKTOP_EXEC "${opensu_desktop_exec}")
else()
    cmake_path(ABSOLUTE_PATH opensu_desktop_bindir BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
        NORMALIZE OUTPUT_VARIABLE bindir)
    set(OPENSU_DESKTOP_EXEC "${bindir}/opensu")
endif()
file(READ "${opensu_desktop_template}" template)
string(CONFIGURE "${template}" entry @ONLY)
file(WRITE "${opensu_desktop_stage}/opensu.desktop" "${entry}")
file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/${opensu_desktop_datadir}/applications"
    TYPE FILE FILES "${opensu_desktop_stage}/opensu.desktop")

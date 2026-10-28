# A program with windows of its own: on Windows it runs without a console window, and main() stays
# its entry point.
function(opensu_windowed target)
    if(WIN32)
        set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE TRUE)
        target_link_options(${target} PRIVATE /ENTRY:mainCRTStartup)
    endif()
endfunction()

# The warning level of openSU's own targets. clang-cl reads GCC's -Wall as -Weverything, so the MSVC
# front end gets /W4, which it maps to -Wall -Wextra.
function(opensu_warnings target)
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

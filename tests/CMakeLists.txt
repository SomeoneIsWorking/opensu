add_executable(vdf_test vdf/vdf_test.cpp)
target_link_libraries(vdf_test PRIVATE iideck::vdf)
add_test(NAME vdf COMMAND vdf_test)

add_subdirectory(library)
add_subdirectory(launch)

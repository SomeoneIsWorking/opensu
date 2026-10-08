add_executable(launcher_status_test launcher_status_test.cpp)
target_link_libraries(launcher_status_test PRIVATE iideck::app)
target_compile_options(launcher_status_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME launcher_status COMMAND launcher_status_test)

add_executable(giveaways_test giveaways_test.cpp)
target_link_libraries(giveaways_test PRIVATE opensu::giveaways)
target_compile_definitions(giveaways_test PRIVATE
    OPENSU_GIVEAWAY_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_compile_options(giveaways_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME giveaways COMMAND giveaways_test)

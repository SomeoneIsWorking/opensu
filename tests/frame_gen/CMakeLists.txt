# Frame generation's pacing through its shipping code: the fork's pacer and `ReadyNeed`.
add_executable(frame_gen_ready_need_replay_test ready_need_replay_test.cpp)
target_link_libraries(frame_gen_ready_need_replay_test PRIVATE opensu::frame_gen)
target_include_directories(frame_gen_ready_need_replay_test
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ui")
opensu_warnings(frame_gen_ready_need_replay_test)
# flow's ready times from a real-time hidden walk of Super Metroid's demo at 4x on an RX 6700 XT.
add_test(NAME frame_gen_ready_need_replay COMMAND frame_gen_ready_need_replay_test
    "${CMAKE_CURRENT_SOURCE_DIR}/data/flow-ready-ns.txt")

# Frame generation's pacing through its shipping code: the fork's pacer and `ReadyNeed`.
add_executable(frame_gen_ready_need_replay_test ready_need_replay_test.cpp)
target_link_libraries(frame_gen_ready_need_replay_test PRIVATE opensu::frame_gen)
target_include_directories(frame_gen_ready_need_replay_test
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ui")
opensu_warnings(frame_gen_ready_need_replay_test)
# flow's ready times from a real-time hidden walk of Super Metroid's demo at 4x on an RX 6700 XT.
add_test(NAME frame_gen_ready_need_replay COMMAND frame_gen_ready_need_replay_test
    "${CMAKE_CURRENT_SOURCE_DIR}/data/flow-ready-ns.txt")

# The RIFE graph on the CPU, the oracle `rife_net` is checked against; `rife_reference_frame`
# writes its frame between two PNGs for looking at.
add_library(frame_gen_rife_reference STATIC rife_reference.cpp)
target_link_libraries(frame_gen_rife_reference PUBLIC opensu::frame_gen)
target_include_directories(frame_gen_rife_reference PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
opensu_warnings(frame_gen_rife_reference)
add_executable(rife_reference_frame rife_reference_frame.cpp)
target_link_libraries(rife_reference_frame PRIVATE frame_gen_rife_reference opensu::fileio
    gpu2d::image)
opensu_warnings(rife_reference_frame)
# The graph on the generator device against the reference, every blob (needs a model's folder).
add_executable(rife_net_check rife_net_check.cpp)
target_link_libraries(rife_net_check PRIVATE frame_gen_rife_reference lucent::lucent)
opensu_warnings(rife_net_check)

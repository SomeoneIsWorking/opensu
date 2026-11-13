# Frame generation on a Vulkan device of its own (docs/plans/remote-frame-generation.md): the device,
# images and buffers (`generator_gpu`), the GPU time of their work (`gpu_spans`) and the generators
# behind one interface (`interpolator`): the Gamescope fork's Lossless generator and openSU's own
# (`flow`, shaders/flow_*.comp), and frames arriving in CPU RGBA paced through them
# (`paced_frames`) for the ready time their first generated frame needs (`ready_need`, measured by
# `ready_watch`);
# `opensu-framegen-pairs` runs them on PNG frames for tools/framegen_quality. Linux and Android,
# where framegen is built.
add_library(opensu_frame_gen STATIC generator_gpu.cpp gpu_spans.cpp lossless_interpolator.cpp
    flow_interpolator.cpp paced_frames.cpp ready_need.cpp ready_watch.cpp)
gpu2d_compile_shaders(opensu_frame_gen shaders/flow_take.comp shaders/flow_reduce.comp
    shaders/flow_search.comp shaders/flow_choose.comp shaders/flow_make.comp)
add_library(opensu::frame_gen ALIAS opensu_frame_gen)
target_include_directories(opensu_frame_gen PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_frame_gen PUBLIC framegen::framegen opensu::vulkan opensu::settings
    lucent::lucent)
target_compile_features(opensu_frame_gen PUBLIC cxx_std_20)
opensu_warnings(opensu_frame_gen)

# tools/framegen_quality's generator: the frames a lower rate leaves out, made from PNG frames.
add_executable(opensu-framegen-pairs pairs_main.cpp)
target_link_libraries(opensu-framegen-pairs PRIVATE opensu::frame_gen opensu::fileio opensu::config
    opensu::library gpu2d::image lucent::lucent)
opensu_warnings(opensu-framegen-pairs)

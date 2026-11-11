# Frame generation on a Vulkan device of its own (docs/plans/remote-frame-generation.md): the device,
# images and buffers (`generator_gpu`) and the generators behind one interface (`interpolator`):
# the Gamescope fork's Lossless generator. Linux only, where framegen is built.
add_library(opensu_frame_gen STATIC generator_gpu.cpp lossless_interpolator.cpp)
add_library(opensu::frame_gen ALIAS opensu_frame_gen)
target_include_directories(opensu_frame_gen PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_frame_gen PUBLIC framegen::framegen opensu::vulkan
    PRIVATE lucent::lucent)
target_compile_features(opensu_frame_gen PUBLIC cxx_std_20)
opensu_warnings(opensu_frame_gen)

# Frame generation on a Vulkan device of its own (docs/plans/remote-frame-generation.md): the device,
# images and buffers (`generator_gpu`) and the generators behind one interface (`interpolator`):
# the Gamescope fork's Lossless generator; `opensu-framegen-pairs` runs them on PNG frames for
# tools/framegen_quality. Linux only, where framegen is built.
add_library(opensu_frame_gen STATIC generator_gpu.cpp lossless_interpolator.cpp)
add_library(opensu::frame_gen ALIAS opensu_frame_gen)
target_include_directories(opensu_frame_gen PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_frame_gen PUBLIC framegen::framegen opensu::vulkan
    PRIVATE lucent::lucent)
target_compile_features(opensu_frame_gen PUBLIC cxx_std_20)
opensu_warnings(opensu_frame_gen)

# tools/framegen_quality's generator: the frames a lower rate leaves out, made from PNG frames.
add_executable(opensu-framegen-pairs pairs_main.cpp)
target_link_libraries(opensu-framegen-pairs PRIVATE opensu::frame_gen opensu::fileio opensu::config
    opensu::library gpu2d::image lucent::lucent)
opensu_warnings(opensu-framegen-pairs)

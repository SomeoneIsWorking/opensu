add_executable(stream_test stream_test.cpp)
target_link_libraries(stream_test PRIVATE opensu::stream)
target_compile_definitions(stream_test PRIVATE
    OPENSU_CODED_PICTURE="${CMAKE_CURRENT_SOURCE_DIR}/coded_picture.bin")
opensu_warnings(stream_test)
add_test(NAME stream COMMAND stream_test)
# PyroWave codes on 16-lane subgroups or wider; lavapipe's lanes follow llvmpipe's vector width.
set_tests_properties(stream PROPERTIES ENVIRONMENT "LP_NATIVE_VECTOR_WIDTH=512")

add_executable(zip_archive_test zip_archive_test.cpp)
target_link_libraries(zip_archive_test PRIVATE opensu::zip ZLIB::ZLIB)
opensu_warnings(zip_archive_test)
add_test(NAME zip_archive COMMAND zip_archive_test)

find_package(CURL REQUIRED)

add_library(opensu_net STATIC web_client.cpp)
add_library(opensu::net ALIAS opensu_net)
target_include_directories(opensu_net PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_net PRIVATE CURL::libcurl)
target_compile_features(opensu_net PUBLIC cxx_std_20)
target_compile_options(opensu_net PRIVATE -Wall -Wextra -Wpedantic)

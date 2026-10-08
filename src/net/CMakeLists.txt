find_package(CURL REQUIRED)

add_library(iideck_net STATIC web_client.cpp)
add_library(iideck::net ALIAS iideck_net)
target_include_directories(iideck_net PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_net PRIVATE CURL::libcurl)
target_compile_features(iideck_net PUBLIC cxx_std_20)
target_compile_options(iideck_net PRIVATE -Wall -Wextra -Wpedantic)

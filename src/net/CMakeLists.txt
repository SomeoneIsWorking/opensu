find_package(CURL REQUIRED)

add_library(opensu_net STATIC web_client.cpp)
add_library(opensu::net ALIAS opensu_net)
target_include_directories(opensu_net PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_net PRIVATE CURL::libcurl lucent::lucent)
# The certificates HTTPS trusts: Android's system store there, libcurl's default elsewhere.
if(ANDROID)
    target_sources(opensu_net PRIVATE trust_store_android.cpp)
else()
    target_sources(opensu_net PRIVATE trust_store_default.cpp)
endif()
target_compile_features(opensu_net PUBLIC cxx_std_20)
opensu_warnings(opensu_net)

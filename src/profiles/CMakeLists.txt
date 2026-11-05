# The player profiles: who plays, each one's saves apart, and the one-time move of the saves kept
# before profiles into the first one's.
add_library(opensu_profiles STATIC profile_book.cpp save_move.cpp)
add_library(opensu::profiles ALIAS opensu_profiles)
target_include_directories(opensu_profiles PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_profiles
    PUBLIC lucent::lucent
    PRIVATE opensu::save_sync opensu::library opensu::retro_saves opensu::fileio opensu::secrets
            nlohmann_json::nlohmann_json)
target_compile_features(opensu_profiles PUBLIC cxx_std_20)
opensu_warnings(opensu_profiles)

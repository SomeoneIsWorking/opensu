add_library(opensu_giveaways STATIC
    offer.cpp
    epic_offers.cpp
    gog_offers.cpp
    steam_offers.cpp
    offer_feed.cpp
    giveaway_board.cpp
    offer_list.cpp
)
add_library(opensu::giveaways ALIAS opensu_giveaways)
target_link_libraries(opensu_giveaways PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(opensu_giveaways PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_giveaways
    PUBLIC opensu::library opensu::net
    PRIVATE opensu::fileio lucent::lucent Threads::Threads
)
target_compile_features(opensu_giveaways PUBLIC cxx_std_20)
opensu_warnings(opensu_giveaways)

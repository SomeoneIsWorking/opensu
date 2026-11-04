# Wiping secrets from memory, account secrets (the desktop's keyring when the build has one, else
# an owner-only file) and the primitives secrets are made and sealed with.
add_library(opensu_secrets STATIC secret.cpp secret_store.cpp crypto.cpp)
add_library(opensu::secrets ALIAS opensu_secrets)
target_include_directories(opensu_secrets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_secrets PRIVATE opensu::fileio lucent::lucent OpenSSL::Crypto)
target_compile_features(opensu_secrets PUBLIC cxx_std_20)
opensu_warnings(opensu_secrets)

# libsecret is the Secret Service on Linux. AUTO uses it when found; ON refuses to build without it.
set(OPENSU_SECRET_SERVICE AUTO CACHE STRING "Keep secrets in the Secret Service (AUTO, ON, OFF)")
set_property(CACHE OPENSU_SECRET_SERVICE PROPERTY STRINGS AUTO ON OFF)
set(opensu_libsecret OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT OPENSU_SECRET_SERVICE STREQUAL "OFF")
    pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
    set(opensu_libsecret ${LIBSECRET_FOUND})
endif()
if(OPENSU_SECRET_SERVICE STREQUAL "ON" AND NOT opensu_libsecret)
    opensu_missing_package("OPENSU_SECRET_SERVICE=ON needs libsecret"
        FEDORA libsecret-devel DEBIAN libsecret-1-dev ARCH libsecret
        OTHERWISE "configure with -DOPENSU_SECRET_SERVICE=AUTO to keep secrets in an owner-only file")
endif()
if(opensu_libsecret)
    target_sources(opensu_secrets PRIVATE secret_service_libsecret.cpp)
    target_link_libraries(opensu_secrets PRIVATE PkgConfig::LIBSECRET)
else()
    target_sources(opensu_secrets PRIVATE secret_service_none.cpp)
endif()

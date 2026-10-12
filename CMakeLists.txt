cmake_minimum_required(VERSION 3.14)
project(simple-make VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

include(FetchContent)

set(CPR_USE_SYSTEM_CURL ON)
set(CPR_ENABLE_INSTALL OFF)

find_package(cpr QUIET)
if(NOT cpr_FOUND)
    FetchContent_Declare(
        cpr
        GIT_REPOSITORY https://github.com/libcpr/cpr.git
        GIT_TAG 1.15.0
        GIT_SHALLOW TRUE
        GIT_SUBMODULES ""
    )
    FetchContent_MakeAvailable(cpr)
endif()

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
        GIT_SHALLOW TRUE
        GIT_SUBMODULES ""
    )
    FetchContent_MakeAvailable(json)
endif()

add_executable(simple-make
    src/main.cpp
    src/search.cpp
    src/db.cpp
    src/build.cpp
    src/parse_cfg.cpp
    src/installed_list.cpp
    src/platform.cpp
    include/installed_list.hpp
)

target_include_directories(simple-make PRIVATE include)

target_compile_definitions(simple-make PRIVATE SIMPLE_MAKE_VERSION="${PROJECT_VERSION}")

target_link_libraries(simple-make PRIVATE
    cpr::cpr
    nlohmann_json::nlohmann_json
    Threads::Threads
)

if(WIN32)
    target_link_libraries(simple-make PRIVATE advapi32)
endif()

install(TARGETS simple-make RUNTIME DESTINATION bin)

set(CPACK_PACKAGE_NAME "simple-make")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_RELEASE "1")
set(CPACK_PACKAGE_CONTACT "ArtemyStudio <artemystudio.co@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Install any project from source with one command")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
set(CPACK_GENERATOR "RPM;DEB")

set(CPACK_RPM_PACKAGE_RELEASE "1")
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "ArtemyStudio")
set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")

include(CPack)

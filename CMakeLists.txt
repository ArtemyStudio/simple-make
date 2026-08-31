cmake_minimum_required(VERSION 3.14)
project(simple-make LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

include(FetchContent)

set(CPR_USE_SYSTEM_CURL ON)

FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
)
FetchContent_MakeAvailable(cpr)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
)
FetchContent_MakeAvailable(json)

add_executable(simple-make 
    src/main.cpp 
    src/search.cpp
    src/db.cpp
    src/build.cpp
    src/parse_cfg.cpp
    src/installed_list.cpp
    include/installed_list.hpp
) 

target_include_directories(simple-make PRIVATE include)

target_link_libraries(simple-make PRIVATE 
    cpr::cpr 
    nlohmann_json::nlohmann_json
    Threads::Threads
)
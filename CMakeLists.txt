cmake_minimum_required(VERSION 3.16)
project(MusicBot CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Dependencies ──────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)
find_package(CURL    REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost   REQUIRED COMPONENTS system)

# tgbot-cpp (installed via apt / vcpkg / manual build)
find_package(TgBot QUIET)
if (NOT TgBot_FOUND)
    # fallback: assume headers in /usr/local/include, lib in /usr/local/lib
    add_library(TgBot::TgBot INTERFACE IMPORTED)
    target_include_directories(TgBot::TgBot INTERFACE /usr/local/include)
    target_link_libraries(TgBot::TgBot INTERFACE TgBot)
endif()

# nlohmann/json (header-only, fetched if not found)
find_package(nlohmann_json QUIET)
if (NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
    FetchContent_MakeAvailable(json)
endif()

# ── Target ────────────────────────────────────────────────────────────────────
add_executable(music_bot main.cpp)

target_link_libraries(music_bot PRIVATE
    TgBot::TgBot
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    Boost::system
    Threads::Threads
)

cmake_minimum_required(VERSION 3.14)
project(TopAcademyTgBot C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_MakeAvailable(json)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static CPR" FORCE)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git GIT_TAG 1.10.x)
FetchContent_MakeAvailable(cpr)

FetchContent_Declare(httplib GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git GIT_TAG v0.14.1)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(sqlite3 URL https://sqlite.org/2023/sqlite-amalgamation-3440200.zip)
FetchContent_MakeAvailable(sqlite3)
add_library(sqlite3 ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_link_libraries(sqlite3 ${CMAKE_DL_LIBS})

find_package(OpenSSL REQUIRED)
find_package(TgBot REQUIRED)

add_executable(${PROJECT_NAME} 
    src/main.cpp
    src/journal_api/journal_client.cpp
    src/db/database.cpp
    src/tma_auth/tma_auth.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src ${sqlite3_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    cpr::cpr
    httplib
    sqlite3
    TgBot::TgBot
    OpenSSL::Crypto
    OpenSSL::SSL
)
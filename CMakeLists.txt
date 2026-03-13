cmake_minimum_required(VERSION 3.20)
project(daniclaw-cpp VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Exports compile_commands.json for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependências via vcpkg ────────────────────────────────────────────────────
find_package(CURL          REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog        CONFIG REQUIRED)
find_package(yaml-cpp      CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# ── Fontes ────────────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/**/*.cpp"
)

# sqlite-vec é C puro — compilado separadamente
set(SQLITE_VEC_SRC src/vendor/sqlite-vec/sqlite-vec.c)
set_source_files_properties(${SQLITE_VEC_SRC} PROPERTIES LANGUAGE C)

add_executable(daniclaw ${SOURCES} ${SQLITE_VEC_SRC})

target_include_directories(daniclaw PRIVATE src)

target_link_libraries(daniclaw PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    yaml-cpp::yaml-cpp
    unofficial::sqlite3::sqlite3
)

# ── Plataforma ────────────────────────────────────────────────────────────────
if(UNIX AND NOT APPLE)
    target_link_libraries(daniclaw PRIVATE pthread)
endif()

# ── Flags de compilação ───────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(daniclaw PRIVATE /W4)
else()
    target_compile_options(daniclaw PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Build type padrão ─────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")

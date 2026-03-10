cmake_minimum_required(VERSION 3.20)
project(ur C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2)
set(GTEST_FORCE_SHARED_CRT
    ON
    CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# cpp-httplib (header-only; HTTP-only mode — HTTPS is handled by a reverse
# proxy)
FetchContent_Declare(
  cpp-httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.18.1)
FetchContent_MakeAvailable(cpp-httplib)

# mbedTLS — crypto only (CTR-DRBG + AES-256-GCM); no TLS stack or X.509 needed
FetchContent_Declare(
  mbedtls
  GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
  GIT_TAG v3.6.2)
set(ENABLE_TESTING
    OFF
    CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS
    OFF
    CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(mbedtls)

# nlohmann/json (header-only)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3)
FetchContent_MakeAvailable(nlohmann_json)

# ftxui — component-based TUI (Phase 3+)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v5.0.0)
set(FTXUI_BUILD_EXAMPLES
    OFF
    CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS
    OFF
    CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

enable_testing()

add_subdirectory(third_party/sqlite3)
add_subdirectory(src/ur)
add_subdirectory(tests/unit)

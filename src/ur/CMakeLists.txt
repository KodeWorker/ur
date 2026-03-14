# Gather all .cpp sources except main.cpp. CONFIGURE_DEPENDS re-runs CMake
# automatically when files are added/removed.
file(
  GLOB_RECURSE
  ur_lib_sources
  CONFIGURE_DEPENDS
  "env.cpp"
  "cli/*.cpp"
  "log/*.cpp"
  "memory/*.cpp"
  "llm/*.cpp"
  "agent/*.cpp"
  "tools/*.cpp")

# ur_lib — all sources except main.cpp, so tests can link against it.
add_library(ur_lib STATIC ${ur_lib_sources})

target_include_directories(ur_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
  ur_lib
  PUBLIC sqlite3
         MbedTLS::mbedcrypto
         httplib::httplib
         nlohmann_json::nlohmann_json
         ftxui::component
         ftxui::dom
         ftxui::screen)

# dlopen / dlsym / dlclose — needed by Loader for shared-library plugins. On
# Linux these live in libdl; macOS includes them in the system library.
if(UNIX AND NOT APPLE)
  target_link_libraries(ur_lib PUBLIC dl)
endif()

# ur — the CLI binary
add_executable(ur main.cpp)
target_link_libraries(ur PRIVATE ur_lib)

install(TARGETS ur DESTINATION bin)

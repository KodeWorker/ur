# ur_lib — all sources except main.cpp, so tests can link against it.
add_library(
  ur_lib STATIC
  env.cpp
  cli/command.cpp
  cli/context.cpp
  cli/tui.cpp
  log/logger.cpp
  memory/workspace.cpp
  memory/database.cpp
  memory/crypto.cpp
  llm/http_provider.cpp
  agent/runner.cpp
  agent/chat.cpp
  agent/persona_updater.cpp)

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

# ur — the CLI binary
add_executable(ur main.cpp)
target_link_libraries(ur PRIVATE ur_lib)

install(TARGETS ur DESTINATION bin)

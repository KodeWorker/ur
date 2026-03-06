# ur_lib — all sources except main.cpp, so tests can link against it.
find_package(OpenSSL REQUIRED)

add_library(ur_lib STATIC cli/command.cpp cli/context.cpp memory/workspace.cpp
                          memory/database.cpp memory/crypto.cpp)

target_include_directories(ur_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ur_lib PUBLIC sqlite3 OpenSSL::Crypto)

# ur — the CLI binary
add_executable(ur main.cpp)
target_link_libraries(ur PRIVATE ur_lib)

install(TARGETS ur DESTINATION bin)

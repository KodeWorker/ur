cmake_minimum_required(VERSION 3.14)
project(GamePoC_ENet LANGUAGES C CXX) # Added C for ENet

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Suppress unused result warnings from raudio.c (part of raylib)
set_source_files_properties(raudio.c PROPERTIES
    COMPILE_FLAGS "-Wunused-result")
set_source_files_properties(raudio.c PROPERTIES
    COMPILE_FLAGS "-Wstringop-overflow")

# Ensure Linux binaries find their .so files in the same folder
if(UNIX AND NOT APPLE)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()

include(FetchContent)

# ----------------------------------------------------------------              
# 1. ENET (Game Networking)
# ----------------------------------------------------------------
FetchContent_Declare(
  enet
  GIT_REPOSITORY https://github.com/lsalzman/enet.git
  GIT_TAG        v1.3.18
)
FetchContent_MakeAvailable(enet)

# ----------------------------------------------------------------
# 2. RAYLIB (Graphics - Shared/Dynamic)
# ----------------------------------------------------------------
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        5.5
)
set(LIBTYPE SHARED CACHE STRING "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# ----------------------------------------------------------------
# 3. DEFINE TARGETS
# ----------------------------------------------------------------

# CLIENT: Graphics + Networking
add_executable(game_client src/client/main.cpp)
target_link_libraries(game_client PRIVATE raylib enet)
target_include_directories(game_client PRIVATE ${enet_SOURCE_DIR}/include)


# SERVER: Networking only
add_executable(game_server src/server/main.cpp)
target_link_libraries(game_server PRIVATE enet)
target_include_directories(game_server PRIVATE ${enet_SOURCE_DIR}/include)

# ----------------------------------------------------------------
# 4. OS-SPECIFIC LINKING & DLL COPYING
# ----------------------------------------------------------------
if(WIN32)
    # Windows networking/system requirements
    target_link_libraries(game_client PRIVATE ws2_32 winmm gdi32 opengl32 shell32)
    target_link_libraries(game_server PRIVATE ws2_32 winmm)
    
    # Automatically copy raylib.dll to the exe folder
    add_custom_command(TARGET game_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:raylib>
        $<TARGET_FILE_DIR:game_client>
    )
    # Note: ENet usually compiles as a static object into your project 
    # even in dynamic mode because it's so small, but you can force it if needed.
else()
    # Linux requirements
    find_package(Threads REQUIRED)
    target_link_libraries(game_client PRIVATE Threads::Threads m dl)
    target_link_libraries(game_server PRIVATE Threads::Threads m dl)
endif()
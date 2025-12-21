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

# ----------------------------------------------------------------              
# 1. Dependencies
# ----------------------------------------------------------------
include(FetchContent)

    # ------------------------------------------------------------
    # 1. ENET (Networking - Static)
    # ------------------------------------------------------------
FetchContent_Declare(
  enet
  GIT_REPOSITORY https://github.com/lsalzman/enet.git
  GIT_TAG        v1.3.18
)
FetchContent_MakeAvailable(enet)

    # ------------------------------------------------------------
    # 2. RAYLIB (Graphics - Shared/Dynamic)
    # ------------------------------------------------------------
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        5.5
)
set(LIBTYPE SHARED CACHE STRING "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

    # ------------------------------------------------------------
    # 3. FlatBuffers (Data Serialization)
    # ------------------------------------------------------------
FetchContent_Declare(
  flatbuffers
  GIT_REPOSITORY https://github.com/google/flatbuffers.git
  GIT_TAG        v25.12.19
)
set(FLATBUFFERS_BUILD_TESTS OFF CACHE INTERNAL "")
set(FLATBUFFERS_INSTALL OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(flatbuffers)

# ----------------------------------------------------------------
# 2. Generate FlatBuffers Code
# ----------------------------------------------------------------

file(GLOB_RECURSE FLAT_BUFFERS ${CMAKE_SOURCE_DIR}/src/fbs/*.fbs)
set(GENERATED_FBS_HEADERS "")

foreach(BUFFER ${FLAT_BUFFERS})
    get_filename_component(FILENAME_WE ${BUFFER} NAME_WE)
    message(STATUS "Configuring FlatBuffer: ${FILENAME_WE}")
    add_custom_command(
        OUTPUT "${CMAKE_SOURCE_DIR}/src/fbs/${FILENAME_WE}_generated.h"
        COMMAND 
            flatc --cpp -o 
            "${CMAKE_SOURCE_DIR}/src/fbs" ${BUFFER}
        DEPENDS flatc ${BUFFER}
        COMMENT "Generating FlatBuffers ${FILENAME_WE}..."
        VERBATIM
    )
    list(APPEND GENERATED_FBS_HEADERS "${CMAKE_SOURCE_DIR}/src/fbs/${FILENAME_WE}_generated.h")
endforeach()

# ----------------------------------------------------------------
# 3. DEFINE TARGETS
# ----------------------------------------------------------------

add_custom_target(generate_fbs DEPENDS ${GENERATED_FBS_HEADERS})

# CLIENT: Graphics + Networking
add_executable(game_client src/client/main.cpp ${GENERATED_FBS_HEADERS})
add_dependencies(game_client generate_fbs)
target_link_libraries(game_client PRIVATE raylib enet flatbuffers)
target_include_directories(game_client PRIVATE ${enet_SOURCE_DIR}/include)
target_include_directories(game_client PRIVATE ${CMAKE_SOURCE_DIR}/src/fbs)

# SERVER: Networking only
add_executable(game_server src/server/main.cpp ${GENERATED_FBS_HEADERS})
add_dependencies(game_server generate_fbs)
target_link_libraries(game_server PRIVATE enet flatbuffers)
target_include_directories(game_server PRIVATE ${enet_SOURCE_DIR}/include)
target_include_directories(game_server PRIVATE ${CMAKE_SOURCE_DIR}/src/fbs)

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
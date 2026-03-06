# SQLite amalgamation — single-file build, no config needed. Download sqlite3.c
# and sqlite3.h from https://www.sqlite.org/download.html and place them in this
# directory before configuring.

add_library(sqlite3 STATIC sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

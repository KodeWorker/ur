# Phase 1 — CLI and Workspace Scaffolding

**Goal**: a compilable binary that can `init` and `clean` the workspace, with no LLM dependency yet.

## Deliverables

- `ur init` — creates workspace dirs and initialises SQLite schema
- `ur clean [--database|workspace|all]` — removes selected workspace artifacts
- All three tables exist in the database after `init`

## Source Files to Create

```
CMakeLists.txt                      project root, C++17, fetches GoogleTest
src/ur/CMakeLists.txt
src/ur/main.cpp                     CLI entry point, argument dispatch
src/ur/cli/command.hpp              Command interface / dispatch table
src/ur/memory/workspace.cpp/.hpp    Create and remove workspace dirs
src/ur/memory/database.cpp/.hpp     Open/create SQLite DB, run schema migrations
tests/unit/CMakeLists.txt
tests/unit/test_workspace.cpp
tests/unit/test_database.cpp
```

## CMake Structure

```cmake
# root CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(ur CXX)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(src/ur)
add_subdirectory(tests/unit)

# fetch GoogleTest for tests
include(FetchContent)
FetchContent_Declare(googletest ...)
```

## Workspace Paths

Resolved at runtime from the platform default root:

| Platform | Root |
|----------|------|
| Linux    | `~/.local/share/ur/` |
| macOS    | `~/Library/Application Support/ur/` |
| Windows  | `%APPDATA%\ur\` |

Subdirectories: `workspace/`, `database/`, `tools/`, `log/`, `keys/`

## Database Schema (created by `init`)

```sql
CREATE TABLE IF NOT EXISTS session (
    id         TEXT PRIMARY KEY,
    title      TEXT,
    model      TEXT,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS message (
    id         TEXT PRIMARY KEY,
    session_id TEXT NOT NULL REFERENCES session(id),
    role       TEXT NOT NULL,
    content    TEXT NOT NULL,
    created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS persona (
    key        TEXT PRIMARY KEY,
    value      TEXT NOT NULL,
    updated_at INTEGER NOT NULL
);
```

## CLI Argument Parsing

Keep it simple in Phase 1 — no external library needed.

```
ur init
ur clean
ur clean --database
ur clean --workspace
ur clean --all        (default)
```

## Acceptance Criteria

- [ ] `cmake -B build && cmake --build build` succeeds with zero warnings
- [ ] `ur init` creates all five workspace subdirs and the SQLite DB
- [ ] `ur init` is idempotent (safe to run twice)
- [ ] `ur clean --all` removes database and workspace contents
- [ ] All unit tests pass via `ctest --test-dir build`

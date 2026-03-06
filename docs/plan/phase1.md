# Phase 1 — CLI and Workspace Scaffolding

**Goal**: a compilable binary that can `init` and `clean` the workspace, with no LLM dependency yet.

## Deliverables

- `ur init` — creates workspace dirs and initialises SQLite schema
- `ur clean [--database|workspace]` — removes selected workspace artifacts
- All three tables exist in the database after `init`

## Source Files to Create

```
CMakeLists.txt                      project root, C++17, fetches GoogleTest
src/ur/CMakeLists.txt
src/ur/main.cpp                     CLI entry point, constructs Context, dispatches commands
src/ur/cli/command.hpp              cmd_init / cmd_clean declarations
src/ur/cli/context.hpp              Context struct + make_context()
src/ur/memory/workspace.cpp/.hpp    init_workspace / remove_workspace
src/ur/memory/database.cpp/.hpp     Database — lazy open, init_schema, drop_all
third_party/sqlite3/sqlite3.c       SQLite amalgamation (bundled)
third_party/sqlite3/sqlite3.h
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

add_subdirectory(third_party/sqlite3)   # sqlite3 static lib
add_subdirectory(src/ur)                # ur_lib + ur binary
add_subdirectory(tests/unit)

# fetch GoogleTest for tests
include(FetchContent)
FetchContent_Declare(googletest ...)
```

`ur_lib` (STATIC) contains all sources except `main.cpp`. Both the `ur` binary and unit tests link against `ur_lib`.

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

## Interfaces

### `cli/context.hpp`
```cpp
namespace ur {
    struct Context {
        Paths    paths;
        Database db;    // lazy — file not opened until first db call
    };
    Context make_context();  // resolve_paths() + Database(path), no I/O
}
```

### `memory/database.hpp`
```cpp
namespace ur {
    class Database {
    public:
        explicit Database(std::filesystem::path path); // stores path, no open
        ~Database();
        void init_schema(); // lazy open + CREATE TABLE IF NOT EXISTS
        void drop_all();    // lazy open + DROP TABLE for all three tables
    private:
        std::filesystem::path path_;
        sqlite3* handle_ = nullptr;
    };
}
```

### `memory/workspace.hpp`
```cpp
namespace ur {
    struct Paths {
        std::filesystem::path root;
        std::filesystem::path workspace;
        std::filesystem::path database;
        std::filesystem::path tools;
        std::filesystem::path log;
        std::filesystem::path keys;
    };
    Paths resolve_paths();                  // platform-specific root
    void  init_workspace(const Paths&);     // create all subdirs (idempotent)
    void  remove_workspace(const Paths&);   // remove workspace/ subdir contents
}
```

### `cli/command.hpp`
```cpp
namespace ur {
    int cmd_init(Context&, int argc, char** argv);
    int cmd_clean(Context&, int argc, char** argv);
}
```

## CLI Argument Parsing

Keep it simple in Phase 1 — no external library needed.

```
ur init
ur clean
ur clean --database
ur clean --workspace
```

`ur clean` with no flag removes both workspace and database (drops all tables).

## Acceptance Criteria

- [ ] `cmake -B build && cmake --build build` succeeds with zero warnings
- [ ] `ur init` creates all five workspace subdirs and the SQLite DB
- [ ] `ur init` is idempotent (safe to run twice)
- [ ] `ur clean` removes database and workspace contents
- [ ] All unit tests pass via `ctest --test-dir build`

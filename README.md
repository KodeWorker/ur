# ur

Your agent sandbox — secured, efficient, local hosted, for you only

## Overview

`ur` is a CLI tool for running LLM-powered agents on your own machine. It manages conversation sessions, persists a user persona built up over time, and controls what tools an agent is allowed to use via a sandboxed plugin system.

## Dependencies

- C++17 compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- CMake 3.20+
- SQLite3
- [llama.cpp](https://github.com/ggerganov/llama.cpp)
- Docker (optional — required for sandbox tier 2)

For development only: GoogleTest (fetched automatically by CMake).

## Build from Source

```shell
git clone --recurse-submodules <repo>
cd ur
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cmake --install build
```

## Configuration

Configuration is provided via environment variables or a `.env` file in the working directory. See `.env.example` for available options.

## Usage

```shell
# init database and workspace
ur init
# clean up database or workspace
ur clean [--database|workspace|all(default)]
# run one-time request
ur run <user_prompt> [--system-prompt=message|file] [--tools=message|file] [--model=provider/name] [--allow-all]
# tui chat with agent with context manager
ur chat [--continue=<id>] [--model=provider/name] [--allow-all]
# view session history
ur history [<id>]
# view user profile created by agent
ur persona
```

## Database Schema

```sql
TABLE session (
    id         TEXT PRIMARY KEY,
    title      TEXT,
    model      TEXT,
    created_at INTEGER,
    updated_at INTEGER
)

TABLE message (
    id         TEXT PRIMARY KEY,
    session_id TEXT REFERENCES session(id),
    role       TEXT,    -- 'user' | 'assistant' | 'tool'
    content    TEXT,
    created_at INTEGER
)

TABLE persona (
    key        TEXT PRIMARY KEY,
    value      TEXT,
    updated_at INTEGER
)
```

## Workspace Structure

```shell
# Linux
root=~/.local/share/ur/
# macOS
root=~/Library/Application Support/ur/
# Windows
root=%APPDATA%\ur\

# ur init creates the following
$root/workspace    # agent read/write sandbox
$root/database     # SQLite database
$root/tools        # custom tool plugins
$root/log          # runtime logs
$root/keys         # API keys and credentials
```

## Providers

### llama.cpp (default)

Runs inference locally using GGUF model files. Specify the model with `--model=llama.cpp/<model-name>`.

## Architecture

```
.
├── docs/
│   ├── devlog/         Daily development notes
│   └── plans/          Feature implementation plans
├── src/ur/             Agent implementation
│   ├── agent/          Orchestration, tool calling, turn management
│   ├── llm/            Provider abstraction layer
│   ├── memory/         Session and persona persistence (SQLite)
│   └── tools/          Tool plugin system
└── tests/unit/         Unit tests, one file per source module
```

### Sandbox tiers

Tool execution is restricted based on the active sandbox tier:

| Tier | Mode | Description |
|------|------|-------------|
| 1 | Workspace-constrained | Tools can only read/write within `$root/workspace/` |
| 2 | Docker runner | Tools execute inside a Docker container with controlled mounts |

The `--allow-all` flag bypasses sandbox restrictions entirely.

### Custom tool plugins

Drop shared libraries or scripts into `$root/tools/`. They are loaded at agent startup and exposed to the agent as callable tools.

## Development

### Test organisation

One test file per source module under `tests/unit/`. Tests are registered with CMake and run via:

```shell
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
# run a specific test binary
./build/tests/unit/<test_name> --gtest_filter=<Suite>.<Case>
```

## Roadmap

**Phase 1** — CLI and workspace scaffolding:
Implement `ur init` and `ur clean`, establish workspace directory layout, SQLite schema creation.

**Phase 2** — Single-turn inference:
Integrate llama.cpp, implement `ur run` for one-shot requests, basic session and message persistence.

**Phase 3** — Multi-turn chat and persona:
Implement `ur chat` with context manager and `--continue`, `ur history`, and `ur persona`; persona is updated by the agent over time.

**Phase 4** — Tool system and sandbox tier 1:
Tool plugin loading from `$root/tools/`, workspace-constrained sandbox, tool-calling loop in the agent.

**Phase 5** — Docker sandbox and extended providers:
Sandbox tier 2 (Docker runner), additional LLM provider support, TUI polish.

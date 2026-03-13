# ur

Your agent sandbox — secured, efficient, local hosted, for you only

## Overview

`ur` is a CLI tool for running LLM-powered agents on your own machine. It manages conversation sessions, persists a user persona built up over time, and controls what tools an agent is allowed to use via a sandboxed plugin system.

## Dependencies

- C++17 compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- CMake 3.20+
- An OpenAI-compatible LLM server (e.g. llama.cpp server, Ollama) — managed and run independently
- Docker (optional — required for sandbox tier 2)

All other dependencies (SQLite, mbedTLS, cpp-httplib, nlohmann/json, ftxui, md4c, GoogleTest) are either bundled or fetched automatically by CMake. No manual library installation required.

## Build from Source

```shell
git clone https://github.com/KodeWorker/ur.git
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
# clean up database and workspace
ur clean [--database|workspace]
# run one-time request
ur run <user_prompt> [--system-prompt=<text>|@<path>] [--model=<name>] [--allow=<tool,...>] [--deny=<tool,...>] [--no-tools] [--allow-all]
# tui chat with agent with context manager
ur chat [--continue=<id|prefix|title>] [--model=<name>] [--system-prompt=<file>]
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
$root/logs         # runtime logs
$root/key          # API key and credential
```

## Providers

`ur` connects to any OpenAI-compatible LLM server over HTTP. The server is started and managed independently — `ur` does not embed or depend on any inference library.

Configure the endpoint via environment variables:

| Variable | Description | Default |
|----------|-------------|---------|
| `UR_LLM_BASE_URL` | Base URL of the OpenAI-compatible server | `http://localhost:8080` |
| `UR_LLM_API_KEY` | API key (leave empty for local servers) | _(empty)_ |

Specify the model name with `--model=<name>`, which is passed directly to the server.

Examples:
- llama.cpp server: `UR_LLM_BASE_URL=http://localhost:8080 ur run "hello"`
- Ollama: `UR_LLM_BASE_URL=http://localhost:11434 ur run --model=mistral "hello"`

## Security

### Database encryption

`ur` supports symmetric encryption (AES-256-GCM) for messages stored in the local SQLite database. Encryption is optional — if no key file is present, `ur` operates in plaintext mode.

`ur init` generates a 256-bit encryption key automatically. No manual step required.

`ur` loads `$root/key/secret.key` at startup. If the key is missing or the wrong size, startup fails with an error. Run `ur init` to generate the key before first use.

The key never leaves the local machine.

### Message transport security

Messages sent to and received from the LLM server are not encrypted by `ur`. Secure the transport at the LLM server layer — configure a reverse proxy (e.g. nginx, Caddy) with TLS in front of the LLM server.

## Architecture

```
.
├── third_party/sqlite3/    SQLite amalgamation (bundled)
├── docs/
│   ├── devlog/             Daily development notes
│   └── plan/               Phase specs (phase1–5)
├── src/ur/
│   ├── cli/                Argument dispatch, Context struct, TUI
│   ├── agent/              Orchestration, tool calling, turn management
│   ├── llm/                OpenAI-compatible HTTP provider abstraction
│   ├── memory/             Workspace, SQLite database, flat-file vector store
│   └── tools/              Tool plugin system
└── tests/unit/             Unit tests, one file per source module
```

### Memory storage

`ur` uses two storage layers:

| Layer | Backend | Used for |
|-------|---------|----------|
| Structured | SQLite (`$root/database/ur.db`) | Sessions, messages, persona key-value pairs |
| Semantic | Flat-file (`$root/database/memory.bin`) | Vector embeddings for long-term memory |

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

### Pre-commit hooks

Install and enable the hooks before committing:

```shell
uv run pre-commit install
```

To run manually against all files:

```shell
uv run pre-commit run --all-files
```

Hooks: `clang-format` (C++ formatting), `cmake-format`/`cmake-lint` (CMake), and standard checks (whitespace, EOF, line endings).

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
Implement `ur run` for one-shot requests via an OpenAI-compatible HTTP provider, basic session and message persistence.

**Phase 3** — Multi-turn chat and persona:
Implement `ur chat` with context manager and `--continue`, `ur history`, and `ur persona`; persona key-value pairs persisted in SQLite; flat-file vector store established for future long-term memory.

**Phase 4** — Tool system and sandbox tier 1:
Tool plugin loading from `$root/tools/`, workspace-constrained sandbox, tool-calling loop in the agent.

**Phase 5** — Memory and context:
Context compression (LLM-summarised rolling window), long-term semantic memory via flat-file vector store, context usage display in the TUI status line.

**Phase 6** — Docker sandbox and streaming TUI:
Sandbox tier 2 (Docker runner), streaming token output, TUI polish (markdown rendering via md4c).

# CLAUDE.md

This file provides guidance to Claude Code when working with this repository.

## Project

`ur` is a local agent sandbox — a CLI tool for running LLM-powered agents securely and efficiently on your own machine. The LLM backend is any OpenAI-compatible HTTP server (e.g. llama.cpp server, Ollama), managed and run independently. **The project has no dependency on llama.cpp or any inference library.**

## Build & Test Commands

```bash
# Configure (from repo root)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Run all tests
ctest --test-dir build

# Run a single test binary
./build/tests/unit/<test_name>

# Run with filter (GoogleTest)
./build/tests/unit/<test_name> --gtest_filter=<TestSuite>.<TestCase>

# Install
cmake --install build --prefix /usr/local
```

## Architecture

**C++17**, CMake + GoogleTest + SQLite (bundled amalgamation).

```
third_party/sqlite3/        SQLite amalgamation (sqlite3.c / sqlite3.h)
src/ur/
├── cli/        Argument dispatch (command.hpp), Context struct, TUI (later phases)
├── agent/      Agent orchestration — tool calling, turn management (Phase 2+)
├── llm/        OpenAI-compatible HTTP provider abstraction (Phase 2+)
├── memory/     workspace.cpp/.hpp, database.cpp/.hpp
└── tools/      Tool plugin system (Phase 4+)
tests/unit/     One test file per source module
docs/
├── devlog/     Daily development notes
└── plan/       Phase specs (phase1.md – phase5.md)
```

**CMake targets**:
- `sqlite3` — static lib from amalgamation
- `ur_lib` — all sources except `main.cpp`; linked by tests and the binary
- `ur` — CLI binary (`main.cpp` + `ur_lib`)

**CLI entry points** (`ur <command>`):
- `init` — create workspace dirs and init database
- `clean [--database|--workspace]` — remove workspace artifacts
- `run <prompt>` — one-shot agent request (Phase 2)
- `chat [--continue=<id>]` — TUI chat with context manager (Phase 3)
- `history [<id>]` — view session history (Phase 3)
- `persona` — view user profile built by agent (Phase 3)

**Workspace layout** (platform-specific root):
- Linux: `~/.local/share/ur/`
- macOS: `~/Library/Application Support/ur/`
- Windows: `%APPDATA%\ur\`

Subdirs: `workspace/`, `database/`, `tools/`, `log/`, `keys/`

**Database schema**: three tables — `session`, `message`, `persona`. See `docs/plan/phase1.md` for full DDL.

## Key Design Decisions

- LLM provider connects via HTTP to an OpenAI-compatible endpoint (`UR_LLM_BASE_URL`, `UR_LLM_API_KEY`).
- SQLite is bundled as an amalgamation in `third_party/sqlite3/` — no system dependency.
- `Context` struct (`cli/context.hpp`) is constructed in `main()` and passed to every command. No I/O at construction time.
- `Database` is lazy — file is not opened until the first call to `init_schema()` or `drop_all()`.
- `clean --database` drops all tables; the database file itself is kept.
- Memory uses two storage layers: SQLite for structured data (sessions, messages, persona), flat-file for vector embeddings (long-term semantic memory). No external vector DB or sqlite extensions.
- Database encryption: AES-256-GCM symmetric encryption on message content at rest. `ur` loads `$root/keys/secret.key` at startup if present; operates in plaintext if absent. Message transport security is delegated to the LLM server (reverse proxy + TLS).
- `tools/` supports custom plugin loading from `$root/tools/`.
- Sandbox tiers control what tools agents are allowed to use at runtime.
- `--allow-all` flag bypasses sandbox restrictions for trusted use.

## Commits

Commits are managed via `uv run pre-commit`. Hooks run automatically on `git commit`.

- Never skip hooks with `--no-verify`
- If a hook fails, fix the issue and recommit — do not amend the previous commit
- Run `uv run pre-commit run --all-files` to check before committing

## Agent Cowork Guidelines

- Agent is for: research, planning, documentation, unit tests, suggestion and review
- Implement one thing at a time
- Do not outsource thinking

## Roadmap

| Phase | Focus | Status |
|-------|-------|--------|
| 1 | CLI scaffolding, workspace init, SQLite | Complete |
| 2 | `ur run`, OpenAI-compatible HTTP provider | Complete |
| 3 | `ur chat`, `ur history`, `ur persona` | Not started |
| 4 | Tool plugins, sandbox tier 1 | Not started |
| 5 | Context compression, long-term memory, context usage display | Not started |
| 6 | Docker sandbox tier 2, streaming TUI | Not started |

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

`ur` is a local agent sandbox — a CLI tool for running LLM-powered agents securely and efficiently on your own machine. Default LLM provider is llama.cpp.

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

**C++17**, CMake + GoogleTest.

```
src/ur/
├── agent/      Agent orchestration (tool calling, turn management)
├── llm/        LLM provider abstractions (llama.cpp and others)
├── memory/     Session/persona persistence (SQLite-backed)
└── tools/      Tool plugin system
tests/unit/     One test file per source module
docs/
├── devlog/     Daily development notes
└── plans/      Feature implementation plans
```

**CLI entry points** (`ur <command>`):
- `init` — create workspace dirs and init database
- `clean [--database|workspace|all]` — remove workspace artifacts
- `run <prompt>` — one-shot agent request
- `chat [--continue=<id>]` — TUI chat with context manager
- `history [<id>]` — view session history
- `persona` — view user profile built by agent

**Workspace layout** (platform-specific root):
- Linux: `~/.local/share/ur/`
- macOS: `~/Library/Application Support/ur/`
- Windows: `%APPDATA%\ur\`

Subdirs: `workspace/`, `database/`, `tools/`, `log/`, `keys/`

**Database schema**: three tables — `session`, `message`, `persona`.

## Commits

Commits are managed via `uv run pre-commit`. Hooks run automatically on `git commit`.

- Never skip hooks with `--no-verify`
- If a hook fails, fix the issue and recommit — do not amend the previous commit
- Run `uv run pre-commit run --all-files` to check before committing

## Agent Cowork Guidelines

- Agent is for: research, planning, documentation, unit tests, suggestion and review
- Implement one thing at a time
- Do not outsource thinking

## Key Design Decisions

- `llm/` uses an abstraction layer so providers (llama.cpp, etc.) are swappable.
- `tools/` supports custom plugin loading from `$root/tools/`.
- Sandbox tiers control what tools agents are allowed to use at runtime.
- `--allow-all` flag bypasses sandbox restrictions for trusted use.

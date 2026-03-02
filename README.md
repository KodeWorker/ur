# ur

Agent assisted workflow — a local-first, cross-platform Python sandbox for AI agents.

## Overview

`ur` gives an AI agent a safe place to work. It handles the LLM loop, conversation history, and a growing set of tools (code execution, browser automation, file ops, HTTP) so you can focus on the task, not the plumbing.

- **Local-first** — everything runs on your machine, no cloud infra required
- **Cross-platform** — Windows, Mac, Linux
- **Provider-agnostic** — Google Gemini or any hosted Ollama models
- **Extensible** — drop a `.py` file in `<data_dir>/tools/` to add a custom tool

## Requirements

- Python 3.12+
- A `GEMINI_API_KEY` **or** a running Ollama server

## Installation

```bash
pip install -e ".[dev]"   # development install with test deps
# or
uv pip install -e ".[dev]"
```

First-time setup:

```bash
ur init
# or
uv run ur init
```

## Configuration

Copy `.env.example` to `.env` and fill in your values:

```bash
cp .env.example .env
```

| Environment variable | Default | Description |
|---|---|---|
| `GEMINI_API_KEY` | — | Gemini API key |
| `UR_MODEL` | `gemini/gemini-2.0-flash` | LLM model to use |
| `UR_OLLAMA_BASE_URL` | `http://localhost:11434` | Ollama server URL |
| `UR_MAX_ITERATIONS` | `20` | Agent loop iteration cap |
| `UR_DATA_DIR` | platform data dir | Override data/db/tools location |

**Platform data directories** (auto-detected):

| OS | Path |
|---|---|
| Windows | `%APPDATA%\ur\` |
| macOS | `~/Library/Application Support/ur/` |
| Linux | `~/.local/share/ur/` |

## Usage

```bash
# Run a single task
# `uv run` is only needed if you installed with `uv pip install -e .`
ur run "Summarise the top 5 HN stories today"

# Override model for one run
ur run "Explain quantum entanglement" --model gemini/gemini-1.5-pro

# Interactive multi-turn chat
ur chat

# List past sessions
ur history

# Inspect a specific session (partial ID match)
ur history a3f2b1
```

## Providers

### Gemini (default)

```bash
# .env
GEMINI_API_KEY=your-key-here
UR_MODEL=gemini/gemini-2.5-flash-lite
```

### Ollama (hosted or local)

```bash
# .env
UR_MODEL=ollama/qwen3:30b
UR_OLLAMA_BASE_URL=http://my-server:11434   # omit for localhost
```

Supported model prefixes: `ollama/` (generate API) and `ollama_chat/` (chat API).

## Architecture

```
.
├── docs/
│   ├── devlog/         Daily development notes
│   └── plans/          Feature implementation plans
├── src/ur/
│   ├── config.py       Settings (pydantic-settings, platformdirs)
│   ├── main.py         Typer CLI entry point
│   ├── tui.py          Textual TUI (run + chat commands)
│   ├── agent/
│   │   ├── loop.py     Core agentic loop — yields tokens, dispatches tools
│   │   ├── session.py  AgentSession — messages, usage, lifecycle
│   │   └── models.py   Message type alias, UsageStats
│   ├── llm/
│   │   └── client.py   LiteLLM wrapper + CompletionStream
│   ├── memory/
│   │   ├── db.py       SQLite schema, get_db context manager
│   │   └── session_store.py  save / list / get session messages
│   └── tools/
│       ├── registry.py ToolRegistry — register, lookup, dispatch
│       └── builtin.py  shell, read_file, write_file, http_get, browser_get
└── tests/unit/         Unit tests, one file per source module
```

### Sandbox tiers

| Tier | Backend | Default | Requires |
|---|---|---|---|
| 1 | subprocess + psutil | yes | nothing |
| 2 | Docker / Podman | `--sandbox docker` | Docker daemon |
| 3 | WASM (wasmtime) | future | `pip install ur[sandbox-wasm]` |

### Optional extras

```bash
pip install -e ".[tools]"         # playwright, markdownify, httpx, aiofiles, psutil
pip install -e ".[sandbox]"       # docker SDK (Tier 2)
pip install -e ".[api]"           # fastapi + uvicorn (REST/SSE)
pip install -e ".[memory]"        # chromadb + sentence-transformers
pip install -e ".[observability]" # opentelemetry, langfuse
pip install -e ".[queue]"         # arq (Redis-backed task queue)
```

The `[tools]` extra enables five built-in tools:

| Tool | Description |
|---|---|
| `shell` | Run a shell command, return stdout+stderr |
| `read_file` | Read a file from disk |
| `write_file` | Write a file to disk |
| `http_get` | Fetch a URL with a plain HTTP GET |
| `browser_get` | Visit a URL with headless Chromium (JS rendered), return page as markdown |
| `web_search` | Search the web and return titles, URLs, and snippets |

After installing `[tools]`, download the Chromium binary once for `browser_get`:

```bash
uv run playwright install chromium
```

### Custom plugins

Drop a `.py` file in `<data_dir>/tools/` and define a `register(registry)` function to add custom tools. Plugins are loaded alphabetically after builtins; a plugin can override a builtin by using the same name.

> **Security:** Every `.py` file in the tools directory is executed unconditionally at startup with the full privileges of the running user. Only place files you trust there — never copy plugin files from untrusted sources without reviewing them first.

## Development

```bash
# Run all tests
pytest

# Run against Ollama
UR_MODEL=ollama_chat/qwen2.5 UR_OLLAMA_BASE_URL=http://my-server:11434 pytest

# Lint
ruff check src tests

# Type check
mypy src
```

### Test organisation

Tests are in `tests/unit/`, one file per source module. No real API calls are made — litellm is patched with a `MockStreamWrapper`.

Provider-specific tests are guarded with skip markers that activate based on `UR_MODEL`:

```python
@skip_if_not_gemini   # skipped when UR_MODEL starts with "ollama"
@skip_if_not_ollama   # skipped when UR_MODEL starts with "gemini"
```

## Roadmap

- **Phase 1** (done) — LLM loop, streaming CLI, SQLite sessions
- **Phase 2** (done) — tool suite: filesystem, code execution, HTTP, browser
- **Phase 3** — token cost display, context window management (sliding-window truncation), `ur history <id>` audit trail
- **Phase 4** — Docker sandbox, REST API, OpenTelemetry tracing
- **Phase 5** — long-term memory (ChromaDB), multi-task queue, multi-agent seams

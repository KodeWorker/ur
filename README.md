# ur

Agent assisted workflow — a local-first, cross-platform Python sandbox for AI agents.

## Overview

`ur` gives an AI agent a place to work. It handles the LLM loop, conversation history, and a growing set of tools (code execution, browser automation, file ops, HTTP) so you can focus on the task, not the plumbing.

- **Local-first** — everything runs on your machine, no cloud infra required
- **Cross-platform** — Windows, Mac, Linux
- **Provider-agnostic** — Anthropic Claude or any hosted Ollama model
- **Extensible** — drop a `.py` file in `~/.ur/tools/` to add a custom tool

## Requirements

- Python 3.12+
- An `ANTHROPIC_API_KEY` **or** a running Ollama server

## Installation

```bash
pip install -e ".[dev]"   # development install with test deps
# or
uv pip install -e ".[dev]"
```

First-time setup:

```bash
ur init
```

## Configuration

Copy `.env.example` to `.env` and fill in your values:

```bash
cp .env.example .env
```

| Environment variable | Default | Description |
|---|---|---|
| `ANTHROPIC_API_KEY` | — | Anthropic API key |
| `UR_MODEL` | `anthropic/claude-sonnet-4-6` | LLM model to use |
| `UR_OLLAMA_BASE_URL` | `http://localhost:11434` | Ollama server URL |
| `UR_MAX_ITERATIONS` | `20` | Agent loop iteration cap |
| `UR_DATA_DIR` | platform data dir | Override data/db location |

**Platform data directories** (auto-detected):

| OS | Path |
|---|---|
| Windows | `%APPDATA%\ur\` |
| macOS | `~/Library/Application Support/ur/` |
| Linux | `~/.local/share/ur/` |

## Usage

```bash
# Run a single task
ur run "Summarise the top 5 HN stories today"

# Override model for one run
ur run "Explain quantum entanglement" --model anthropic/claude-opus-4-6

# Interactive multi-turn chat
ur chat

# List past sessions
ur history

# Inspect a specific session (partial ID match)
ur history a3f2b1
```

## Providers

### Anthropic (default)

```bash
# .env
ANTHROPIC_API_KEY=sk-ant-...
UR_MODEL=anthropic/claude-sonnet-4-6
```

### Ollama (hosted or local)

```bash
# .env
UR_MODEL=ollama_chat/qwen2.5
UR_OLLAMA_BASE_URL=http://my-server:11434   # omit for localhost
```

Supported model prefixes: `ollama/` (generate API) and `ollama_chat/` (chat API).

## Architecture

```
src/ur/
├── config.py           Settings (pydantic-settings, platformdirs)
├── main.py             Typer CLI entry point
├── agent/
│   ├── loop.py         Core agentic loop — yields tokens, dispatches tools
│   ├── session.py      AgentSession — messages, usage, lifecycle
│   └── models.py       Message type alias, UsageStats
├── llm/
│   └── client.py       LiteLLM wrapper + CompletionStream
├── memory/
│   ├── db.py           SQLite schema, get_db context manager
│   └── session_store.py  save / list / get session messages
└── sandbox/            (Phase 2) ExecutionBackend protocol
    ├── subprocess_backend.py   Tier 1: default, zero deps
    ├── docker_backend.py       Tier 2: opt-in
    └── wasm_backend.py         Tier 3: stub / future
```

### Sandbox tiers

| Tier | Backend | Default | Requires |
|---|---|---|---|
| 1 | subprocess + psutil | yes | nothing |
| 2 | Docker / Podman | `--sandbox docker` | Docker daemon |
| 3 | WASM (wasmtime) | future | `pip install ur[sandbox-wasm]` |

### Optional extras

```bash
pip install -e ".[tools]"         # playwright, httpx, aiofiles, psutil
pip install -e ".[sandbox]"       # docker SDK (Tier 2)
pip install -e ".[api]"           # fastapi + uvicorn (REST/SSE)
pip install -e ".[memory]"        # chromadb + sentence-transformers
pip install -e ".[observability]" # opentelemetry, langfuse
pip install -e ".[queue]"         # arq (Redis-backed task queue)
```

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
@skip_if_not_anthropic   # skipped when UR_MODEL starts with "ollama"
@skip_if_not_ollama      # skipped when UR_MODEL starts with "anthropic"
```

## Roadmap

- **Phase 1** (done) — LLM loop, streaming CLI, SQLite sessions
- **Phase 2** — tool suite: filesystem, code execution, HTTP, browser
- **Phase 3** — full audit trail, token cost display, `ur history <id>`
- **Phase 4** — Docker sandbox, REST API, OpenTelemetry tracing
- **Phase 5** — long-term memory (ChromaDB), multi-task queue, multi-agent seams

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

```bash
# Install (dev)
pip install -e ".[dev]"
# or
uv pip install -e ".[dev]"

# First-time setup (creates data dirs + SQLite db)
ur init

# Run tests
pytest

# Run a single test
pytest tests/unit/test_config.py::test_name

# Run tests against Ollama instead of Gemini
UR_MODEL=ollama_chat/qwen2.5 UR_OLLAMA_BASE_URL=http://my-server:11434 pytest

# Lint
ruff check src tests

# Type check
mypy src
```

## Architecture

`ur` is a local-first AI agent sandbox. The agent loop streams tokens from an LLM via litellm and persists sessions to SQLite.

### Key design decisions

- **Messages are plain dicts** — litellm/OpenAI format `{"role": ..., "content": ...}`. No wrapper class.
- **Session lifecycle owned by CLI** — `loop.run()` does not call `session.complete()`; callers do.
- **Dumb loop, smart tools** — `agent/loop.py` is ~25 lines; tool dispatch comes in Phase 2.
- **Provider detected once at init** — `LLMClient.__init__` sets `self.provider` via `Provider` enum (`GEMINI`, `OLLAMA`, `OTHER`). The `stream()` method uses it to conditionally pass `api_key` or `api_base`.
- **`get_settings()` is a singleton** — returns a cached `Settings` instance; reset by setting `_settings = None` in tests.
- **No `__init__.py` in sub-packages** — `agent/`, `llm/`, `memory/` use namespace packages.
- **`GEMINI_API_KEY` has no `UR_` prefix** — read directly so litellm can also pick it up. All other settings use `UR_` prefix.

### Data flow

```
CLI (main.py)
  → AgentSession (agent/session.py)   # holds messages + usage
  → loop.run() (agent/loop.py)        # async generator, yields tokens
    → LLMClient.stream() (llm/client.py)  # wraps litellm.acompletion
      → CompletionStream              # async iter over chunks, accumulates full_text + usage
  → session_store.save_session()      # persists to SQLite via aiosqlite
```

### Testing conventions

- `asyncio_mode = "auto"` — all tests can be `async def` without decorators.
- `pytest-env` sets `UR_MODEL=gemini/gemini-test` and `GEMINI_API_KEY=gm-test-key` automatically.
- litellm is patched with `MockStreamWrapper` (defined in `tests/conftest.py`) — no real API calls.
- `make_chunk()` builds mock litellm streaming chunks.
- Provider-specific tests use `skip_if_not_gemini` / `skip_if_not_ollama` markers imported from `tests/conftest.py`.
- `tmp_settings` fixture wires `Settings` to `tmp_path` to avoid touching real data dirs.

### Optional extras

```bash
pip install -e ".[tools]"         # playwright, httpx, aiofiles, psutil
pip install -e ".[sandbox]"       # docker SDK (Tier 2 sandbox)
pip install -e ".[api]"           # fastapi + uvicorn
pip install -e ".[memory]"        # chromadb + sentence-transformers
pip install -e ".[observability]" # opentelemetry, langfuse
pip install -e ".[queue]"         # arq (Redis-backed task queue)
```

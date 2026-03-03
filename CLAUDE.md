# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

```bash
# Run all tests
uv run pytest

# Run a single test file
uv run pytest tests/unit/test_loop.py

# Run a single test
uv run pytest tests/unit/test_loop.py::test_name

# Run tests against Ollama
UR_MODEL=ollama_chat/qwen2.5 UR_OLLAMA_BASE_URL=http://my-server:11434 uv run pytest

# Lint
uv run ruff check src tests

# Type check
uv run mypy src

# Install with all dev deps
uv pip install -e ".[dev]"

# Install tool extras (required for shell/file/http/browser tools)
uv pip install -e ".[tools]"

# Download Chromium once for browser_get
uv run playwright install chromium
```

## Architecture

The core loop (`src/ur/agent/loop.py:run`) is an async generator that:
1. Calls `LLMClient.stream()` and yields `StreamChunk` tokens
2. If the model returns tool calls and a `ToolRegistry` is provided, dispatches them via `_execute_tool` and loops
3. If no registry or no tool calls, records the final assistant message and returns
4. Exits with an error message if `max_iterations` is exhausted

**Data flow:** `main.py` (Typer CLI) → `tui.py` (Textual TUI) → `agent/loop.run()` → `LLMClient.stream()` → `litellm` (Gemini) or `ollama.AsyncClient` (Ollama)

**Key types:**
- `StreamChunk` — has `kind` (`"content"`, `"reasoning"`, `"tool_call"`, `"tool_result"`) and `text`
- `AgentSession` — owns the message list and `UsageStats`; lifecycle managed by CLI callers, not `loop.run()`
- Messages are plain dicts in OpenAI/litellm format `{"role": ..., "content": ...}` with extra internal keys (`created_at`, `reasoning`) stripped before API calls

**LLM providers** (`src/ur/llm/client.py`):
- `Provider` enum detected once at `LLMClient.__init__` from the model name prefix
- Gemini → litellm (`acompletion`); Ollama → `ollama.AsyncClient` (native API, different streaming path via `OllamaCompletionStream`)
- Ollama requires `tool_calls[].function.arguments` as a dict (not JSON string) — handled by `_to_ollama_messages()`
- `GEMINI_API_KEY` env var has **no** `UR_` prefix; all others use `UR_` prefix

**Tool system:**
- `ToolRegistry` maps names to `ToolSpec(fn, schema)`. All tool functions are `async` and return `str`.
- Builtins in `tools/builtin.py`: `shell`, `read_file`, `write_file`, `http_get`, `browser_get`, `web_search`
- Plugins: drop a `.py` file in `<data_dir>/tools/` with a `register(registry: ToolRegistry)` function; loaded alphabetically after builtins; plugins can override builtins by reusing the same name
- Tool dispatch in `loop._execute_tool` catches all exceptions and returns the error as a string (never raises)

**Settings** (`src/ur/config.py`): pydantic-settings `Settings` class with `UR_` env prefix (except `GEMINI_API_KEY`); singleton via `@lru_cache get_settings()`. In tests, always construct `Settings(...)` directly (see `tmp_settings` fixture) to avoid cache contamination.

**Persistence:** aiosqlite at `settings.db_path` (`data_dir/ur.db`). `memory/db.py` owns schema and `get_db` context manager. `memory/session_store.py` provides save/list/load.

## Testing conventions

- `asyncio_mode = "auto"` — all async tests work without `@pytest.mark.asyncio`
- `pytest-env` sets `UR_MODEL=gemini/gemini-2.5-flash-lite` and `GEMINI_API_KEY=gm-test-key` as defaults
- No real API calls — patch litellm with `MockStreamWrapper` from `tests/conftest.py`
- Provider skip markers: `skip_if_not_gemini`, `skip_if_not_ollama` — import from `tests/conftest.py`
- Helper builders in `conftest.py`: `make_chunk()`, `make_tool_call_chunk()`
- One test file per source module under `tests/unit/`

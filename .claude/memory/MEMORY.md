# Project Memory: ur

## Overview
Local-first AI agent sandbox (Python 3.12+). Phase 1 complete (LLM loop + SQLite sessions). Phase 2 = tool suite.

## Key files
- `src/ur/config.py` — pydantic-settings `Settings`, `get_settings()` singleton
- `src/ur/llm/client.py` — `LLMClient`, `CompletionStream`, `Provider` enum
- `src/ur/agent/loop.py` — async generator yielding tokens; tool dispatch TBD in Phase 2
- `src/ur/agent/session.py` — `AgentSession` (messages + usage)
- `src/ur/memory/db.py` — SQLite schema, `get_db` context manager
- `src/ur/main.py` — Typer CLI entry point
- `tests/conftest.py` — `MockStreamWrapper`, `tmp_settings`, skip markers

## Architecture patterns
- Messages are plain dicts `{"role": ..., "content": ...}` (litellm/OpenAI format)
- `Provider` enum (GEMINI/OLLAMA/OTHER) detected once at `LLMClient.__init__`
- `GEMINI_API_KEY` env var has no `UR_` prefix; all others use `UR_` prefix
- No `__init__.py` in sub-packages (namespace packages)
- Session lifecycle owned by CLI callers, not `loop.run()`

## Test setup
- `asyncio_mode = "auto"` — all tests can be async
- `pytest-env` sets `UR_MODEL=gemini/gemini-test` and `GEMINI_API_KEY=gm-test-key`
- No real API calls — patch litellm with `MockStreamWrapper` from conftest
- Provider skip markers: `skip_if_not_gemini`, `skip_if_not_ollama` (import from conftest)

## Current branch: feat/phase-1 (Phase 1 done, no Phase 2 code yet)

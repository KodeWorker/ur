# Arch-Designer Agent Memory

## Key files (confirmed)
- `src/ur/agent/models.py` — `Message = dict[str, Any]`, `UsageStats` dataclass
- `src/ur/agent/session.py` — `AgentSession` dataclass; owns message list; embeds `created_at` in each msg dict (Phase 1 only — identified as leaky)
- `src/ur/agent/loop.py` — async generator; Phase 1 breaks after first LLM turn; Phase 2 will add tool dispatch here
- `src/ur/llm/client.py` — `LLMClient`, `CompletionStream`, `Provider` StrEnum
- `src/ur/memory/db.py` — SQLite schema via `SCHEMA` constant; `init_db`, `get_db` context manager
- `src/ur/memory/session_store.py` — `save_session`, `list_sessions`, `get_session_messages`
- `src/ur/main.py` — Typer CLI; CLI owns session lifecycle (create, save, status update)
- `tests/conftest.py` — `MockStreamWrapper`, `tmp_settings`, `make_chunk`, skip markers

## Confirmed architecture patterns
- Messages are plain dicts in litellm/OpenAI format; no wrapper classes
- `Provider` enum detected once at `LLMClient.__init__` from model string prefix
- No `__init__.py` in sub-packages (namespace packages)
- Session lifecycle owned by CLI callers (`main.py`), not `loop.run()`
- `asyncio_mode = "auto"` — all tests can be async
- litellm patched via `MockStreamWrapper` in tests; no real API calls

## Known structural debt (as of Phase 1 completion)
- `session_store.py` save path: `str(msg["content"])` for non-string content — silently
  corrupts lists/None; will break Phase 2 tool dispatch. See `issue-10-message-serialization.md`.
- `created_at` embedded in message dicts (session.py) — ur-internal concern leaking
  into OpenAI wire-format payload; litellm ignores it currently but it's incorrect.
- `Message = dict[str, Any]` — no TypedDict contract; mypy cannot distinguish roles.
- `messages` table missing `tool_calls`, `tool_call_id`, `name` columns for Phase 2.

## Links to detailed notes
- `issue-10-message-serialization.md` — full analysis and fix plan for Issue #10

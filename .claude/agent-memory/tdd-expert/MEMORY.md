# TDD Expert — Persistent Memory

## Project: ur (local-first AI agent sandbox)

### Key test files
- `tests/conftest.py` — `MockStreamWrapper`, `make_chunk`, `tmp_settings`, `skip_if_not_gemini`, `skip_if_not_ollama`
- `tests/unit/test_llm_client.py` — `CompletionStream`, `LLMClient`, `Provider` tests
- `tests/unit/test_loop.py` — `loop.run()` async generator tests
- `tests/unit/test_main.py` — `_run`, `_history` CLI helper tests (issues #65/#66)

### Confirmed behaviors (locked in by tests)
- `LLMClient.stream()` propagates `litellm.AuthenticationError` naturally (no try/except wrapping `acompletion`)
- `loop.run()` propagates exceptions from mid-stream iteration without swallowing them
- `_run()` marks session `"completed"` on success, `"failed"` on `Exception`, `"interrupted"` on `BaseException`
- `_run()` always calls `save_session` in the `finally` block regardless of outcome
- `_run()` raises `typer.Exit(1)` after catching `Exception` (not `BaseException`)

### litellm exception constructors
- `litellm.AuthenticationError(message, llm_provider, model)` — positional args required
- `litellm.APIConnectionError(message, llm_provider, model)` — same pattern
- Use `uv run python -c "import litellm, inspect; print(inspect.signature(litellm.X.__init__))"` to check

### Mock patterns for erroring streams
When testing that `loop.run()` propagates errors from mid-stream iteration:
- Create an `_ErroringChunkSource` class that yields real `make_chunk()` objects then raises
- Wrap it in `CompletionStream(_ErroringChunkSource())` — CompletionStream processes raw chunks
- Do NOT yield plain strings from the source — CompletionStream calls `.choices[0].delta.content` on each item
- Patch `ur.agent.loop.LLMClient` and set `.return_value.stream = AsyncMock(return_value=stream)`

### Mock patterns for _run / CLI helper tests
- `agent_run` is an async generator — use a helper returning `async def _stub(...): for c in chunks: yield c`
- To make an async generator raise: `async def _stub(...): raise exc; yield` (yield makes it an async gen)
- Patch `ur.main.Live` with a `MagicMock` context manager (set `__enter__`/`__exit__` on instance)
- Patch `ur.main.LLMClient` to avoid real network calls
- Patch `ur.main.init_db` with `AsyncMock` to skip DB init when testing in isolation
- `save_session` can be patched OR called for real against the tmp DB — both work

### _history prefix-lookup — empty string gotcha
- `_history` guards with `if session_id:` — an empty string `""` is falsy, falls through to list path
- To test the ambiguous-prefix branch, assign explicit IDs: `session.id = "aaaa0000-..."`
- `AgentSession` is a dataclass so `session.id` is directly assignable

### Characterization test pattern
When the production code is already correct but untested:
- Write the test, confirm it passes immediately (no RED phase for existing-correct behavior)
- This is valid TDD — locks in regression protection
- Note it as a "characterization test" in docstring/comment

### uv usage
- Always use `uv run pytest ...` and `uv run python ...` — `python` and `python3` are not on PATH

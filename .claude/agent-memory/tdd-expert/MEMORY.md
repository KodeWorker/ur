# TDD Expert — Persistent Memory

## Project: ur (local-first AI agent sandbox)

### Key test files
- `tests/conftest.py` — `MockStreamWrapper`, `make_chunk`, `tmp_settings`, `skip_if_not_gemini`, `skip_if_not_ollama`
- `tests/unit/test_llm_client.py` — `CompletionStream`, `LLMClient`, `Provider` tests
- `tests/unit/test_loop.py` — `loop.run()` async generator tests

### Confirmed behaviors (locked in by tests)
- `LLMClient.stream()` propagates `litellm.AuthenticationError` naturally (no try/except wrapping `acompletion`)
- `loop.run()` propagates exceptions from mid-stream iteration without swallowing them
- Both behaviors are characterization tests — code was already correct, tests add regression protection

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

### Characterization test pattern
When the production code is already correct but untested:
- Write the test, confirm it passes immediately (no RED phase for existing-correct behavior)
- This is valid TDD — locks in regression protection
- Note it as a "characterization test" in docstring/comment

### uv usage
- Always use `uv run pytest ...` and `uv run python ...` — `python` and `python3` are not on PATH

# Phase 3 — Audit Trail, Token Cost & Context Management

## Overview

Phase 3 has three goals:

1. **Audit trail** — full `ur history <id>` inspection of past sessions including tool
   calls and results.
2. **Token cost display** — live and end-of-session token usage with estimated cost
   shown in the TUI status bar.
3. **Context management** — prevent the agent loop from silently blowing the model's
   context window; give the operator control over how the message history is handled
   when it grows too large.

---

## 1. Audit Trail (`ur history <id>`)

`ur history <id>` is already implemented in `src/ur/main.py` (partial-ID lookup,
role-coloured output, tool-call/result rendering). Phase 3 has no new work here beyond
ensuring tool call/result messages saved by Phase 2 render correctly.

**Verification checklist:**
- [ ] `ur history <partial-id>` shows reasoning, tool calls, tool results, and final reply
- [ ] Ambiguous / not-found prefix returns a clear error

---

## 2. Token Cost Display

### Current state

`UsageStats` accumulates `input_tokens` and `output_tokens` per session.
`UrApp._update_status()` already writes a status-bar line; it currently shows
`{provider} · {model}` but not token counts or cost.

### Changes

#### `src/ur/config.py`

Add an optional cost-per-million-tokens setting so users can configure their own rates:

```python
cost_per_1m_input:  float | None = Field(default=None, env="UR_COST_PER_1M_INPUT")
cost_per_1m_output: float | None = Field(default=None, env="UR_COST_PER_1M_OUTPUT")
```

Default `None` means "display tokens only, no cost estimate".

#### `src/ur/tui.py` — `_update_status`

Extend the status bar to show running token totals and, when rates are configured,
a cost estimate:

```
gemini/gemini-2.0-flash  ·  ↑ 1 234  ↓ 456  ·  ~$0.0012
```

Format: `↑` = input tokens, `↓` = output tokens, cost only shown when both rate
fields are set.

#### `src/ur/main.py` — `ur history` table

Add a `Cost` column to the sessions table (shown only when rates are configured,
hidden otherwise to avoid confusion).

---

## 3. Context Management

### Problem

`session.messages` grows without bound. Every iteration of `loop.run()` sends the
full history to the LLM. For long `ur chat` sessions or agentic tasks with many tool
round-trips, this silently approaches (and eventually exceeds) the model's context
window, causing API errors or degraded responses with no warning.

Current code has no defence:

```python
# loop.py — every iteration sends everything
messages = session.messages
stream = await client.stream(messages, tools=tools_list)
```

### Design

Introduce a `ContextManager` that sits between the session message list and the API
call. It receives the full message list and returns a (possibly truncated) list safe
to send to the model, along with a flag indicating whether truncation occurred.

#### Strategy enum

```python
class ContextStrategy(StrEnum):
    SLIDING_WINDOW = "sliding_window"   # drop oldest non-system messages
    SUMMARISE      = "summarise"        # summarise dropped messages (Phase 5)
    RAISE          = "raise"            # error if limit exceeded
```

Phase 3 implements `SLIDING_WINDOW` and `RAISE`. `SUMMARISE` is reserved for Phase 5
(long-term memory).

#### `src/ur/context.py` — new module

```python
@dataclass
class ContextResult:
    messages: list[Message]   # safe-to-send slice
    dropped:  int             # number of messages removed (0 = no truncation)

class ContextManager:
    def __init__(
        self,
        max_tokens:       int,
        strategy:         ContextStrategy = ContextStrategy.SLIDING_WINDOW,
        token_estimator:  Callable[[list[Message]], int] = _estimate_tokens,
    ) -> None: ...

    def fit(self, messages: list[Message]) -> ContextResult: ...
```

`fit()` logic for `SLIDING_WINDOW`:

1. Separate the system message (index 0, if present) from the rest.
2. If `_estimate_tokens(messages) <= max_tokens`, return unchanged.
3. Drop oldest non-system messages one at a time until the estimate fits, keeping
   at minimum the last user message so the model always has context for the current
   turn.
4. Return `ContextResult(messages=trimmed, dropped=n)`.

**Token estimation** (`_estimate_tokens`): approximate with
`sum(len(json.dumps(m)) // 4 for m in messages)` — deliberately conservative
(real tokenisers count slightly fewer). No dependency on `tiktoken` or model-specific
tokenisers; this is a heuristic guard, not an exact budget.

#### `src/ur/config.py`

```python
context_max_tokens:  int            = Field(default=128_000,  env="UR_CONTEXT_MAX_TOKENS")
context_strategy:    ContextStrategy = Field(default=ContextStrategy.SLIDING_WINDOW,
                                             env="UR_CONTEXT_STRATEGY")
```

`UR_CONTEXT_MAX_TOKENS` default of 128 000 covers most current models (Gemini 2.x,
Llama 3.3, Qwen 2.5). Users running smaller models (e.g. `ollama/qwen2.5:7b`) should
lower it.

#### `src/ur/agent/loop.py`

Instantiate a `ContextManager` in `run()` and apply it before each API call:

```python
ctx_mgr = ContextManager(
    max_tokens=settings.context_max_tokens,
    strategy=settings.context_strategy,
)

for _ in range(max_iterations):
    ctx = ctx_mgr.fit(messages)
    if ctx.dropped:
        yield StreamChunk(kind="context_drop", text=str(ctx.dropped))
    stream = await client.stream(ctx.messages, tools=tools_list)
    ...
```

Add `"context_drop"` to `StreamChunk.kind` so the TUI can display a notice.

#### `src/ur/tui.py` — `TurnWidget.add_context_notice`

When the loop emits a `context_drop` chunk, mount a dim notice inside the current
`TurnWidget`:

```
⚠ context trimmed — 4 messages dropped to fit context window
```

This is a `Static` with `classes="context-notice"`, styled dim amber
(`color: $warning`), not a collapsible (it is short and always relevant).

#### `_stream_turn` handler

```python
elif chunk.kind == "context_drop":
    await turn.add_context_notice(int(chunk.text))
```

---

## File Change Summary

| File | Changes |
|---|---|
| `src/ur/config.py` | Add `cost_per_1m_input`, `cost_per_1m_output`, `context_max_tokens`, `context_strategy` |
| `src/ur/context.py` | New: `ContextStrategy`, `ContextResult`, `ContextManager`, `_estimate_tokens` |
| `src/ur/agent/loop.py` | Pass `ContextManager`; yield `context_drop` chunks; accept `settings` param |
| `src/ur/agent/models.py` | Add `"context_drop"` to `StreamChunk.kind` literal |
| `src/ur/tui.py` | `_update_status` shows tokens + cost; `TurnWidget.add_context_notice`; handle `context_drop` chunk |
| `src/ur/main.py` | Optional `Cost` column in `ur history` table |
| `tests/unit/test_context.py` | New: unit tests for `ContextManager.fit` across all strategies |
| `tests/unit/test_loop.py` | Add context truncation integration tests |
| `.env.example` | Document new `UR_CONTEXT_*` and `UR_COST_*` env vars |

---

## Out of Scope for Phase 3

- **Summarisation strategy** — requires an extra LLM call; deferred to Phase 5.
- **Exact token counting** — requires provider-specific tokenisers (`tiktoken` for
  OpenAI, Gemini's `count_tokens` API). The heuristic estimator is intentionally
  simple; exact counting can replace it in Phase 4/5 without changing the interface.
- **Per-message token display in `ur history`** — deferred.
- **Cost data for all models** — a model-cost registry is a Phase 4/5 concern;
  Phase 3 only exposes user-configurable rates.

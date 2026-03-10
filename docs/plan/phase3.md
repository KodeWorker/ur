# Phase 3 — Multi-Turn Chat and Persona

**Goal**: `ur chat` opens an interactive TUI session with persistent context. The agent builds a persona for the user over time.

**Prerequisite**: Phase 2 complete.

## Deliverables

- `ur chat [--continue=<id>] [--model=...]` — interactive TUI
- `ur history [<id>]` — list sessions or show messages for one session
- `ur persona` — display current persona key/value pairs

## TUI Backend — ftxui

Phase 3 adopts [ftxui](https://github.com/ArthurSonzogni/FTXUI) as the TUI
library, added via CMake FetchContent.

Rationale: ftxui's component model scales naturally across future phases without
a rewrite:

| Phase | UI additions |
|-------|-------------|
| 3 | Input box, scrollable chat history, spinner, collapsible reasoning block |
| 4 | Inline tool-call / tool-result display |
| 5 | Status bar: token count, context usage, compression indicator |
| 6 | Streaming token output |

Key design points:
- `Chat::run()` drives ftxui's `ScreenInteractive` event loop rather than a
  plain `while(getline)`. The provider call runs on a background thread and
  posts an event back to the UI thread on completion.
- Reasoning block rendered via ftxui `Collapsible` component — collapsed by
  default, user can expand with a keypress.
- Spinner is an ftxui `Spinner` or animated `Text` component on the UI thread,
  driven by a periodic `PostEvent` from the background thread.

CMake targets used: `ftxui::component`, `ftxui::dom`, `ftxui::screen`.

## Source Files to Create

```
src/ur/agent/chat.cpp/.hpp          Multi-turn loop, context window management
src/ur/agent/persona_updater.cpp    Extracts persona facts from conversation and upserts
src/ur/cli/tui.cpp/.hpp             ftxui-based TUI wrapper (input, chat view, spinner)
tests/unit/test_chat.cpp
tests/unit/test_persona_updater.cpp
```

## Chat Loop

```
1. Load prior messages if --continue=<id> (resume session)
2. Otherwise create a new session
3. Loop:
   a. Read user input from TUI
   b. Append user message to context + DB
   c. Start spinner animation on a background thread; call provider.complete(context); stop spinner on return
   d. Strip `<think>…</think>` from response:
      - If reasoning present: save to DB as role `"reason"`, render as collapsible block in TUI (collapsed by default)
      - Save cleaned content to DB as role `"assistant"`, append to in-memory context
   e. Stream/print response (see Reasoning Display below)
   f. Run persona_updater on latest exchange
   g. Repeat until user exits (Ctrl-C or `/exit` slash command)
      - Input starting with `/` is a TUI slash command, not sent to the agent
      - `/exit` — quit the session
```

## Reasoning Display (Thinking Models)

`ur` targets llama.cpp as its primary backend. When running thinking models
(DeepSeek-R1, QwQ, etc.) through llama.cpp server, reasoning is always embedded
as a `<think>…</think>` block at the start of `content` — there is no separate
`reasoning_content` field.

Design:
- `HttpProvider::complete()` returns the full raw `content` string unchanged
- `Chat` (Phase 3) strips the `<think>…</think>` prefix from `content` before
  storing the assistant message and before passing it back to the persona updater
- The stripped reasoning block is rendered as a collapsible block above the
  final answer (collapsed by default); `ur run` discards it silently
- Reasoning is **not** stored in the database — it is ephemeral display only

## Context Window Management

- Keep a rolling window of the last N messages (configurable, default 20)
- Always include the system prompt if provided
- Do not truncate the persona — inject it as part of the system prompt

## Persona Updater

After each assistant turn, run a lightweight secondary LLM call (or regex heuristics) to extract facts about the user and upsert them into the `persona` table.

### Meaningful-turn filter

Skip the persona updater unless **both** conditions hold:

- **Length** — user message exceeds 50 characters
- **Depth** — session has at least 3 completed exchanges (6 messages in context)

These thresholds are intentionally conservative starting points and should be
tuned once real usage data is available. A "hi" or single-word reply never
triggers extraction. Open questions for later refinement:

- Should depth count only user messages, or all messages?
- Should a very long first message (e.g. a detailed self-introduction) bypass the depth check?
- Is 50 chars the right length floor, or should it be token-based?

```sql
INSERT INTO persona (key, value, updated_at)
VALUES (?, ?, ?)
ON CONFLICT(key) DO UPDATE SET value=excluded.value, updated_at=excluded.updated_at;
```

## `ur history` Output

```
# ur history          → list sessions
<id>  <title>  <created_at>  <model>

# ur history <id>     → messages for session
[user]      hello
[reason]    ...
[assistant] hi there...
```

## `ur persona` Output

```
name:       Alice
timezone:   UTC+9
interests:  Rust, distributed systems
```

## Memory Storage Architecture

Two storage layers, each responsible for a distinct concern:

| Layer | Backend | Used for |
|-------|---------|----------|
| Structured | SQLite (`database/ur.db`) | Sessions, messages, persona key-value pairs |
| Semantic | Flat-file (`database/memory.bin`) | Vector embeddings for long-term memory (future phase) |

Phase 3 only implements the structured layer. The flat-file vector store is deferred to a later phase but the split is established here so the two layers are never conflated.

Design rules:
- SQLite is the source of truth for all queryable, relational data
- Vector embeddings are write-once, append-only blobs — no joins, no transactions needed
- No external vector database or sqlite extensions required

## Acceptance Criteria

- [ ] `ur chat` enters the loop and responds to input
- [ ] `--continue=<id>` resumes a previous session with full history
- [ ] Messages are persisted after each turn (crash-safe)
- [ ] `ur history` lists sessions; `ur history <id>` shows messages (decrypted transparently)
- [ ] `ur persona` shows accumulated key/value pairs (decrypted transparently)

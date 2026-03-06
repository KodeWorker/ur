# Phase 3 — Multi-Turn Chat and Persona

**Goal**: `ur chat` opens an interactive TUI session with persistent context. The agent builds a persona for the user over time.

**Prerequisite**: Phase 2 complete.

## Deliverables

- `ur chat [--continue=<id>] [--model=...]` — interactive TUI
- `ur history [<id>]` — list sessions or show messages for one session
- `ur persona` — display current persona key/value pairs

## Source Files to Create

```
src/ur/agent/chat.cpp/.hpp          Multi-turn loop, context window management
src/ur/agent/persona_updater.cpp    Extracts persona facts from conversation and upserts
src/ur/cli/tui.cpp/.hpp             Minimal line-based TUI (input prompt, streaming output)
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
   c. Call provider.complete(context)
   d. Append assistant message to context + DB
   e. Stream/print response
   f. Run persona_updater on latest exchange
   g. Repeat until user exits (Ctrl-C / "exit" / "quit")
```

## Context Window Management

- Keep a rolling window of the last N messages (configurable, default 20)
- Always include the system prompt if provided
- Do not truncate the persona — inject it as part of the system prompt

## Persona Updater

After each assistant turn, run a lightweight secondary LLM call (or regex heuristics) to extract facts about the user and upsert them into the `persona` table.

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
[assistant] hi there...
```

## `ur persona` Output

```
name:       Alice
timezone:   UTC+9
interests:  Rust, distributed systems
```

## Acceptance Criteria

- [ ] `ur chat` enters the loop and responds to input
- [ ] `--continue=<id>` resumes a previous session with full history
- [ ] Messages are persisted after each turn (crash-safe)
- [ ] `ur history` lists sessions; `ur history <id>` shows messages
- [ ] `ur persona` shows accumulated key/value pairs

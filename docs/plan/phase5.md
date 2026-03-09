# Phase 5 — Memory and Context

**Goal**: replace naive rolling-window truncation with context compression; add context usage
display in the TUI; establish long-term semantic memory via a flat-file vector store.

**Prerequisite**: Phase 4 complete.

## Deliverables

- Context compression: summarise the oldest messages when the context window fills up,
  preserving meaning without hard truncation
- Context usage display: show token count and window utilisation in the TUI status line
- Long-term memory: embed each assistant turn and persist vectors to
  `$root/database/memory.bin`; retrieve semantically relevant past exchanges at chat start

## Source Files to Create

```
src/ur/agent/context_manager.cpp/.hpp   Token counting, compression, retrieval injection
src/ur/memory/vector_store.cpp/.hpp     Flat-file vector index: append, load, cosine search
tests/unit/test_context_manager.cpp
tests/unit/test_vector_store.cpp
```

## Context Manager

Replaces the Phase 3 rolling-window approach.

```
On each turn before calling provider.complete():
1. Count tokens in current context (estimate: chars / 4)
2. If tokens > threshold (default: 80% of model context length):
   a. Take the oldest N messages beyond the keep-recent window
   b. Call provider with a summarise prompt → condensed paragraph
   c. Replace those N messages with a single synthetic [summary] message
   d. Persist summary to DB as a message with role='summary'
3. Inject top-K semantically relevant past messages from vector store
   (retrieved by embedding the latest user message)
```

### Token Budget

| Slot | Budget |
|------|--------|
| System prompt + persona | reserved, never compressed |
| Recent messages (keep-recent window) | last 10 turns |
| Retrieved memory | top 3 results |
| Compression trigger | 80% of model context length |

## Vector Store

Flat-file format (`memory.bin`): append-only binary records.

```
Each record:
  [8 bytes: message_id length][message_id bytes]
  [4 bytes: dimension count N]
  [N × 4 bytes: float32 embedding values]
```

Operations:
- `append(message_id, embedding)` — write one record to end of file
- `load()` → vector of (message_id, embedding) pairs
- `search(query_embedding, top_k)` → top-K message_ids by cosine similarity

Embeddings are generated via the LLM provider's embeddings endpoint
(`POST /v1/embeddings`), using the same `UR_LLM_BASE_URL`.

## Context Usage Display

Shown in the TUI status line (Phase 5 adds this to the existing line from Phase 3):

```
model: mistral | session: abc123 | turns: 12 | ctx: 3240 / 4096 (79%)
```

Token count is estimated locally (chars / 4) — no extra API call.

## Acceptance Criteria

- [ ] Context is compressed when token usage exceeds 80% of the configured limit
- [ ] Compressed summaries are persisted in the DB and visible in `ur history`
- [ ] Vector store appends an embedding after each assistant turn
- [ ] Top-3 semantically relevant past messages are injected into context on retrieval
- [ ] TUI status line shows live token count and window utilisation percentage
- [ ] `vector_store` unit tests cover append, load, and cosine search correctness
